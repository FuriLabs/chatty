/* chatty-ma-key-chat.c
 *
 * Copyright 2022 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-ma-key-chat"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>
#define CMATRIX_USE_EXPERIMENTAL_API
#include <cmatrix.h>

#include "chatty-ma-buddy.h"
#include "chatty-ma-account.h"
#include "chatty-ma-key-chat.h"

struct _ChattyMaKeyChat
{
  ChattyChat       parent_instance;

  ChattyMaAccount *ma_account;
  CmClient        *cm_client;
  ChattyMaBuddy   *buddy;
  GListStore      *empty;

  CmVerificationEvent *key_event;
  ChattyChatState  chat_state;
};

G_DEFINE_TYPE (ChattyMaKeyChat, chatty_ma_key_chat, CHATTY_TYPE_CHAT)

static ChattyChatState
chatty_ma_key_chat_get_chat_state (ChattyChat *chat)
{
  ChattyMaKeyChat *self = (ChattyMaKeyChat *)chat;

  g_assert (CHATTY_IS_MA_KEY_CHAT (self));

  return self->chat_state;
}

static ChattyAccount *
chatty_ma_key_chat_get_account (ChattyChat *chat)
{
  ChattyMaKeyChat *self = (ChattyMaKeyChat *)chat;

  g_assert (CHATTY_IS_MA_KEY_CHAT (self));

  return CHATTY_ACCOUNT (self->ma_account);
}

static GListModel *
chatty_ma_key_chat_get_messages (ChattyChat *chat)
{
  ChattyMaKeyChat *self = CHATTY_MA_KEY_CHAT (chat);

  g_assert (CHATTY_IS_MA_KEY_CHAT (self));

  return G_LIST_MODEL (self->empty);
}

static const char *
chatty_ma_key_chat_get_name (ChattyItem *item)
{
  return _("Key Verification");
}

static ChattyProtocol
chatty_ma_key_chat_get_protocols (ChattyItem *item)
{
  return CHATTY_PROTOCOL_MATRIX;
}

static void
chatty_ma_key_chat_finalize (GObject *object)
{
  ChattyMaKeyChat *self = (ChattyMaKeyChat *)object;

  g_clear_object (&self->key_event);
  g_clear_object (&self->cm_client);
  g_clear_object (&self->buddy);
  g_clear_object (&self->empty);

  G_OBJECT_CLASS (chatty_ma_key_chat_parent_class)->finalize (object);
}

static void
chatty_ma_key_chat_class_init (ChattyMaKeyChatClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ChattyItemClass *item_class = CHATTY_ITEM_CLASS (klass);
  ChattyChatClass *chat_class = CHATTY_CHAT_CLASS (klass);

  object_class->finalize = chatty_ma_key_chat_finalize;

  item_class->get_name = chatty_ma_key_chat_get_name;
  item_class->get_protocols = chatty_ma_key_chat_get_protocols;

  chat_class->get_chat_state = chatty_ma_key_chat_get_chat_state;
  chat_class->get_account  = chatty_ma_key_chat_get_account;
  chat_class->get_messages = chatty_ma_key_chat_get_messages;
}

static void
chatty_ma_key_chat_init (ChattyMaKeyChat *self)
{
  self->chat_state = CHATTY_CHAT_VERIFICATION;

  self->empty = g_list_store_new (CHATTY_TYPE_MESSAGE);
}

static void
key_event_updated_cb (ChattyMaKeyChat *self)
{
  g_assert (CHATTY_IS_MA_KEY_CHAT (self));

  g_signal_emit_by_name (self, "changed", 0);
}

ChattyMaKeyChat *
chatty_ma_key_chat_new (gpointer             ma_account,
                        CmVerificationEvent *key_event)
{
  ChattyMaKeyChat *self;
  CmEventType event_type;

  g_return_val_if_fail (CHATTY_MA_ACCOUNT (ma_account), NULL);
  g_return_val_if_fail (CM_IS_EVENT (key_event), NULL);

  event_type = cm_event_get_m_type (CM_EVENT (key_event));
  g_return_val_if_fail (event_type == CM_M_KEY_VERIFICATION_START ||
                        event_type == CM_M_KEY_VERIFICATION_REQUEST, NULL);

  self = g_object_new (CHATTY_TYPE_MA_KEY_CHAT, NULL);
  g_set_weak_pointer (&self->ma_account, ma_account);
  self->cm_client = g_object_ref (chatty_ma_account_get_cm_client (ma_account));
  self->key_event = g_object_ref (key_event);
  self->buddy = chatty_ma_buddy_new_with_user (cm_event_get_sender (CM_EVENT (key_event)));

  g_signal_connect_object (self->key_event, "updated",
                           G_CALLBACK (key_event_updated_cb),
                           self,
                           G_CONNECT_SWAPPED);

  return self;
}

CmEvent *
chatty_ma_key_chat_get_event (ChattyMaKeyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_MA_KEY_CHAT (self), NULL);

  return CM_EVENT (self->key_event);
}

ChattyItem *
chatty_ma_key_chat_get_sender (ChattyMaKeyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_MA_KEY_CHAT (self), NULL);

  return CHATTY_ITEM (self->buddy);
}

GPtrArray *
chatty_ma_key_get_emoji (ChattyMaKeyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_MA_KEY_CHAT (self), NULL);

  return g_object_get_data (G_OBJECT (self->key_event), "emoji");
}

/* decimal is an array of 3 */
guint16 *
chatty_ma_key_get_decimal (ChattyMaKeyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_MA_KEY_CHAT (self), NULL);

  return g_object_get_data (G_OBJECT (self->key_event), "decimal");
}

static void
ma_key_cancel_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  ChattyMaKeyChat *self;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;
  gboolean success;

  self = g_task_get_source_object (task);
  success = cm_verification_event_cancel_finish (self->key_event, result, &error);

  if (success)
    self->chat_state = CHATTY_CHAT_LEFT;

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, success);
}

void
chatty_ma_key_cancel_async (ChattyMaKeyChat     *self,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  GTask *task;

  g_return_if_fail (CHATTY_IS_MA_KEY_CHAT (self));

  task = g_task_new (self, NULL, callback, user_data);

  cm_verification_event_cancel_async (self->key_event, NULL,
                                      ma_key_cancel_cb, task);
}

gboolean
chatty_ma_key_cancel_finish (ChattyMaKeyChat *self,
                             GAsyncResult    *result,
                             GError         **error)
{
  g_return_val_if_fail (CHATTY_IS_MA_KEY_CHAT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ma_key_accept_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;
  gboolean success;

  success = cm_verification_event_continue_finish (CM_VERIFICATION_EVENT (object), result, &error);

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, success);
}

void
chatty_ma_key_accept_async (ChattyMaKeyChat     *self,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  GTask *task;

  g_return_if_fail (CHATTY_IS_MA_KEY_CHAT (self));

  task = g_task_new (self, NULL, callback, user_data);

  cm_verification_event_continue_async (self->key_event, NULL,
                                        ma_key_accept_cb, task);
}

gboolean
chatty_ma_key_accept_finish (ChattyMaKeyChat  *self,
                             GAsyncResult     *result,
                             GError          **error)
{
  g_return_val_if_fail (CHATTY_IS_MA_KEY_CHAT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ma_key_match_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;
  gboolean success;

  success = cm_verification_event_match_finish (CM_VERIFICATION_EVENT (object), result, &error);

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, success);
}

void
chatty_ma_key_match_async (ChattyMaKeyChat     *self,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  GTask *task;

  g_return_if_fail (CHATTY_IS_MA_KEY_CHAT (self));

  task = g_task_new (self, NULL, callback, user_data);

  cm_verification_event_match_async (self->key_event, NULL,
                                     ma_key_match_cb, task);
}

gboolean
chatty_ma_key_match_finish (ChattyMaKeyChat  *self,
                            GAsyncResult     *result,
                            GError          **error)
{
  g_return_val_if_fail (CHATTY_IS_MA_KEY_CHAT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
