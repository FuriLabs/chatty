/* chatty-message-row.c
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *   Andrea Schäfer <mosibasu@me.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib/gi18n.h>
#include <ctype.h>

#include "chatty-avatar.h"
#include "chatty-enums.h"
#include "chatty-file.h"
#include "chatty-file-item.h"
#include "chatty-image-item.h"
#include "chatty-text-item.h"
#include "chatty-clock.h"
#include "chatty-message-row.h"


struct _ChattyMessageRow
{
  GtkListBoxRow  parent_instance;

  GtkWidget  *content_grid;
  GtkWidget  *avatar_image;
  GtkWidget  *hidden_box;
  GtkWidget  *author_label;

  GtkWidget  *message_event_box;
  GtkWidget  *message_content;
  GtkWidget  *files_box;
  GtkWidget  *content_separator;
  GtkWidget  *message_title;
  GtkWidget  *message_body;

  GtkWidget  *footer_label;

  GtkWidget  *content;

  GtkWidget  *popover;
  GtkGesture *multipress_gesture;
  GtkGesture *longpress_gesture;
  GtkGesture *activate_gesture;

  GBinding   *name_binding;

  ChattyMessage *message;
  ChattyProtocol protocol;
  gulong         clock_id;
  gboolean       is_im;
  gboolean       force_hide_footer;
};

G_DEFINE_TYPE (ChattyMessageRow, chatty_message_row, GTK_TYPE_LIST_BOX_ROW)

#define URL_WWW "www"
#define URL_HTTP "http"
#define URL_HTTPS "https"
#define URL_FILE "file"

#define get_url_start(_start, _match, _type, _suffix, _out)             \
  do {                                                                  \
    char *_uri = _match - strlen (_type);                               \
    size_t len = strlen (_type _suffix);                                \
                                                                        \
    if (*_out)                                                          \
      break;                                                            \
                                                                        \
    if (_match - _start >= strlen (_type) &&                            \
        g_ascii_strncasecmp (_uri, _type _suffix, len) == 0 &&          \
        (isalnum (_uri[len]) || _uri[len] == '/'))                      \
      *_out = match - strlen (_type);                                   \
  } while (0)

static char *
find_url (const char  *buffer,
          char       **end)
{
  char *match, *url = NULL;
  const char *start;

  start = buffer;

  /*
   * linkify http://,  https://, file://, and www.
   */
  while ((match = strpbrk (start, ":."))) {
    get_url_start (start, match, URL_HTTP, "://", &url);
    get_url_start (start, match, URL_HTTPS, "://", &url);
    get_url_start (start, match, URL_FILE, "://", &url);
    get_url_start (start, match, URL_WWW, ".", &url);

    start = match + 1;

    if (url && url > buffer &&
        !isspace (url[-1]) && !ispunct (url[-1]))
      url = NULL;

    if (url)
      break;
  }

  if (url) {
    size_t url_end;

    url_end = strcspn (url, " \n()[],\t\r");
    if (url_end)
      *end = url + url_end;
    else
      *end = url + strlen (url);
  }

  return url;
}

static char *
text_item_linkify (const char *message)
{
  g_autoptr(GString) link_str = NULL;
  GString *str = NULL;
  char *start, *end, *url;

  if (!message || !*message)
    return NULL;

  str = g_string_sized_new (256);
  link_str = g_string_sized_new (256);
  start = end = (char *)message;

  while ((url = find_url (start, &end))) {
    g_autofree char *link = NULL;
    g_autofree char *escaped_link = NULL;
    char *escaped = NULL;

    escaped = g_markup_escape_text (start, url - start);
    g_string_append (str, escaped);
    g_free (escaped);

    link = g_strndup (url, end - url);
    escaped_link = g_markup_escape_text (url, end - url);
    g_string_set_size (link_str, 0);
    /* Don't escape sub-delims and gen-delims */
    g_string_append_uri_escaped (link_str, link, ":/?#[]@!$&'()*+,;=", TRUE);
    escaped = g_markup_escape_text (link_str->str, link_str->len);
    g_string_append_printf (str, "<a href=\"%s\">%s</a>", escaped, escaped_link);
    g_free (escaped);

    start = end;
  }

  /* Append rest of the string, only if we there is already content */
  if (str->len && start && *start) {
    g_autofree char *escaped = NULL;

    escaped = g_markup_escape_text (start, -1);
    g_string_append (str, escaped);
  }

  return g_string_free (str, FALSE);
}

static gchar *
chatty_msg_list_escape_message (const char *message)
{
#ifdef PURPLE_ENABLED
  g_autofree char *nl_2_br = NULL;
  g_autofree char *striped = NULL;
  g_autofree char *escaped = NULL;
  g_autofree char *linkified = NULL;
  char *result;

  nl_2_br = purple_strdup_withhtml (message);
  striped = purple_markup_strip_html (nl_2_br);
  escaped = purple_markup_escape_text (striped, -1);
  linkified = purple_markup_linkify (escaped);
  // convert all tags to lowercase for GtkLabel markup parser
  purple_markup_html_to_xhtml (linkified, &result, NULL);

  return result;
#endif

  return g_strdup ("");
}

static void
text_item_update_quotes (ChattyMessageRow *self)
{
  const char *text, *end;
  char *quote;

  text = gtk_label_get_text (GTK_LABEL (self->message_body));
  end = text;

  if (!gtk_label_get_attributes (GTK_LABEL (self->message_body))) {
    PangoAttrList *list;

    list = pango_attr_list_new ();
    gtk_label_set_attributes (GTK_LABEL (self->message_body), list);
    pango_attr_list_unref (list);

  }

  if (!text || !*text)
    return;

  do {
    quote = strchr (end, '>');

    if (quote &&
        (quote == text ||
         *(quote - 1) == '\n')) {
      PangoAttrList *list;
      PangoAttribute *attribute;

      list = gtk_label_get_attributes (GTK_LABEL (self->message_body));
      end = strchr (quote, '\n');

      if (!end)
        end = quote + strlen (quote);

      attribute = pango_attr_foreground_new (30000, 30000, 30000);
      attribute->start_index = quote - text;
      attribute->end_index = end - text + 1;
      pango_attr_list_insert (list, attribute);
    } else if (quote && *quote) {
      /* Move forward one character if '>' happend midst a line */
      end = end + 1;
    }
  } while (quote && *quote);
}

static void
copy_clicked_cb (ChattyMessageRow *self)
{
  GtkClipboard *clipboard;
  const char *text;

  g_assert (CHATTY_IS_MESSAGE_ROW (self));

  clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
  text = gtk_label_get_text (GTK_LABEL (self->message_body));

  if (text && *text)
    gtk_clipboard_set_text (clipboard, text, -1);
}

static void
message_row_show_popover (ChattyMessageRow *self)
{
  g_assert (CHATTY_IS_MESSAGE_ROW (self));

  if (chatty_message_get_msg_type (self->message) != CHATTY_MESSAGE_TEXT)
    return;

  if (!self->popover) {
    GtkWidget *item;

    self->popover = gtk_popover_new (self->message_event_box);
    item = g_object_new (GTK_TYPE_MODEL_BUTTON,
                         "margin", 12,
                         "text", _("Copy"),
                         NULL);
    gtk_widget_show (item);
    g_signal_connect_swapped (item, "clicked",
                              G_CALLBACK (copy_clicked_cb), self);
    gtk_container_add (GTK_CONTAINER (self->popover), item);
  }

  gtk_popover_popup (GTK_POPOVER (self->popover));
}


static void
message_row_hold_cb (ChattyMessageRow *self)
{
  g_assert (CHATTY_IS_MESSAGE_ROW (self));

  message_row_show_popover (self);
  gtk_gesture_set_state (self->longpress_gesture, GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
message_activate_gesture_cb (ChattyMessageRow *self)
{
  g_autoptr(GFile) file = NULL;
  g_autofree char *uri = NULL;
  GList *file_list;
  const char *path;

  g_assert (CHATTY_IS_MESSAGE_ROW (self));

  if (chatty_message_get_msg_type (self->message) != CHATTY_MESSAGE_IMAGE)
    return;

  file_list = chatty_message_get_files (self->message);

  if (!file_list || !file_list->data)
    return;

  path = chatty_file_get_path (file_list->data);
  g_return_if_fail (path);

  if (self->protocol == CHATTY_PROTOCOL_MATRIX)
    file = g_file_new_build_filename (path, NULL);
  else
    file = g_file_new_build_filename (g_get_user_data_dir (), "chatty", path, NULL);

  uri = g_file_get_uri (file);

  gtk_show_uri_on_window (NULL, uri, GDK_CURRENT_TIME, NULL);
}

static const char *
message_row_get_clock_signal (time_t time_stamp)
{
  double diff;

  diff = difftime (time (NULL), time_stamp);

  if (diff >= - SECONDS_PER_MINUTE &&
      diff <= SECONDS_PER_HOUR)
    return g_intern_static_string ("changed");

  if (diff < 0)
    return NULL;

  if (diff < SECONDS_PER_WEEK)
    return g_intern_static_string ("day-changed");

  return NULL;
}

static gboolean
chatty_message_row_update_footer (ChattyMessageRow *self)
{
  g_autofree char *time_str = NULL;
  g_autofree char *footer = NULL;
  const char *status_str = "", *time_signal;
  ChattyMsgStatus status;
  time_t time_stamp;

  g_assert (CHATTY_IS_MESSAGE_ROW (self));
  g_assert (self->message);

  if (self->force_hide_footer) {
    g_clear_signal_handler (&self->clock_id, chatty_clock_get_default ());
    g_object_set_data (G_OBJECT (self), "time-signal", NULL);

    return G_SOURCE_REMOVE;
  }

  status = chatty_message_get_status (self->message);

  if (status == CHATTY_STATUS_SENDING_FAILED)
    status_str = "<span color='red'> x</span>";
  else if (status == CHATTY_STATUS_SENT)
    status_str = " ✓";
  else if (status == CHATTY_STATUS_DELIVERED)
    status_str = "<span color='#6cba3d'> ✓</span>";

  time_stamp = chatty_message_get_time (self->message);
  time_str = chatty_clock_get_human_time (chatty_clock_get_default (),
                                          time_stamp, TRUE);

  footer = g_strconcat (time_str, status_str, NULL);
  gtk_label_set_markup (GTK_LABEL (self->footer_label), footer);
  gtk_widget_set_visible (self->footer_label, footer && *footer);

  time_signal = message_row_get_clock_signal (time_stamp);

  if (time_signal != g_object_get_data (G_OBJECT (self), "time-signal")) {
    g_clear_signal_handler (&self->clock_id, chatty_clock_get_default ());
    g_object_set_data (G_OBJECT (self), "time-signal", (gpointer)time_signal);

    if (time_signal)
      self->clock_id = g_signal_connect_object (chatty_clock_get_default (), time_signal,
                                                G_CALLBACK (chatty_message_row_update_footer),
                                                self, G_CONNECT_SWAPPED);
    return G_SOURCE_REMOVE;
  }

  return G_SOURCE_CONTINUE;
}

static void
message_row_update_message (ChattyMessageRow *self)
{
  g_autofree char *message = NULL;

  g_assert (CHATTY_IS_MESSAGE_ROW (self));
  g_assert (self->message);

  chatty_message_row_update_footer (self);

  /* Don't show author label for outgoing SMS/MMS */
  if (chatty_message_get_msg_direction (self->message) == CHATTY_DIRECTION_OUT &&
      self->protocol & (CHATTY_PROTOCOL_MMS | CHATTY_PROTOCOL_MMS_SMS))
    return;

  if (!self->is_im || self->protocol == CHATTY_PROTOCOL_MATRIX) {
    const char *alias;

    alias = chatty_message_get_user_alias (self->message);

    if (alias)
      gtk_label_set_label (GTK_LABEL (self->author_label), alias);

    g_clear_object (&self->name_binding);

    if (chatty_message_get_user (self->message))
      self->name_binding = g_object_bind_property (chatty_message_get_user (self->message), "name",
                                                   self->author_label, "label",
                                                   G_BINDING_SYNC_CREATE);

    alias = gtk_label_get_label (GTK_LABEL (self->author_label));
    gtk_widget_set_visible (self->author_label, alias && *alias);
  }
}

static void
chatty_message_row_dispose (GObject *object)
{
  ChattyMessageRow *self = (ChattyMessageRow *)object;

  g_clear_object (&self->message);
  g_clear_object (&self->multipress_gesture);
  g_clear_object (&self->longpress_gesture);

  G_OBJECT_CLASS (chatty_message_row_parent_class)->dispose (object);
}

static void
chatty_message_row_class_init (ChattyMessageRowClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = chatty_message_row_dispose;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/Chatty/"
                                               "ui/chatty-message-row.ui");
  gtk_widget_class_bind_template_child (widget_class, ChattyMessageRow, content_grid);
  gtk_widget_class_bind_template_child (widget_class, ChattyMessageRow, avatar_image);
  gtk_widget_class_bind_template_child (widget_class, ChattyMessageRow, hidden_box);
  gtk_widget_class_bind_template_child (widget_class, ChattyMessageRow, author_label);

  gtk_widget_class_bind_template_child (widget_class, ChattyMessageRow, message_event_box);
  gtk_widget_class_bind_template_child (widget_class, ChattyMessageRow, message_content);
  gtk_widget_class_bind_template_child (widget_class, ChattyMessageRow, files_box);
  gtk_widget_class_bind_template_child (widget_class, ChattyMessageRow, content_separator);
  gtk_widget_class_bind_template_child (widget_class, ChattyMessageRow, message_title);
  gtk_widget_class_bind_template_child (widget_class, ChattyMessageRow, message_body);

  gtk_widget_class_bind_template_child (widget_class, ChattyMessageRow, footer_label);
}

static void
chatty_message_row_init (ChattyMessageRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->multipress_gesture = gtk_gesture_multi_press_new (self->message_event_box);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (self->multipress_gesture), GDK_BUTTON_SECONDARY);
  g_signal_connect_swapped (self->multipress_gesture, "pressed",
                            G_CALLBACK (message_row_show_popover), self);

  self->longpress_gesture = gtk_gesture_long_press_new (self->message_event_box);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (self->longpress_gesture), TRUE);
  g_signal_connect_swapped (self->longpress_gesture, "pressed",
                            G_CALLBACK (message_row_hold_cb), self);

  self->activate_gesture = gtk_gesture_multi_press_new (self->message_event_box);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (self->activate_gesture), GDK_BUTTON_PRIMARY);
  g_signal_connect_swapped (self->activate_gesture, "pressed",
                            G_CALLBACK (message_activate_gesture_cb), self);
}

static void
message_row_add_files (ChattyMessageRow *self)
{
  GList *files;

  g_assert (CHATTY_IS_MESSAGE_ROW (self));

  files = chatty_message_get_files (self->message);
  gtk_widget_set_visible (self->files_box, files && files->data);

  for (GList *file = files; file && file->data; file = file->next) {
    const char *mime_type;
    GtkWidget *child;

    g_assert (CHATTY_IS_FILE (file->data));
    mime_type = chatty_file_get_mime_type (file->data);

    if ((mime_type && g_str_has_prefix (mime_type, "image")) ||
        chatty_message_get_msg_type (self->message) == CHATTY_MESSAGE_IMAGE) {
      child = chatty_image_item_new (self->message, self->protocol);
    } else {
      child = chatty_file_item_new (self->message, file->data);
    }

    gtk_widget_set_visible (child, TRUE);
    gtk_container_add (GTK_CONTAINER (self->files_box), child);
  }
}

GtkWidget *
chatty_message_row_new (ChattyMessage  *message,
                        ChattyProtocol  protocol,
                        gboolean        is_im)
{
  ChattyMessageRow *self;
  GtkStyleContext *sc;
  const char *text, *subject;
  ChattyMsgDirection direction;

  g_return_val_if_fail (CHATTY_IS_MESSAGE (message), NULL);

  self = g_object_new (CHATTY_TYPE_MESSAGE_ROW, NULL);
  self->protocol = protocol;

  self->message = g_object_ref (message);
  self->is_im = !!is_im;
  direction = chatty_message_get_msg_direction (message);
  sc = gtk_widget_get_style_context (self->message_content);

  message_row_add_files (self);

  subject = chatty_message_get_subject (message);
  text = chatty_message_get_text (message);
  gtk_widget_set_visible (self->message_title, subject && *subject);
  gtk_widget_set_visible (self->message_body, text && *text);

  if (protocol == CHATTY_PROTOCOL_XMPP ||
      protocol == CHATTY_PROTOCOL_TELEGRAM) {
    g_autofree char *content = NULL;

    content = chatty_msg_list_escape_message (text);
    gtk_label_set_markup (GTK_LABEL (self->message_body), content);
  } else {
    if (subject && *subject) {
      g_autofree char *content = NULL;

      content = text_item_linkify (subject);

      if (content && *content)
        gtk_label_set_markup (GTK_LABEL (self->message_title), content);
      else
        gtk_label_set_text (GTK_LABEL (self->message_title), subject);
    }

    if (text && *text) {
      g_autofree char *content = NULL;

      content = text_item_linkify (text);

      if (content && *content)
        gtk_label_set_markup (GTK_LABEL (self->message_body), content);
      else
        gtk_label_set_text (GTK_LABEL (self->message_body), text);
    }
  }

  if (((text && *text) || (subject && *subject)) &&
      chatty_message_get_files (message))
    gtk_widget_set_visible (self->content_separator, TRUE);

  text_item_update_quotes (self);

  if (direction == CHATTY_DIRECTION_IN) {
    gtk_style_context_add_class (sc, "bubble_white");
    gtk_widget_set_halign (self->content_grid, GTK_ALIGN_START);
    gtk_widget_set_halign (self->message_event_box, GTK_ALIGN_START);
    gtk_widget_set_halign (self->author_label, GTK_ALIGN_START);
  } else if (direction == CHATTY_DIRECTION_OUT && protocol == CHATTY_PROTOCOL_MMS_SMS) {
    gtk_style_context_add_class (sc, "bubble_green");
  } else if (direction == CHATTY_DIRECTION_OUT) {
    gtk_style_context_add_class (sc, "bubble_blue");
  } else { /* System message */
    gtk_style_context_add_class (sc, "bubble_purple");
    gtk_widget_set_hexpand (self->message_event_box, TRUE);
  }

  if (direction == CHATTY_DIRECTION_OUT) {
    gtk_label_set_xalign (GTK_LABEL (self->footer_label), 1);
    gtk_widget_set_halign (self->content_grid, GTK_ALIGN_END);
    gtk_widget_set_halign (self->message_event_box, GTK_ALIGN_END);
    gtk_widget_set_halign (self->author_label, GTK_ALIGN_END);
  } else {
    gtk_label_set_xalign (GTK_LABEL (self->footer_label), 0);
  }

  if ((is_im && protocol != CHATTY_PROTOCOL_MATRIX) || direction == CHATTY_DIRECTION_SYSTEM ||
      direction == CHATTY_DIRECTION_OUT)
    gtk_widget_hide (self->avatar_image);
  else
    chatty_avatar_set_item (CHATTY_AVATAR (self->avatar_image),
                            chatty_message_get_user (message));

  g_signal_connect_object (message, "updated",
                           G_CALLBACK (message_row_update_message),
                           self, G_CONNECT_SWAPPED);
  message_row_update_message (self);

  return GTK_WIDGET (self);
}

ChattyMessage *
chatty_message_row_get_item (ChattyMessageRow *self)
{
  g_return_val_if_fail (CHATTY_IS_MESSAGE_ROW (self), NULL);

  return self->message;
}

void
chatty_message_row_hide_footer (ChattyMessageRow *self)
{
  g_return_if_fail (CHATTY_IS_MESSAGE_ROW (self));

  self->force_hide_footer = TRUE;
  gtk_widget_hide (self->footer_label);
}

void
chatty_message_row_set_alias (ChattyMessageRow *self,
                              const char       *alias)
{
  g_return_if_fail (CHATTY_IS_MESSAGE_ROW (self));

  chatty_avatar_set_title (CHATTY_AVATAR (self->avatar_image), alias);
}

void
chatty_message_row_hide_user_detail (ChattyMessageRow *self)
{
  g_return_if_fail (CHATTY_IS_MESSAGE_ROW (self));

  gtk_widget_hide (self->author_label);
  if (gtk_widget_get_visible (self->avatar_image)) {
    gtk_widget_hide (self->avatar_image);
    gtk_widget_show (self->hidden_box);
  }
}
