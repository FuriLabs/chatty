/*
 * Copyright (C) 2020 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#pragma once

#define HANDY_USE_UNSTABLE_API
#include <handy.h>

#include "users/chatty-item.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_NEW_CHAT_DIALOG (chatty_new_chat_dialog_get_type())
G_DECLARE_FINAL_TYPE (ChattyNewChatDialog, chatty_new_chat_dialog, CHATTY, NEW_CHAT_DIALOG, HdyDialog)


GtkWidget *chatty_new_chat_dialog_new (GtkWindow *parent_window);
void chatty_new_chat_set_edit_mode (ChattyNewChatDialog *self, gboolean edit);
ChattyItem *chatty_new_chat_dialog_get_selected_item (ChattyNewChatDialog *self);

G_END_DECLS
