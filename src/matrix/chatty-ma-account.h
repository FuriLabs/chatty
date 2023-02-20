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
#define CMATRIX_USE_EXPERIMENTAL_API
#include "cmatrix.h"

#include "chatty-chat.h"
#include "chatty-enums.h"
#include "chatty-account.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_MA_ACCOUNT (chatty_ma_account_get_type ())

G_DECLARE_FINAL_TYPE (ChattyMaAccount, chatty_ma_account, CHATTY, MA_ACCOUNT, ChattyAccount)

ChattyMaAccount  *chatty_ma_account_new_from_client    (CmClient        *cm_client);
CmClient         *chatty_ma_account_get_cm_client      (ChattyMaAccount *self);
const char       *chatty_ma_account_get_login_username (ChattyMaAccount *self);
const char       *chatty_ma_account_get_homeserver     (ChattyMaAccount *self);
void              chatty_ma_account_set_homeserver     (ChattyMaAccount *self,
                                                        const char      *server_url);
const char       *chatty_ma_account_get_device_id      (ChattyMaAccount *self);
GListModel       *chatty_ma_account_get_chat_list      (ChattyMaAccount *self);
void              chatty_ma_account_send_file          (ChattyMaAccount *self,
                                                        ChattyChat      *chat,
                                                        const char      *file_name);
void              chatty_ma_account_get_details_async  (ChattyMaAccount *self,
                                                        GCancellable    *cancellable,
                                                        GAsyncReadyCallback callback,
                                                        gpointer         user_data);
gboolean          chatty_ma_account_get_details_finish (ChattyMaAccount *self,
                                                        GAsyncResult    *result,
                                                        GError         **error);
void              chatty_ma_account_set_name_async     (ChattyMaAccount *self,
                                                        const char      *name,
                                                        GCancellable    *cancellable,
                                                        GAsyncReadyCallback callback,
                                                        gpointer         user_data);
gboolean          chatty_ma_account_set_name_finish    (ChattyMaAccount *self,
                                                        GAsyncResult    *result,
                                                        GError         **error);
void              chatty_ma_account_get_3pid_async     (ChattyMaAccount *self,
                                                        GCancellable    *cancellable,
                                                        GAsyncReadyCallback callback,
                                                        gpointer         user_data);
gboolean          chatty_ma_account_get_3pid_finish    (ChattyMaAccount *self,
                                                        GPtrArray      **emails,
                                                        GPtrArray      **phones,
                                                        GAsyncResult    *result,
                                                        GError         **error);
void              chatty_ma_account_delete_3pid_async  (ChattyMaAccount *self,
                                                        const char      *value,
                                                        ChattyIdType     type,
                                                        GCancellable    *cancellable,
                                                        GAsyncReadyCallback callback,
                                                        gpointer         user_data);
gboolean          chatty_ma_account_delete_3pid_finish (ChattyMaAccount *self,
                                                        GAsyncResult    *result,
                                                        GError         **error);

G_END_DECLS
