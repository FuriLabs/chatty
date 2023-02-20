/* chatty-main-view.h
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

#include "chatty-item.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_MAIN_VIEW (chatty_main_view_get_type ())

G_DECLARE_FINAL_TYPE (ChattyMainView, chatty_main_view, CHATTY, MAIN_VIEW, GtkBox)

void           chatty_main_view_set_db            (ChattyMainView *self,
                                                   gpointer        db);
void           chatty_main_view_set_item          (ChattyMainView *self,
                                                   ChattyItem     *item);
ChattyItem    *chatty_main_view_get_item          (ChattyMainView *self);

G_END_DECLS
