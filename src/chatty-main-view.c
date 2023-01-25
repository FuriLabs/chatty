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

#include "chatty-chat.h"
#include "chatty-ma-key-chat.h"
#include "chatty-history.h"
#include "chatty-invite-view.h"
#include "chatty-verification-view.h"
#include "chatty-chat-view.h"
#include "chatty-main-view.h"

struct _ChattyMainView
{
  GtkBox        parent_instance;

  GtkWidget    *notification_revealer;
  GtkWidget    *notification_label;
  GtkWidget    *close_button;

  GtkWidget    *main_stack;
  GtkWidget    *content_view;
  GtkWidget    *invite_view;
  GtkWidget    *verification_view;
  GtkWidget    *empty_view;

  ChattyItem   *item;
};

G_DEFINE_TYPE (ChattyMainView, chatty_main_view, GTK_TYPE_BOX)

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

  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, notification_revealer);
  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, notification_label);
  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, close_button);

  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, main_stack);
  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, content_view);
  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, invite_view);
  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, verification_view);
  gtk_widget_class_bind_template_child (widget_class, ChattyMainView, empty_view);

  gtk_widget_class_bind_template_callback (widget_class, main_view_notification_closed_cb);

  g_type_ensure (CHATTY_TYPE_CHAT_VIEW);
  g_type_ensure (CHATTY_TYPE_VERIFICATION_VIEW);
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

  chatty_chat_view_set_db (CHATTY_CHAT_VIEW (self->content_view), db);
}

void
chatty_main_view_set_item (ChattyMainView *self,
                           ChattyItem     *item)
{
  g_return_if_fail (CHATTY_IS_MAIN_VIEW (self));
  g_return_if_fail (!item || CHATTY_IS_ITEM (item));

  if (self->item && item != self->item) {
    if (CHATTY_IS_CHAT (self->item) && chatty_chat_get_account (CHATTY_CHAT (self->item))) {
      ChattyAccount *account;

      account = chatty_chat_get_account (CHATTY_CHAT (self->item));
      g_signal_handlers_disconnect_by_func (account,
                                            main_view_account_status_changed_cb,
                                            self);
    }
  }

  g_set_object (&self->item, item);
  chatty_verification_view_set_item (CHATTY_VERIFICATION_VIEW (self->verification_view), item);

  if (!item || (CHATTY_IS_CHAT (item) && !CHATTY_IS_MA_KEY_CHAT (item)))
    chatty_chat_view_set_chat (CHATTY_CHAT_VIEW (self->content_view), (ChattyChat *)item);

  if (CHATTY_IS_CHAT (item)) {
    ChattyChatState state;

    state = chatty_chat_get_chat_state (CHATTY_CHAT (item));

    if (state == CHATTY_CHAT_INVITED)
      chatty_invite_view_set_chat (CHATTY_INVITE_VIEW (self->invite_view), (ChattyChat *)item);

    if (state == CHATTY_CHAT_INVITED)
      gtk_stack_set_visible_child (GTK_STACK (self->main_stack), self->invite_view);
    else if (state == CHATTY_CHAT_VERIFICATION)
      gtk_stack_set_visible_child (GTK_STACK (self->main_stack), self->verification_view);
    else
      gtk_stack_set_visible_child (GTK_STACK (self->main_stack), self->content_view);
  } else {
    gtk_revealer_set_reveal_child (GTK_REVEALER (self->notification_revealer), FALSE);
    gtk_stack_set_visible_child (GTK_STACK (self->main_stack), self->empty_view);
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
