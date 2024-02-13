/* chatty-message-bar.h
 *
 * Copyright 2023 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#include "chatty-chat.h"
#include "chatty-history.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_MESSAGE_BAR (chatty_message_bar_get_type ())
G_DECLARE_FINAL_TYPE (ChattyMessageBar, chatty_message_bar, CHATTY, MESSAGE_BAR, GtkBox)

void     chatty_message_bar_set_db      (ChattyMessageBar *self,
                                         ChattyHistory    *history);
void     chatty_message_bar_set_chat    (ChattyMessageBar *self,
                                         ChattyChat       *chat);

G_END_DECLS
