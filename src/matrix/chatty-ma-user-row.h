/* chatty-list-row.h
 *
 * Copyright 2025 Chatty Developers
 *
 * Author(s):
 *   Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

#include "chatty-ma-buddy.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_MA_USER_ROW (chatty_ma_user_row_get_type ())

G_DECLARE_FINAL_TYPE(ChattyMaUserRow, chatty_ma_user_row, CHATTY, MA_USER_ROW, GtkListBoxRow)

GtkWidget  *chatty_ma_user_row_new        (ChattyMaBuddy *buddy);

G_END_DECLS
