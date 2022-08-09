/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* matrix-utils.h
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib-object.h>

#include "cm-enums.h"

void          matrix_utils_clear                    (char          *buffer,
                                                     size_t         length);
void          matrix_utils_free_buffer              (char          *buffer);

void          matrix_utils_get_pixbuf_async         (const char    *file,
                                                     GCancellable  *cancellable,
                                                     GAsyncReadyCallback callback,
                                                     gpointer       user_data);
GdkPixbuf    *matrix_utils_get_pixbuf_finish        (GAsyncResult  *result,
                                                     GError       **error);
