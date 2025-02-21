/*
 * Copyright 2023 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CHATTY_TYPE_SIDE_BAR (chatty_side_bar_get_type ())
G_DECLARE_FINAL_TYPE (ChattySideBar, chatty_side_bar, CHATTY, SIDE_BAR, AdwNavigationPage)

GtkWidget  *chatty_side_bar_get_chat_list         (ChattySideBar *self);
void        chatty_side_bar_set_show_archived     (ChattySideBar *self,
                                                   gboolean       show_archived);
void        chatty_side_bar_set_show_end_title_buttons     (ChattySideBar *self,
                                                            gboolean       visible);
void        chatty_side_bar_toggle_search         (ChattySideBar *self);

G_END_DECLS
