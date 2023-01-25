/* chatty-ma-key-chat.c
 *
 * Copyright 2022 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>
#include <glib-object.h>

#include "chatty-chat.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_MA_KEY_CHAT (chatty_ma_key_chat_get_type ())

G_DECLARE_FINAL_TYPE (ChattyMaKeyChat, chatty_ma_key_chat, CHATTY, MA_KEY_CHAT, ChattyChat)

ChattyMaKeyChat     *chatty_ma_key_chat_new          (gpointer             ma_account,
                                                      CmVerificationEvent *key_event);
CmEvent             *chatty_ma_key_chat_get_event    (ChattyMaKeyChat     *self);
ChattyItem          *chatty_ma_key_chat_get_sender   (ChattyMaKeyChat     *self);
GPtrArray           *chatty_ma_key_get_emoji         (ChattyMaKeyChat     *self);
guint16             *chatty_ma_key_get_decimal       (ChattyMaKeyChat     *self);
void                 chatty_ma_key_cancel_async      (ChattyMaKeyChat     *self,
                                                      GAsyncReadyCallback  callback,
                                                      gpointer             user_data);
gboolean             chatty_ma_key_cancel_finish     (ChattyMaKeyChat     *self,
                                                      GAsyncResult        *result,
                                                      GError             **error);
void                 chatty_ma_key_accept_async      (ChattyMaKeyChat     *self,
                                                      GAsyncReadyCallback  callback,
                                                      gpointer             user_data);
gboolean             chatty_ma_key_accept_finish     (ChattyMaKeyChat     *self,
                                                      GAsyncResult        *result,
                                                      GError             **error);
void                 chatty_ma_key_match_async       (ChattyMaKeyChat     *self,
                                                      GAsyncReadyCallback  callback,
                                                      gpointer             user_data);
gboolean             chatty_ma_key_match_finish      (ChattyMaKeyChat     *self,
                                                      GAsyncResult        *result,
                                                      GError             **error);

G_END_DECLS

