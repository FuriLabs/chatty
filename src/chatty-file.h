/* chatty-file.c
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
#include <glib-object.h>

#include "chatty-enums.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_FILE (chatty_file_get_type ())

G_DECLARE_FINAL_TYPE (ChattyFile, chatty_file, CHATTY, FILE, GObject)


ChattyFile       *chatty_file_new_for_cm_event   (gpointer             cm_event);
ChattyFile       *chatty_file_new_for_path       (const char          *path);
ChattyFile       *chatty_file_new_full           (const char          *file_name,
                                                  const char          *url,
                                                  const char          *path,
                                                  const char          *mime_type,
                                                  gsize                size,
                                                  gsize                width,
                                                  gsize                height,
                                                  gsize                duration);
const char       *chatty_file_get_name           (ChattyFile          *self);
const char       *chatty_file_get_url            (ChattyFile          *self);
const char       *chatty_file_get_path           (ChattyFile          *self);
const char       *chatty_file_get_mime_type      (ChattyFile          *self);
gsize             chatty_file_get_size           (ChattyFile          *self);
void              chatty_file_set_status         (ChattyFile          *self,
                                                  ChattyFileStatus     status);
ChattyFileStatus  chatty_file_get_status         (ChattyFile          *self);
void              chatty_file_get_stream_async   (ChattyFile          *self,
                                                  GCancellable        *cancellable,
                                                  GAsyncReadyCallback  callback,
                                                  gpointer             user_data);
GInputStream     *chatty_file_get_stream_finish  (ChattyFile          *self,
                                                  GAsyncResult        *result,
                                                  GError             **error);
gsize             chatty_file_get_width          (ChattyFile          *self);
gsize             chatty_file_get_height         (ChattyFile          *self);
gsize             chatty_file_get_duration       (ChattyFile          *self);


G_END_DECLS
