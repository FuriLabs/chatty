/* chatty-image-item.h
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

#include "chatty-file.h"
#include "chatty-message.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_FILE_ITEM (chatty_file_item_get_type ())

G_DECLARE_FINAL_TYPE (ChattyFileItem, chatty_file_item, CHATTY, FILE_ITEM, GtkEventBox)

GtkWidget       *chatty_file_item_new        (ChattyMessage   *message,
                                              ChattyFile      *file);
GtkStyleContext *chatty_file_item_get_style  (ChattyFileItem  *self);
ChattyMessage   *chatty_file_item_get_item   (ChattyFileItem  *self);

G_END_DECLS
