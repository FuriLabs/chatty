/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-ma-account.h
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

#include "chatty-chat.h"
#include "chatty-enums.h"
#include "users/chatty-account.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_MA_ACCOUNT (chatty_ma_account_get_type ())

G_DECLARE_FINAL_TYPE (ChattyMaAccount, chatty_ma_account, CHATTY, MA_ACCOUNT, ChattyAccount)

ChattyMaAccount  *chatty_ma_account_new                (const char      *username,
                                                        const char      *password);
void              chatty_ma_account_set_history_db     (ChattyMaAccount *self,
                                                        gpointer         history_db);
void              chatty_ma_account_set_db             (ChattyMaAccount *self,
                                                        gpointer         matrix_db);
void              chatty_ma_account_save_async         (ChattyMaAccount *self,
                                                        gboolean         force,
                                                        GCancellable    *cancellable,
                                                        GAsyncReadyCallback callback,
                                                        gpointer         user_data);
gboolean          chatty_ma_account_save_finish        (ChattyMaAccount *self,
                                                        GAsyncResult    *result,
                                                        GError         **error);
const char       *chatty_ma_account_get_homeserver     (ChattyMaAccount *self);
void              chatty_ma_account_set_homeserver     (ChattyMaAccount *self,
                                                        const char      *server_url);
GListModel       *chatty_ma_account_get_chat_list      (ChattyMaAccount *self);
void              chatty_ma_account_send_file          (ChattyMaAccount *self,
                                                        ChattyChat      *chat,
                                                        const char      *file_name);
void              chatty_ma_account_delete_chat_async  (ChattyMaAccount *self,
                                                        ChattyChat      *chat,
                                                        GAsyncReadyCallback callback,
                                                        gpointer         user_data);
gboolean          chatty_ma_account_delete_chat_finish (ChattyMaAccount *self,
                                                        GAsyncResult    *result,
                                                        GError         **error);

G_END_DECLS
