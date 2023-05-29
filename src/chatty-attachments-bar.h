/* chatty-attachments-bar.h
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

#define CHATTY_TYPE_ATTACHMENTS_BAR (chatty_attachments_bar_get_type ())

G_DECLARE_FINAL_TYPE (ChattyAttachmentsBar, chatty_attachments_bar, CHATTY, ATTACHMENTS_BAR, GtkBox)

GtkWidget *chatty_attachments_bar_new       (void);
void       chatty_attachments_bar_reset     (ChattyAttachmentsBar *self);
void       chatty_attachments_bar_add_file  (ChattyAttachmentsBar *self,
                                             const char            *file_path);
GList     *chatty_attachments_bar_get_files (ChattyAttachmentsBar *self);

G_END_DECLS

