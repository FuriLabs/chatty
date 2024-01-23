/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-window"

#ifdef HAVE_CONFIG_H
# include "config.h"
# include "version.h"
#endif

#define _GNU_SOURCE
#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>

#include "chatty-window.h"
#include "chatty-history.h"
#include "chatty-manager.h"
#include "chatty-purple.h"
#include "chatty-settings.h"
#include "chatty-mm-chat.h"
#include "chatty-chat-list.h"
#include "chatty-side-bar.h"
#include "chatty-main-view.h"
#include "chatty-manager.h"
#include "chatty-utils.h"
#include "dialogs/chatty-info-dialog.h"
#include "dialogs/chatty-settings-dialog.h"
#include "dialogs/chatty-new-chat-dialog.h"
#include "dialogs/chatty-new-muc-dialog.h"
#include "chatty-log.h"

struct _ChattyWindow
{
  AdwApplicationWindow parent_instance;

  ChattySettings *settings;

  GtkWidget *chat_list;

  GtkWidget *content_box;
  GtkWidget *side_bar;
  GtkWidget *main_view;

  GtkWidget *new_chat_dialog;
  GtkWidget *chat_info_dialog;

  GtkWidget *settings_dialog;

  ChattyManager *manager;
  ChattyItem    *item;

  gulong         chat_changed_handler;
};


G_DEFINE_TYPE (ChattyWindow, chatty_window, ADW_TYPE_APPLICATION_WINDOW)

static void
chatty_window_set_item (ChattyWindow *self,
                        ChattyItem   *item)
{
  g_return_if_fail (CHATTY_IS_WINDOW (self));
  g_return_if_fail (!item || CHATTY_IS_ITEM (item));

  if (self->item == item)
    return;

  g_set_object (&self->item, item);
  chatty_main_view_set_item (CHATTY_MAIN_VIEW (self->main_view), item);
}

static void
window_chat_changed_cb (ChattyWindow *self,
                        ChattyChat   *chat)
{
  GListModel *users;

  users = chatty_chat_get_users (chat);

  /* allow changing state only for 1:1 SMS/MMS chats  */
  if (CHATTY_IS_MM_CHAT (chat) && g_list_model_get_n_items (users) == 1) {
    chatty_chat_list_refilter (CHATTY_CHAT_LIST (self->chat_list));
  }
}

static void
window_set_item (ChattyWindow *self,
                 ChattyChat   *chat)
{
  g_assert (CHATTY_IS_WINDOW (self));

  g_clear_signal_handler (&self->chat_changed_handler,
                          chatty_main_view_get_item (CHATTY_MAIN_VIEW (self->main_view)));

  if (CHATTY_IS_CHAT (chat)) {
    self->chat_changed_handler = g_signal_connect_object (chat, "changed",
                                                          G_CALLBACK (window_chat_changed_cb),
                                                          self, G_CONNECT_SWAPPED);
  }

  if (!chat)
    adw_leaflet_set_visible_child (ADW_LEAFLET (self->content_box), self->side_bar);

  chatty_window_set_item (self, CHATTY_ITEM (chat));
}

static void
chatty_window_open_item (ChattyWindow *self,
                         ChattyItem   *item)
{
  g_assert (CHATTY_IS_WINDOW (self));
  g_assert (CHATTY_IS_ITEM (item));
  CHATTY_INFO (chatty_item_get_name (item),
               "Opening item of type: %s, name:", G_OBJECT_TYPE_NAME (item));

  if (CHATTY_IS_CONTACT (item)) {
    const char *number;

    number = chatty_item_get_username (item);
    chatty_window_set_uri (self, number, NULL);

    return;
  }

#ifdef PURPLE_ENABLED
  if (CHATTY_IS_PP_BUDDY (item) ||
      CHATTY_IS_PP_CHAT (item))
    chatty_purple_start_chat (chatty_purple_get_default (), item);
#endif

  if (CHATTY_IS_MM_CHAT (item)) {
    chatty_window_open_chat (CHATTY_WINDOW (self), CHATTY_CHAT (item));
  }
}

static void
window_chat_list_selection_changed (ChattyWindow   *self,
                                    ChattyChatList *list)
{
  GPtrArray *chat_list;
  ChattyChat *chat;

  g_assert (CHATTY_IS_WINDOW (self));
  g_assert (CHATTY_IS_CHAT_LIST (list));

  chat_list = chatty_chat_list_get_selected (list);

  if (!chat_list->len) {
    GListModel *model;

    model = chatty_chat_list_get_filter_model (CHATTY_CHAT_LIST (self->chat_list));
    if (g_list_model_get_n_items (model) == 0)
      chatty_window_set_item (self, NULL);

    return;
  }

  chat = chat_list->pdata[0];
  g_return_if_fail (CHATTY_IS_CHAT (chat));

  if (chat == (gpointer)chatty_main_view_get_item (CHATTY_MAIN_VIEW (self->main_view))) {
    adw_leaflet_set_visible_child (ADW_LEAFLET (self->content_box), self->main_view);

    if (chatty_window_get_active_chat (self))
      chatty_chat_set_unread_count (chat, 0);

    return;
  }

#ifdef PURPLE_ENABLED
  if (CHATTY_IS_PP_CHAT (chat))
    chatty_window_open_item (self, CHATTY_ITEM (chat));
  else
#endif
    chatty_window_open_chat (self, chat);
}

static void
notify_fold_cb (ChattyWindow *self)
{
  gboolean folded;

  if (!gtk_widget_get_mapped (GTK_WIDGET (self)))
    return;

  folded = adw_leaflet_get_folded (ADW_LEAFLET (self->content_box));
  chatty_chat_list_set_selection_mode (CHATTY_CHAT_LIST (self->chat_list), !folded);

  if (!folded) {
    ChattyItem *item;

    item = chatty_main_view_get_item (CHATTY_MAIN_VIEW (self->main_view));
    chatty_chat_list_select_item (CHATTY_CHAT_LIST (self->chat_list), item);
  }
}

static void
window_show_new_chat_dialog (ChattyWindow *self,
                             gboolean      can_multi_select)
{
  ChattyNewChatDialog *dialog;

  g_assert (CHATTY_IS_WINDOW (self));

  dialog = CHATTY_NEW_CHAT_DIALOG (self->new_chat_dialog);
  chatty_new_chat_dialog_set_multi_selection (dialog, can_multi_select);
  gtk_window_present (GTK_WINDOW (self->new_chat_dialog));
}

static void
window_chat_deleted_cb (ChattyWindow *self,
                        ChattyChat   *chat)
{
  g_assert (CHATTY_IS_WINDOW (self));
  g_assert (CHATTY_IS_CHAT (chat));

  if (chatty_main_view_get_item (CHATTY_MAIN_VIEW (self->main_view)) != (gpointer)chat)
    return;

  window_set_item (self, NULL);
}

static void
new_chat_selection_changed_cb (ChattyWindow        *self,
                               ChattyNewChatDialog *dialog)
{
  g_autoptr(GString) users = g_string_new (NULL);
  GListModel *model;
  guint n_items;
  const char *name;

  g_assert (CHATTY_IS_WINDOW (self));
  g_assert (CHATTY_IS_NEW_CHAT_DIALOG (dialog));

  model = chatty_new_chat_dialog_get_selected_items (dialog);
  n_items = g_list_model_get_n_items (model);

  if (n_items == 0)
    goto end;

  for (guint i = 0; i < n_items; i++) {
    g_autoptr(ChattyItem) item = NULL;
    const char *phone_number;

    item = g_list_model_get_item (model, i);

    if (CHATTY_IS_CONTACT (item)) {
      phone_number = chatty_item_get_username (item);
      g_string_append (users, phone_number);
      g_string_append (users, ",");
    }
  }

  /* Remove the trailing "," */
  if (users->len >= 1)
    g_string_truncate (users, users->len - 1);

  if (n_items == 1) {
    g_autoptr(ChattyItem) item = NULL;

    item = g_list_model_get_item (model, 0);

    if (!CHATTY_IS_CONTACT (item) ||
        !chatty_contact_is_dummy (CHATTY_CONTACT (item))) {
      chatty_window_open_item (self, item);
      goto end;
    }
  }

  name = chatty_new_chat_dialog_get_chat_title (dialog);
  chatty_window_set_uri (self, users->str, name);

 end:
  gtk_widget_set_visible (GTK_WIDGET (dialog), FALSE);
}

static void
chatty_window_archive_chat (GtkWidget  *widget,
                            const char *action_name,
                            GVariant   *param)
{
  ChattyWindow *self = CHATTY_WINDOW (widget);
  ChattyItem *item;

  g_assert (CHATTY_IS_WINDOW (self));

  item = chatty_main_view_get_item (CHATTY_MAIN_VIEW (self->main_view));
  g_return_if_fail (CHATTY_IS_MM_CHAT (item));
  g_return_if_fail (chatty_chat_is_im (CHATTY_CHAT (item)));

  chatty_item_set_state (item, CHATTY_ITEM_ARCHIVED);
}

static void
chatty_window_unarchive_chat (GtkWidget  *widget,
                              const char *action_name,
                              GVariant   *param)
{
  ChattyWindow *self = CHATTY_WINDOW (widget);
  ChattyItem *item;

  g_assert (CHATTY_IS_WINDOW (self));

  item = chatty_main_view_get_item (CHATTY_MAIN_VIEW (self->main_view));
  g_return_if_fail (CHATTY_IS_MM_CHAT (item));
  g_return_if_fail (chatty_chat_is_im (CHATTY_CHAT (item)));

  chatty_item_set_state (item, CHATTY_ITEM_VISIBLE);
}

static void
window_block_chat_response_cb (GObject         *dialog,
                               GAsyncResult    *response,
                               gpointer         user_data)
{
  ChattyWindow *self = CHATTY_WINDOW (user_data);
  g_autoptr(GError) error = NULL;
  int dialog_response = -1;

  g_assert (CHATTY_IS_WINDOW (self));


  dialog_response = gtk_alert_dialog_choose_finish (GTK_ALERT_DIALOG (dialog), response, &error);

  if (!chatty_utils_dialog_response_is_cancel (GTK_ALERT_DIALOG (dialog), dialog_response)) {
    ChattyItem *item;

    item = chatty_main_view_get_item (CHATTY_MAIN_VIEW (self->main_view));
    chatty_item_set_state (item, CHATTY_ITEM_BLOCKED);
  }
  g_clear_object (&dialog);
}

static void
chatty_window_block_chat (GtkWidget  *widget,
                          const char *action_name,
                          GVariant   *param)
{
  ChattyWindow *self = CHATTY_WINDOW (widget);
  ChattyItem *item;
  GtkWindow *window;
  GtkAlertDialog *dialog;
  const char *list[] = {_("Cancel"), _("Continue"), NULL};

  g_assert (CHATTY_IS_WINDOW (self));

  item = chatty_main_view_get_item (CHATTY_MAIN_VIEW (self->main_view));
  g_return_if_fail (CHATTY_IS_MM_CHAT (item));
  g_return_if_fail (chatty_chat_is_im (CHATTY_CHAT (item)));

  dialog = gtk_alert_dialog_new (_("You shall no longer be notified for new messages, continue?"));
  gtk_alert_dialog_set_buttons (dialog, list);
  gtk_alert_dialog_set_cancel_button (dialog, 0);
  gtk_alert_dialog_set_default_button (dialog, 1);
  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
  gtk_alert_dialog_choose (dialog, window, NULL, window_block_chat_response_cb, self);
}

static void
chatty_window_unblock_chat (GtkWidget  *widget,
                            const char *action_name,
                            GVariant   *param)
{
  ChattyWindow *self = CHATTY_WINDOW (widget);
  ChattyItem *item;

  g_assert (CHATTY_IS_WINDOW (self));

  item = chatty_main_view_get_item (CHATTY_MAIN_VIEW (self->main_view));
  g_return_if_fail (CHATTY_IS_MM_CHAT (item));
  g_return_if_fail (chatty_chat_is_im (CHATTY_CHAT (item)));

  chatty_item_set_state (item, CHATTY_ITEM_VISIBLE);

}

static void
window_delete_chat_response_cb (GObject         *dialog,
                                GAsyncResult    *response,
                                gpointer         user_data)
{
  ChattyWindow *self = CHATTY_WINDOW (user_data);
  g_autoptr(GError) error = NULL;
  int dialog_response = -1;

  g_assert (CHATTY_IS_WINDOW (self));

  dialog_response = gtk_alert_dialog_choose_finish (GTK_ALERT_DIALOG (dialog), response, &error);

  if (!chatty_utils_dialog_response_is_cancel (GTK_ALERT_DIALOG (dialog), dialog_response)) {
    ChattyChat *chat;

    chat = (ChattyChat *)chatty_main_view_get_item (CHATTY_MAIN_VIEW (self->main_view));

    chatty_history_delete_chat (chatty_manager_get_history (self->manager),
                                chat);
#ifdef PURPLE_ENABLED
    if (CHATTY_IS_PP_CHAT (chat)) {
      chatty_pp_chat_delete (CHATTY_PP_CHAT (chat));
    } else
#endif
    if (CHATTY_IS_MM_CHAT (chat)) {
      chatty_mm_chat_delete (CHATTY_MM_CHAT (chat));
    } else {
      g_return_if_reached ();
    }

    window_set_item (self, NULL);

    if (!adw_leaflet_get_folded (ADW_LEAFLET (self->content_box)))
      chatty_chat_list_select_first (CHATTY_CHAT_LIST (self->chat_list));
  }
  g_clear_object (&dialog);
}

static void
chatty_window_delete_chat (GtkWidget  *widget,
                           const char *action_name,
                           GVariant   *param)
{
  ChattyWindow *self = CHATTY_WINDOW (widget);
  g_autofree char *text = NULL;
  ChattyChat *chat;
  const char *name;
  const char *sub_text;
  GtkWindow *window;
  GtkAlertDialog *dialog;
  g_autofree char *secondary_text = NULL;

  const char *list[] = {_("Cancel"), _("Delete"), NULL};

  g_assert (CHATTY_IS_WINDOW (self));

  chat = (ChattyChat *)chatty_main_view_get_item (CHATTY_MAIN_VIEW (self->main_view));
  g_return_if_fail (chat);

  name = chatty_item_get_name (CHATTY_ITEM (chat));

  if (chatty_chat_is_im (chat)) {
    text = g_strdup_printf (_("Delete chat with “%s”"), name);
    sub_text = _("This deletes the conversation history");
  } else {
    text = g_strdup_printf (_("Disconnect group chat “%s”"), name);
    sub_text = _("This removes chat from chats list");
  }

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));

  dialog = gtk_alert_dialog_new ("%s", text);
  secondary_text = g_strdup_printf("%s", sub_text);
  gtk_alert_dialog_set_detail (dialog, secondary_text);
  gtk_alert_dialog_set_buttons (dialog, list);
  gtk_alert_dialog_set_cancel_button (dialog, 0);
  gtk_alert_dialog_set_default_button (dialog, 1);
  gtk_alert_dialog_choose (dialog, window, NULL, window_delete_chat_response_cb, self);
}

static void
chatty_window_leave_chat (GtkWidget  *widget,
                          const char *action_name,
                          GVariant   *param)
{
  ChattyWindow *self = CHATTY_WINDOW (widget);
  ChattyChat *chat;

  g_assert (CHATTY_IS_WINDOW (self));

  chat = (ChattyChat *)chatty_main_view_get_item (CHATTY_MAIN_VIEW (self->main_view));
  g_warn_if_fail (chat);

  if (chat) {
    ChattyAccount *account;

    account = chatty_chat_get_account (chat);
    chatty_account_leave_chat_async (account, chat, NULL, NULL);
  }

  window_set_item (self, NULL);

  if (!adw_leaflet_get_folded (ADW_LEAFLET (self->content_box)))
    chatty_chat_list_select_first (CHATTY_CHAT_LIST (self->chat_list));
}

static void
chatty_window_show_archived (GtkWidget  *widget,
                             const char *action_name,
                             GVariant   *param)
{
  ChattyWindow *self = CHATTY_WINDOW (widget);

  g_assert (CHATTY_IS_WINDOW (self));

  g_debug ("Show archived chats");

  chatty_side_bar_set_show_archived (CHATTY_SIDE_BAR (self->side_bar), TRUE);
}

static void
chatty_window_show_chat_details (GtkWidget  *widget,
                                 const char *action_name,
                                 GVariant   *param)
{
  ChattyWindow *self = CHATTY_WINDOW (widget);
  ChattyInfoDialog *dialog;
  ChattyChat *chat;

  g_assert (CHATTY_IS_WINDOW (self));

  g_debug ("Show chat details");

  chat = (ChattyChat *)chatty_main_view_get_item (CHATTY_MAIN_VIEW (self->main_view));
  g_return_if_fail (CHATTY_IS_CHAT (chat));

  dialog = CHATTY_INFO_DIALOG (self->chat_info_dialog);

  chatty_info_dialog_set_chat (dialog, chat);
  gtk_window_present (GTK_WINDOW (dialog));
}

static void
chatty_window_show_settings (GtkWidget  *widget,
                             const char *action_name,
                             GVariant   *param)
{
  ChattyWindow *self = CHATTY_WINDOW (widget);

  g_assert (CHATTY_IS_WINDOW (self));

  g_debug ("Show settings");

  if (!self->settings_dialog)
    self->settings_dialog = chatty_settings_dialog_new (GTK_WINDOW (self));
  gtk_window_present (GTK_WINDOW (self->settings_dialog));
}

static void
chatty_window_go_back (GtkWidget  *widget,
                       const char *action_name,
                       GVariant   *param)
{
  ChattyWindow *self = CHATTY_WINDOW (widget);

  g_assert (CHATTY_IS_WINDOW (self));

  if (!chatty_chat_list_is_archived (CHATTY_CHAT_LIST (self->chat_list)))
    window_set_item (self, NULL);
}

static void
chatty_window_start_new_chat (GtkWidget  *widget,
                              const char *action_name,
                              GVariant   *param)
{
  ChattyWindow *self = CHATTY_WINDOW (widget);

  g_assert (CHATTY_IS_WINDOW (self));

  window_show_new_chat_dialog (self, FALSE);
}

static void
chatty_window_start_sms_mms_chat (GtkWidget  *widget,
                                  const char *action_name,
                                  GVariant   *param)
{
  ChattyWindow *self = CHATTY_WINDOW (widget);

  g_assert (CHATTY_IS_WINDOW (self));

  window_show_new_chat_dialog (self, TRUE);
}

static void
chatty_window_start_group_chat (GtkWidget  *widget,
                                const char *action_name,
                                GVariant   *param)
{
  ChattyWindow *self = CHATTY_WINDOW (widget);
  GtkWidget *dialog;

  g_assert (CHATTY_IS_WINDOW (self));

  dialog = chatty_new_muc_dialog_new (GTK_WINDOW (self));
  gtk_window_present (GTK_WINDOW (dialog));
}

static void
chatty_window_call_user (GtkWidget  *widget,
                         const char *action_name,
                         GVariant   *param)
{
  ChattyWindow *self = CHATTY_WINDOW (widget);
  g_autofree char *uri = NULL;
  g_autoptr(GtkUriLauncher) uri_launcher = NULL;
  GtkWindow *window;
  ChattyChat *chat;

  g_assert (CHATTY_IS_WINDOW (self));

  chat = (ChattyChat *)chatty_main_view_get_item (CHATTY_MAIN_VIEW (self->main_view));
  g_return_if_fail (CHATTY_IS_MM_CHAT (chat));

  uri = g_strconcat ("tel://", chatty_chat_get_chat_name (chat), NULL);

  CHATTY_INFO (uri, "Calling uri:");

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
  uri_launcher = gtk_uri_launcher_new (uri);
  gtk_uri_launcher_launch (uri_launcher, window, NULL, NULL, NULL);
}

static void
chatty_window_unmap (GtkWidget *widget)
{
  ChattyWindow *self = (ChattyWindow *)widget;
  GtkWindow    *window = (GtkWindow *)widget;
  GdkRectangle  geometry;
  gboolean      is_maximized;

  is_maximized = gtk_window_is_maximized (window);

  chatty_settings_set_window_maximized (self->settings, is_maximized);
  gtk_window_get_default_size (window, &geometry.width, &geometry.height);
  chatty_settings_set_window_geometry (self->settings, &geometry);

  GTK_WIDGET_CLASS (chatty_window_parent_class)->unmap (widget);
}

static void
chatty_window_map (GtkWidget *widget)
{
  ChattyWindow *self = (ChattyWindow *)widget;

  notify_fold_cb (self);

  GTK_WIDGET_CLASS (chatty_window_parent_class)->map (widget);
}

static void
chatty_window_constructed (GObject *object)
{
  ChattyWindow *self = (ChattyWindow *)object;
  GtkWindow    *window = (GtkWindow *)object;
  GdkRectangle  geometry;

  self->settings = g_object_ref (chatty_settings_get_default ());
  chatty_settings_get_window_geometry (self->settings, &geometry);
  gtk_window_set_default_size (window, geometry.width, geometry.height);

  if (chatty_settings_get_window_maximized (self->settings))
    gtk_window_maximize (window);

  self->new_chat_dialog = chatty_new_chat_dialog_new (GTK_WINDOW (self));
  g_signal_connect_object (self->new_chat_dialog, "selection-changed",
                           G_CALLBACK (new_chat_selection_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  self->chat_info_dialog = chatty_info_dialog_new (GTK_WINDOW (self));

  G_OBJECT_CLASS (chatty_window_parent_class)->constructed (object);
}


static void
chatty_window_finalize (GObject *object)
{
  ChattyWindow *self = (ChattyWindow *)object;

  g_clear_pointer ((GtkWindow **)&self->new_chat_dialog, gtk_window_destroy);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (chatty_window_parent_class)->finalize (object);
}

static void
chatty_window_dispose (GObject *object)
{
  ChattyWindow *self = (ChattyWindow *)object;

  g_clear_object (&self->item);

  /* XXX: Why it fails without the check? */
  if (CHATTY_IS_MAIN_VIEW (self->main_view))
    g_clear_signal_handler (&self->chat_changed_handler,
                            chatty_main_view_get_item (CHATTY_MAIN_VIEW (self->main_view)));
  g_clear_object (&self->manager);

  G_OBJECT_CLASS (chatty_window_parent_class)->dispose (object);
}


static void
chatty_window_class_init (ChattyWindowClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed  = chatty_window_constructed;
  object_class->finalize     = chatty_window_finalize;
  object_class->dispose      = chatty_window_dispose;

  widget_class->map = chatty_window_map;
  widget_class->unmap = chatty_window_unmap;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/Chatty/"
                                               "ui/chatty-window.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyWindow, content_box);
  gtk_widget_class_bind_template_child (widget_class, ChattyWindow, side_bar);
  gtk_widget_class_bind_template_child (widget_class, ChattyWindow, main_view);

  gtk_widget_class_bind_template_callback (widget_class, notify_fold_cb);
  gtk_widget_class_bind_template_callback (widget_class, window_chat_list_selection_changed);

  gtk_widget_class_install_action (widget_class, "win.new-chat", NULL, chatty_window_start_new_chat);
  gtk_widget_class_install_action (widget_class, "win.new-sms-mms", NULL, chatty_window_start_sms_mms_chat);
  gtk_widget_class_install_action (widget_class, "win.new-group-chat", NULL, chatty_window_start_group_chat);

  gtk_widget_class_install_action (widget_class, "win.leave-chat", NULL, chatty_window_leave_chat);
  gtk_widget_class_install_action (widget_class, "win.block-chat", NULL, chatty_window_block_chat);
  gtk_widget_class_install_action (widget_class, "win.unblock-chat", NULL, chatty_window_unblock_chat);
  gtk_widget_class_install_action (widget_class, "win.archive-chat", NULL, chatty_window_archive_chat);
  gtk_widget_class_install_action (widget_class, "win.unarchive-chat", NULL, chatty_window_unarchive_chat);
  gtk_widget_class_install_action (widget_class, "win.delete-chat", NULL, chatty_window_delete_chat);
  gtk_widget_class_install_action (widget_class, "win.call-user", NULL, chatty_window_call_user);

  gtk_widget_class_install_action (widget_class, "win.show-archived", NULL, chatty_window_show_archived);
  gtk_widget_class_install_action (widget_class, "win.show-chat-details", NULL, chatty_window_show_chat_details);
  gtk_widget_class_install_action (widget_class, "win.show-settings", NULL, chatty_window_show_settings);
  gtk_widget_class_install_action (widget_class, "win.go-back", NULL, chatty_window_go_back);
}

static void
chatty_window_init (ChattyWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->chat_list = chatty_side_bar_get_chat_list (CHATTY_SIDE_BAR (self->side_bar));
  self->manager = g_object_ref (chatty_manager_get_default ());
  chatty_main_view_set_db (CHATTY_MAIN_VIEW (self->main_view),
                           chatty_manager_get_history (self->manager));
  g_signal_connect_object (self->manager, "chat-deleted",
                           G_CALLBACK (window_chat_deleted_cb), self,
                           G_CONNECT_SWAPPED);
}

GtkWidget *
chatty_window_new (GtkApplication *application)
{
  g_assert (GTK_IS_APPLICATION (application));

  return g_object_new (CHATTY_TYPE_WINDOW,
                       "application", application,
                       NULL);
}

void
chatty_window_set_uri (ChattyWindow *self,
                       const char   *uri,
                       const char   *name)
{
  if (!chatty_manager_set_uri (self->manager, uri, name))
    return;

  gtk_widget_set_visible (self->new_chat_dialog, FALSE);
}

ChattyChat *
chatty_window_get_active_chat (ChattyWindow *self)
{
  ChattyItem *item = NULL;

  g_return_val_if_fail (CHATTY_IS_WINDOW (self), NULL);

  if (chatty_utils_window_has_toplevel_focus (GTK_WINDOW (self)))
    item = chatty_main_view_get_item (CHATTY_MAIN_VIEW (self->main_view));

  if (CHATTY_IS_CHAT (item))
    return CHATTY_CHAT (item);

  return NULL;
}

void
chatty_window_open_chat (ChattyWindow *self,
                         ChattyChat   *chat)
{
  g_return_if_fail (CHATTY_IS_WINDOW (self));
  g_return_if_fail (CHATTY_IS_CHAT (chat));

  CHATTY_INFO (chatty_chat_get_chat_name (chat),
               "opening chat (%p), type: %s, chat-name:", chat, G_OBJECT_TYPE_NAME (chat));

  window_set_item (self, chat);

  adw_leaflet_set_visible_child (ADW_LEAFLET (self->content_box), self->main_view);

  if (chatty_window_get_active_chat (self))
    chatty_chat_set_unread_count (chat, 0);
}
