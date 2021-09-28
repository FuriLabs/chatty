/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-media.c
 *
 * Copyright 2020, 2021 Purism SPC
 *           2021, Chris Talbot
 *
 * Author(s):
 *   Chris Talbot
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#define G_LOG_DOMAIN "chatty-multimedia"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/stat.h>
#include <errno.h>

#include "chatty-media.h"
#include "chatty-log.h"

/**
 * SECTION: chatty-media
 * @title: ChattyMultmedia
 * @short_description: Functions to transform multimedia
 * @include: "chatty-media.h"
 *
 */


/**
 * chatty_media_scale_image_to_size_sync:
 * @name: A string
 * @protocol: A #ChattyProtocol flag
 *
 * This function takes in a ChattyFileInfo, and scales the image in a new file
 * to be a size below the original_desired_size. It then creates a new
 * ChattyFileInfo to pass back. the original ChattyFileInfo is untouched.
 * original_desired_size is in bytes
 *
 * Returns: A newly allowcated #ChattyFileInfo with all valid protocols
 * set.
 */

ChattyFileInfo *
chatty_media_scale_image_to_size_sync (ChattyFileInfo *input_file,
                                       gsize           desired_size,
                                       gboolean        use_temp_file)
{
  g_autoptr(GFileInfo) file_info = NULL;
  g_autoptr(GFile) resized_file = NULL;
  g_autoptr(GdkPixbuf) dest = NULL;
  g_autoptr(GdkPixbuf) src = NULL;
  g_autoptr(GString) path = NULL;
  g_autoptr(GError) error = NULL;
  ChattyFileInfo *new_attachment;
  g_autofree char *basename = NULL;
  char *file_extension = NULL;
  int width, height, new_aspect;
  float aspect_ratio;

  if (!input_file->mime_type || !g_str_has_prefix (input_file->mime_type, "image")) {
    g_warning ("File is not an image! Cannot Resize");
    return NULL;
  }

  /* Most gifs are animated, so this cannot resize them */
  if (strstr (input_file->mime_type, "gif")) {
    g_warning ("File is a gif! Cannot resize");
    return NULL;
  }

  /*
   * https://developer.gnome.org/gdk-pixbuf/stable/
   * https://developer.gnome.org/gdk-pixbuf/stable/gdk-pixbuf-File-saving.html#gdk-pixbuf-save
   * https://developer.gnome.org/gdk-pixbuf/stable/gdk-pixbuf-File-Loading.html#gdk-pixbuf-new-from-file-at-scale
   */

  src = gdk_pixbuf_new_from_file (input_file->path, &error);
  if (error) {
    g_warning ("Error in loading: %s\n", error->message);
    return NULL;
  }

  /* Make sure the pixbuf is in the correct orientation */
  dest = gdk_pixbuf_apply_embedded_orientation (src);

  width = gdk_pixbuf_get_width (dest);
  height = gdk_pixbuf_get_height (dest);

  if (width > height) {
    aspect_ratio = (float) width / (float) height;
  } else {
    aspect_ratio = (float) height / (float) width;
  }

  /*
   * Image size scales about linearly with either width or height changes
   * Some (conservative) experimental figures for jpeg quality 80%:
   * 2560 by 2560: ~750000 Bytes
   * 2048 by 2048: ~500000 Bytes
   * 1600 by 1600: ~300000 Bytes
   * 1080 by 1080: ~150000 Bytes
   * 720 by 720:   ~ 80000 Bytes
   * 480 by 480:   ~ 50000 Bytes
   * 320 by 320:   ~ 25000 Bytes
   */

  if (desired_size < 25000 * aspect_ratio) {
    g_warning ("Requested size is too small!\n");
    return NULL;
  } else if (desired_size < 50000 * aspect_ratio) {
    new_aspect = 320;
  } else if (desired_size < 80000 * aspect_ratio) {
    new_aspect = 480;
  } else if (desired_size < 150000 * aspect_ratio) {
    new_aspect = 720;
  } else if (desired_size < 300000 * aspect_ratio) {
    new_aspect = 1080;
  } else if (desired_size < 500000 * aspect_ratio) {
    new_aspect = 1600;
  } else if (desired_size < 750000 * aspect_ratio) {
    new_aspect = 2048;
  } else {
    new_aspect = 2560;
  }

  g_debug ("Old width: %d, Old height: %d", width, height);
  if (width > height) {
    height = new_aspect;
    width = (float) new_aspect * aspect_ratio;
  } else {
    width = new_aspect;
    height = (float) new_aspect * aspect_ratio;
  }
  g_debug ("New width: %d, New height: %d", width, height);

  dest = gdk_pixbuf_scale_simple (dest, width, height,
                                  GDK_INTERP_BILINEAR);

  new_attachment = g_try_new0 (ChattyFileInfo, 1);
  if (new_attachment == NULL) {
    g_warning ("Error in creating new attachment\n");
    return NULL;
  }

  if (use_temp_file) {
    path = g_string_new (g_build_filename (g_get_tmp_dir (), "chatty/", NULL));
  } else {
    path = g_string_new (g_build_filename (g_get_user_cache_dir (), "chatty/", NULL));
  }

  CHATTY_TRACE_MSG ("New Directory Path: %s", path->str);

  if (g_mkdir_with_parents (path->str, S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
    g_warning ("Error creating directory: %s", strerror (errno));
    return NULL;
  }

  basename = g_path_get_basename (input_file->path);
  file_extension = strrchr (basename, '.');
  if (file_extension) {
    g_string_append_len (path, basename, file_extension - basename);
    g_string_append (path, "-resized.jpg");
  } else {
    g_string_append_printf (path, "%s-resized.jpg", basename);
  }

  CHATTY_TRACE_MSG ("New File Path: %s", path->str);
  resized_file = g_file_new_for_path (path->str);

  /* Putting the quality at 80 seems to work well experimentally */
  gdk_pixbuf_save (dest, path->str, "jpeg", &error, "quality", "80", NULL);

  if (error) {
    g_warning ("Error in saving: %s\n", error->message);
    return NULL;
  }

  /* Debug: Figure out size of images */
  file_info = g_file_query_info (resized_file,
                                 "*",
                                 G_FILE_QUERY_INFO_NONE,
                                 NULL,
                                 &error);

  if (error) {
      g_warning ("Error getting file info: %s", error->message);
      return NULL;
  }

  /*
   * https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types
   */
  new_attachment->file_name = g_file_get_basename (resized_file);

  /* We are saving it as a jpeg, so we know the MIME type */
  new_attachment->mime_type = g_strdup ("image/jpeg");

  new_attachment->size = g_file_info_get_size (file_info);
  new_attachment->path = g_file_get_path (resized_file);
  new_attachment->url  = g_file_get_uri (resized_file);

  return new_attachment;
}

typedef struct {
  ChattyFileInfo *input_file;
  gulong          desired_size;
  gboolean        use_temp_file;
} ChattyMediaScaleData;

static void
scale_image_thread (GTask         *task,
                    gpointer       source_object,
                    gpointer       task_data,
                    GCancellable  *cancellable)
{
  ChattyMediaScaleData *scale_data = task_data;
  ChattyFileInfo *new_file;

  new_file = chatty_media_scale_image_to_size_sync (scale_data->input_file,
                                       scale_data->desired_size,
                                       scale_data->use_temp_file);

  if (new_file) {
    g_task_return_pointer (task, new_file, NULL);
  } else {
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "Error in creating scaled image");
    return;
  }

}

void
chatty_media_scale_image_to_size_async (ChattyFileInfo      *input_file,
                                        gulong               desired_size,
                                        gboolean             use_temp_file,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  ChattyMediaScaleData *scale_data;

  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback);

  task = g_task_new (NULL, cancellable, callback, user_data);
  if (!input_file->mime_type || !g_str_has_prefix (input_file->mime_type, "image")) {
    g_warning ("File is not an image! Cannot Resize");
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                             "File is not an image! Cannot Resize");
    return;
  }

  /* Most gifs are animated, so this cannot resize them */
  if (strstr (input_file->mime_type, "gif")) {
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                             "File is a gif! Cannot resize");
    return;
  }

  scale_data = g_try_new0 (ChattyMediaScaleData, 1);
  if (scale_data == NULL) {
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NO_SPACE,
                             "Error in creating new attachment");
    return;
  }

  scale_data->input_file = input_file;
  scale_data->desired_size = desired_size;
  scale_data->use_temp_file = use_temp_file;

  g_task_set_task_data (task, scale_data, g_free);
  g_task_run_in_thread (task, scale_image_thread);
}
