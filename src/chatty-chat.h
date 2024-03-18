/* chatty-chat.h
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>

#include "chatty-item.h"
#include "chatty-account.h"
#include "chatty-message.h"
#include "chatty-enums.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_CHAT (chatty_chat_get_type ())

G_DECLARE_DERIVABLE_TYPE (ChattyChat, chatty_chat, CHATTY, CHAT, ChattyItem)

struct _ChattyChatClass
{
  ChattyItemClass  parent_class;

  void              (*set_data)           (ChattyChat *self,
                                           gpointer    account,
                                           gpointer    history_db);
  gboolean          (*is_im)              (ChattyChat *self);
  gboolean          (*has_file_upload)    (ChattyChat *self);
  ChattyChatState   (*get_chat_state)     (ChattyChat *self);
  const char       *(*get_chat_name)      (ChattyChat *self);
  ChattyAccount    *(*get_account)        (ChattyChat *self);
  GListModel       *(*get_messages)       (ChattyChat *self);
  GListModel       *(*get_users)          (ChattyChat *self);
  const char       *(*get_last_message)   (ChattyChat *self);
  const char       *(*get_topic)          (ChattyChat *self);
  void              (*set_topic)          (ChattyChat *self,
                                           const char *topic);
  void              (*load_past_messages) (ChattyChat *self,
                                           int         limit);
  gboolean          (*is_loading_history) (ChattyChat *self);
  guint             (*get_unread_count)   (ChattyChat *self);
  void              (*set_unread_count)   (ChattyChat *self,
                                           guint       unread_count);
  void              (*send_message_async) (ChattyChat    *chat,
                                           ChattyMessage *message,
                                           GAsyncReadyCallback callback,
                                           gpointer       user_data);
  gboolean         (*send_message_finish) (ChattyChat    *chat,
                                           GAsyncResult  *result,
                                           GError       **error);
  void              (*get_files_async)    (ChattyChat    *self,
                                           ChattyMessage *message,
                                           GAsyncReadyCallback callback,
                                           gpointer       user_data);
  gboolean          (*get_files_finish)   (ChattyChat    *self,
                                           GAsyncResult  *result,
                                           GError       **error);
  ChattyEncryption  (*get_encryption)     (ChattyChat *self);
  void              (*set_encryption)     (ChattyChat *self,
                                           gboolean    enable);
  void              (*set_encryption_async) (ChattyChat *self,
                                             gboolean    enable,
                                             GAsyncReadyCallback callback,
                                             gpointer       user_data);
  gboolean          (*set_encryption_finish) (ChattyChat    *self,
                                              GAsyncResult  *result,
                                              GError       **error);
  void              (*accept_invite_async)   (ChattyChat           *self,
                                              GAsyncReadyCallback   callback,
                                              gpointer              user_data);
  gboolean          (*accept_invite_finish)  (ChattyChat           *self,
                                              GAsyncResult         *result,
                                              GError              **error);
  void              (*reject_invite_async)   (ChattyChat           *self,
                                              GAsyncReadyCallback   callback,
                                              gpointer              user_data);
  gboolean          (*reject_invite_finish)  (ChattyChat           *self,
                                              GAsyncResult         *result,
                                              GError              **error);
  gboolean          (*get_buddy_typing)   (ChattyChat *self);
  void              (*set_typing)         (ChattyChat *self,
                                           gboolean    is_typing);
  void              (*invite_async)       (ChattyChat *self,
                                           const char *username,
                                           const char *invite_msg,
                                           GCancellable *cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer       user_data);
  gboolean         (*invite_finish)       (ChattyChat    *self,
                                           GAsyncResult  *result,
                                           GError       **error);
  void             (*show_notification)   (ChattyChat    *self,
                                           const char    *name);
  void             (*withdraw_notification)   (ChattyChat    *self);
};

ChattyChat         *chatty_chat_new                (const char *account_username,
                                                    const char *chat_name,
                                                    gboolean    is_im);
void                chatty_chat_set_data           (ChattyChat *self,
                                                    gpointer    account,
                                                    gpointer    history_db);
gboolean            chatty_chat_is_im              (ChattyChat *self);
char               *chatty_chat_generate_name      (ChattyChat *self,
                                                    GListModel *members);
ChattyChatState     chatty_chat_get_chat_state     (ChattyChat *self);
gboolean            chatty_chat_has_file_upload    (ChattyChat *self);
const char         *chatty_chat_get_chat_name      (ChattyChat *self);
ChattyAccount      *chatty_chat_get_account        (ChattyChat *self);
GListModel         *chatty_chat_get_messages       (ChattyChat *self);
void                chatty_chat_load_past_messages (ChattyChat *self,
                                                    int         count);
gboolean            chatty_chat_is_loading_history (ChattyChat *self);
GListModel         *chatty_chat_get_users          (ChattyChat *self);
const char         *chatty_chat_get_topic          (ChattyChat *self);
void                chatty_chat_set_topic          (ChattyChat *self,
                                                    const char *topic);
const char         *chatty_chat_get_last_message   (ChattyChat *self);
guint               chatty_chat_get_unread_count   (ChattyChat *self);
void                chatty_chat_set_unread_count   (ChattyChat *self,
                                                    guint       unread_count);
time_t              chatty_chat_get_last_msg_time  (ChattyChat *self);
void                chatty_chat_send_message_async (ChattyChat    *chat,
                                                    ChattyMessage *message,
                                                    GAsyncReadyCallback callback,
                                                    gpointer       user_data);
gboolean           chatty_chat_send_message_finish (ChattyChat    *self,
                                                    GAsyncResult  *result,
                                                    GError       **error);
void                chatty_chat_get_files_async    (ChattyChat    *self,
                                                    ChattyMessage *message,
                                                    GAsyncReadyCallback callback,
                                                    gpointer       user_data);
gboolean            chatty_chat_get_files_finish   (ChattyChat    *self,
                                                    GAsyncResult  *result,
                                                    GError       **error);
ChattyEncryption    chatty_chat_get_encryption     (ChattyChat *self);
void                chatty_chat_set_encryption     (ChattyChat *self,
                                                    gboolean    enable);
void                chatty_chat_set_encryption_async (ChattyChat     *self,
                                                      gboolean        enable,
                                                      GAsyncReadyCallback callback,
                                                      gpointer        user_data);
gboolean            chatty_chat_set_encryption_finish (ChattyChat    *self,
                                                       GAsyncResult  *result,
                                                       GError       **error);

void                chatty_chat_accept_invite_async   (ChattyChat          *self,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
gboolean            chatty_chat_accept_invite_finish  (ChattyChat           *self,
                                                       GAsyncResult         *result,
                                                       GError              **error);
void                chatty_chat_reject_invite_async   (ChattyChat          *self,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
gboolean            chatty_chat_reject_invite_finish  (ChattyChat           *self,
                                                       GAsyncResult         *result,
                                                       GError              **error);
gboolean            chatty_chat_get_buddy_typing   (ChattyChat *self);
void                chatty_chat_set_typing         (ChattyChat *self,
                                                    gboolean    is_typing);
void                chatty_chat_invite_async       (ChattyChat *self,
                                                    const char *username,
                                                    const char *invite_msg,
                                                    GCancellable *cancellable,
                                                    GAsyncReadyCallback callback,
                                                    gpointer    user_data);
gboolean           chatty_chat_invite_finish       (ChattyChat    *self,
                                                    GAsyncResult  *result,
                                                    GError       **error);
void               chatty_chat_show_notification   (ChattyChat    *self,
                                                    const char    *name);
void               chatty_chat_withdraw_notification   (ChattyChat    *self);

G_END_DECLS
