/* chatty-main-view.c
 *
 * Copyright 2022 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-main-view"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>

#include "chatty-avatar.h"
#include "chatty-chat.h"
#include "chatty-ma-key-chat.h"
#include "chatty-ma-chat.h"
#include "chatty-mm-chat.h"
#include "chatty-history.h"
#include "chatty-invite-page.h"
#include "chatty-verification-page.h"
#include "chatty-chat-page.h"
#include "chatty-main-view.h"

struct _ChattyMainView
{
  AdwNavigationPage        parent_instance;

  GtkWidget    *header_bar;
  GtkWidget    *avatar;
  GtkWidget    *title;
  GtkWidget    *menu_button;
  GtkWidget    *call_button;

  GtkWidget    *notification_revealer;
  GtkWidget    *notification_label;
  GtkWidget    *close_button;

  GtkWidget    *main_stack;
  GtkWidget    *chat_page;
  GtkWidget    *invite_page;
  GtkWidget    *verification_page;
  GtkWidget    *empty_page;

  GtkWidget    *menu_popover;
  GtkWidget    *leave_button;
  GtkWidget    *block_button;
  GtkWidget    *unblock_button;
  GtkWidget    *archive_button;
  GtkWidget    *unarchive_button;
  GtkWidget    *delete_button;
  GtkWidget    *back_button;

  ChattyItem   *item;

  GBinding     *title_binding;
  gulong        content_handler;
};

G_DEFINE_TYPE (ChattyMainView, chatty_main_view, ADW_TYPE_NAVIGATION_PAGE)

static void
header_bar_update_item_state_button (ChattyMainView *self,
                                     ChattyItem     *item)
{
  ChattyItemState state;

  gtk_widget_set_visible (self->block_button, FALSE);
  gtk_widget_set_visible (self->unblock_button, FALSE);
  gtk_widget_set_visible (self->archive_button, FALSE);
  gtk_widget_set_visible (self->unarchive_button, FALSE);

  if (!item || !CHATTY_IS_MM_CHAT (item))
    return;

  state = chatty_item_get_state (item);

  if (state == CHATTY_ITEM_VISIBLE) {
    gtk_widget_set_visible (self->block_button, TRUE);
    gtk_widget_set_visible (self->archive_button, TRUE);
  } else if (state == CHATTY_ITEM_ARCHIVED) {
    gtk_widget_set_visible (self->unarchive_button, TRUE);
  } else if (state == CHATTY_ITEM_BLOCKED) {
    gtk_widget_set_visible (self->unblock_button, TRUE);
  }
}

static void
header_bar_chat_changed_cb (ChattyMainView *self,
                            ChattyItem     *item)
{
  GListModel *users;

  users = chatty_chat_get_users (CHATTY_CHAT (item));

  /* allow changing state only for 1:1 SMS/MMS chats  */
  if (item == self->item &&
      CHATTY_IS_MM_CHAT (item) &&
      g_list_model_get_n_items (users) == 1)
    header_bar_update_item_state_button (self, item);
}

static void
main_view_account_status_changed_cb (ChattyMainView *self)
{
  ChattyAccount *account;
  const char *status_text = "";
  ChattyStatus status;
  gboolean reveal = FALSE;

  g_assert (CHATTY_IS_MAIN_VIEW (self));

  account = chatty_chat_get_account (CHATTY_CHAT (self->item));
  status  = chatty_account_get_status (account);

  if (status != CHATTY_CONNECTED)
    reveal = TRUE;

  if (status == CHATTY_DISCONNECTED)
    status_text = _("Account disconnected");
  else if (status == CHATTY_CONNECTING)
    status_text = _("Account connecting…");

  gtk_label_set_label (GTK_LABEL (self->notification_label), status_text);
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->notification_revealer), reveal);
}

static void
main_view_notification_closed_cb (ChattyMainView *self)
{
  g_assert (CHATTY_IS_MAIN_VIEW (self));
}

static void
chatty_main_view_dispose (GObject *object)
{
  ChattyMainView *self = (ChattyMainView *)object;

  if (CHATTY_IS_CHAT (self->item) && chatty_chat_get_account (CHATTY_CHAT (self->item))) {
    ChattyAccount *account;

    account = chatty_chat_get_account (CHATTY_CHAT (self->item));
    g_signal_handlers_disconnect_by_func (account,
                                          main_view_account_status_changed_cb,
                                          self);
  }

  g_clear_object (&self->item);

  G_OBJECT_CLASS (chatty_main_view_parent_class)->dispose (object);
}

static void
chatty_main_view_class_init (ChattyMainViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = chatty_main_view_dispose;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/Chatty/"
                                               "ui/chatty-main-view.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, header_bar);
  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, avatar);
  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, title);
  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, menu_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, call_button);

  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, notification_revealer);
  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, notification_label);
  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, close_button);

  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, main_stack);
  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, chat_page);
  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, invite_page);
  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, verification_page);
  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, empty_page);

  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, menu_popover);
  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, leave_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, block_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, block_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, unblock_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, archive_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, unarchive_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, delete_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, back_button);

  gtk_widget_class_bind_template_callback (widget_class, main_view_notification_closed_cb);

  g_type_ensure (CHATTY_TYPE_CHAT_PAGE);
  g_type_ensure (CHATTY_TYPE_VERIFICATION_PAGE);
}

static void
chatty_main_view_init (ChattyMainView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

void
chatty_main_view_set_db (ChattyMainView *self,
                         gpointer        db)
{
  g_return_if_fail (CHATTY_IS_MAIN_VIEW (self));
  g_return_if_fail (CHATTY_IS_HISTORY (db));

  chatty_chat_page_set_db (CHATTY_CHAT_PAGE (self->chat_page), db);
}

void
chatty_main_view_set_item (ChattyMainView *self,
                           ChattyItem     *item)
{
  g_return_if_fail (CHATTY_IS_MAIN_VIEW (self));
  g_return_if_fail (!item || CHATTY_IS_ITEM (item));

  if (self->item && item != self->item) {
    g_clear_signal_handler (&self->content_handler, self->item);
    g_clear_object (&self->title_binding);

    if (CHATTY_IS_CHAT (self->item) && chatty_chat_get_account (CHATTY_CHAT (self->item))) {
      ChattyAccount *account;

      account = chatty_chat_get_account (CHATTY_CHAT (self->item));
      g_signal_handlers_disconnect_by_func (account,
                                            main_view_account_status_changed_cb,
                                            self);
    }
  }

  g_set_object (&self->item, item);
  adw_window_title_set_title (ADW_WINDOW_TITLE (self->title), "");
  header_bar_update_item_state_button (self, item);

  gtk_widget_set_visible (self->menu_button, !!item);
  gtk_widget_set_visible (self->avatar, !!item);
  gtk_widget_set_visible (self->call_button, FALSE);

  if (item) {
    gtk_widget_set_visible (self->leave_button, !CHATTY_IS_MM_CHAT (item));
    /* We can't delete MaChat */
    gtk_widget_set_visible (self->delete_button, !CHATTY_IS_MA_CHAT (item));

    if (CHATTY_IS_MM_CHAT (item)) {
      GListModel *users;
      const char *name;

      users = chatty_chat_get_users (CHATTY_CHAT (item));
      name = chatty_chat_get_chat_name (CHATTY_CHAT (item));

      /* allow changing state only for 1:1 SMS/MMS chats  */
      if (g_list_model_get_n_items (users) == 1)
        header_bar_update_item_state_button (self, item);

      if (g_list_model_get_n_items (users) == 1 &&
          chatty_utils_username_is_valid (name, CHATTY_PROTOCOL_MMS_SMS)) {
        g_autoptr(ChattyMmBuddy) buddy = NULL;
        g_autoptr(GAppInfo) app_info = NULL;

        app_info = g_app_info_get_default_for_uri_scheme ("tel");
        buddy = g_list_model_get_item (users, 0);

        if (app_info)
          gtk_widget_set_visible (self->call_button, TRUE);
      }
    }

    chatty_avatar_set_item (CHATTY_AVATAR (self->avatar), item);
    self->title_binding = g_object_bind_property (item, "name",
                                                  self->title, "title",
                                                  G_BINDING_SYNC_CREATE);

    if (CHATTY_IS_CHAT (item))
      self->content_handler = g_signal_connect_object (item, "changed",
                                                       G_CALLBACK (header_bar_chat_changed_cb),
                                                       self, G_CONNECT_SWAPPED);
  }

  chatty_verification_page_set_item (CHATTY_VERIFICATION_PAGE (self->verification_page), item);

  if (!item || (CHATTY_IS_CHAT (item) && !CHATTY_IS_MA_KEY_CHAT (item)))
    chatty_chat_page_set_chat (CHATTY_CHAT_PAGE (self->chat_page), (ChattyChat *)item);

  if (CHATTY_IS_CHAT (item)) {
    ChattyChatState state;

    state = chatty_chat_get_chat_state (CHATTY_CHAT (item));

    if (state == CHATTY_CHAT_INVITED)
      chatty_invite_page_set_chat (CHATTY_INVITE_PAGE (self->invite_page), (ChattyChat *)item);

    if (state == CHATTY_CHAT_INVITED) {
      gtk_stack_set_visible_child (GTK_STACK (self->main_stack), self->invite_page);
      gtk_widget_set_visible (self->back_button, TRUE);
    } else if (state == CHATTY_CHAT_VERIFICATION) {
      gtk_stack_set_visible_child (GTK_STACK (self->main_stack), self->verification_page);
      gtk_widget_set_visible (self->back_button, TRUE);
    } else {
      gtk_stack_set_visible_child (GTK_STACK (self->main_stack), self->chat_page);
      gtk_widget_set_visible (self->back_button, TRUE);
    }
  } else {
    gtk_revealer_set_reveal_child (GTK_REVEALER (self->notification_revealer), FALSE);
    gtk_stack_set_visible_child (GTK_STACK (self->main_stack), self->empty_page);
    gtk_widget_set_visible (self->back_button, FALSE);
  }

  if (CHATTY_IS_CHAT (item) && chatty_chat_get_account (CHATTY_CHAT (item))) {
    ChattyAccount *account;

    account = chatty_chat_get_account (CHATTY_CHAT (item));

    g_signal_connect_object (account, "notify::status",
                             G_CALLBACK (main_view_account_status_changed_cb),
                             self, G_CONNECT_SWAPPED);
    main_view_account_status_changed_cb (self);
  }
}

ChattyItem *
chatty_main_view_get_item (ChattyMainView *self)
{
  g_return_val_if_fail (CHATTY_IS_MAIN_VIEW (self), NULL);

  return self->item;
}
