/* chatty-ma-chat-info.c
 *
 * Copyright 2021 Purism SPC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-ma-chat-info"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>

#include "chatty-avatar.h"
#include "chatty-list-row.h"
#include "chatty-fp-row.h"
#include "chatty-ma-chat.h"
#include "chatty-ma-chat-info.h"
#include "chatty-log.h"

struct _ChattyMaChatInfo
{
  ChattyChatInfo parent_instance;

  ChattyChat    *chat;

  GtkWidget     *avatar;

  GtkWidget     *name_row;
  GtkWidget     *matrix_id_row;
  GtkWidget     *topic_row;
  GtkWidget     *encryption_spinner;
  GtkWidget     *encryption_row;
};

G_DEFINE_TYPE (ChattyMaChatInfo, chatty_ma_chat_info, CHATTY_TYPE_CHAT_INFO)

static void     ma_chat_info_encryption_row_changed_cb    (ChattyMaChatInfo *self);

static void
ma_chat_encrypt_changed_cb (ChattyMaChatInfo *self)
{
  g_assert (CHATTY_IS_MA_CHAT_INFO (self));

  g_signal_handlers_block_by_func (self->encryption_row,
                                   ma_chat_info_encryption_row_changed_cb,
                                   self);
  adw_switch_row_set_active (ADW_SWITCH_ROW (self->encryption_row),
                             chatty_chat_get_encryption (self->chat) == CHATTY_ENCRYPTION_ENABLED);
  g_signal_handlers_unblock_by_func (self->encryption_row,
                                     ma_chat_info_encryption_row_changed_cb,
                                     self);
}

static void
ma_chat_info_set_encryption_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  g_autoptr(ChattyMaChatInfo) self = user_data;

  g_assert (CHATTY_IS_MA_CHAT_INFO (self));

  if (gtk_widget_in_destruction (GTK_WIDGET (self)))
    return;

  gtk_widget_set_visible (GTK_WIDGET (self->encryption_spinner), false);
  gtk_spinner_stop (GTK_SPINNER (self->encryption_spinner));
  ma_chat_encrypt_changed_cb (self);
}

static void
ma_chat_info_encryption_row_changed_cb (ChattyMaChatInfo *self)
{
  g_assert (CHATTY_IS_MA_CHAT_INFO (self));
  g_assert (self->chat);

  if (!adw_switch_row_get_active (ADW_SWITCH_ROW (self->encryption_row)))
    return;

  gtk_widget_set_visible (GTK_WIDGET (self->encryption_spinner), true);
  gtk_spinner_start (GTK_SPINNER (self->encryption_spinner));
  chatty_chat_set_encryption_async (self->chat, TRUE,
                                    ma_chat_info_set_encryption_cb,
                                    g_object_ref (self));
}

static void
chatty_ma_chat_info_set_item (ChattyChatInfo *info,
                              ChattyChat     *chat)
{
  ChattyMaChatInfo *self = (ChattyMaChatInfo *)info;

  g_assert (CHATTY_IS_MA_CHAT_INFO (self));
  g_assert (!chat || CHATTY_IS_MA_CHAT (chat));

  if (self->chat && chat != self->chat) {
    chatty_avatar_set_item (CHATTY_AVATAR (self->avatar), (ChattyItem *) chat);
    g_signal_handlers_disconnect_by_func (self->chat,
                                          ma_chat_encrypt_changed_cb,
                                          self);
  }

  if (!g_set_object (&self->chat, chat) || !chat)
    return;

  chatty_avatar_set_item (CHATTY_AVATAR (self->avatar), CHATTY_ITEM (chat));
  adw_action_row_set_subtitle (ADW_ACTION_ROW (self->matrix_id_row),
                               chatty_chat_get_chat_name (self->chat));
  adw_action_row_set_subtitle (ADW_ACTION_ROW (self->name_row),
                               chatty_item_get_name (CHATTY_ITEM (self->chat)));
  adw_action_row_set_subtitle (ADW_ACTION_ROW (self->topic_row),
                               chatty_ma_chat_get_topic (CHATTY_MA_CHAT (self->chat)));

  g_signal_connect_swapped (self->chat, "notify::encrypt",
                            G_CALLBACK (ma_chat_encrypt_changed_cb),
                            self);
  gtk_widget_set_sensitive (self->encryption_row,
                            chatty_ma_chat_can_set_encryption (CHATTY_MA_CHAT (self->chat)));
  ma_chat_encrypt_changed_cb (self);
}

static void
chatty_ma_chat_info_finalize (GObject *object)
{
  ChattyMaChatInfo *self = (ChattyMaChatInfo *)object;

  g_clear_object (&self->chat);

  G_OBJECT_CLASS (chatty_ma_chat_info_parent_class)->finalize (object);
}

static void
chatty_ma_chat_info_class_init (ChattyMaChatInfoClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  ChattyChatInfoClass *info_class = CHATTY_CHAT_INFO_CLASS (klass);

  object_class->finalize = chatty_ma_chat_info_finalize;

  info_class->set_item = chatty_ma_chat_info_set_item;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/Chatty/"
                                               "ui/chatty-ma-chat-info.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyMaChatInfo, avatar);

  gtk_widget_class_bind_template_child (widget_class, ChattyMaChatInfo, name_row);
  gtk_widget_class_bind_template_child (widget_class, ChattyMaChatInfo, matrix_id_row);
  gtk_widget_class_bind_template_child (widget_class, ChattyMaChatInfo, topic_row);
  gtk_widget_class_bind_template_child (widget_class, ChattyMaChatInfo, encryption_spinner);
  gtk_widget_class_bind_template_child (widget_class, ChattyMaChatInfo, encryption_row);

  gtk_widget_class_bind_template_callback (widget_class, ma_chat_info_encryption_row_changed_cb);
}

static void
chatty_ma_chat_info_init (ChattyMaChatInfo *self)
{
  GtkWidget *clamp;

  gtk_widget_init_template (GTK_WIDGET (self));

  clamp = gtk_widget_get_ancestor (self->avatar, ADW_TYPE_CLAMP);

  if (clamp) {
    adw_clamp_set_maximum_size (ADW_CLAMP (clamp), 360);
    adw_clamp_set_tightening_threshold (ADW_CLAMP (clamp), 320);
  }
}

GtkWidget *
chatty_ma_chat_info_new (void)
{
  return g_object_new (CHATTY_TYPE_MA_CHAT_INFO, NULL);
}
