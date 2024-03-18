/* chatty-invite-page.c
 *
 * Copyright 2022 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-invite-page"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>

#include "chatty-avatar.h"
#include "chatty-manager.h"
#include "chatty-invite-page.h"

struct _ChattyInvitePage
{
  GtkBox         parent_instance;

  GtkWidget     *invite_title;
  GtkWidget     *chat_avatar;
  GtkWidget     *invite_subtitle;

  GtkWidget     *accept_button;
  GtkWidget     *accept_spinner;
  GtkWidget     *reject_button;
  GtkWidget     *reject_spinner;

  ChattyAccount *account;
  ChattyChat    *chat;

  gulong         name_handler;
  gulong         account_handler;
};

G_DEFINE_TYPE (ChattyInvitePage, chatty_invite_page, GTK_TYPE_BOX)

static void
invite_account_status_changed_cb (ChattyInvitePage *self)
{
  gboolean can_connect;

  g_assert (CHATTY_IS_INVITE_PAGE (self));

  if (!self->chat || chatty_chat_get_chat_state (self->chat) != CHATTY_CHAT_INVITED)
    return;

  can_connect = chatty_account_get_status (self->account) == CHATTY_CONNECTED;
  gtk_widget_set_sensitive (self->accept_button, can_connect);
}

static void
chat_invite_name_changed_cb (ChattyInvitePage *self)
{
  g_autofree char *title = NULL;
  const char *name;

  g_assert (CHATTY_IS_INVITE_PAGE (self));

  if (!self->chat || chatty_chat_get_chat_state (self->chat) != CHATTY_CHAT_INVITED)
    return;

  name = chatty_item_get_name (CHATTY_ITEM (self->chat));
  /* TRANSLATORS: %s is the chat room name */
  title = g_strdup_printf (_("Do you want to join “%s”?"), name);
  gtk_label_set_label (GTK_LABEL (self->invite_title), title);
}

static void
chat_accept_invite_cb (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  g_autoptr(ChattyInvitePage) self = user_data;
  g_autoptr(GError) error = NULL;

  if (gtk_widget_in_destruction (user_data))
    return;

  gtk_spinner_stop (GTK_SPINNER (self->accept_spinner));
  gtk_widget_set_sensitive (self->accept_button, TRUE);

  if (chatty_chat_accept_invite_finish (self->chat, result, &error)) {
    ChattyManager *manager;

    manager = chatty_manager_get_default ();
    /* The item will be deleted from invite list and added to joined list */
    g_signal_emit_by_name (manager, "chat-deleted", self->chat);
  }

  if (error)
    g_warning ("Error: %s", error->message);
}

static void
invite_accept_clicked_cb (ChattyInvitePage *self)
{
  g_assert (CHATTY_IS_INVITE_PAGE (self));

  gtk_spinner_start (GTK_SPINNER (self->accept_spinner));
  gtk_widget_set_sensitive (self->accept_button, FALSE);
  chatty_chat_accept_invite_async (self->chat,
                                   chat_accept_invite_cb,
                                   g_object_ref (self));
}

static void
chat_reject_invite_cb (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  g_autoptr(ChattyInvitePage) self = user_data;
  g_autoptr(GError) error = NULL;

  if (gtk_widget_in_destruction (user_data))
    return;

  gtk_spinner_stop (GTK_SPINNER (self->reject_spinner));
  gtk_widget_set_sensitive (self->accept_button, TRUE);

  if (chatty_chat_reject_invite_finish (self->chat, result, &error)) {
    ChattyManager *manager;

    manager = chatty_manager_get_default ();
    g_signal_emit_by_name (manager, "chat-deleted", self->chat);
  }

  if (error)
    g_warning ("Error: %s", error->message);
}

static void
invite_reject_clicked_cb (ChattyInvitePage *self)
{
  g_assert (CHATTY_IS_INVITE_PAGE (self));

  gtk_spinner_start (GTK_SPINNER (self->reject_spinner));
  gtk_widget_set_sensitive (self->accept_button, FALSE);
  chatty_chat_reject_invite_async (self->chat,
                                   chat_reject_invite_cb,
                                   g_object_ref (self));
}

static void
chatty_invite_page_dispose (GObject *object)
{
  ChattyInvitePage *self = (ChattyInvitePage *)object;

  g_clear_object (&self->chat);

  G_OBJECT_CLASS (chatty_invite_page_parent_class)->dispose (object);
}

static void
chatty_invite_page_class_init (ChattyInvitePageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = chatty_invite_page_dispose;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/Chatty/"
                                               "ui/chatty-invite-page.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyInvitePage, invite_title);
  gtk_widget_class_bind_template_child (widget_class, ChattyInvitePage, chat_avatar);
  gtk_widget_class_bind_template_child (widget_class, ChattyInvitePage, invite_subtitle);

  gtk_widget_class_bind_template_child (widget_class, ChattyInvitePage, accept_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyInvitePage, accept_spinner);
  gtk_widget_class_bind_template_child (widget_class, ChattyInvitePage, reject_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyInvitePage, reject_spinner);

  gtk_widget_class_bind_template_callback (widget_class, invite_accept_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, invite_reject_clicked_cb);
}

static void
chatty_invite_page_init (ChattyInvitePage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

void
chatty_invite_page_set_chat (ChattyInvitePage *self,
                             ChattyChat       *chat)
{
  g_return_if_fail (CHATTY_IS_INVITE_PAGE (self));
  g_return_if_fail (!chat || CHATTY_IS_CHAT (chat));

  if (self->chat) {
    g_clear_signal_handler (&self->account_handler, self->account);
    g_clear_signal_handler (&self->name_handler, self->chat);
    chatty_avatar_set_item (CHATTY_AVATAR (self->chat_avatar), NULL);
  }

  g_set_object (&self->chat, chat);

  if (chat) {
    if (chatty_chat_get_chat_state (chat) != CHATTY_CHAT_INVITED)
      return;

    self->account = chatty_chat_get_account (chat);
    chatty_avatar_set_item (CHATTY_AVATAR (self->chat_avatar), CHATTY_ITEM (chat));

    self->account_handler = g_signal_connect_object (self->account, "notify::status",
                                                     G_CALLBACK (invite_account_status_changed_cb),
                                                     self, G_CONNECT_SWAPPED);
    self->name_handler = g_signal_connect_object (chat, "notify::name",
                                                  G_CALLBACK (chat_invite_name_changed_cb),
                                                  self, G_CONNECT_SWAPPED);
    invite_account_status_changed_cb (self);
    chat_invite_name_changed_cb (self);
  }
}
