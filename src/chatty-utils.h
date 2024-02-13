/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <stdio.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "chatty-enums.h"

#define MAX_GMT_ISO_SIZE 256


gboolean chatty_utils_window_has_toplevel_focus (GtkWindow *window);
const char *chatty_utils_get_purple_dir (void);
char *chatty_utils_jabber_id_strip (const char *name);
void chatty_utils_sanitize_filename (char *name);
char *chatty_utils_find_url (const char  *buffer,
                             char       **end);
char *chatty_utils_strip_utm_from_url (const char *url_to_parse);
char *chatty_utils_strip_utm_from_message (const char *message);
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
GdkPixbuf           *chatty_utils_get_pixbuf_from_data  (const guchar *buf,
                                                         gsize         count);
void      chatty_utils_create_thumbnail_async  (GFile               *file,
                                                GAsyncReadyCallback callback,
                                                gpointer       user_data);
gboolean  chatty_utils_create_thumbnail_finish (GAsyncResult  *result,
                                                GError       **error);
gboolean chatty_utils_dialog_response_is_cancel (GtkAlertDialog *alertdialog,
                                                 int             response);
