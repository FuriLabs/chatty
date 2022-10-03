/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-header-bar.h
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

#include "chatty-chat.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_HEADER_BAR (chatty_header_bar_get_type ())

G_DECLARE_FINAL_TYPE (ChattyHeaderBar, chatty_header_bar, CHATTY, HEADER_BAR, GtkBox)

void        chatty_header_bar_set_can_search   (ChattyHeaderBar *self,
                                                gboolean         can_search);
void        chatty_header_bar_show_archived    (ChattyHeaderBar *self,
                                                gboolean         show_archived);
void        chatty_header_bar_set_item         (ChattyHeaderBar *self,
                                                ChattyItem      *item);
void        chatty_header_bar_set_content_box  (ChattyHeaderBar *self,
                                                GtkWidget       *widget);
void        chatty_header_bar_set_search_bar   (ChattyHeaderBar *self,
                                                GtkWidget       *widget);

G_END_DECLS
