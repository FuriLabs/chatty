/* chatty-chat-view.h
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

#include "chatty-history.h"
#include "chatty-chat.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_CHAT_PAGE (chatty_chat_page_get_type ())

G_DECLARE_FINAL_TYPE (ChattyChatPage, chatty_chat_page, CHATTY, CHAT_PAGE, AdwBin)

GtkWidget  *chatty_chat_page_new      (void);
void        chatty_chat_page_set_chat (ChattyChatPage *self,
                                       ChattyChat     *chat);
ChattyChat *chatty_chat_page_get_chat (ChattyChatPage *self);
void        chatty_chat_page_set_db   (ChattyChatPage *self,
                                       ChattyHistory  *history);

G_END_DECLS
