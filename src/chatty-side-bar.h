/*
 * Copyright 2023 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CHATTY_TYPE_SIDE_BAR (chatty_side_bar_get_type ())
G_DECLARE_FINAL_TYPE (ChattySideBar, chatty_side_bar, CHATTY, SIDE_BAR, GtkBox)

GtkWidget  *chatty_side_bar_get_chat_list         (ChattySideBar *self);
void        chatty_side_bar_set_show_archived     (ChattySideBar *self,
                                                   gboolean       show_archived);

G_END_DECLS
