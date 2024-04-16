/* chatty-message-bar.c
 *
 * Copyright 2023 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-message-bar"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>

#include "chatty-account.h"
#include "chatty-attachments-bar.h"
#include "chatty-history.h"
#include "chatty-log.h"
#include "chatty-message.h"
#include "chatty-ma-chat.h"
#include "chatty-mm-chat.h"
#include "chatty-purple.h"
#include "chatty-settings.h"
#include "chatty-message-bar.h"

struct _ChattyMessageBar
{
  GtkBox         parent_instance;

  GtkWidget     *attachment_revealer;
  GtkWidget     *attachment_bar;

  GtkWidget     *send_file_button;
  GtkWidget     *scrolled_window;
  GtkWidget     *message_input;
  GtkWidget     *send_message_button;
  GtkTextBuffer *message_buffer;

  ChattyHistory *history;
  ChattyChat    *chat;
  guint          save_timeout_id;
  gboolean       draft_is_loading;
  gboolean       is_self_change;
  char          *preedit;
};


#define SAVE_TIMEOUT      300 /* milliseconds */

G_DEFINE_TYPE (ChattyMessageBar, chatty_message_bar, GTK_TYPE_BOX)

static const char *emoticons[][2] = {
  {":)", "🙂"},
  {";)", "😉"},
  {":(", "🙁"},
  {":'(", "😢"},
  {":/", "😕"},
  {":D", "😀"},
  {":'D", "😂"},
  {";P", "😜"},
  {":P", "😛"},
  {";p", "😜"},
  {":p", "😛"},
  {":o", "😮"},
  {"B)", "😎 "},
  {"SANTA", "🎅"},
  {"FROSTY", "⛄"},
};

static void
chatty_message_chat_encrypt_changed_cb (ChattyMessageBar *self)
{
  const char *icon_name = "send-symbolic";

  g_assert (CHATTY_IS_MESSAGE_BAR (self));

  if (chatty_chat_get_encryption (self->chat) == CHATTY_ENCRYPTION_ENABLED)
    icon_name = "send-encrypted-symbolic";

  gtk_button_set_icon_name (GTK_BUTTON (self->send_message_button), icon_name);
}

static void
message_bar_file_chooser_response_cb (GObject         *dialog,
                                      GAsyncResult    *response,
                                      gpointer        user_data)

{
  ChattyMessageBar *self = CHATTY_MESSAGE_BAR (user_data);
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) open_filename = NULL;

  g_assert (CHATTY_IS_MESSAGE_BAR (self));
  open_filename = gtk_file_dialog_open_finish (GTK_FILE_DIALOG (dialog), response, &error);
  g_clear_object (&dialog);

  if (!open_filename) {
    if (!g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
      g_warning ("Error getting file: %s", error->message);
    return;
  }

  chatty_attachments_bar_add_file (CHATTY_ATTACHMENTS_BAR (self->attachment_bar), open_filename);

  /* Currently multiple files are allowed only for MMS chats */
  gtk_widget_set_sensitive (self->send_file_button, CHATTY_IS_MM_CHAT (self->chat));
  /* Files with message content is supported only by MMS chats */
  gtk_widget_set_sensitive (self->message_input, CHATTY_IS_MM_CHAT (self->chat));
}

static void
chat_page_show_file_chooser (ChattyMessageBar *self)
{
  GtkWindow *window;
  GtkFileDialog *dialog;

  g_assert (CHATTY_IS_MESSAGE_BAR (self));

  g_debug ("Show file chooser");

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
  dialog = gtk_file_dialog_new ();

  gtk_file_dialog_open (dialog, window, NULL, message_bar_file_chooser_response_cb, self);
}

static void
chatty_message_account_status_changed_cb (ChattyMessageBar *self)
{
  ChattyAccount *account;
  gboolean enabled;

  account = chatty_chat_get_account (self->chat);
  g_return_if_fail (account);

  enabled = chatty_account_get_status (account) == CHATTY_CONNECTED;
  gtk_widget_set_sensitive (self->send_file_button, enabled);
  gtk_widget_set_sensitive (self->send_message_button, enabled);

  gtk_widget_set_visible (self->send_file_button, chatty_chat_has_file_upload (self->chat));
}

static gboolean
update_emote (gpointer user_data)
{
  ChattyMessageBar *self = user_data;
  GtkTextIter pos, end;
  GtkTextMark *mark;
  int i;

  i = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (self->message_buffer), "emote-index"));
  mark = gtk_text_buffer_get_insert (self->message_buffer);
  gtk_text_buffer_get_iter_at_mark (self->message_buffer, &end, mark);
  pos = end;

  self->is_self_change = TRUE;
  gtk_text_iter_backward_chars (&pos, strlen (emoticons[i][0]));
  gtk_text_buffer_delete (self->message_buffer, &pos, &end);
  gtk_text_buffer_insert (self->message_buffer, &pos, emoticons[i][1], -1);
  self->is_self_change = FALSE;

  return G_SOURCE_REMOVE;
}

static void
chatty_check_for_emoticon (ChattyMessageBar *self)
{
  GtkTextIter start, end;
  GtkTextMark *mark;
  g_autofree char *text = NULL;

  g_assert (CHATTY_IS_MESSAGE_BAR (self));

  mark = gtk_text_buffer_get_insert (self->message_buffer);
  gtk_text_buffer_get_iter_at_mark (self->message_buffer, &end, mark);
  start = end;
  gtk_text_iter_backward_chars (&start, 7);
  text = gtk_text_buffer_get_text (self->message_buffer, &start, &end, FALSE);

  for (guint i = 0; i < G_N_ELEMENTS (emoticons); i++)
    {
      g_autofree char *match = g_strconcat (" ", emoticons[i][0], NULL);

      /* Convert to emoji only if it is set apart from other text */
      if (g_str_equal (text, emoticons[i][0]) ||
          g_str_has_suffix (text, match)) {
        g_object_set_data (G_OBJECT (self->message_buffer), "emote-index", GINT_TO_POINTER (i));
        /* xxx: We can't modify the buffer midst a change, so update it after some timeout  */
        /* fixme: there is likely a better way to handle this */
        g_timeout_add (1, update_emote, self);
        break;
      }
    }
}

static gboolean
chat_page_save_message_to_db (gpointer user_data)
{
  ChattyMessageBar *self = user_data;
  g_autoptr(ChattyMessage) draft = NULL;
  g_autofree char *text = NULL;
  g_autofree char *message_buffer_text = NULL;
  g_autofree char *uid = NULL;
  int cursor_location = 0;

  g_assert (CHATTY_IS_MESSAGE_BAR (self));

  g_clear_handle_id (&self->save_timeout_id, g_source_remove);
  g_return_val_if_fail (self->chat, G_SOURCE_REMOVE);

  CHATTY_TRACE (chatty_chat_get_chat_name (self->chat), "Saving draft to");

  g_object_get (self->message_buffer,
                "text",
                &message_buffer_text,
                "cursor-position",
                &cursor_location,
                NULL);
  if (self->preedit && *self->preedit) {
    GString *message_string = NULL;
    message_string = g_string_new (message_buffer_text);
    message_string = g_string_insert (message_string, cursor_location, self->preedit);
    text = g_string_free (message_string, false);
  } else
    text = g_strdup (message_buffer_text);

  uid = g_uuid_string_random ();
  draft = chatty_message_new (NULL, text, uid, time (NULL),
                              CHATTY_MESSAGE_TEXT,
                              CHATTY_DIRECTION_OUT, CHATTY_STATUS_DRAFT);
  chatty_history_add_message (self->history, self->chat, draft);
  gtk_text_buffer_set_modified (self->message_buffer, FALSE);

  return G_SOURCE_REMOVE;
}

static void
chatty_update_typing_status (ChattyMessageBar *self)
{
  gboolean is_typing;

  g_assert (CHATTY_IS_MESSAGE_BAR (self));

  is_typing = gtk_text_buffer_get_char_count (self->message_buffer) > 0;
  chatty_chat_set_typing (self->chat, is_typing);
}

static void
message_bar_attachment_revealer_notify_cb (ChattyMessageBar *self)
{
  gboolean has_files, has_text;

  g_assert (CHATTY_IS_MESSAGE_BAR (self));

  has_files = gtk_revealer_get_reveal_child (GTK_REVEALER (self->attachment_revealer));
  has_text = gtk_text_buffer_get_char_count (self->message_buffer) > 0;

  gtk_widget_set_visible (self->send_message_button, has_files || has_text);

  if (!has_files) {
    gtk_widget_set_sensitive (self->send_file_button, TRUE);
    gtk_widget_set_sensitive (self->message_input, TRUE);
  }
}

static void
message_bar_input_changed_cb (ChattyMessageBar *self)
{
  gboolean has_text, has_files;

  g_assert (CHATTY_IS_MESSAGE_BAR (self));

  if (self->is_self_change)
    return;

  g_clear_handle_id (&self->save_timeout_id, g_source_remove);
  has_text = gtk_text_buffer_get_char_count (self->message_buffer) > 0;
  has_files = gtk_revealer_get_reveal_child (GTK_REVEALER (self->attachment_revealer));
  gtk_widget_set_visible (self->send_message_button, has_text || has_files);

  if (!self->draft_is_loading)
    self->save_timeout_id = g_timeout_add (SAVE_TIMEOUT,
                                           chat_page_save_message_to_db,
                                           self);
  if (self->draft_is_loading)
    return;

  if (chatty_settings_get_send_typing (chatty_settings_get_default ()))
    chatty_update_typing_status (self);

  if (chatty_settings_get_convert_emoticons (chatty_settings_get_default ()) &&
      chatty_item_get_protocols (CHATTY_ITEM (self->chat)) != CHATTY_PROTOCOL_MMS_SMS)
    chatty_check_for_emoticon (self);

  if (gtk_text_buffer_get_line_count (self->message_buffer) > 3)
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scrolled_window),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  else
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scrolled_window),
                                    GTK_POLICY_NEVER, GTK_POLICY_NEVER);
}

static void
message_bar_send_file_button_clicked_cb (ChattyMessageBar *self,
                                         GtkButton        *button)
{
  g_assert (CHATTY_IS_MESSAGE_BAR (self));
  g_assert (GTK_IS_BUTTON (button));
  g_return_if_fail (chatty_chat_has_file_upload (self->chat));

  if (CHATTY_IS_MM_CHAT (self->chat)) {
    chat_page_show_file_chooser (self);
  } else if (CHATTY_IS_MA_CHAT (self->chat)) {
    chat_page_show_file_chooser (self);
  } else {
#ifdef PURPLE_ENABLED
    chatty_pp_chat_show_file_upload (CHATTY_PP_CHAT (self->chat));
#endif
  }
}

static void
view_send_message_async_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  g_autoptr(ChattyMessageBar) self = user_data;

  chatty_chat_set_unread_count (self->chat, 0);
  chatty_chat_withdraw_notification (self->chat);
}

static void
message_bar_send_message_button_clicked_cb (ChattyMessageBar *self)
{
  ChattyAccount *account;
  g_autoptr(ChattyMessage) msg = NULL;
  g_autofree char *message = NULL;
  g_autofree char *message_buffer_text = NULL;
  GtkTextIter    start, end;
  GList *files;
  int cursor_location = 0;

  g_assert (CHATTY_IS_MESSAGE_BAR (self));

  files = chatty_attachments_bar_get_files (CHATTY_ATTACHMENTS_BAR (self->attachment_bar));

  gtk_text_buffer_get_bounds (self->message_buffer, &start, &end);
  message_buffer_text = gtk_text_buffer_get_text (self->message_buffer, &start, &end, FALSE);
  g_object_get (self->message_buffer,
                "cursor-position",
                &cursor_location,
                NULL);
  if (self->preedit && *self->preedit) {
    GString *message_string = NULL;
    message_string = g_string_new (message_buffer_text);
    message_string = g_string_insert (message_string, cursor_location, self->preedit);
    message = g_string_free (message_string, false);
  } else
    message = g_strdup (message_buffer_text);

#ifdef PURPLE_ENABLED
  if (CHATTY_IS_PP_CHAT (self->chat) &&
      chatty_pp_chat_run_command (CHATTY_PP_CHAT (self->chat), message)) {
    g_clear_handle_id (&self->save_timeout_id, g_source_remove);

    gtk_widget_set_visible (self->send_message_button, FALSE);
    gtk_text_buffer_delete (self->message_buffer, &start, &end);
    /* Make sure the text view things is necessary to reset the im context */
    gtk_text_view_set_editable (GTK_TEXT_VIEW (self->message_input), FALSE);
    gtk_text_view_set_editable (GTK_TEXT_VIEW (self->message_input), TRUE);
    /* This clears preedit */
    gtk_text_view_reset_im_context (GTK_TEXT_VIEW (self->message_input));

    return;
  }
#endif

  account = chatty_chat_get_account (self->chat);
  if (chatty_account_get_status (account) != CHATTY_CONNECTED)
    goto end;

  gtk_widget_grab_focus (self->message_input);

  if (gtk_text_buffer_get_char_count (self->message_buffer) || (self->preedit && *self->preedit) || files) {
    g_autofree char *escaped = NULL;

#ifdef PURPLE_ENABLED
    if (CHATTY_IS_PP_CHAT (self->chat))
      escaped = purple_markup_escape_text (message, -1);
#endif

    msg = chatty_message_new (NULL, escaped ? escaped : message,
                              NULL, time (NULL),
                              escaped ? CHATTY_MESSAGE_HTML_ESCAPED : CHATTY_MESSAGE_TEXT,
                              CHATTY_DIRECTION_OUT, 0);
    if (files) {
      chatty_message_set_files (msg, files);
    }
    chatty_chat_send_message_async (self->chat, msg,
                                    view_send_message_async_cb,
                                    g_object_ref (self));

    gtk_widget_set_visible (self->send_message_button, FALSE);
  }
  chatty_attachments_bar_reset (CHATTY_ATTACHMENTS_BAR (self->attachment_bar));

  /* We don't send matrix text if there are files.  So instead of
   * deleting the existing text, simply make the entry sensitive again. */
  if (CHATTY_IS_MA_CHAT (self->chat) && files)
    gtk_widget_set_sensitive (self->message_input, TRUE);
  else {
    gtk_text_buffer_delete (self->message_buffer, &start, &end);
    /* Make sure the text view things is necessary to reset the im context */
    gtk_text_view_set_editable (GTK_TEXT_VIEW (self->message_input), FALSE);
    gtk_text_view_set_editable (GTK_TEXT_VIEW (self->message_input), TRUE);
    /* This clears preedit */
    gtk_text_view_reset_im_context (GTK_TEXT_VIEW (self->message_input));
  }

  {
    g_autoptr(ChattyMessage) draft = NULL;
    g_autofree char *uid = NULL;

    uid = g_uuid_string_random ();
    draft = chatty_message_new (NULL, "", uid, time (NULL),
                                CHATTY_MESSAGE_TEXT,
                                CHATTY_DIRECTION_OUT, CHATTY_STATUS_DRAFT);
    chatty_history_add_message (self->history, self->chat, draft);
  }

 end:
  g_clear_handle_id (&self->save_timeout_id, g_source_remove);
}

static void
message_bar_strip_utm_id_paste_cb (AdwMessageDialog *dialog,
                                   const char       *response,
                                   gpointer          user_data)

{
  if (g_strcmp0 (response, "dont_show") == 0) {
    ChattySettings  *settings;

    settings = chatty_settings_get_default ();
    chatty_settings_set_strip_url_tracking_ids_dialog (settings, TRUE);
  }
}

static void
message_bar_text_buffer_clipboard_paste_done_cb (ChattyMessageBar *self)
{
  ChattySettings  *settings;
  g_autofree char *stripped_message = NULL;
  g_autofree char *message_buffer_text = NULL;
  GtkTextIter      start, end;

  settings = chatty_settings_get_default ();
  if (!chatty_settings_get_strip_url_tracking_ids (settings)) {
    chatty_settings_set_strip_url_tracking_ids_dialog (settings, FALSE);
    return;
  }

  gtk_text_buffer_get_bounds (self->message_buffer, &start, &end);
  message_buffer_text = gtk_text_buffer_get_text (self->message_buffer, &start, &end, FALSE);

  stripped_message = chatty_utils_strip_utm_from_message (message_buffer_text);

  if (g_strcmp0 (stripped_message, message_buffer_text) != 0) {

    if (!chatty_settings_get_strip_url_tracking_ids_dialog (settings)) {
      AdwDialog *dialog;
      GtkWindow *window;

      window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
      dialog = adw_alert_dialog_new (_("Alert"),
                                     _("Found and automatically deleted tracking IDs from the pasted link."));
      adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "close", _("Close"));
      adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "dont_show", _("Don't Show Again"));

      adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "close");
      adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (dialog), "close");

      adw_dialog_present (dialog, GTK_WIDGET (window));

      g_signal_connect (dialog, "response", G_CALLBACK (message_bar_strip_utm_id_paste_cb), self);
    }
    gtk_text_buffer_begin_user_action (self->message_buffer);
    gtk_text_buffer_delete (self->message_buffer, &start, &end);
    gtk_text_buffer_insert (self->message_buffer, &start, stripped_message, -1);
    gtk_text_buffer_end_user_action (self->message_buffer);
  }
}

static void
message_bar_text_view_preddit_changed_cb (ChattyMessageBar *self,
                                          char             *preedit)
{
  gboolean has_text, has_files;
  g_clear_pointer (&self->preedit, g_free);
  if (preedit && *preedit)
    self->preedit = g_strdup (preedit);

  has_text = (gtk_text_buffer_get_char_count (self->message_buffer) > 0) || (self->preedit && *self->preedit);
  has_files = gtk_revealer_get_reveal_child (GTK_REVEALER (self->attachment_revealer));
  gtk_widget_set_visible (self->send_message_button, has_text || has_files);

  g_clear_handle_id (&self->save_timeout_id, g_source_remove);
  if (!self->draft_is_loading)
    self->save_timeout_id = g_timeout_add (SAVE_TIMEOUT,
                                           chat_page_save_message_to_db,
                                           self);
}

static void
message_bar_activate (GtkWidget  *widget,
                      const char *action_name,
                      GVariant   *param)
{
  ChattyMessageBar *self = CHATTY_MESSAGE_BAR (widget);

  if (chatty_settings_get_return_sends_message (chatty_settings_get_default ())) {
    if (gtk_text_buffer_get_char_count (self->message_buffer) > 0)
      message_bar_send_message_button_clicked_cb (self);
    else
      gtk_widget_error_bell (self->message_input);
  } else {
    gtk_text_buffer_insert_at_cursor (self->message_buffer, "\n", strlen ("\n"));
  }
}

static void
chatty_message_bar_map (GtkWidget *widget)
{
  ChattyMessageBar *self = (ChattyMessageBar *)widget;

  GTK_WIDGET_CLASS (chatty_message_bar_parent_class)->map (widget);

  gtk_widget_grab_focus (self->message_input);
}

static void
chatty_message_bar_finalize (GObject *object)
{
  ChattyMessageBar *self = (ChattyMessageBar *)object;

  g_clear_handle_id (&self->save_timeout_id, g_source_remove);
  g_clear_object (&self->chat);
  g_clear_object (&self->history);

  G_OBJECT_CLASS (chatty_message_bar_parent_class)->finalize (object);
}

static void
chatty_message_bar_class_init (ChattyMessageBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = chatty_message_bar_finalize;

  widget_class->map = chatty_message_bar_map;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/Chatty/"
                                               "ui/chatty-message-bar.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyMessageBar, attachment_revealer);
  gtk_widget_class_bind_template_child (widget_class, ChattyMessageBar, attachment_bar);

  gtk_widget_class_bind_template_child (widget_class, ChattyMessageBar, send_file_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyMessageBar, scrolled_window);
  gtk_widget_class_bind_template_child (widget_class, ChattyMessageBar, message_input);
  gtk_widget_class_bind_template_child (widget_class, ChattyMessageBar, send_message_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyMessageBar, message_buffer);

  gtk_widget_class_bind_template_callback (widget_class, message_bar_attachment_revealer_notify_cb);
  gtk_widget_class_bind_template_callback (widget_class, message_bar_input_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, message_bar_send_file_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, message_bar_send_message_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, message_bar_text_view_preddit_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, message_bar_text_buffer_clipboard_paste_done_cb);

  gtk_widget_class_install_action (widget_class, "message-bar.activate", NULL, message_bar_activate);
}

static void
attachment_files_changed_cb (ChattyMessageBar *self)
{
  GListModel *files = NULL;
  guint n_items;

  g_assert (CHATTY_IS_MESSAGE_BAR (self));

  if (gtk_widget_in_destruction (GTK_WIDGET (self)))
    return;

  files = chatty_attachments_bar_get_files_list (CHATTY_ATTACHMENTS_BAR (self->attachment_bar));
  n_items = g_list_model_get_n_items (files);
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->attachment_revealer), n_items);
}

static void
chatty_message_bar_init (ChattyMessageBar *self)
{
  GListModel *files = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  files = chatty_attachments_bar_get_files_list (CHATTY_ATTACHMENTS_BAR (self->attachment_bar));
  g_signal_connect_object (files, "items-changed",
                           G_CALLBACK (attachment_files_changed_cb),
                           self, G_CONNECT_SWAPPED);
}

static void
chat_get_draft_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  g_autoptr(ChattyMessageBar) self = user_data;
  g_autofree char *draft = NULL;

  draft = chatty_history_get_draft_finish (self->history, result, NULL);

  self->draft_is_loading = TRUE;
  g_object_set (self->message_buffer, "text", draft ? draft : "", NULL);
  self->draft_is_loading = FALSE;
}

void
chatty_message_bar_set_chat (ChattyMessageBar *self,
                             ChattyChat       *chat)
{
  ChattyAccount *account;
  ChattyProtocol protocol;

  g_return_if_fail (CHATTY_IS_MESSAGE_BAR (self));
  g_return_if_fail (!chat || CHATTY_IS_CHAT (chat));

  if (self->chat && chat != self->chat) {
    gtk_button_set_icon_name (GTK_BUTTON (self->send_message_button), "send-symbolic");
    g_signal_handlers_disconnect_by_func (chatty_chat_get_account (self->chat),
                                          chatty_message_account_status_changed_cb,
                                          self);
    g_signal_handlers_disconnect_by_func (self->chat,
                                          chatty_message_chat_encrypt_changed_cb,
                                          self);
  }

  gtk_widget_set_sensitive (self->message_input, !!chat);

  if (gtk_text_buffer_get_line_count (self->message_buffer) > 3)
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scrolled_window),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  else
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scrolled_window),
                                    GTK_POLICY_NEVER, GTK_POLICY_NEVER);

  if (chat) {
    gtk_widget_set_valign (self->message_input, GTK_ALIGN_CENTER);
  } else {
    gtk_widget_set_valign (self->message_input, GTK_ALIGN_FILL);
  }

  if (!g_set_object (&self->chat, chat) || !chat)
    return;

  protocol = chatty_item_get_protocols (CHATTY_ITEM (chat));
  if (protocol == CHATTY_PROTOCOL_MMS_SMS) {
    gtk_widget_remove_css_class (self->send_message_button, "suggested-action");
    gtk_widget_add_css_class (self->send_message_button, "button_send_green");
  } else {
    gtk_widget_remove_css_class (self->send_message_button, "button_send_green");
    gtk_widget_add_css_class (self->send_message_button, "suggested-action");
  }

  chatty_attachments_bar_reset (CHATTY_ATTACHMENTS_BAR (self->attachment_bar));

  chatty_history_get_draft_async (self->history, self->chat,
                                  chat_get_draft_cb,
                                  g_object_ref (self));

  account = chatty_chat_get_account (chat);
  if (account)
    g_signal_connect_object (account, "notify::status",
                             G_CALLBACK (chatty_message_account_status_changed_cb),
                             self,
                             G_CONNECT_SWAPPED);

  g_signal_connect_object (self->chat, "notify::encrypt",
                           G_CALLBACK (chatty_message_chat_encrypt_changed_cb),
                           self, G_CONNECT_SWAPPED);
  chatty_message_account_status_changed_cb (self);
  chatty_message_chat_encrypt_changed_cb (self);

  gtk_widget_grab_focus (self->message_input);
}

void
chatty_message_bar_set_db (ChattyMessageBar *self,
                           ChattyHistory    *history)
{
  g_return_if_fail (CHATTY_IS_MESSAGE_BAR (self));
  g_return_if_fail (CHATTY_IS_HISTORY (history));
  g_return_if_fail (!self->history);

  g_set_object (&self->history, history);
}
