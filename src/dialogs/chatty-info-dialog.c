/* chatty-info-dialog.c
 *
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#define G_LOG_DOMAIN "chatty-info-dialog"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>

#ifdef PURPLE_ENABLED
# include "chatty-pp-chat-info.h"
#endif

#include "chatty-mm-chat.h"
#include "chatty-ma-chat.h"
#include "chatty-utils.h"
#include "chatty-ma-chat-info.h"
#include "chatty-mm-chat-info.h"
#include "chatty-info-dialog.h"

struct _ChattyInfoDialog
{
  AdwWindow       parent_instance;

  GtkWidget      *main_stack;
  GtkWidget      *chat_type_stack;
  GtkWidget      *header_bar;
  GtkWidget      *ma_chat_info;
  GtkWidget      *pp_chat_info;
  GtkWidget      *mm_chat_info;
  GtkWidget      *invite_page;

  GtkWidget      *cancel_button;
  GtkWidget      *apply_button;
  GtkWidget      *new_invite_button;
  GtkWidget      *invite_button;

  GtkWidget      *contact_id_entry;
  GtkWidget      *message_entry;

  ChattyChat     *chat;
};

G_DEFINE_TYPE (ChattyInfoDialog, chatty_info_dialog, ADW_TYPE_WINDOW)

static void
info_dialog_new_invite_clicked_cb (ChattyInfoDialog *self)
{
  g_assert (CHATTY_IS_INFO_DIALOG (self));

  gtk_widget_set_visible (self->new_invite_button, FALSE);
  gtk_widget_set_visible (self->invite_button, TRUE);
  gtk_stack_set_visible_child (GTK_STACK (self->main_stack),
                               self->invite_page);
  gtk_widget_grab_focus (self->contact_id_entry);
}

static void
info_dialog_cancel_clicked_cb (ChattyInfoDialog *self)
{
  g_assert (CHATTY_IS_INFO_DIALOG (self));

  gtk_widget_set_visible (self->invite_button, FALSE);
  gtk_widget_set_visible (self->new_invite_button, TRUE);
  gtk_stack_set_visible_child (GTK_STACK (self->main_stack),
                               self->chat_type_stack);
}

static void
cancel_button_clicked_cb (ChattyInfoDialog *self)
{
  g_assert (CHATTY_IS_INFO_DIALOG (self));
  gtk_widget_set_visible (GTK_WIDGET (self), FALSE);

  chatty_mm_chat_info_cancel_changes (CHATTY_MM_CHAT_INFO (self->mm_chat_info),
                                      self->chat);
}

static void
apply_button_clicked_cb (ChattyInfoDialog *self)
{

  g_assert (CHATTY_IS_INFO_DIALOG (self));
  gtk_widget_set_visible (GTK_WIDGET (self), FALSE);

  chatty_mm_chat_info_apply_changes (CHATTY_MM_CHAT_INFO (self->mm_chat_info),
                                     self->chat);
}

static void
chat_invite_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  g_autoptr(ChattyInfoDialog) self = user_data;

  g_assert (CHATTY_IS_INFO_DIALOG (self));
}

static void
info_dialog_invite_clicked_cb (ChattyInfoDialog *self)
{
  const char *name, *invite_message;

  g_assert (CHATTY_IS_INFO_DIALOG (self));
  g_return_if_fail (self->chat);

  name = gtk_editable_get_text (GTK_EDITABLE (self->contact_id_entry));
  invite_message = gtk_editable_get_text (GTK_EDITABLE (self->message_entry));

  if (name && *name)
    chatty_chat_invite_async (CHATTY_CHAT (self->chat), name, invite_message, NULL,
                              chat_invite_cb, g_object_ref (self));

  gtk_editable_set_text (GTK_EDITABLE (self->contact_id_entry), "");
  gtk_editable_set_text (GTK_EDITABLE (self->message_entry), "");
  info_dialog_cancel_clicked_cb (self);
}

static void
info_dialog_contact_id_changed_cb (ChattyInfoDialog *self,
                                   GtkEntry         *entry)
{
  const char *username;
  ChattyProtocol protocol, item_protocol;

  g_assert (CHATTY_IS_INFO_DIALOG (self));
  g_assert (GTK_IS_ENTRY (entry));
  g_return_if_fail (self->chat);

  item_protocol = chatty_item_get_protocols (CHATTY_ITEM (self->chat));
  username = gtk_editable_get_text (GTK_EDITABLE (entry));

  protocol = chatty_utils_username_is_valid (username, item_protocol);
  gtk_widget_set_sensitive (self->invite_button, protocol == item_protocol);
}

static void
chatty_info_dialog_finalize (GObject *object)
{
  ChattyInfoDialog *self = (ChattyInfoDialog *)object;

  g_clear_object (&self->chat);

  G_OBJECT_CLASS (chatty_info_dialog_parent_class)->finalize (object);
}

static void
chatty_info_dialog_class_init (ChattyInfoDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = chatty_info_dialog_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/Chatty/"
                                               "ui/chatty-info-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyInfoDialog, main_stack);
  gtk_widget_class_bind_template_child (widget_class, ChattyInfoDialog, chat_type_stack);
  gtk_widget_class_bind_template_child (widget_class, ChattyInfoDialog, header_bar);
  gtk_widget_class_bind_template_child (widget_class, ChattyInfoDialog, ma_chat_info);
  gtk_widget_class_bind_template_child (widget_class, ChattyInfoDialog, mm_chat_info);
  gtk_widget_class_bind_template_child (widget_class, ChattyInfoDialog, invite_page);

  gtk_widget_class_bind_template_child (widget_class, ChattyInfoDialog, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyInfoDialog, apply_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyInfoDialog, new_invite_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyInfoDialog, invite_button);

  gtk_widget_class_bind_template_child (widget_class, ChattyInfoDialog, contact_id_entry);
  gtk_widget_class_bind_template_child (widget_class, ChattyInfoDialog, message_entry);

  gtk_widget_class_bind_template_callback (widget_class, info_dialog_new_invite_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, info_dialog_cancel_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, info_dialog_invite_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, info_dialog_contact_id_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, cancel_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, apply_button_clicked_cb);

  g_type_ensure (CHATTY_TYPE_MA_CHAT_INFO);
  g_type_ensure (CHATTY_TYPE_MM_CHAT_INFO);
}

static void
chatty_info_dialog_init (ChattyInfoDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET(self));

#ifdef PURPLE_ENABLED
  self->pp_chat_info = chatty_pp_chat_info_new ();
  gtk_stack_add_child (GTK_STACK (self->chat_type_stack), self->pp_chat_info);
#endif
}

GtkWidget *
chatty_info_dialog_new (GtkWindow *parent_window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (parent_window), NULL);

  return g_object_new (CHATTY_TYPE_INFO_DIALOG,
                       "transient-for", parent_window,
                       NULL);
}

void
chatty_info_dialog_set_chat (ChattyInfoDialog *self,
                             ChattyChat       *chat)
{
  g_return_if_fail (CHATTY_IS_INFO_DIALOG (self));
  g_return_if_fail (!chat || CHATTY_IS_CHAT (chat));

  gtk_editable_set_text (GTK_EDITABLE (self->contact_id_entry), "");
  gtk_editable_set_text (GTK_EDITABLE (self->message_entry), "");

  if (!g_set_object (&self->chat, chat))
    return;


  gtk_widget_set_visible (self->cancel_button, FALSE);
  gtk_widget_set_visible (self->apply_button, FALSE);
  adw_header_bar_set_show_end_title_buttons (ADW_HEADER_BAR (self->header_bar), TRUE);

  if (CHATTY_IS_MA_CHAT (chat)) {
    chatty_chat_info_set_item (CHATTY_CHAT_INFO (self->ma_chat_info), chat);
    if (self->pp_chat_info)
      chatty_chat_info_set_item (CHATTY_CHAT_INFO (self->mm_chat_info), NULL);
    if (self->pp_chat_info)
      chatty_chat_info_set_item (CHATTY_CHAT_INFO (self->pp_chat_info), NULL);
    gtk_stack_set_visible_child (GTK_STACK (self->chat_type_stack), self->ma_chat_info);
  } else if (CHATTY_IS_MM_CHAT (chat)) {
    chatty_chat_info_set_item (CHATTY_CHAT_INFO (self->ma_chat_info), NULL);
    if (self->pp_chat_info)
      chatty_chat_info_set_item (CHATTY_CHAT_INFO (self->pp_chat_info), NULL);
    chatty_chat_info_set_item (CHATTY_CHAT_INFO (self->mm_chat_info), chat);
    if (!chatty_chat_is_im (chat)) {
      gtk_widget_set_visible (self->cancel_button, TRUE);
      gtk_widget_set_visible (self->apply_button, TRUE);
      adw_header_bar_set_show_end_title_buttons (ADW_HEADER_BAR (self->header_bar), FALSE);
    }
    gtk_stack_set_visible_child (GTK_STACK (self->chat_type_stack), self->mm_chat_info);
  } else {
    chatty_chat_info_set_item (CHATTY_CHAT_INFO (self->ma_chat_info), NULL);
    if (self->pp_chat_info)
      chatty_chat_info_set_item (CHATTY_CHAT_INFO (self->pp_chat_info), chat);
    chatty_chat_info_set_item (CHATTY_CHAT_INFO (self->mm_chat_info), NULL);
    gtk_stack_set_visible_child (GTK_STACK (self->chat_type_stack), self->pp_chat_info);
  }

  if (chatty_item_get_protocols (CHATTY_ITEM (chat)) == CHATTY_PROTOCOL_XMPP &&
      !chatty_chat_is_im (self->chat))
    gtk_widget_set_visible (self->new_invite_button, TRUE);
  else
    gtk_widget_set_visible (self->new_invite_button, FALSE);
}
