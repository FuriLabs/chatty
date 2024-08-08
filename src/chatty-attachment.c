/* chatty-attachment.c
 *
 * Copyright 2021 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-attachment"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "chatty-utils.h"
#include "chatty-attachment.h"
#include "chatty-log.h"

struct _ChattyAttachment
{
  GtkBox        parent_instance;

  GtkWidget    *overlay;
  GtkWidget    *label;
  GtkWidget    *remove_button;
  GtkWidget    *spinner;

  GCancellable *cancellable;
  GFile        *file;
};

G_DEFINE_TYPE (ChattyAttachment, chatty_attachment, GTK_TYPE_BOX)

typedef struct _AttachmentData {
  ChattyAttachment *self;
  GtkWidget *image;
} AttachmentData;

enum {
  DELETED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static void attachment_data_free (AttachmentData *data);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (AttachmentData, attachment_data_free)

static void
attachment_data_free (AttachmentData *data)
{
  if (!data)
    return;

  g_clear_object (&data->image);
  g_clear_object (&data->self);
  g_free (data);
}

static void
attachment_update_image (ChattyAttachment *self,
                         GtkWidget        *image,
                         GFile            *file)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GFileInfo) file_info = NULL;
  const char *thumbnail;

  g_assert (CHATTY_IS_ATTACHMENT (self));
  g_assert (image);
  g_assert (file);

  file_info = g_file_query_info (file,
                                 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
                                 G_FILE_ATTRIBUTE_STANDARD_ICON ","
                                 G_FILE_ATTRIBUTE_THUMBNAIL_PATH,
                                 G_FILE_QUERY_INFO_NONE,
                                 NULL, &error);
  if (error)
    g_warning ("Error querying info: %s", error->message);

  thumbnail = g_file_info_get_attribute_byte_string (file_info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
  gtk_widget_set_visible (self->spinner, FALSE);

  if (thumbnail) {
    GtkWidget *frame;

    frame = gtk_frame_new (NULL);
    gtk_widget_set_margin_end (frame, 6);
    gtk_widget_set_margin_top (frame, 6);
    gtk_image_set_from_file (GTK_IMAGE (image), thumbnail);
    gtk_image_set_pixel_size (GTK_IMAGE (image), 96);
    gtk_frame_set_child (GTK_FRAME (frame), image);
    gtk_overlay_set_child (GTK_OVERLAY (self->overlay), frame);
  } else {
    g_autofree char *file_mime_type = NULL;

    gtk_widget_set_margin_end (image, 6);
    file_mime_type = g_content_type_get_mime_type (g_file_info_get_content_type (file_info));

    if (!file_mime_type)
      gtk_image_set_from_icon_name (GTK_IMAGE (image), "text-x-generic-symbolic");
    else if (strstr (file_mime_type, "vcard"))
      gtk_image_set_from_icon_name (GTK_IMAGE (image), "contact-new-symbolic");
    else if (strstr (file_mime_type, "calendar"))
      gtk_image_set_from_icon_name (GTK_IMAGE (image), "x-office-calendar-symbolic");
    else {
      g_autoptr(GIcon) icon = NULL;

      icon = g_content_type_get_symbolic_icon (file_mime_type);
      gtk_image_set_from_gicon (GTK_IMAGE (image), icon);
    }
    gtk_image_set_pixel_size (GTK_IMAGE (image), 96);
    gtk_overlay_set_child (GTK_OVERLAY (self->overlay), image);
  }

  gtk_overlay_add_overlay (GTK_OVERLAY (self->overlay), self->remove_button);
}

static void
file_create_thumbnail_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  g_autoptr (AttachmentData) data = user_data;
  g_autoptr (GError) error = NULL;

  chatty_utils_create_thumbnail_finish (result, &error);

  if (error && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  if (error) {
    g_warning ("Error creating thumbnail: %s", error->message);
    return;
  }

  if (gtk_widget_in_destruction (GTK_WIDGET (data->self)))
    return;

  attachment_update_image (data->self, data->image, data->self->file);
}

static void
attachment_remove_clicked_cb (ChattyAttachment *self)
{
  g_assert (CHATTY_IS_ATTACHMENT (self));

  g_signal_emit (self, signals[DELETED], 0);
}

static void
chatty_attachment_finalize (GObject *object)
{
  ChattyAttachment *self = (ChattyAttachment *)object;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->file);

  G_OBJECT_CLASS (chatty_attachment_parent_class)->finalize (object);
}

static void
chatty_attachment_class_init (ChattyAttachmentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = chatty_attachment_finalize;

  signals [DELETED] =
    g_signal_new ("deleted",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/Chatty/"
                                               "ui/chatty-attachment.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyAttachment, overlay);
  gtk_widget_class_bind_template_child (widget_class, ChattyAttachment, label);
  gtk_widget_class_bind_template_child (widget_class, ChattyAttachment, spinner);
  gtk_widget_class_bind_template_child (widget_class, ChattyAttachment, remove_button);

  gtk_widget_class_bind_template_callback (widget_class, attachment_remove_clicked_cb);
}

static void
chatty_attachment_init (ChattyAttachment *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->cancellable = g_cancellable_new ();
}

GtkWidget *
chatty_attachment_new (GFile *file)
{
  ChattyAttachment *self;

  self = g_object_new (CHATTY_TYPE_ATTACHMENT, NULL);
  chatty_attachment_set_file (self, file);

  return GTK_WIDGET (self);
}

GFile *
chatty_attachment_get_file (ChattyAttachment *self)
{
  g_return_val_if_fail (CHATTY_IS_ATTACHMENT (self), NULL);

  return self->file;
}

/**
 * chatty_attachment_set_file:
 * @self: The attachment
 * @file: (transfer full): The file
 *
 * Sets the file for the attachment
 */
void
chatty_attachment_set_file (ChattyAttachment *self,
                            GFile            *file)
{
  g_autoptr(GFileInfo) file_info = NULL;
  g_autoptr(GError) error = NULL;
  const char *thumbnail;
  GtkWidget *image;
  gboolean thumbnail_failed, thumbnail_valid;

  g_assert (CHATTY_IS_ATTACHMENT (self));
  g_assert (G_IS_FILE (file));

  g_set_object (&self->file, file);

  file_info = g_file_query_info (file,
                                 G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                 G_FILE_ATTRIBUTE_STANDARD_ICON ","
                                 G_FILE_ATTRIBUTE_THUMBNAIL_PATH ","
                                 G_FILE_ATTRIBUTE_THUMBNAIL_IS_VALID ","
                                 G_FILE_ATTRIBUTE_THUMBNAILING_FAILED,
                                 G_FILE_QUERY_INFO_NONE,
                                 NULL, &error);
  if (error)
    g_warning ("Error querying info: %s", error->message);

  image = gtk_image_new ();
  gtk_widget_set_tooltip_text (image, g_file_peek_path (file));
  gtk_widget_set_size_request (image, -1, 96);
  thumbnail = g_file_info_get_attribute_byte_string (file_info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
  thumbnail_failed = g_file_info_get_attribute_boolean (file_info, G_FILE_ATTRIBUTE_THUMBNAILING_FAILED);
  thumbnail_valid = g_file_info_get_attribute_boolean (file_info, G_FILE_ATTRIBUTE_THUMBNAIL_IS_VALID);

  CHATTY_TRACE_MSG ("has thumbnail: %d, failed thumbnail: %d, valid thumbnail: %d",
                    !!thumbnail, thumbnail_failed, thumbnail_valid);

  gtk_label_set_text (GTK_LABEL (self->label), g_file_info_get_name (file_info));

  if (thumbnail || (thumbnail_failed && thumbnail_valid)) {
    attachment_update_image (self, image, file);
  } else {
    AttachmentData *data;

    gtk_widget_set_visible (self->spinner, TRUE);
    data = g_new0 (AttachmentData, 1);
    data->self = g_object_ref (self);
    data->image = g_object_ref (image);
    chatty_utils_create_thumbnail_async (self->file,
                                         self->cancellable,
                                         file_create_thumbnail_cb,
                                         data);
  }
}
