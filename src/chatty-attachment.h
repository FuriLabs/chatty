/* chatty-attachment.h
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

#define CHATTY_TYPE_ATTACHMENT (chatty_attachment_get_type ())

G_DECLARE_FINAL_TYPE (ChattyAttachment, chatty_attachment, CHATTY, ATTACHMENT, GtkBox)

GtkWidget   *chatty_attachment_new        (GFile             *file);
GFile       *chatty_attachment_get_file   (ChattyAttachment  *self);

G_END_DECLS
