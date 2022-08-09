/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* matrix-utils.c
 *
 * Copyright 2020, 2022 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-matrix-utils"
#define BUFFER_SIZE 256

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define __STDC_WANT_LIB_EXT1__ 1
#include <string.h>
#include <glib/gi18n.h>

#include "chatty-log.h"
#include "chatty-utils.h"
#include "matrix-utils.h"

void
matrix_utils_clear (char   *buffer,
                    size_t  length)
{
  if (!buffer || length == 0)
    return;

  /* Brushing up your C: Note: we are not comparing with -1 */
  if (length == -1)
    length = strlen (buffer);

#ifdef __STDC_LIB_EXT1__
  memset_s (buffer, length, 0, length);
#elif HAVE_EXPLICIT_BZERO
  explicit_bzero (buffer, length);
#else
  volatile char *end = buffer + length;

  while (buffer != end)
    *(buffer++) = 0;
#endif
}

void
matrix_utils_free_buffer (char *buffer)
{
  matrix_utils_clear (buffer, -1);
  g_free (buffer);
}

static void
pixbuf_load_stream_cb (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;
  GdkPixbuf *pixbuf;

  g_assert (G_IS_TASK (task));

  pixbuf = gdk_pixbuf_new_from_stream_finish (result, &error);

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, pixbuf, g_object_unref);
}

static void
avatar_file_read_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  g_autoptr(GFileInputStream) stream = NULL;
  GCancellable *cancellable;
  GError *error = NULL;
  int width;

  g_assert (G_IS_TASK (task));

  stream = g_file_read_finish (G_FILE (object), result, &error);

  cancellable = g_task_get_cancellable (task);
  width = GPOINTER_TO_INT (g_object_get_data (object, "width"));

  if (error)
    g_task_return_error (task, error);
  else
    gdk_pixbuf_new_from_stream_at_scale_async (G_INPUT_STREAM (stream),
                                               MIN (width, 512), -1, TRUE,
                                               cancellable,
                                               pixbuf_load_stream_cb,
                                               g_steal_pointer (&task));
}

static void
avatar_file_info_read_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;
  int height, width;

  g_assert (G_IS_TASK (task));

  gdk_pixbuf_get_file_info_finish (result, &width, &height, &error);

  if (error) {
    g_task_return_error (task, error);
  } else {
    g_autoptr(GFile) file = NULL;
    GCancellable *cancellable;
    char *path;

    cancellable = g_task_get_cancellable (task);
    path = g_task_get_task_data (task);
    file = g_file_new_for_path (path);

    g_object_set_data (G_OBJECT (file), "height", GINT_TO_POINTER (height));
    g_object_set_data (G_OBJECT (file), "width", GINT_TO_POINTER (width));

    g_file_read_async (file, G_PRIORITY_DEFAULT, cancellable,
                       avatar_file_read_cb,
                       g_steal_pointer (&task));
  }
}

void
matrix_utils_get_pixbuf_async (const char          *file,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  GTask *task;

  g_return_if_fail (file && *file);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback);

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_task_data (task, g_strdup (file), g_free);

  gdk_pixbuf_get_file_info_async (file, cancellable,
                                  avatar_file_info_read_cb,
                                  task);

}

GdkPixbuf *
matrix_utils_get_pixbuf_finish (GAsyncResult  *result,
                                GError       **error)
{
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}
