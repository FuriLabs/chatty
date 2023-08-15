/* chatty-fp-row.h
 *
 * Copyright 2021 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CHATTY_TYPE_FP_ROW (chatty_fp_row_get_type ())

G_DECLARE_FINAL_TYPE (ChattyFpRow, chatty_fp_row, CHATTY, FP_ROW, GtkListBoxRow)

GtkWidget *chatty_fp_row_new      (GtkStringObject *item);
void       chatty_fp_row_set_item (ChattyFpRow     *self,
                                   GtkStringObject *item);


G_END_DECLS
