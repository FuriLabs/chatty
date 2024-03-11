/* chatty-file-item.c
 *
 * Copyright 2023 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-file-item"

#include <glib/gi18n.h>
#include <gst/gst.h>

#include "chatty-enums.h"
#include "chatty-file.h"
#include "chatty-chat-page.h"
#include "chatty-progress-button.h"
#include "chatty-message.h"
#include "chatty-file-item.h"
#include "chatty-settings.h"


struct _ChattyFileItem
{
  AdwBin         parent_instance;

  GtkWidget     *file_overlay;
  GtkWidget     *file_widget;
  GtkWidget     *progress_button;
  GtkWidget     *file_title;
  GtkWidget     *video_frame;
  GtkWidget     *video;

  ChattyMessage *message;
  ChattyFile    *file;
  GdkPixbufAnimation     *pixbuf_animation;
  GdkPixbufAnimationIter *animation_iter;
  guint                   animation_id;
};

G_DEFINE_TYPE (ChattyFileItem, chatty_file_item, ADW_TYPE_BIN)

static void
file_item_get_stream_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  g_autoptr(ChattyFileItem) self = user_data;
  g_autoptr(GInputStream) stream = NULL;

  if (gtk_widget_in_destruction (GTK_WIDGET (self)))
    return;

  stream = chatty_file_get_stream_finish (self->file, result, NULL);

  if (!stream)
    return;
}

static void
image_item_paint (ChattyFileItem *self,
                  GdkPixbuf      *pixbuf)
{
  if (pixbuf) {
    gtk_widget_remove_css_class (self->file_widget, "dim-label");
    gtk_image_set_from_gicon (GTK_IMAGE (self->file_widget), G_ICON (pixbuf));
    gtk_image_set_pixel_size (GTK_IMAGE (self->file_widget), 220);
  } else {
    gtk_widget_add_css_class (self->file_widget, "dim-label");
    gtk_image_set_from_icon_name (GTK_IMAGE (self->file_widget),
                                  "image-x-generic-symbolic");
    return;
  }
}

static void
image_item_get_stream_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  g_autoptr(ChattyFileItem) self = user_data;
  g_autoptr(GInputStream) stream = NULL;
  g_autoptr(GdkPixbuf) pixbuf = NULL;
  int scale_factor;

  if (gtk_widget_in_destruction (GTK_WIDGET (self)))
    return;

  stream = chatty_file_get_stream_finish (self->file, result, NULL);

  if (!stream)
    return;

  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));
  pixbuf = gdk_pixbuf_new_from_stream_at_scale (stream, 200 * scale_factor,
                                                -1, TRUE, NULL, NULL);
  image_item_paint (self, pixbuf);
}

static gboolean
image_item_update_animation_cb (gpointer user_data)
{
  ChattyFileItem *self = user_data;
  int timeout = 0;

  if (gdk_pixbuf_animation_iter_advance (self->animation_iter, NULL)) {
    GdkPixbuf *pixbuf;

    pixbuf = gdk_pixbuf_animation_iter_get_pixbuf (self->animation_iter);
    gtk_image_set_from_gicon (GTK_IMAGE (self->file_widget), G_ICON (pixbuf));
    gtk_image_set_pixel_size (GTK_IMAGE (self->file_widget), 220);

    timeout = gdk_pixbuf_animation_iter_get_delay_time (self->animation_iter);
    if (timeout > 0)
       self->animation_id = g_timeout_add (timeout,
                                           image_item_update_animation_cb,
                                           self);
  }

  if (timeout <= 0) {
    g_clear_object (&self->animation_iter);
    self->animation_id = 0;
  }

  return G_SOURCE_REMOVE;
}

static void
process_vcard (ChattyFileItem *self)
{
  g_autoptr(GFile) text_file = NULL;
  g_autofree char *contents = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) lines = NULL;
  gsize length;

  gtk_label_set_text (GTK_LABEL (self->file_title), _("Contact"));

  if (chatty_message_get_cm_event (self->message))
    text_file = g_file_new_build_filename (chatty_file_get_path (self->file), NULL);
  else
    text_file = g_file_new_build_filename (g_get_user_data_dir (), "chatty", chatty_file_get_path (self->file), NULL);

  g_file_load_contents (text_file,
                        NULL,
                        &contents,
                        &length,
                        NULL,
                        &error);

  if (error) {
    g_warning ("error opening file: %s", error->message);
    return;
  }

  lines = g_strsplit_set (contents, "\r\n", -1);
  for (int i = 0; lines[i] != NULL; i++) {
     if (g_str_has_prefix (lines[i], "FN:")) {
       g_autofree char *contact = NULL;

       contact = g_strdup_printf (_("Contact: %s"), lines[i] + 3);
       gtk_label_set_text (GTK_LABEL (self->file_title), contact);
       /*
        * Formatted name ("FN:") is better formatted and required in later versions
        * of vCals vs. Name ("N:"). Since we found it, we can be done searching.
        */
       break;
     }
     if (g_str_has_prefix (lines[i], "N:")) {
       g_autofree char *contact = NULL;
       g_auto(GStrv) tokens = NULL;
       unsigned int token_length = 0;

       tokens = g_strsplit_set (lines[i] + 2, ";", 5);
       token_length = g_strv_length (tokens);
       /* https://www.rfc-editor.org/rfc/rfc2426#section-3.1.2  This should be a length of 5, but let's not risk it*/
       /* Translators: Order is: Family Name, Given Name, Additional/Middle Names, Honorific Prefixes, and Honorific Suffixes */
       if (token_length == 5) {
         if (*tokens[3])
           contact = g_strdup_printf (_("Contact: %s %s %s %s"), tokens[3], tokens[1], tokens[0], tokens[4]);
         else
           contact = g_strdup_printf (_("Contact: %s %s %s"), tokens[1], tokens[0], tokens[4]);

       } else if (token_length == 4) {
         if (*tokens[3])
           contact = g_strdup_printf (_("Contact: %s %s %s"), tokens[3], tokens[1], tokens[0]);
         else
           contact = g_strdup_printf (_("Contact: %s %s"), tokens[1], tokens[0]);

       } else if (token_length == 3 || token_length == 2)
         contact = g_strdup_printf (_("Contact: %s %s"), tokens[1], tokens[0]);
       else
         contact = g_strdup_printf (_("Contact: %s"), tokens[0]);

       gtk_label_set_text (GTK_LABEL (self->file_title), contact);
       /*
        * Formatted name ("FN:") is better formatted and required in later versions
        * of vCals vs. Name ("N:"). So if we find Name ("N:"), let's keep
        * searching for formatted name.
        */
       continue;
     }
  }
}

static void
process_vcal (ChattyFileItem *self)
{
  g_autoptr(GFile) text_file = NULL;
  g_autofree char *contents = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) lines = NULL;
  gsize length;

  gtk_label_set_text (GTK_LABEL (self->file_title), _("Invite"));

  if (chatty_message_get_cm_event (self->message))
    text_file = g_file_new_build_filename (chatty_file_get_path (self->file), NULL);
  else
    text_file = g_file_new_build_filename (g_get_user_data_dir (), "chatty", chatty_file_get_path (self->file), NULL);

  g_file_load_contents (text_file,
                        NULL,
                        &contents,
                        &length,
                        NULL,
                        &error);

  if (error) {
    g_warning ("error opening file: %s", error->message);
    return;
  }

  lines = g_strsplit_set (contents, "\r\n", -1);
  for (int i = 0; lines[i] != NULL; i++) {
     if (g_str_has_prefix (lines[i], "SUMMARY")) {
       g_auto(GStrv) tokens = NULL;
       unsigned int token_length = 0;

       tokens = g_strsplit_set (lines[i] + 2, ":", 2);
       token_length = g_strv_length (tokens);
       /* https://www.rfc-editor.org/rfc/rfc5545#section-3.8.1.12  This should be a length of 2, but let's not risk it*/
       if (token_length == 2) {
         g_autofree char *calendar = NULL;
         calendar = g_strdup_printf (_("Invite: %s"), tokens[1]);
         gtk_label_set_text (GTK_LABEL (self->file_title), calendar);
       }

       break;
     }
  }
}

static void
update_file_widget_icon (ChattyFileItem *self,
                         const char *file_mime_type)
{
  ChattyMsgType msg_type = CHATTY_MESSAGE_UNKNOWN;
  if (file_mime_type && *file_mime_type) {
    /* g_content_type_get_symbolic_icon () thinks vcards and vcalendars are text files */
    if (strstr (file_mime_type, "vcard")) {
      gtk_image_set_from_icon_name (GTK_IMAGE (self->file_widget), "contact-new-symbolic");
      process_vcard (self);
    } else if (strstr (file_mime_type, "calendar")) {
      gtk_image_set_from_icon_name (GTK_IMAGE (self->file_widget), "x-office-calendar-symbolic");
      process_vcal (self);
    } else {
      g_autoptr(GIcon) new_file_widget = NULL;
      new_file_widget = g_content_type_get_symbolic_icon (file_mime_type);
      gtk_image_set_from_gicon (GTK_IMAGE (self->file_widget), new_file_widget);
    }
  }

  /*
   * If we don't have a MIME type, try to guess the type baed on the message.
   * This is useful for Matrix, were we may not know the MIME type until it
   * is downloaded.
   */
  if (self->message && (!file_mime_type || !*file_mime_type))
    msg_type = chatty_message_get_msg_type (self->message);

  switch (msg_type)
    {
    case CHATTY_MESSAGE_IMAGE:
      gtk_image_set_from_icon_name (GTK_IMAGE (self->file_widget), "image-x-generic-symbolic");
      break;
    case CHATTY_MESSAGE_VIDEO:
      gtk_image_set_from_icon_name (GTK_IMAGE (self->file_widget), "video-x-generic-symbolic");
      break;
    case CHATTY_MESSAGE_AUDIO:
      gtk_image_set_from_icon_name (GTK_IMAGE (self->file_widget), "audio-x-generic-symbolic");
      break;
    case CHATTY_MESSAGE_FILE:
    case CHATTY_MESSAGE_UNKNOWN:
    case CHATTY_MESSAGE_TEXT:
    case CHATTY_MESSAGE_HTML:
    case CHATTY_MESSAGE_HTML_ESCAPED:
    case CHATTY_MESSAGE_MATRIX_HTML:
    case CHATTY_MESSAGE_LOCATION:
    case CHATTY_MESSAGE_MMS:
    default:
      break;
    }
}

static gboolean
item_set_file (gpointer user_data)
{
  ChattyFileItem *self = user_data;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFileInfo) file_info = NULL;
  ChattySettings *settings;
  g_autofree char *file_mime_type = NULL;
  unsigned int gst_version_major = 0;
  unsigned int gst_version_minor = 0;
  unsigned int gst_version_micro = 0;
  unsigned int gst_version_nano = 0;

  /* It's possible that we get signals after dispose().
   * Fix warning in those cases
   */
  if (!self->file)
    return G_SOURCE_REMOVE;

  if (chatty_message_get_cm_event (self->message))
    file = g_file_new_build_filename (chatty_file_get_path (self->file), NULL);
  else
    file = g_file_new_build_filename (g_get_user_data_dir (), "chatty", chatty_file_get_path (self->file), NULL);

  file_info = g_file_query_info (file,
                                 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                 G_FILE_QUERY_INFO_NONE,
                                 NULL,
                                 &error);
  if (error)
    g_warning ("Cannot figure out file info: '%s'", error->message);

  g_clear_error (&error);

  /*
   * Before you download a file from Matrix, you may not know what type of file
   * it is yet. Now that we downloaded it, let's figure it out so render it
   * and/or give it the right icon.
   */
  file_mime_type = g_strdup (chatty_file_get_mime_type (self->file));
  if (!file_mime_type) {
    if (g_file_info_get_content_type (file_info) == NULL) {
      file_mime_type = g_strdup ("application/octet-stream");
    } else {
      file_mime_type = g_content_type_get_mime_type (g_file_info_get_content_type (file_info));
      if (file_mime_type == NULL) {
        if (g_file_info_get_content_type (file_info) != NULL)
          file_mime_type = g_strdup (g_file_info_get_content_type (file_info));
        else
          file_mime_type = g_strdup ("application/octet-stream");
      }
    }
    update_file_widget_icon (self, file_mime_type);
  }

  settings = chatty_settings_get_default ();
  if (!chatty_settings_get_render_attachments (settings)) {
    chatty_file_get_stream_async (self->file, NULL,
                                  file_item_get_stream_cb,
                                  g_object_ref (self));

    return G_SOURCE_REMOVE;
  }
  gst_version (&gst_version_major,
               &gst_version_minor,
               &gst_version_micro,
               &gst_version_nano);

  if ((file_mime_type && g_str_has_prefix (file_mime_type, "image")) ||
      chatty_message_get_msg_type (self->message) == CHATTY_MESSAGE_IMAGE) {
    g_autofree char *path = NULL;

    if (chatty_message_get_cm_event (self->message))
      path = g_strdup (chatty_file_get_path (self->file));
    else
      path = g_build_path (G_DIR_SEPARATOR_S, g_get_user_data_dir (), "chatty", chatty_file_get_path (self->file), NULL);

    self->pixbuf_animation = gdk_pixbuf_animation_new_from_file (path, &error);
    if (!error && !gdk_pixbuf_animation_is_static_image (self->pixbuf_animation)) {
      GdkPixbuf *pixbuf;
      int timeout = 0;

      gtk_widget_remove_css_class (self->file_widget, "dim-label");

      self->animation_iter = gdk_pixbuf_animation_get_iter (self->pixbuf_animation, NULL);
      pixbuf = gdk_pixbuf_animation_iter_get_pixbuf (self->animation_iter);
      gtk_image_set_from_gicon (GTK_IMAGE (self->file_widget), G_ICON (pixbuf));
      gtk_image_set_pixel_size (GTK_IMAGE (self->file_widget), 220);

      timeout = gdk_pixbuf_animation_iter_get_delay_time (self->animation_iter);
      if (timeout > 0)
        self->animation_id = g_timeout_add (timeout,
                                            image_item_update_animation_cb,
                                            self);
      /*
       * chatty_file_get_stream_async () breaks the animation, so don't let
       * it run
       */
      return G_SOURCE_REMOVE;

    } else if (gdk_pixbuf_animation_is_static_image (self->pixbuf_animation)) {
      g_clear_object (&self->pixbuf_animation);

      chatty_file_get_stream_async (self->file, NULL,
                                    image_item_get_stream_cb,
                                    g_object_ref (self));

      return G_SOURCE_REMOVE;
    } else if (error)
      g_warning ("Error getting animation from file: '%s'", error->message);
  /*
   * For some reason, with gstreamer 1.22.10, some webm videos don't work with GtkVideo
   * on the Librem5 nor Pinephone Pro (arm64), but work fine on my laptop (amd64). Per:
   *
   * https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/3384#note_2326261
   *
   * This is fixed in 1.24. I confirmed it works in Flatpak gtk4-widget-factory,
   * but I couldn't confirm in Sid.
   *
   * Considering MMS does not "officially" support webm, and Chatty matrix
   * doesn't show videos, the author of this comment is probably the only
   * person who will actually notice a difference.
   *
   * TODO: Fix this when the fix is in gstreamer.
   */
  #if defined(__x86_64__) || defined(_M_X64)
  } else if (
             (file_mime_type && g_str_has_prefix (file_mime_type, "video")) ||
             chatty_message_get_msg_type (self->message) == CHATTY_MESSAGE_VIDEO
            ) {
  #else
  } else if (
             (
              ((gst_version_major >= 1) && (gst_version_minor >= 24)) ||
               (file_mime_type && (strstr (file_mime_type, "webm") == NULL))
             ) &&
             (
              (file_mime_type && g_str_has_prefix (file_mime_type, "video")) ||
              chatty_message_get_msg_type (self->message) == CHATTY_MESSAGE_VIDEO
             )
            ) {
  #endif
    /* If there is an error loading the file, don't show the video widget */
    if (file_info) {
      GtkMediaStream *media_stream;
      const GError *media_error = NULL;

      gtk_widget_set_visible (self->file_overlay, FALSE);
      gtk_widget_set_visible (self->video_frame, TRUE);
      gtk_widget_set_size_request (self->video, 220, 220);
      gtk_video_set_file (GTK_VIDEO (self->video), file);

      /*
       * Since Chatty is mainly a mobile program, mute the video by default.
       * Nobody likes "that guy" who blasts a video in a crowded space.
       */
      media_stream = gtk_video_get_media_stream (GTK_VIDEO (self->video));
      if (media_stream) {
        gtk_media_stream_set_muted (media_stream, TRUE);
        media_error = gtk_media_stream_get_error (media_stream);
        if (media_error) {
          g_warning ("Error in Media Stream: '%s'", media_error->message);
          gtk_widget_set_visible (self->file_overlay, TRUE);
          gtk_widget_set_visible (self->video_frame, FALSE);
        }
      }
    }
  }

  chatty_file_get_stream_async (self->file, NULL,
                                file_item_get_stream_cb,
                                g_object_ref (self));
  return G_SOURCE_REMOVE;
}

static void
file_item_update_message (ChattyFileItem *self)
{
  ChattyFileStatus status;

  g_assert (CHATTY_IS_FILE_ITEM (self));
  g_assert (self->message);
  g_assert (self->file);

  status = chatty_file_get_status (self->file);

  if (status == CHATTY_FILE_UNKNOWN)
    chatty_progress_button_set_fraction (CHATTY_PROGRESS_BUTTON (self->progress_button), 0.0);
  else if (status == CHATTY_FILE_DOWNLOADING)
    chatty_progress_button_pulse (CHATTY_PROGRESS_BUTTON (self->progress_button));
  else
    gtk_widget_set_visible (self->progress_button, FALSE);

  /* Update in idle so that self is added to the parent container */
  if (status == CHATTY_FILE_DOWNLOADED)
    g_idle_add (item_set_file, self);
}

static void
file_progress_button_action_clicked_cb (ChattyFileItem *self)
{
  GtkWidget *view;

  g_assert (CHATTY_IS_FILE_ITEM (self));

  view = gtk_widget_get_ancestor (GTK_WIDGET (self), CHATTY_TYPE_CHAT_PAGE);

  g_signal_emit_by_name (view, "file-requested", self->message);
}

static void
file_item_button_clicked_cb (GObject         *file_launcher,
                             GAsyncResult    *response,
                             gpointer         user_data)
{
  ChattyFileItem *self = CHATTY_FILE_ITEM (user_data);
  g_autoptr(GError) error = NULL;

  g_assert (CHATTY_IS_FILE_ITEM (self));

  if (!gtk_file_launcher_launch_finish (GTK_FILE_LAUNCHER (file_launcher), response, &error)) {
    if (!g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
      g_warning ("Error getting file: %s", error->message);
  }

  g_clear_object (&file_launcher);
}

static void
file_item_button_clicked (ChattyFileItem *self)
{
  g_autoptr(GFile) file = NULL;
  GList *file_list;
  const char *path;
  GtkFileLauncher *file_launcher = NULL;
  GtkWindow *window;

  g_assert (CHATTY_IS_FILE_ITEM (self));
  g_assert (self->file);

  file_list = chatty_message_get_files (self->message);

  if (!file_list || !file_list->data)
    return;

  path = chatty_file_get_path (file_list->data);
  if (!path)
    return;

  if (chatty_message_get_cm_event (self->message))
    file = g_file_new_build_filename (path, NULL);
  else
    file = g_file_new_build_filename (g_get_user_data_dir (), "chatty", path, NULL);

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
  file_launcher = gtk_file_launcher_new (file);
  gtk_file_launcher_launch (file_launcher, window, NULL, file_item_button_clicked_cb, self);
}

static void
chatty_file_item_dispose (GObject *object)
{
  ChattyFileItem *self = (ChattyFileItem *)object;
  GtkMediaStream *media_stream;

  g_clear_object (&self->message);
  g_clear_object (&self->file);
  g_clear_object (&self->pixbuf_animation);
  g_clear_object (&self->animation_iter);

  if (self->animation_id != 0)
    g_source_remove (self->animation_id);

  /*
   * For some reason, when the widget is destroyed, the media still plays.
   * Let's stop it when we close out.
   */
  media_stream = gtk_video_get_media_stream (GTK_VIDEO (self->video));
  if (media_stream)
    gtk_media_stream_set_playing (media_stream, FALSE);

  G_OBJECT_CLASS (chatty_file_item_parent_class)->dispose (object);
}

static void
chatty_file_item_class_init (ChattyFileItemClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = chatty_file_item_dispose;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/Chatty/"
                                               "ui/chatty-file-item.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyFileItem, file_overlay);
  gtk_widget_class_bind_template_child (widget_class, ChattyFileItem, file_widget);
  gtk_widget_class_bind_template_child (widget_class, ChattyFileItem, progress_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyFileItem, file_title);
  gtk_widget_class_bind_template_child (widget_class, ChattyFileItem, video_frame);
  gtk_widget_class_bind_template_child (widget_class, ChattyFileItem, video);

  gtk_widget_class_bind_template_callback (widget_class, file_progress_button_action_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, file_item_button_clicked);

  g_type_ensure (CHATTY_TYPE_PROGRESS_BUTTON);
}

static void
chatty_file_item_init (ChattyFileItem *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
chatty_file_item_new (ChattyMessage *message,
                      ChattyFile    *file,
                      const char    *file_mime_type)
{
  ChattyFileItem *self;
  const char *file_name;

  g_return_val_if_fail (CHATTY_IS_MESSAGE (message), NULL);

  if (!file) {
    GList *files;

    files = chatty_message_get_files (message);
    if (files)
      file = files->data;
  }

  g_return_val_if_fail (CHATTY_IS_FILE (file), NULL);

  self = g_object_new (CHATTY_TYPE_FILE_ITEM, NULL);
  self->message = g_object_ref (message);
  self->file = g_object_ref (file);

  file_name = chatty_file_get_name (file);
  gtk_widget_set_visible (self->file_title, file_name && *file_name);

  if (file_name)
    gtk_label_set_text (GTK_LABEL (self->file_title), file_name);

  update_file_widget_icon (self, file_mime_type);

  g_signal_connect_object (file, "status-changed",
                           G_CALLBACK (file_item_update_message),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self, "notify::scale-factor",
                           G_CALLBACK (file_item_update_message),
                           self, G_CONNECT_SWAPPED);
  file_item_update_message (self);

  return GTK_WIDGET (self);
}

ChattyMessage *
chatty_file_item_get_item (ChattyFileItem *self)
{
  g_return_val_if_fail (CHATTY_IS_FILE_ITEM (self), NULL);

  return self->message;
}
