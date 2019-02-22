/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include "chatty-window.h"
#include "chatty-account.h"
#include "chatty-buddy-list.h"
#include "chatty-conversation.h"
#include "chatty-purple-init.h"
#include "chatty-config.h"
#include "chatty-dialogs.h"

static void chatty_dialogs_reset_settings_dialog (void);
static void chatty_dialogs_reset_new_contact_dialog (void);
static void chatty_dialogs_reset_invite_contact_dialog (void);

static chatty_dialog_data_t chatty_dialog_data;

chatty_dialog_data_t *chatty_get_dialog_data (void)
{
  return &chatty_dialog_data;
}

static gboolean
cb_switch_prefs_state_changed (GtkSwitch *widget,
                               gboolean   state,
                               gpointer   data)
{
  switch (GPOINTER_TO_INT(data)) {
    case CHATTY_PREF_SEND_RECEIPTS:
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/blist/send_receipts", state);
      break;
    case CHATTY_PREF_MESSAGE_CARBONS:
      if (state) {
        chatty_purple_load_plugin ("core-riba-carbons");
        purple_prefs_set_bool (CHATTY_PREFS_ROOT "/plugins/message_carbons", TRUE);
      } else {
        chatty_purple_unload_plugin ("core-riba-carbons");
        purple_prefs_set_bool (CHATTY_PREFS_ROOT "/plugins/message_carbons", FALSE);
      }
      break;
    case CHATTY_PREF_TYPING_NOTIFICATION:
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/conversations/send_typing", state);
      break;
    case CHATTY_PREF_SHOW_OFFLINE:
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/blist/show_offline_buddies", state);
      break;
    case CHATTY_PREF_INDICATE_OFFLINE:
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/blist/greyout_offline_buddies", state);
      break;
    case CHATTY_PREF_INDICATE_IDLE:
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/blist/blur_idle_buddies", state);
      break;
    case CHATTY_PREF_CONVERT_SMILEY:
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/conversations/convert_emoticons", state);
      break;
    case CHATTY_PREF_RETURN_SENDS:
      purple_prefs_set_bool (CHATTY_PREFS_ROOT "/conversations/return_sends", state);
      break;
    case CHATTY_PREF_MUC_NOTIFICATIONS:
      chatty_conv_set_muc_prefs (CHATTY_PREF_MUC_NOTIFICATIONS, state);
      break;
    case CHATTY_PREF_MUC_PERSISTANT:
      chatty_conv_set_muc_prefs (CHATTY_PREF_MUC_PERSISTANT, state);
      break;
    case CHATTY_PREF_MUC_AUTOJOIN:
      chatty_conv_set_muc_prefs (CHATTY_PREF_MUC_AUTOJOIN, state);
      break;
    default:
      break;
  }

  gtk_switch_set_state (widget, state);

  return TRUE;
}


static gboolean
cb_dialog_delete (GtkWidget *widget,
                  GdkEvent  *event,
                  gpointer   user_data)
{
  gtk_widget_hide_on_delete (widget);

  return TRUE;
}


static void
cb_button_settings_back_clicked (GtkButton *sender,
                                 gpointer   data)
{
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  chatty_dialogs_reset_settings_dialog ();

  gtk_stack_set_visible_child_name (chatty_dialog->stack_panes_settings,
                                    "view-settings");
}


static void
cb_button_new_chat_back_clicked (GtkButton *sender,
                                 gpointer   data)
{
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  chatty_dialogs_reset_settings_dialog ();

  gtk_stack_set_visible_child_name (chatty_dialog->stack_panes_new_chat,
                                    "view-new-chat");
}


static void
cb_button_muc_info_back_clicked (GtkButton *sender,
                                 gpointer   data)
{
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  chatty_dialogs_reset_settings_dialog ();

  gtk_stack_set_visible_child_name (chatty_dialog->stack_panes_muc_info,
                                    "view-muc-info");
}


static void
cb_button_delete_account_clicked (GtkButton *sender,
                                  gpointer   data)
{
  GtkWidget     *dialog;
  PurpleAccount *account;
  int            response;

  chatty_data_t *chatty = chatty_get_data ();

  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  account = chatty->selected_account;

  dialog = gtk_message_dialog_new (chatty->main_window,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_OK_CANCEL,
                                   _("Delete Account"));

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG(dialog),
                                            _("Delete account %s?"),
                                            purple_account_get_username (account));

  gtk_dialog_set_default_response (GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
  gtk_window_set_position (GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

  response = gtk_dialog_run (GTK_DIALOG(dialog));

  if (response == GTK_RESPONSE_OK) {
    purple_accounts_delete (account);

    chatty_account_populate_account_list (chatty->list_manage_account,
                                          LIST_MANAGE_ACCOUNT);

    gtk_stack_set_visible_child_name (chatty_dialog->stack_panes_settings,
                                      "view-settings");
  }

  gtk_widget_destroy (dialog);
}


static void
cb_button_edit_pw_clicked (GtkButton *sender,
                           gpointer   data)
{
  GtkEntry *entry_account_pwd;

  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  entry_account_pwd = (GtkEntry *)data;

  gtk_widget_set_sensitive (GTK_WIDGET(chatty_dialog->button_save_account), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET(entry_account_pwd), TRUE);
  gtk_entry_set_text (GTK_ENTRY(entry_account_pwd), "");
  gtk_widget_grab_focus (GTK_WIDGET(entry_account_pwd));
}


static void
cb_button_save_account_clicked (GtkButton *sender,
                                gpointer   data)
{
  GtkEntry *entry_account_pwd;

  chatty_data_t *chatty = chatty_get_data ();

  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  entry_account_pwd = (GtkEntry *)data;

  purple_account_set_password (chatty->selected_account,
                               gtk_entry_get_text (GTK_ENTRY(entry_account_pwd)));

  purple_account_set_remember_password (chatty->selected_account, TRUE);

  if (purple_account_is_connected (chatty->selected_account)) {
   purple_account_disconnect (chatty->selected_account);
  }

  if (purple_account_is_disconnected (chatty->selected_account)) {
   purple_account_connect (chatty->selected_account);
  }

  gtk_stack_set_visible_child_name (chatty_dialog->stack_panes_settings,
                                    "view-settings");
}


static void
cb_list_account_manage_row_activated (GtkListBox    *box,
                                      GtkListBoxRow *row,
                                      gpointer       user_data)
{
  const char *account_name;
  const char *protocol_name;

  chatty_data_t *chatty = chatty_get_data ();

  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  if (g_object_get_data (G_OBJECT (row), "row-new-account")) {
    gtk_widget_grab_focus (GTK_WIDGET(chatty_dialog->entry_account_name));
    gtk_stack_set_visible_child_name (chatty_dialog->stack_panes_settings,
                                      "view-add-account");
  } else {
    gtk_stack_set_visible_child_name (chatty_dialog->stack_panes_settings,
                                      "view-edit-account");
    chatty->selected_account = g_object_get_data (G_OBJECT (row), "row-account");

    account_name = purple_account_get_username (chatty->selected_account);
    protocol_name = purple_account_get_protocol_name (chatty->selected_account);

    gtk_label_set_text (chatty_dialog->label_name, account_name);
    gtk_label_set_text (chatty_dialog->label_protocol, protocol_name);

    if (purple_account_is_connected (chatty->selected_account)) {
      gtk_label_set_text (chatty_dialog->label_status, _("connected"));
    } else if (purple_account_is_connecting (chatty->selected_account)) {
      gtk_label_set_text (chatty_dialog->label_status, _("connecting..."));
    } else if (purple_account_is_disconnected (chatty->selected_account)) {
      gtk_label_set_text (chatty_dialog->label_status, _("disconnected"));
    }
  }
}


static void
chatty_entry_contact_name_check (GtkEntry  *entry,
                                 GtkWidget *button)
{
  PurpleBuddy    *buddy;
  const char     *name;

  chatty_data_t  *chatty = chatty_get_data ();

  name = gtk_entry_get_text (entry);

  if ((*name != '\0') && chatty->selected_account) {
    buddy = purple_find_buddy (chatty->selected_account, name);
  }

  if ((*name != '\0') && !buddy) {
    gtk_widget_set_sensitive (button, TRUE);
  } else {
    gtk_widget_set_sensitive (button, FALSE);
  }
}


static void
cb_contact_name_insert_text (GtkEntry    *entry,
                             const gchar *text,
                             gint         length,
                             gint        *position,
                             gpointer     data)
{
  chatty_entry_contact_name_check (entry, GTK_WIDGET(data));
}


static void
cb_contact_name_delete_text (GtkEntry    *entry,
                             gint         start_pos,
                             gint         end_pos,
                             gpointer     data)
{
  chatty_entry_contact_name_check (entry, GTK_WIDGET(data));
}


static void
cb_account_name_insert_text (GtkEntry    *entry,
                             const gchar *text,
                             gint         length,
                             gint        *position,
                             gpointer     data)
{
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  gtk_widget_set_sensitive (GTK_WIDGET(chatty_dialog->button_add_account),
                            *position ? TRUE : FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET(chatty_dialog->entry_account_pwd),
                            *position ? TRUE : FALSE);
}


static void
cb_account_name_delete_text (GtkEntry    *entry,
                             gint         start_pos,
                             gint         end_pos,
                             gpointer     data)
{
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  gtk_widget_set_sensitive (GTK_WIDGET(chatty_dialog->button_add_account),
                            start_pos ? TRUE : FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET(chatty_dialog->entry_account_pwd),
                            start_pos ? TRUE : FALSE);
}


static void
cb_invite_name_insert_text (GtkEntry    *entry,
                            const gchar *text,
                            gint         length,
                            gint        *position,
                            gpointer     data)
{
  gtk_widget_set_sensitive (GTK_WIDGET(data), *position ? TRUE : FALSE);
}


static void
cb_invite_name_delete_text (GtkEntry    *entry,
                            gint         start_pos,
                            gint         end_pos,
                            gpointer     data)
{
  gtk_widget_set_sensitive (GTK_WIDGET(data), start_pos ? TRUE : FALSE);
}


static void
cb_button_show_add_contact_clicked (GtkButton *sender,
                                    gpointer   data)
{
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  chatty_dialogs_reset_new_contact_dialog ();

  gtk_stack_set_visible_child_name (chatty_dialog->stack_panes_new_chat,
                                    "view-new-contact");
}


static void
cb_button_add_contact_clicked (GtkButton *sender,
                               gpointer   data)
{
  const char *who;
  const char *alias;

  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  who = gtk_entry_get_text (GTK_ENTRY(chatty_dialog->entry_contact_name));
  alias = gtk_entry_get_text (GTK_ENTRY(chatty_dialog->entry_contact_nick));

  chatty_blist_add_buddy (who, alias);

  gtk_stack_set_visible_child_name (chatty_dialog->stack_panes_new_chat,
                                    "view-new-chat");
}


static void
cb_button_show_invite_contact_clicked (GtkButton *sender,
                                       gpointer   data)
{
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  chatty_dialogs_reset_invite_contact_dialog ();

  gtk_widget_grab_focus (GTK_WIDGET(chatty_dialog->entry_invite_name));

  gtk_stack_set_visible_child_name (chatty_dialog->stack_panes_muc_info,
                                    "view-invite-contact");
}


static void
cb_button_invite_contact_clicked (GtkButton *sender,
                                  gpointer   data)
{
  const char *name;
  const char *invite_msg;

  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  name = gtk_entry_get_text (GTK_ENTRY(chatty_dialog->entry_invite_name));
  invite_msg = gtk_entry_get_text (GTK_ENTRY(chatty_dialog->entry_invite_msg));

  if (name != NULL) {
    chatty_conv_invite_muc_user (name, invite_msg);
  }

  gtk_stack_set_visible_child_name (chatty_dialog->stack_panes_muc_info,
                                    "view-muc-info");
}


static void
cb_button_add_account_clicked (GtkButton *sender,
                               gpointer   data)
{
  PurpleAccount *account;
  const gchar   *name;
  const gchar   *pwd;

  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  name = gtk_entry_get_text (GTK_ENTRY(chatty_dialog->entry_account_name));
  pwd  = gtk_entry_get_text (GTK_ENTRY(chatty_dialog->entry_account_pwd));

  account = purple_account_new (name, "prpl-jabber");

  purple_account_set_password (account, pwd);
  purple_account_set_remember_password (account, TRUE);
  purple_account_set_enabled (account, CHATTY_UI, TRUE);
  purple_accounts_add (account);

  gtk_stack_set_visible_child_name (chatty_dialog->stack_panes_settings,
                                    "view-settings");
}


static void
cb_button_edit_topic_clicked (GtkToggleButton *sender,
                              gpointer         data)
{
  GtkStyleContext *sc;
  GtkTextIter      start, end;
  gboolean         edit_mode;

  chatty_data_t        *chatty = chatty_get_data ();
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  sc = gtk_widget_get_style_context (GTK_WIDGET(chatty_dialog->box_topic_frame));

  gtk_text_buffer_get_bounds (chatty->muc.msg_buffer_topic,
                              &start,
                              &end);

  if (gtk_toggle_button_get_active (sender)) {
    edit_mode = TRUE;

    chatty_dialog->current_topic = gtk_text_buffer_get_text (chatty->muc.msg_buffer_topic,
                                                             &start, &end,
                                                             FALSE);


    gtk_widget_grab_focus (GTK_WIDGET(chatty_dialog->textview_muc_topic));

    gtk_style_context_remove_class (sc, "topic_no_edit");
    gtk_style_context_add_class (sc, "topic_edit");
  } else {
    edit_mode = FALSE;

    if (g_strcmp0 (chatty_dialog->current_topic, chatty_dialog->new_topic) != 0) {
      chatty_conv_set_muc_topic (chatty_dialog->new_topic);
    }

    gtk_style_context_remove_class (sc, "topic_edit");
    gtk_style_context_add_class (sc, "topic_no_edit");
  }

  gtk_text_view_set_editable (GTK_TEXT_VIEW(chatty_dialog->textview_muc_topic), edit_mode);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW(chatty_dialog->textview_muc_topic), edit_mode);
  gtk_widget_set_can_focus (GTK_WIDGET(chatty_dialog->textview_muc_topic), edit_mode);
}


static gboolean
cb_textview_key_released (GtkWidget   *widget,
                          GdkEventKey *key_event,
                          gpointer     data)
{
  GtkTextIter start, end;

  chatty_data_t        *chatty = chatty_get_data ();
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  gtk_text_buffer_get_bounds (chatty->muc.msg_buffer_topic,
                              &start,
                              &end);

  chatty_dialog->new_topic = gtk_text_buffer_get_text (chatty->muc.msg_buffer_topic,
                                                       &start, &end,
                                                       FALSE);

  return TRUE;
}


static void
chatty_dialogs_reset_settings_dialog (void)
{
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  gtk_widget_set_sensitive (GTK_WIDGET(chatty_dialog->button_save_account), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET(chatty_dialog->button_add_account), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET(chatty_dialog->entry_account_pwd), FALSE);

  gtk_entry_set_text (GTK_ENTRY(chatty_dialog->entry_account_name), "");
  gtk_entry_set_text (GTK_ENTRY(chatty_dialog->entry_account_pwd), "");
}


static void
chatty_dialogs_reset_new_contact_dialog (void)
{
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  chatty_account_populate_account_list (chatty_dialog->list_select_account,
                                        LIST_SELECT_CHAT_ACCOUNT);
}


static void
chatty_dialogs_reset_invite_contact_dialog (void)
{
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  chatty_account_populate_account_list (chatty_dialog->list_select_account,
                                        LIST_SELECT_CHAT_ACCOUNT);
}


static void
chatty_dialogs_create_add_account_view (GtkBuilder *builder)
{
  GtkWidget  *dialog;
  GtkWidget  *button_back;

  chatty_data_t        *chatty = chatty_get_data ();
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  chatty_dialog->button_add_account = GTK_WIDGET (gtk_builder_get_object (builder, "button_add_account"));
  button_back = GTK_WIDGET (gtk_builder_get_object (builder, "button_add_account_back"));

  chatty_dialog->entry_account_name =
    GTK_ENTRY (gtk_builder_get_object (builder, "entry_add_account_id"));

  chatty_dialog->entry_account_pwd =
    GTK_ENTRY (gtk_builder_get_object (builder, "entry_add_account_pwd"));

  g_signal_connect_after (G_OBJECT(chatty_dialog->entry_account_name),
                          "insert_text",
                          G_CALLBACK(cb_account_name_insert_text),
                          NULL);

  g_signal_connect_after (G_OBJECT(chatty_dialog->entry_account_name),
                          "delete_text",
                          G_CALLBACK(cb_account_name_delete_text),
                          NULL);

  dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));

  gtk_window_set_transient_for (GTK_WINDOW(dialog),
                                GTK_WINDOW(chatty->main_window));

  g_signal_connect (G_OBJECT(chatty_dialog->button_add_account),
                    "clicked",
                    G_CALLBACK (cb_button_add_account_clicked),
                    NULL);

  g_signal_connect (G_OBJECT(button_back),
                    "clicked",
                    G_CALLBACK (cb_button_settings_back_clicked),
                    NULL);
}


static void
chatty_dialogs_create_edit_account_view (GtkBuilder *builder)
{
  GtkWidget  *button_back;
  GtkWidget  *button_edit_pw;
  GtkWidget  *button_delete;
  GtkEntry   *entry_account_pwd;

  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  chatty_dialog->dialog_edit_account = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));
  button_delete = GTK_WIDGET (gtk_builder_get_object (builder, "button_delete_account"));
  button_edit_pw = GTK_WIDGET (gtk_builder_get_object (builder, "button_edit_pw"));
  chatty_dialog->button_save_account = GTK_WIDGET (gtk_builder_get_object (builder, "button_save_account"));
  button_back = GTK_WIDGET (gtk_builder_get_object (builder, "button_edit_account_back"));
  chatty_dialog->label_name = GTK_LABEL (gtk_builder_get_object (builder, "label_account_id"));
  chatty_dialog->label_protocol = GTK_LABEL (gtk_builder_get_object (builder, "label_protocol"));
  chatty_dialog->label_status = GTK_LABEL (gtk_builder_get_object (builder, "label_status"));
  entry_account_pwd = GTK_ENTRY (gtk_builder_get_object (builder, "entry_account_pwd"));


  gtk_entry_set_text (GTK_ENTRY(entry_account_pwd), "xxxxxxxxx");

  g_signal_connect (G_OBJECT(button_delete),
                    "clicked",
                    G_CALLBACK (cb_button_delete_account_clicked),
                    NULL);

  g_signal_connect (G_OBJECT(button_edit_pw),
                    "clicked",
                    G_CALLBACK (cb_button_edit_pw_clicked),
                    (gpointer)entry_account_pwd);

  g_signal_connect (G_OBJECT(chatty_dialog->button_save_account),
                    "clicked",
                    G_CALLBACK (cb_button_save_account_clicked),
                    (gpointer)entry_account_pwd);

  g_signal_connect (G_OBJECT(button_back),
                    "clicked",
                    G_CALLBACK (cb_button_settings_back_clicked),
                    NULL);
}


GtkWidget *
chatty_dialogs_create_dialog_settings (void)
{
  GtkBuilder    *builder;
  GtkWidget     *dialog;
  GtkListBox    *list_privacy_prefs;
  GtkListBox    *list_xmpp_prefs;
  GtkListBox    *list_editor_prefs;
  GtkSwitch     *switch_prefs_send_receipts;
  GtkSwitch     *switch_prefs_message_carbons;
  GtkSwitch     *switch_prefs_typing_notification;
  GtkSwitch     *switch_prefs_show_offline;
  GtkSwitch     *switch_prefs_indicate_offline;
  GtkSwitch     *switch_prefs_indicate_idle;
  GtkSwitch     *switch_prefs_convert_smileys;
  GtkSwitch     *switch_prefs_return_sends;
  HdyActionRow  *row_pref_message_carbons;

  chatty_data_t        *chatty = chatty_get_data ();
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();
  chatty_purple_data_t *chatty_purple = chatty_get_purple_data ();

  builder = gtk_builder_new_from_resource ("/sm/puri/chatty/ui/chatty-dialog-settings.ui");

  dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));

  chatty_dialog->stack_panes_settings = GTK_STACK (gtk_builder_get_object (builder, "stack_panes_settings"));
  chatty->list_manage_account = GTK_LIST_BOX (gtk_builder_get_object (builder, "list_manage_account"));
  list_privacy_prefs = GTK_LIST_BOX (gtk_builder_get_object (builder, "privacy_prefs_listbox"));
  switch_prefs_send_receipts = GTK_SWITCH (gtk_builder_get_object (builder, "pref_send_receipts"));
  switch_prefs_message_carbons = GTK_SWITCH (gtk_builder_get_object (builder, "pref_message_carbons"));
  row_pref_message_carbons = HDY_ACTION_ROW (gtk_builder_get_object (builder, "row_pref_message_carbons"));
  switch_prefs_typing_notification = GTK_SWITCH (gtk_builder_get_object (builder, "pref_typing_notification"));

  list_xmpp_prefs = GTK_LIST_BOX (gtk_builder_get_object (builder, "xmpp_prefs_listbox"));
  switch_prefs_show_offline = GTK_SWITCH (gtk_builder_get_object (builder, "pref_show_offline"));
  switch_prefs_indicate_offline = GTK_SWITCH (gtk_builder_get_object (builder, "pref_indicate_offline"));
  switch_prefs_indicate_idle = GTK_SWITCH (gtk_builder_get_object (builder, "pref_indicate_idle"));

  list_editor_prefs = GTK_LIST_BOX (gtk_builder_get_object (builder, "editor_prefs_listbox"));
  switch_prefs_convert_smileys = GTK_SWITCH (gtk_builder_get_object (builder, "pref_convert_smileys"));
  switch_prefs_return_sends = GTK_SWITCH (gtk_builder_get_object (builder, "pref_return_sends"));

  gtk_list_box_set_header_func (chatty->list_manage_account, hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_header_func (list_privacy_prefs, hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_header_func (list_xmpp_prefs, hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_header_func (list_editor_prefs, hdy_list_box_separator_header, NULL, NULL);

  g_signal_connect (G_OBJECT(dialog),
                    "delete-event",
                    G_CALLBACK(cb_dialog_delete),
                    NULL);

  g_signal_connect (G_OBJECT(chatty->list_manage_account),
                    "row-activated",
                    G_CALLBACK(cb_list_account_manage_row_activated),
                    NULL);

  gtk_switch_set_state (switch_prefs_message_carbons,
                        chatty_purple->plugin_carbons_loaded);
  gtk_switch_set_state (switch_prefs_send_receipts,
                        purple_prefs_get_bool (CHATTY_PREFS_ROOT "/conversations/send_receipts"));
  gtk_switch_set_state (switch_prefs_typing_notification,
                        purple_prefs_get_bool (CHATTY_PREFS_ROOT "/conversations/send_typing"));
  gtk_switch_set_state (switch_prefs_show_offline,
                        purple_prefs_get_bool (CHATTY_PREFS_ROOT "/blist/show_offline_buddies"));
  gtk_switch_set_state (switch_prefs_indicate_offline,
                        purple_prefs_get_bool (CHATTY_PREFS_ROOT "/blist/greyout_offline_buddies"));
  gtk_switch_set_state (switch_prefs_indicate_idle,
                        purple_prefs_get_bool (CHATTY_PREFS_ROOT "/blist/blur_idle_buddies"));
  gtk_switch_set_state (switch_prefs_convert_smileys,
                        purple_prefs_get_bool (CHATTY_PREFS_ROOT "/conversations/convert_emoticons"));
  gtk_switch_set_state (switch_prefs_return_sends,
                        purple_prefs_get_bool (CHATTY_PREFS_ROOT "/conversations/return_sends"));

  if (chatty_purple->plugin_carbons_available) {
    gtk_widget_show (GTK_WIDGET(row_pref_message_carbons));
  } else {
    gtk_widget_hide (GTK_WIDGET(row_pref_message_carbons));
  }

  g_signal_connect (switch_prefs_send_receipts,
                    "state-set",
                    G_CALLBACK(cb_switch_prefs_state_changed),
                    (gpointer)CHATTY_PREF_SEND_RECEIPTS);
  g_signal_connect (switch_prefs_message_carbons,
                    "state-set",
                    G_CALLBACK(cb_switch_prefs_state_changed),
                    (gpointer)CHATTY_PREF_MESSAGE_CARBONS);
  g_signal_connect (switch_prefs_typing_notification,
                    "state-set",
                    G_CALLBACK(cb_switch_prefs_state_changed),
                    (gpointer)CHATTY_PREF_TYPING_NOTIFICATION);
  g_signal_connect (switch_prefs_show_offline,
                    "state-set",
                    G_CALLBACK(cb_switch_prefs_state_changed),
                    (gpointer)CHATTY_PREF_SHOW_OFFLINE);
  g_signal_connect (switch_prefs_indicate_offline,
                    "state-set",
                    G_CALLBACK(cb_switch_prefs_state_changed),
                    (gpointer)CHATTY_PREF_INDICATE_OFFLINE);
  g_signal_connect (switch_prefs_indicate_idle,
                    "state-set",
                    G_CALLBACK(cb_switch_prefs_state_changed),
                    (gpointer)CHATTY_PREF_INDICATE_IDLE);
  g_signal_connect (switch_prefs_convert_smileys,
                    "state-set",
                    G_CALLBACK(cb_switch_prefs_state_changed),
                    (gpointer)CHATTY_PREF_CONVERT_SMILEY);
  g_signal_connect (switch_prefs_return_sends,
                    "state-set",
                    G_CALLBACK(cb_switch_prefs_state_changed),
                    (gpointer)CHATTY_PREF_RETURN_SENDS);

  chatty_account_populate_account_list (chatty->list_manage_account,
                                        LIST_MANAGE_ACCOUNT);

  gtk_window_set_transient_for (GTK_WINDOW(dialog),
                                GTK_WINDOW(chatty->main_window));

  chatty_dialogs_create_edit_account_view (builder);
  chatty_dialogs_create_add_account_view (builder);

  g_object_unref (builder);

  return dialog;
}


GtkWidget *
chatty_dialogs_create_dialog_new_chat (void)
{
  GtkBuilder *builder;
  GtkWidget  *dialog;
  GtkWidget  *button_back;
  GtkWidget  *button_add_contact;
  GtkWidget  *button_show_add_contact;

  chatty_data_t        *chatty = chatty_get_data ();
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  builder = gtk_builder_new_from_resource ("/sm/puri/chatty/ui/chatty-dialog-new-chat.ui");

  dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));

  chatty->pane_view_new_chat = GTK_BOX (gtk_builder_get_object (builder, "pane_view_new_chat"));
  chatty->search_entry_contacts = GTK_ENTRY (gtk_builder_get_object (builder, "search_entry_contacts"));
  chatty->label_contact_id = GTK_WIDGET (gtk_builder_get_object (builder, "label_contact_id"));
  chatty_dialog->stack_panes_new_chat = GTK_STACK (gtk_builder_get_object (builder, "stack_panes_new_chat"));
  chatty_dialog->list_select_account = GTK_LIST_BOX (gtk_builder_get_object (builder, "list_select_chat_account"));
  chatty_dialog->entry_contact_name = GTK_ENTRY (gtk_builder_get_object (builder, "entry_contact_name"));
  chatty_dialog->entry_contact_nick = GTK_ENTRY (gtk_builder_get_object (builder, "entry_contact_alias"));
  button_back = GTK_WIDGET (gtk_builder_get_object (builder, "button_back"));
  button_show_add_contact = GTK_WIDGET (gtk_builder_get_object (builder, "button_show_add_contact"));
  button_add_contact = GTK_WIDGET (gtk_builder_get_object (builder, "button_add_contact"));

  g_signal_connect (G_OBJECT(dialog),
                    "delete-event",
                    G_CALLBACK(cb_dialog_delete),
                    NULL);

  g_signal_connect (G_OBJECT(button_back),
                    "clicked",
                    G_CALLBACK (cb_button_new_chat_back_clicked),
                    NULL);

  g_signal_connect (G_OBJECT(button_show_add_contact),
                    "clicked",
                    G_CALLBACK (cb_button_show_add_contact_clicked),
                    NULL);

  g_signal_connect (G_OBJECT(button_add_contact),
                    "clicked",
                    G_CALLBACK (cb_button_add_contact_clicked),
                    NULL);

  gtk_list_box_set_header_func (chatty_dialog->list_select_account,
                                hdy_list_box_separator_header,
                                NULL, NULL);

  g_signal_connect_after (G_OBJECT(chatty_dialog->entry_contact_name),
                          "insert_text",
                          G_CALLBACK(cb_contact_name_insert_text),
                          (gpointer)button_add_contact);

  g_signal_connect_after (G_OBJECT(chatty_dialog->entry_contact_name),
                          "delete_text",
                          G_CALLBACK(cb_contact_name_delete_text),
                          (gpointer)button_add_contact);

  gtk_window_set_transient_for (GTK_WINDOW(dialog),
                                GTK_WINDOW(chatty->main_window));

  g_object_unref (builder);

  return dialog;
}


GtkWidget *
chatty_dialogs_create_dialog_muc_info (void)
{
  GtkBuilder *builder;
  GtkWidget  *dialog;
  GtkWidget  *button_back;
  GtkWidget  *button_invite_contact;
  GtkWidget  *button_show_invite_contact;
  GtkListBox *list_muc_settings;

  chatty_data_t        *chatty = chatty_get_data ();
  chatty_dialog_data_t *chatty_dialog = chatty_get_dialog_data ();

  builder = gtk_builder_new_from_resource ("/sm/puri/chatty/ui/chatty-dialog-muc-info.ui");

  dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));

  chatty->pane_view_muc_info = GTK_BOX (gtk_builder_get_object (builder, "pane_view_muc_info"));
  chatty->muc.label_chat_id = GTK_WIDGET (gtk_builder_get_object (builder, "muc.label_chat_id"));
  chatty->muc.label_num_user = GTK_WIDGET (gtk_builder_get_object (builder, "muc.label_num_user"));
  chatty->muc.label_topic = GTK_WIDGET (gtk_builder_get_object (builder, "muc.label_topic"));
  chatty->muc.button_edit_topic = GTK_WIDGET (gtk_builder_get_object (builder, "muc.button_edit_topic"));
  chatty->muc.box_topic_editor = GTK_WIDGET (gtk_builder_get_object (builder, "muc.box_topic_editor"));
  chatty->muc.switch_prefs_notifications = GTK_SWITCH (gtk_builder_get_object (builder, "pref_muc_notifications"));
  chatty->muc.switch_prefs_persistant = GTK_SWITCH (gtk_builder_get_object (builder, "pref_muc_persistant"));
  chatty->muc.switch_prefs_autojoin = GTK_SWITCH (gtk_builder_get_object (builder, "pref_muc_autojoin"));
  chatty_dialog->box_topic_frame = GTK_WIDGET (gtk_builder_get_object (builder, "box_topic_frame"));
  chatty_dialog->textview_muc_topic = GTK_WIDGET (gtk_builder_get_object (builder, "textview_muc_topic"));
  chatty_dialog->stack_panes_muc_info = GTK_STACK (gtk_builder_get_object (builder, "stack_panes_muc_info"));
  chatty_dialog->entry_invite_name = GTK_ENTRY (gtk_builder_get_object (builder, "entry_invite_name"));
  chatty_dialog->entry_invite_msg = GTK_ENTRY (gtk_builder_get_object (builder, "entry_invite_msg"));
  button_back = GTK_WIDGET (gtk_builder_get_object (builder, "button_back"));
  button_show_invite_contact = GTK_WIDGET (gtk_builder_get_object (builder, "button_show_invite_contact"));
  button_invite_contact = GTK_WIDGET (gtk_builder_get_object (builder, "button_invite_contact"));

  list_muc_settings = GTK_LIST_BOX (gtk_builder_get_object (builder, "list_muc_settings"));
  gtk_list_box_set_header_func (list_muc_settings, hdy_list_box_separator_header, NULL, NULL);

  chatty->muc.msg_buffer_topic = gtk_text_buffer_new (NULL);
  gtk_text_view_set_buffer (GTK_TEXT_VIEW(chatty_dialog->textview_muc_topic),
                            chatty->muc.msg_buffer_topic);

  g_signal_connect_after (G_OBJECT(chatty_dialog->textview_muc_topic),
                          "key-release-event",
                          G_CALLBACK(cb_textview_key_released),
                          NULL);

  g_signal_connect (G_OBJECT(chatty->muc.button_edit_topic),
                    "clicked",
                    G_CALLBACK (cb_button_edit_topic_clicked),
                    NULL);

  g_signal_connect (chatty->muc.switch_prefs_notifications,
                    "state-set",
                    G_CALLBACK(cb_switch_prefs_state_changed),
                    (gpointer)CHATTY_PREF_MUC_NOTIFICATIONS);

  g_signal_connect (chatty->muc.switch_prefs_persistant,
                    "state-set",
                    G_CALLBACK(cb_switch_prefs_state_changed),
                    (gpointer)CHATTY_PREF_MUC_PERSISTANT);

  g_signal_connect (chatty->muc.switch_prefs_autojoin,
                    "state-set",
                    G_CALLBACK(cb_switch_prefs_state_changed),
                    (gpointer)CHATTY_PREF_MUC_AUTOJOIN);

  g_signal_connect (G_OBJECT(dialog),
                    "delete-event",
                    G_CALLBACK(cb_dialog_delete),
                    NULL);

  g_signal_connect (G_OBJECT(button_back),
                    "clicked",
                    G_CALLBACK (cb_button_muc_info_back_clicked),
                    NULL);

  g_signal_connect (G_OBJECT(button_show_invite_contact),
                    "clicked",
                    G_CALLBACK (cb_button_show_invite_contact_clicked),
                    NULL);

  g_signal_connect (G_OBJECT(button_invite_contact),
                    "clicked",
                    G_CALLBACK (cb_button_invite_contact_clicked),
                    NULL);

  gtk_list_box_set_header_func (chatty_dialog->list_select_account,
                                hdy_list_box_separator_header,
                                NULL, NULL);

  g_signal_connect_after (G_OBJECT(chatty_dialog->entry_invite_name),
                          "insert_text",
                          G_CALLBACK(cb_invite_name_insert_text),
                          (gpointer)button_invite_contact);

  g_signal_connect_after (G_OBJECT(chatty_dialog->entry_invite_name),
                          "delete_text",
                          G_CALLBACK(cb_invite_name_delete_text),
                          (gpointer)button_invite_contact);

  gtk_window_set_transient_for (GTK_WINDOW(dialog),
                                GTK_WINDOW(chatty->main_window));

  g_object_unref (builder);

  return dialog;
}


void
chatty_dialogs_show_dialog_join_muc (void)
{
  GtkBuilder *builder;
  GtkWidget  *dialog;
  GtkWidget  *button_join_chat;
  GtkListBox *list_select_muc_account;
  GtkEntry   *entry_group_chat_id;
  GtkEntry   *entry_group_chat_pw;
  GtkSwitch  *switch_prefs_chat_autojoin;
  gboolean    autojoin;
  int         response;

  chatty_data_t *chatty = chatty_get_data ();

  builder = gtk_builder_new_from_resource ("/sm/puri/chatty/ui/chatty-dialog-join-muc.ui");

  dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));

  list_select_muc_account = GTK_LIST_BOX (gtk_builder_get_object (builder, "list_select_muc_account"));
  button_join_chat = GTK_WIDGET (gtk_builder_get_object (builder, "button_join_chat"));
  entry_group_chat_id = GTK_ENTRY (gtk_builder_get_object (builder, "entry_group_chat_id"));
  entry_group_chat_pw = GTK_ENTRY (gtk_builder_get_object (builder, "entry_group_chat_pw"));
  switch_prefs_chat_autojoin = GTK_SWITCH (gtk_builder_get_object (builder, "switch_prefs_chat_autojoin"));

  gtk_list_box_set_header_func (list_select_muc_account,
                                hdy_list_box_separator_header,
                                NULL, NULL);

  g_signal_connect (G_OBJECT(entry_group_chat_id),
                    "insert_text",
                    G_CALLBACK(cb_contact_name_insert_text),
                    (gpointer)button_join_chat);

  chatty_account_populate_account_list (list_select_muc_account,
                                        LIST_SELECT_MUC_ACCOUNT);

  gtk_window_set_transient_for (GTK_WINDOW(dialog),
                                GTK_WINDOW(chatty->main_window));

  response = gtk_dialog_run (GTK_DIALOG(dialog));

  if (response == GTK_RESPONSE_OK) {
    autojoin = gtk_switch_get_state (switch_prefs_chat_autojoin);

    chatty_blist_join_group_chat (chatty->selected_account,
                                  gtk_entry_get_text (entry_group_chat_id),
                                  NULL,
                                  gtk_entry_get_text (entry_group_chat_pw),
                                  autojoin);
  }

  gtk_widget_destroy (dialog);
  g_object_unref (builder);
}


void
chatty_dialogs_show_dialog_welcome (gboolean show_label_sms)
{
  GtkBuilder *builder;
  GtkWidget  *dialog;
  GtkWidget  *label_sms;

  chatty_data_t *chatty = chatty_get_data ();

  builder = gtk_builder_new_from_resource ("/sm/puri/chatty/ui/chatty-dialog-welcome.ui");

  label_sms = GTK_WIDGET (gtk_builder_get_object (builder, "label_sms"));

  if (show_label_sms) {
    gtk_widget_show (label_sms);
  }

  dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));

  gtk_window_set_transient_for (GTK_WINDOW(dialog),
                                GTK_WINDOW(chatty->main_window));

  gtk_dialog_run (GTK_DIALOG(dialog));


  gtk_widget_destroy (dialog);
  g_object_unref (builder);
}
