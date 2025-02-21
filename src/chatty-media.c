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

#define G_LOG_DOMAIN "chatty-media"

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

ChattyFile *
chatty_media_scale_image_to_size_sync (ChattyFile *input_file,
                                       gsize       desired_size,
                                       gboolean    use_temp_file)
{
  g_autoptr(GFileInfo) file_info = NULL;
  g_autoptr(GFile) resized_file = NULL;
  g_autoptr(GdkPixbuf) dest = NULL;
  g_autoptr(GdkPixbuf) src = NULL;
  g_autoptr(GString) path = NULL;
  g_autoptr(GError) error = NULL;
  ChattyFile *new_attachment;
  g_autofree char *basename = NULL;
  char *file_extension = NULL;
  int width = -1, height = -1;
  const char *qualities[] = {"80", "70", "60", "40", NULL};
  const char *mime_type;
  gsize new_size;

  mime_type = chatty_file_get_mime_type (input_file);
  if (!mime_type || !g_str_has_prefix (mime_type, "image")) {
    g_warning ("File is not an image! Cannot Resize");
    return NULL;
  }

  /* Most gifs are animated, so this cannot resize them */
  if (strstr (mime_type, "gif")) {
    g_warning ("File is a gif! Cannot resize");
    return NULL;
  }

  /*
   * https://developer.gnome.org/gdk-pixbuf/stable/
   * https://developer.gnome.org/gdk-pixbuf/stable/gdk-pixbuf-File-saving.html#gdk-pixbuf-save
   * https://developer.gnome.org/gdk-pixbuf/stable/gdk-pixbuf-File-Loading.html#gdk-pixbuf-new-from-file-at-scale
   */

  src = gdk_pixbuf_new_from_file (chatty_file_get_path (input_file), &error);
  if (error) {
    g_warning ("Error in loading: %s\n", error->message);
    return NULL;
  }

  {
    float aspect_ratio;
    int size;

    /* We don't have to apply the embedded orientation here
     * as we care only the largest of the width/height */
    width = gdk_pixbuf_get_width (src);
    height = gdk_pixbuf_get_height (src);
    aspect_ratio = MAX (width, height) / (float)(MIN (width, height));

    /*
     * Image size scales about linearly with either width or height changes
     * Some experimental figures for jpeg quality 80%:
     * 2048 by 2048: ~1,000,000 Bytes at 60 quality
     * 1600 by 1600: ~  500,000 Bytes at 40 quality
     * 1080 by 1080: ~  150,000 Bytes at 80 quality
     * 720 by 720:   ~   80,000 Bytes at 80 quality
     * 480 by 480:   ~   50,000 Bytes at 80 quality
     * 320 by 320:   ~   25,000 Bytes at 80 quality
     */

    if (desired_size < 25000 * aspect_ratio) {
      g_warning ("Requested size is too small!\n");
      return NULL;
    }

    if (desired_size < 50000 * aspect_ratio)
      size = 320;
    else if (desired_size < 80000 * aspect_ratio)
      size = 480;
    else if (desired_size < 150000 * aspect_ratio)
      size = 720;
    else if (desired_size < 300000 * aspect_ratio)
      size = 1080;
    else if (desired_size < 500000 * aspect_ratio)
      size = 1600;
    else
      size = 2048;

    /* Don't grow image more than the available size */
    if (width > height) {
      height = CLAMP (size, 0, height);
      width = -1;
    } else {
      width = CLAMP (size, 0, width);
      height = -1;
    }

    g_debug ("New width: %d, New height: %d", width, height);
  }

  /* Try qualities in descending order until one works. If the last one isn't
   * small enough, let it try anyway */
  for (const char **quality = qualities; *quality != NULL; quality++) {
    g_clear_object (&src);

    g_debug ("Quality %s", *quality);
    src = gdk_pixbuf_new_from_file_at_size (chatty_file_get_path (input_file), width, height, &error);

    /* Make sure the pixbuf is in the correct orientation */
    dest = gdk_pixbuf_apply_embedded_orientation (src);

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

    basename = g_path_get_basename (chatty_file_get_path (input_file));
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
    gdk_pixbuf_save (dest, path->str, "jpeg", &error, "quality", *quality, NULL);

    if (error) {
      g_warning ("Error in saving: %s\n", error->message);
      return NULL;
    }

    /* Debug: Figure out size of images */
    file_info = g_file_query_info (resized_file,
                                   G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                   G_FILE_QUERY_INFO_NONE,
                                   NULL,
                                   &error);

    if (error) {
      g_warning ("Error getting file info: %s", error->message);
      return NULL;
    }
    new_size = g_file_info_get_size (file_info);
    if (new_size <= desired_size) {
      g_debug ("Resized at quality %s to size %" G_GSIZE_FORMAT, *quality, new_size);
      break;
    }
  }

    if (new_size >= desired_size)
      g_warning ("Resized to size %" G_GSIZE_FORMAT " above size %" G_GSIZE_FORMAT, new_size, desired_size);

  /*
   * https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types
   */
  new_attachment = chatty_file_new_full (g_file_get_basename (resized_file),
                                         g_file_get_uri (resized_file),
                                         g_file_get_path (resized_file),
                                         "image/jpeg",
                                         g_file_info_get_size (file_info),
                                         0, 0, 0);

  return new_attachment;
}
