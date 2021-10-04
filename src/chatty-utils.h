/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <stdio.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <purple.h>

#include "chatty-enums.h"

#define MAX_GMT_ISO_SIZE 256
#define SECONDS_PER_DAY    86400.0

typedef struct _ChattyFileInfo ChattyFileInfo;

struct _ChattyFileInfo {
  char *file_name;
  char *url;
  char *path;
  char *mime_type;
  gpointer user_data;
  gsize width;
  gsize height;
  gsize size;
  /* For audio files */
  gsize duration;
  int status;
};

char *chatty_utils_jabber_id_strip (const char *name);
char *chatty_utils_check_phonenumber (const char *phone_number,
                                      const char *country);
ChattyProtocol chatty_utils_username_is_valid  (const char     *name,
                                                ChattyProtocol  protocol);
ChattyProtocol chatty_utils_groupname_is_valid (const char     *name,
                                                ChattyProtocol  protocol);
gboolean chatty_utils_get_item_position (GListModel *list,
                                         gpointer    item,
                                         guint      *position);
gboolean chatty_utils_remove_list_item  (GListStore *store,
                                         gpointer    item);
char       *chatty_utils_get_human_time (time_t unix_time);
PurpleBlistNode *chatty_utils_get_conv_blist_node (PurpleConversation *conv);
GdkPixbuf           *chatty_utils_get_pixbuf_from_data  (const guchar *buf,
                                                         gsize         count);
ChattyMsgDirection   chatty_utils_direction_from_flag   (PurpleMessageFlags flag);
void                 chatty_file_info_free              (ChattyFileInfo     *file_info);
void      chatty_utils_create_thumbnail_async  (const char     *file,
                                                GAsyncReadyCallback callback,
                                                gpointer       user_data);
gboolean  chatty_utils_create_thumbnail_finish (GAsyncResult  *result,
                                                GError       **error);
