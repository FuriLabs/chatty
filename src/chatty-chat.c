/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-chat.c
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-chat"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define _GNU_SOURCE
#include <string.h>

#include "chatty-history.h"
#include "chatty-chat.h"

/**
 * SECTION: chatty-chat
 * @title: ChattyChat
 * @short_description: The base class for Chats
 */

#define LAZY_LOAD_MSGS_LIMIT 20

typedef struct
{
  char *chat_name;
  char *user_name;

  gpointer account;
  gpointer history;

  gboolean is_im;
} ChattyChatPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (ChattyChat, chatty_chat, CHATTY_TYPE_ITEM)

enum {
  PROP_0,
  PROP_ENCRYPT,
  PROP_BUDDY_TYPING,
  PROP_LOADING_HISTORY,
  N_PROPS
};

enum {
  CHANGED,
  N_SIGNALS
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];


static void
chatty_chat_real_set_data (ChattyChat *self,
                           gpointer    account,
                           gpointer    history)
{
  ChattyChatPrivate *priv = chatty_chat_get_instance_private (self);

  g_assert (CHATTY_IS_CHAT (self));

  g_set_object (&priv->account, account);
  g_set_object (&priv->history, history);
}

static gboolean
chatty_chat_real_is_im (ChattyChat *self)
{
  ChattyChatPrivate *priv = chatty_chat_get_instance_private (self);

  g_assert (CHATTY_IS_CHAT (self));

  return priv->is_im;
}

static const char *
chatty_chat_real_get_chat_name (ChattyChat *self)
{
  ChattyChatPrivate *priv = chatty_chat_get_instance_private (self);

  g_assert (CHATTY_IS_CHAT (self));

  if (priv->chat_name)
    return priv->chat_name;

  return "";
}

static const char *
chatty_chat_real_get_username (ChattyChat *self)
{
  ChattyChatPrivate *priv = chatty_chat_get_instance_private (self);

  g_assert (CHATTY_IS_CHAT (self));

  if (priv->user_name)
    return priv->user_name;

  return "";
}

static ChattyAccount *
chatty_chat_real_get_account (ChattyChat *self)
{
  ChattyChatPrivate *priv = chatty_chat_get_instance_private (self);

  g_assert (CHATTY_IS_CHAT (self));

  return priv->account;
}

static void
chatty_chat_real_load_past_messages (ChattyChat *self,
                                     int         count)
{
  /* Do nothing */
}

static gboolean
chatty_chat_real_is_loading_history (ChattyChat *self)
{
  return FALSE;
}

static GListModel *
chatty_chat_real_get_messages (ChattyChat *self)
{
  return NULL;
}

static GListModel *
chatty_chat_real_get_users (ChattyChat *self)
{
  return NULL;
}

static const char *
chatty_chat_real_get_last_message (ChattyChat *self)
{
  return "";
}

static guint
chatty_chat_real_get_unread_count (ChattyChat *self)
{
  return 0;
}

static void
chatty_chat_real_set_unread_count (ChattyChat *self,
                                   guint       unread_count)
{
  /* Do nothing */
}

static time_t
chatty_chat_real_get_last_msg_time (ChattyChat *self)
{
  return 0;
}

static void
chatty_chat_real_send_message_async (ChattyChat          *self,
                                     ChattyMessage       *message,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_assert (CHATTY_IS_CHAT (self));
  g_assert (CHATTY_IS_MESSAGE (message));

  g_task_report_new_error (self, callback, user_data,
                           chatty_chat_real_send_message_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Sending messages not supported");
}

static gboolean
chatty_chat_real_send_message_finish (ChattyChat    *self,
                                      GAsyncResult  *result,
                                      GError       **error)
{
  g_assert (CHATTY_IS_CHAT (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static ChattyEncryption
chatty_chat_real_get_encryption (ChattyChat *self)
{
  return CHATTY_ENCRYPTION_UNKNOWN;
}

static gboolean
chatty_chat_real_get_buddy_typing (ChattyChat *self)
{
  return FALSE;
}

static void
chatty_chat_real_set_typing (ChattyChat *self,
                             gboolean    is_typing)
{
  /* Do nothing */
}

static const char *
chatty_chat_real_get_name (ChattyItem *item)
{
  ChattyChat *self = (ChattyChat *)item;
  ChattyChatPrivate *priv = chatty_chat_get_instance_private (self);

  g_assert (CHATTY_IS_CHAT (self));

  if (priv->chat_name)
    return priv->chat_name;

  return "";
}

static void
chatty_chat_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  ChattyChat *self = (ChattyChat *)object;

  switch (prop_id)
    {
    case PROP_ENCRYPT:
      g_value_set_boolean (value, chatty_chat_get_encryption (self) == CHATTY_ENCRYPTION_ENABLED);
      break;

    case PROP_BUDDY_TYPING:
      g_value_set_boolean (value, chatty_chat_get_buddy_typing (self));
      break;

    case PROP_LOADING_HISTORY:
      g_value_set_boolean (value, chatty_chat_is_loading_history (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
chatty_chat_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  ChattyChat *self = (ChattyChat *)object;

  switch (prop_id)
    {
    case PROP_ENCRYPT:
      chatty_chat_set_encryption (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
chatty_chat_finalize (GObject *object)
{
  ChattyChat *self = (ChattyChat *)object;
  ChattyChatPrivate *priv = chatty_chat_get_instance_private (self);

  g_free (priv->chat_name);
  g_free (priv->user_name);

  g_clear_object (&priv->account);
  g_clear_object (&priv->history);

  G_OBJECT_CLASS (chatty_chat_parent_class)->finalize (object);
}

static void
chatty_chat_class_init (ChattyChatClass *klass)
{
  GObjectClass *object_class  = G_OBJECT_CLASS (klass);
  ChattyItemClass *item_class = CHATTY_ITEM_CLASS (klass);

  object_class->get_property = chatty_chat_get_property;
  object_class->set_property = chatty_chat_set_property;
  object_class->finalize = chatty_chat_finalize;

  item_class->get_name = chatty_chat_real_get_name;

  klass->set_data = chatty_chat_real_set_data;
  klass->is_im = chatty_chat_real_is_im;
  klass->get_chat_name = chatty_chat_real_get_chat_name;
  klass->get_username = chatty_chat_real_get_username;
  klass->get_account = chatty_chat_real_get_account;
  klass->load_past_messages = chatty_chat_real_load_past_messages;
  klass->is_loading_history = chatty_chat_real_is_loading_history;
  klass->get_messages = chatty_chat_real_get_messages;
  klass->get_users = chatty_chat_real_get_users;
  klass->get_last_message = chatty_chat_real_get_last_message;
  klass->get_unread_count = chatty_chat_real_get_unread_count;
  klass->set_unread_count = chatty_chat_real_set_unread_count;
  klass->get_last_msg_time = chatty_chat_real_get_last_msg_time;
  klass->send_message_async = chatty_chat_real_send_message_async;
  klass->send_message_finish = chatty_chat_real_send_message_finish;
  klass->get_encryption = chatty_chat_real_get_encryption;
  klass->get_buddy_typing = chatty_chat_real_get_buddy_typing;
  klass->set_typing = chatty_chat_real_set_typing;

  properties[PROP_ENCRYPT] =
    g_param_spec_boolean ("encrypt",
                          "Encrypt",
                          "Whether the chat is encrypted or not",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_BUDDY_TYPING] =
    g_param_spec_boolean ("buddy-typing",
                          "Buddy typing",
                          "Whether the buddy in chat is typing",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_LOADING_HISTORY] =
    g_param_spec_boolean ("loading-history",
                          "Loading history",
                          "Whether the chat history is being loading",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * ChattyChat::changed:
   * @self: a #ChattyChat
   *
   * Emitted when chat changes
   */
  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
chatty_chat_init (ChattyChat *self)
{
}

ChattyChat *
chatty_chat_new (const char *account_username,
                 const char *chat_name,
                 gboolean    is_im)
{
  ChattyChat *self;
  ChattyChatPrivate *priv;

  self = g_object_new (CHATTY_TYPE_CHAT, NULL);
  priv = chatty_chat_get_instance_private (self);
  priv->user_name = g_strdup (account_username);
  priv->chat_name = g_strdup (chat_name);
  priv->is_im = !!is_im;

  return self;
}

void
chatty_chat_set_data (ChattyChat *self,
                      gpointer    account,
                      gpointer    history)
{
  g_return_if_fail (CHATTY_IS_CHAT (self));
  g_return_if_fail (!account || CHATTY_IS_ACCOUNT (account));
  g_return_if_fail (CHATTY_IS_HISTORY (history));

  CHATTY_CHAT_GET_CLASS (self)->set_data (self, account, history);
}

/**
 * chatty_chat_is_im:
 * @self: A #ChattyChat
 *
 * Get if @self is an instant message or not.
 *
 * Returns: %TRUE if @self is an instant message.
 * %FALSE if @self is a multiuser chat.
 */
gboolean
chatty_chat_is_im (ChattyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_CHAT (self), FALSE);

  return CHATTY_CHAT_GET_CLASS (self)->is_im (self);
}

/**
 * chatty_chat_get_name:
 * @self: a #ChattyChat
 *
 * Get the name of Chat.  In purple/pidgin it’s
 * termed as ‘alias.’ If real name is empty,
 * it may fallbacks to the user id.  The user id
 * may have the resource stripped (eg: For the
 * user id xmpp@example.com/someclient.6 you shall
 * get xmpp@example.com)
 *
 * Returns: (transfer none): the name of Chat.
 */
const char *
chatty_chat_get_chat_name (ChattyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_CHAT (self), "");

  return CHATTY_CHAT_GET_CLASS (self)->get_chat_name (self);
}

const char *
chatty_chat_get_username (ChattyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_CHAT (self), "");

  return CHATTY_CHAT_GET_CLASS (self)->get_username (self);
}

ChattyAccount *
chatty_chat_get_account (ChattyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_CHAT (self), NULL);

  return CHATTY_CHAT_GET_CLASS (self)->get_account (self);
}

GListModel *
chatty_chat_get_messages (ChattyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_CHAT (self), NULL);

  return CHATTY_CHAT_GET_CLASS (self)->get_messages (self);
}

/**
 * chatty_chat_load_past_messages:
 * @self: A #ChattyChat
 * @count: number of messages to load
 *
 * Load @count number of past messages.
 * if @count is 0 or less, the default
 * number of messages shall be loaded
 */
void
chatty_chat_load_past_messages (ChattyChat *self,
                                int         count)
{
  g_return_if_fail (CHATTY_IS_CHAT (self));

  if (count <= 0)
    count = LAZY_LOAD_MSGS_LIMIT;

  CHATTY_CHAT_GET_CLASS (self)->load_past_messages (self, count);
}

gboolean
chatty_chat_is_loading_history (ChattyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_CHAT (self), FALSE);

  return CHATTY_CHAT_GET_CLASS (self)->is_loading_history (self);
}

GListModel *chatty_chat_get_users (ChattyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_CHAT (self), NULL);

  return CHATTY_CHAT_GET_CLASS (self)->get_users (self);
}

const char *
chatty_chat_get_last_message (ChattyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_CHAT (self), "");

  return CHATTY_CHAT_GET_CLASS (self)->get_last_message (self);
}

guint
chatty_chat_get_unread_count (ChattyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_CHAT (self), 0);

  return CHATTY_CHAT_GET_CLASS (self)->get_unread_count (self);
}

void
chatty_chat_set_unread_count (ChattyChat *self,
                              guint       unread_count)
{
  g_return_if_fail (CHATTY_IS_CHAT (self));

  CHATTY_CHAT_GET_CLASS (self)->set_unread_count (self, unread_count);
}

time_t
chatty_chat_get_last_msg_time (ChattyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_CHAT (self), 0);

  return CHATTY_CHAT_GET_CLASS (self)->get_last_msg_time (self);
}

void
chatty_chat_send_message_async (ChattyChat          *self,
                                ChattyMessage       *message,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_return_if_fail (CHATTY_IS_CHAT (self));
  g_return_if_fail (CHATTY_IS_MESSAGE (message));

  CHATTY_CHAT_GET_CLASS (self)->send_message_async (self, message, callback, user_data);
}

gboolean
chatty_chat_send_message_finish (ChattyChat    *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  g_return_val_if_fail (CHATTY_IS_CHAT (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return CHATTY_CHAT_GET_CLASS (self)->send_message_finish (self, result, error);
}

ChattyEncryption
chatty_chat_get_encryption (ChattyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_CHAT (self), CHATTY_ENCRYPTION_UNKNOWN);

  return CHATTY_CHAT_GET_CLASS (self)->get_encryption (self);
}

void
chatty_chat_set_encryption (ChattyChat *self,
                            gboolean    enable)
{
  g_return_if_fail (CHATTY_IS_CHAT (self));

  CHATTY_CHAT_GET_CLASS (self)->set_encryption (self, !!enable);
}

/**
 * chatty_chat_set_typing:
 * @self: A #ChattyChat
 * @is_typing: Whether self is typing or not
 *
 * Set whether the username associated with @self
 * is typing or not.  This change is propagated
 * to the real chat (ie, this information is send
 * to the server), and not just locally set.
 */
void
chatty_chat_set_typing (ChattyChat *self,
                        gboolean    is_typing)
{
  g_return_if_fail (CHATTY_IS_CHAT (self));

  CHATTY_CHAT_GET_CLASS (self)->set_typing (self, !!is_typing);
}

gboolean
chatty_chat_get_buddy_typing (ChattyChat *self)
{
  g_return_val_if_fail (CHATTY_IS_CHAT (self), FALSE);

  return CHATTY_CHAT_GET_CLASS (self)->get_buddy_typing (self);
}
