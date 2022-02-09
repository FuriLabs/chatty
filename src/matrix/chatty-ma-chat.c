/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-ma-chat.c
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-ma-chat"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>
#define CMATRIX_USE_EXPERIMENTAL_API
#include "cmatrix.h"

#include "contrib/gtk.h"
#include "chatty-history.h"
#include "chatty-utils.h"
#include "matrix-utils.h"
#include "chatty-ma-buddy.h"
#include "chatty-ma-chat.h"
#include "chatty-log.h"

/**
 * SECTION: chatty-chat
 * @title: ChattyChat
 * @short_description: An abstraction over #PurpleConversation
 * @include: "chatty-chat.h"
 *
 * libpurple doesn’t have a nice OOP interface for managing anything.
 * This class hides all the complexities surrounding it.
 */

struct _ChattyMaChat
{
  ChattyChat           parent_instance;

  char                *room_name;
  char                *generated_name;
  char                *room_id;
  char                *encryption;
  char                *last_batch;
  GdkPixbuf           *avatar;
  ChattyFileInfo      *avatar_file;
  GCancellable        *avatar_cancellable;
  ChattyMaBuddy       *self_buddy;
  GListStore          *buddy_list;
  GListStore          *message_list;
  GtkSortListModel    *sorted_message_list;

  JsonObject       *json_data;
  ChattyAccount    *account;
  CmClient         *cm_client;
  CmRoom           *cm_room;
  ChattyHistory    *history_db;

  ChattyItemState visibility_state;
  gint64          highlight_count;
  int             unread_count;
  int             room_name_update_ts;

  guint          notification_shown : 1;

  guint          prev_batch_loading : 1;
  guint          history_is_loading : 1;
  guint          avatar_is_loading : 1;

  guint          room_name_loaded : 1;
  guint          buddy_typing : 1;
};

G_DEFINE_TYPE (ChattyMaChat, chatty_ma_chat, CHATTY_TYPE_CHAT)

enum {
  PROP_0,
  PROP_JSON_DATA,
  PROP_ROOM_ID,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static int
sort_message (gconstpointer a,
              gconstpointer b,
              gpointer      user_data)
{
  time_t time_a, time_b;

  time_a = chatty_message_get_time ((gpointer)a);
  time_b = chatty_message_get_time ((gpointer)b);

  return time_a - time_b;
}

static void
chatty_mat_chat_update_name (ChattyMaChat *self)
{
  g_assert (CHATTY_IS_MA_CHAT (self));

  if (self->room_name)
    return;

  g_free (self->generated_name);
  self->generated_name = chatty_chat_generate_name (CHATTY_CHAT (self),
                                                    G_LIST_MODEL (self->buddy_list));
  g_signal_emit_by_name (self, "avatar-changed");
  chatty_history_update_chat (self->history_db, CHATTY_CHAT (self));
}

static ChattyMaBuddy *
ma_chat_find_buddy (ChattyMaChat *self,
                    GListModel   *model,
                    const char   *matrix_id,
                    guint        *index)
{
  guint n_items;

  g_assert (CHATTY_IS_MA_CHAT (self));
  g_assert (G_IS_LIST_MODEL (model));
  g_return_val_if_fail (matrix_id && *matrix_id, NULL);

  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++) {
    g_autoptr(ChattyMaBuddy) buddy = NULL;

    buddy = g_list_model_get_item (model, i);
    if (g_str_equal (chatty_item_get_username (CHATTY_ITEM (buddy)), matrix_id)) {
      if (index)
        *index = i;

      return buddy;
    }
  }

  return NULL;
}

static ChattyMaBuddy *
ma_chat_add_buddy (ChattyMaChat *self,
                   GListStore   *store,
                   const char   *matrix_id)
{
  g_autoptr(ChattyMaBuddy) buddy = NULL;

  g_assert (CHATTY_IS_MA_CHAT (self));

  if (!matrix_id || *matrix_id != '@')
    g_return_val_if_reached (NULL);

  buddy = chatty_ma_buddy_new (matrix_id, self->cm_client);
  g_list_store_append (store, buddy);

  return buddy;
}

static void
ma_chat_get_avatar_pixbuf_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  g_autoptr(ChattyMaChat) self = user_data;
  GdkPixbuf *pixbuf;
  g_autoptr(GError) error = NULL;

  g_assert (CHATTY_IS_MA_CHAT (self));

  pixbuf = matrix_utils_get_pixbuf_finish (result, &error);

  if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("Error loading avatar file: %s", error->message);

  if (!error) {
    g_set_object (&self->avatar, pixbuf);
    g_signal_emit_by_name (self, "avatar-changed");
  }

  self->avatar_is_loading = FALSE;
}

static ChattyFileInfo *
ma_chat_new_file (ChattyMaChat *self,
                  JsonObject   *object,
                  JsonObject   *content)
{
  ChattyFileInfo *file = NULL;
  const char *url;

  g_assert (CHATTY_IS_MA_CHAT (self));

  url = matrix_utils_json_object_get_string (object, "url");

  if (url && g_str_has_prefix (url, "mxc://")) {
    file = g_new0 (ChattyFileInfo, 1);

    file->url = g_strdup (url);
    file->file_name = g_strdup (matrix_utils_json_object_get_string (object, "body"));
    object = matrix_utils_json_object_get_object (content, "info");
    file->mime_type = g_strdup (matrix_utils_json_object_get_string (object, "mimetype"));
    file->height = matrix_utils_json_object_get_int (object, "h");
    file->width = matrix_utils_json_object_get_int (object, "w");
    file->size = matrix_utils_json_object_get_int (object, "size");
  }

  return file;
}

static void
chat_handle_m_media (ChattyMaChat  *self,
                     ChattyMessage *message,
                     JsonObject    *content,
                     const char    *type,
                     gboolean       encrypted)
{
  ChattyFileInfo *file = NULL;
  JsonObject *object;

  g_assert (CHATTY_IS_MA_CHAT (self));
  g_assert (CHATTY_IS_MESSAGE (message));
  g_assert (content);
  g_assert (type);

  CHATTY_TRACE_MSG ("Got media, type: %s, encrypted: %d", type, !!encrypted);

  if (encrypted)
    object = matrix_utils_json_object_get_object (content, "file");
  else
    object = content;

  if (!matrix_utils_json_object_get_string (object, "url"))
    return;

  if (!g_str_equal (type, "m.image") &&
      !g_str_equal (type, "m.video") &&
      !g_str_equal (type, "m.file") &&
      !g_str_equal (type, "m.audio"))
    return;

  file = ma_chat_new_file (self, object, content);

  if (!file)
    return;

  g_object_set_data_full (G_OBJECT (message), "file-url", g_strdup (file->url), g_free);
  chatty_message_set_files (message, g_list_append (NULL, file));
  return;
}

static void
matrix_add_message_from_data (ChattyMaChat  *self,
                              ChattyMaBuddy *buddy,
                              JsonObject    *root,
                              JsonObject    *object,
                              gboolean       encrypted)
{
  g_autoptr(ChattyMessage) message = NULL;
  JsonObject *content;
  const char *body, *type;
  ChattyMsgDirection direction = CHATTY_DIRECTION_IN;
  ChattyMsgType msg_type;
  const char *uuid;
  time_t ts;

  g_assert (CHATTY_IS_MA_CHAT (self));
  g_assert (object);

  content = matrix_utils_json_object_get_object (object, "content");
  type = matrix_utils_json_object_get_string (content, "msgtype");

  if (!type)
    return;

  if (g_str_equal (type, "m.image"))
    msg_type = CHATTY_MESSAGE_IMAGE;
  else if (g_str_equal (type, "m.video"))
    msg_type = CHATTY_MESSAGE_VIDEO;
  else if (g_str_equal (type, "m.file"))
    msg_type = CHATTY_MESSAGE_FILE;
  else if (g_str_equal (type, "m.audio"))
    msg_type = CHATTY_MESSAGE_AUDIO;
  else if (g_str_equal (type, "m.location"))
    msg_type = CHATTY_MESSAGE_LOCATION;
  else
    msg_type = CHATTY_MESSAGE_TEXT;

  body = matrix_utils_json_object_get_string (content, "body");
  if (root)
    uuid = matrix_utils_json_object_get_string (root, "event_id");
  else
    uuid = matrix_utils_json_object_get_string (object, "event_id");

  /* timestamp is in milliseconds */
  ts = matrix_utils_json_object_get_int (object, "origin_server_ts");
  ts = ts / 1000;

  if (buddy == self->self_buddy)
    direction = CHATTY_DIRECTION_OUT;

  CHATTY_TRACE_MSG ("Got message, direction: %s, type %s",
                    direction == CHATTY_DIRECTION_OUT ? "out" : "in", type);

  if (direction == CHATTY_DIRECTION_OUT && uuid) {
    JsonObject *data_unsigned;
    const char *transaction_id;
    guint n_items = 0, limit;

    if (root)
      data_unsigned = matrix_utils_json_object_get_object (root, "unsigned");
    else
      data_unsigned = matrix_utils_json_object_get_object (object, "unsigned");
    transaction_id = matrix_utils_json_object_get_string (data_unsigned, "transaction_id");

    if (transaction_id)
      n_items = g_list_model_get_n_items (G_LIST_MODEL (self->message_list));

    if (n_items > 50)
      limit = n_items - 50;
    else
      limit = 0;

    /* Note: i, limit and n_items are unsigned */
    for (guint i = n_items - 1; i + 1 > limit; i--) {
      g_autoptr(ChattyMessage) msg = NULL;
      const char *event_id;

      msg = g_list_model_get_item (G_LIST_MODEL (self->message_list), i);
      event_id = g_object_get_data (G_OBJECT (msg), "event-id");

      if (event_id && g_str_equal (event_id, transaction_id)) {
        chatty_message_set_uid (msg, uuid);
        chatty_history_add_message (self->history_db, CHATTY_CHAT (self), msg);
        return;
      }
    }
  }

  /* We should move to more precise time (ie, time in ms) as it is already provided */
  message = chatty_message_new (CHATTY_ITEM (buddy), body, uuid, ts, msg_type, direction, 0);
  chatty_message_set_encrypted (message, encrypted);

  if (msg_type != CHATTY_MESSAGE_TEXT)
    chat_handle_m_media (self, message, content, type, encrypted);

  g_list_store_append (self->message_list, message);
  chatty_history_add_message (self->history_db, CHATTY_CHAT (self), message);
}

static void
handle_m_room_encrypted (ChattyMaChat  *self,
                         ChattyMaBuddy *buddy,
                         JsonObject    *root)
{
  g_autofree char *plaintext = NULL;
  g_autofree char *json_str = NULL;

  g_assert (CHATTY_IS_MA_CHAT (self));

  if (!root)
    return;

  json_str = matrix_utils_json_object_to_string (root, FALSE);
  plaintext = cm_room_decrypt_content (self->cm_room, json_str);

  if (plaintext) {
    g_autoptr(JsonObject) message = NULL;

    message = matrix_utils_string_to_json_object (plaintext);
    matrix_add_message_from_data (self, buddy, root, message, TRUE);
  }
}

static void
ma_chat_handle_ephemeral (ChattyMaChat *self,
                          JsonObject   *root)
{
  JsonObject *object;
  JsonArray *array;

  g_assert (CHATTY_IS_MA_CHAT (self));
  g_assert (root);

  array = matrix_utils_json_object_get_array (root, "events");

  if (array) {
    g_autoptr(GList) elements = NULL;

    elements = json_array_get_elements (array);

    for (GList *node = elements; node; node = node->next) {
      const char *type;

      object = json_node_get_object (node->data);
      type = matrix_utils_json_object_get_string (object, "type");
      object = matrix_utils_json_object_get_object (object, "content");

      if (g_strcmp0 (type, "m.typing") == 0) {
        array = matrix_utils_json_object_get_array (object, "user_ids");

        if (array) {
          const char *username, *name = NULL;
          guint typing_count = 0;
          gboolean buddy_typing = FALSE;
          gboolean self_typing = FALSE;

          typing_count = json_array_get_length (array);
          buddy_typing = typing_count >= 2;

          /* Handle the first item so that we don’t have to
             handle buddy_typing in the loop */
          username = cm_client_get_user_id (self->cm_client);
          if (typing_count)
            name = json_array_get_string_element (array, 0);

          if (g_strcmp0 (name, username) == 0)
            self_typing = TRUE;
          else if (typing_count)
            buddy_typing = TRUE;

          /* Check if the server says we are typing too */
          for (guint i = 0; !self_typing && i < typing_count; i++)
            if (g_str_equal (json_array_get_string_element (array, i), username))
              self_typing = TRUE;

          if (self->buddy_typing != buddy_typing) {
            self->buddy_typing = buddy_typing;
            g_object_notify (G_OBJECT (self), "buddy-typing");
          }
        }
      }
    }
  }
}

static void
parse_chat_array (ChattyMaChat *self,
                  JsonArray    *array)
{
  g_autoptr(GList) events = NULL;

  if (!array)
    return;

  events = json_array_get_elements (array);
  CHATTY_TRACE_MSG ("Got %u events", json_array_get_length (array));

  for (GList *event = events; event; event = event->next) {
    ChattyMaBuddy *buddy;
    JsonObject *object;
    const char *type, *sender;

    object = json_node_get_object (event->data);
    type = matrix_utils_json_object_get_string (object, "type");
    sender = matrix_utils_json_object_get_string (object, "sender");

    if (!type || !*type || !sender || !*sender)
      continue;

    buddy = ma_chat_find_buddy (self, G_LIST_MODEL (self->buddy_list), sender, NULL);

    if (!buddy)
      buddy = ma_chat_add_buddy (self, self->buddy_list, sender);

    if (!self->self_buddy &&
        g_strcmp0 (sender, chatty_item_get_username (CHATTY_ITEM (self))) == 0)
      g_set_object (&self->self_buddy, buddy);

    if (g_str_equal (type, "m.room.message")) {
      matrix_add_message_from_data (self, buddy, NULL, object, FALSE);
    } else if (g_str_equal (type, "m.room.encrypted")) {
      handle_m_room_encrypted (self, buddy, object);
    }
  }

  if (!self->room_name_loaded) {
    self->room_name_loaded = TRUE;
    g_object_notify (G_OBJECT (self), "name");
    g_signal_emit_by_name (self, "avatar-changed");
  }
}

static void
matrix_chat_set_json_data (ChattyMaChat *self,
                           JsonObject   *object)
{
  g_assert (CHATTY_IS_MA_CHAT (self));

  g_clear_pointer (&self->json_data, json_object_unref);
  self->json_data = object;

  if (!object)
    return;

  object = matrix_utils_json_object_get_object (self->json_data, "ephemeral");
  if (object)
    ma_chat_handle_ephemeral (self, object);

  object = matrix_utils_json_object_get_object (self->json_data, "unread_notifications");
  if (object)
    self->highlight_count = matrix_utils_json_object_get_int (object, "highlight_count");

  object = matrix_utils_json_object_get_object (self->json_data, "timeline");
  parse_chat_array (self, matrix_utils_json_object_get_array (object, "events"));

  object = matrix_utils_json_object_get_object (self->json_data, "unread_notifications");
  if (object) {
    guint old_count;

    old_count = self->unread_count;
    self->unread_count = matrix_utils_json_object_get_int (object, "notification_count");
    chatty_chat_show_notification (CHATTY_CHAT (self), NULL);
    g_signal_emit_by_name (self, "changed", 0);

    /* Reset notification state on new messages */
    if (self->unread_count > old_count)
      self->notification_shown = FALSE;
  }
}

static void
get_messages_cb (GObject      *obj,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  ChattyMaChat *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(JsonNode) node = NULL;
  g_autoptr(GError) error = NULL;
  JsonObject *root = NULL;
  g_autofree char *json_str;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (CHATTY_IS_MA_CHAT (self));

  json_str = cm_room_load_prev_batch_finish (self->cm_room, result, &error);
  self->prev_batch_loading = FALSE;
  self->history_is_loading = FALSE;
  g_object_notify (G_OBJECT (self), "loading-history");

  if (!json_str) {
    if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("error: %s", error->message);
    g_task_return_boolean (task, FALSE);
    return;
  }

  node = json_from_string (json_str, &error);
  root = json_node_get_object (node);
  parse_chat_array (self, matrix_utils_json_object_get_array (root, "chunk"));
}

static void
ma_chat_load_db_messages_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  ChattyMaChat *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GPtrArray) messages = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (CHATTY_IS_MA_CHAT (self));

  g_object_freeze_notify (G_OBJECT (self));

  messages = chatty_history_get_messages_finish (self->history_db, result, &error);
  self->history_is_loading = FALSE;
  g_object_notify (G_OBJECT (self), "loading-history");

  if (error &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
    g_warning ("Error fetching messages from db: %s,", error->message);

  CHATTY_TRACE_MSG ("Messages loaded from db: %u", !messages ? 0 : messages->len);

  if (messages && messages->len) {
    g_list_store_splice (self->message_list, 0, 0, messages->pdata, messages->len);
    chatty_chat_show_notification (CHATTY_CHAT (self), NULL);
    g_signal_emit_by_name (self, "changed", 0);
    g_task_return_boolean (task, TRUE);
  } else if (!messages && !self->prev_batch_loading) {
    self->history_is_loading = TRUE;
    self->prev_batch_loading = TRUE;
    g_object_notify (G_OBJECT (self), "loading-history");
    cm_room_load_prev_batch_async (self->cm_room, NULL,
                                   get_messages_cb,
                                   g_steal_pointer (&task));
  } else {
    /* TODO */
    /* g_task_return_boolean (); */
  }

  g_object_thaw_notify (G_OBJECT (self));
}

static gboolean
chatty_ma_chat_is_im (ChattyChat *chat)
{
  return TRUE;
}

static gboolean
chatty_ma_chat_has_file_upload (ChattyChat *chat)
{
  return TRUE;
}

static const char *
chatty_ma_chat_get_chat_name (ChattyChat *chat)
{
  ChattyMaChat *self = (ChattyMaChat *)chat;

  g_assert (CHATTY_IS_MA_CHAT (self));

  return self->room_id;
}

static void
chatty_ma_chat_real_past_messages (ChattyChat *chat,
                                   int         count)
{
  ChattyMaChat *self = (ChattyMaChat *)chat;
  GListModel *model;
  GTask *task;
  guint n_items;

  g_assert (CHATTY_IS_MA_CHAT (self));
  g_assert (count > 0);

  if (self->history_is_loading)
    return;

  CHATTY_TRACE (self->room_id, "Loading %d past messages from", count);

  self->history_is_loading = TRUE;
  g_object_notify (G_OBJECT (self), "loading-history");

  model = chatty_chat_get_messages (chat);
  n_items = g_list_model_get_n_items (model);

  task = g_task_new (self, NULL, NULL, NULL);
  g_object_set_data (G_OBJECT (task), "count", GUINT_TO_POINTER (n_items));

  chatty_history_get_messages_async (self->history_db, chat,
                                     g_list_model_get_item (model, 0),
                                     count, ma_chat_load_db_messages_cb,
                                     task);
}

static gboolean
chatty_ma_chat_is_loading_history (ChattyChat *chat)
{
  ChattyMaChat *self = (ChattyMaChat *)chat;

  g_assert (CHATTY_IS_MA_CHAT (self));

  return self->history_is_loading;
}

static GListModel *
chatty_ma_chat_get_messages (ChattyChat *chat)
{
  ChattyMaChat *self = (ChattyMaChat *)chat;

  g_assert (CHATTY_IS_MA_CHAT (self));

  return G_LIST_MODEL (self->sorted_message_list);
}

static ChattyAccount *
chatty_ma_chat_get_account (ChattyChat *chat)
{
  ChattyMaChat *self = (ChattyMaChat *)chat;

  g_assert (CHATTY_IS_MA_CHAT (self));

  return self->account;
}


static ChattyEncryption
chatty_ma_chat_get_encryption (ChattyChat *chat)
{
  ChattyMaChat *self = (ChattyMaChat *)chat;

  g_assert (CHATTY_IS_MA_CHAT (self));

  if (self->cm_room && cm_room_is_encrypted (self->cm_room))
    return CHATTY_ENCRYPTION_ENABLED;

  return CHATTY_ENCRYPTION_DISABLED;
}

static void
ma_chat_set_encryption_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  ChattyMaChat *self;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;
  gboolean ret;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (CHATTY_IS_MA_CHAT (self));

  ret = cm_room_enable_encryption_finish (self->cm_room, result, &error);
  CHATTY_DEBUG (self->room_id, "Setting encryption, success: %d, room:", ret);

  if (error) {
    g_warning ("Failed to set encryption: %s", error->message);
    g_task_return_error (task, error);
  } else {
    g_object_notify (G_OBJECT (self), "encrypt");
    g_task_return_boolean (task, ret);
  }
}

static void
chatty_ma_chat_set_encryption_async (ChattyChat          *chat,
                                     gboolean             enable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  ChattyMaChat *self = (ChattyMaChat *)chat;
  g_autoptr(GTask) task = NULL;

  g_assert (CHATTY_IS_MA_CHAT (self));
  g_assert (self->cm_client);

  task = g_task_new (self, NULL, callback, user_data);

  if (!enable) {
    g_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_SUPPORTED,
                             "Disabling encryption not allowed");
    return;
  }

  CHATTY_DEBUG (self->room_id, "Setting encryption for room:");
  cm_room_enable_encryption_async (self->cm_room, NULL,
                                   ma_chat_set_encryption_cb,
                                   g_steal_pointer (&task));
}

static const char *
chatty_ma_chat_get_last_message (ChattyChat *chat)
{
  ChattyMaChat *self = (ChattyMaChat *)chat;
  g_autoptr(ChattyMessage) message = NULL;
  GListModel *model;
  guint n_items;

  g_assert (CHATTY_IS_MA_CHAT (self));

  model = G_LIST_MODEL (self->message_list);
  n_items = g_list_model_get_n_items (model);

  if (n_items == 0)
    return "";

  message = g_list_model_get_item (model, n_items - 1);

  return chatty_message_get_text (message);
}

static guint
chatty_ma_chat_get_unread_count (ChattyChat *chat)
{
  ChattyMaChat *self = (ChattyMaChat *)chat;

  g_assert (CHATTY_IS_MA_CHAT (self));

  return self->unread_count;
}

static void
chat_set_read_marker_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  ChattyMaChat *self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (CHATTY_IS_MA_CHAT (self));

  if (cm_room_set_read_marker_finish (self->cm_room, result, &error)) {
    self->unread_count = 0;
    g_signal_emit_by_name (self, "changed", 0);
  } else if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("Error updating read marker: %s", error->message);
}

static void
chatty_ma_chat_set_unread_count (ChattyChat *chat,
                                 guint       unread_count)
{
  ChattyMaChat *self = (ChattyMaChat *)chat;

  g_assert (CHATTY_IS_MA_CHAT (self));

  if (self->unread_count == unread_count)
    return;

  if (unread_count == 0) {
    g_autoptr(ChattyMessage) message = NULL;
    GListModel *model;
    guint n_items;

    model = G_LIST_MODEL (self->message_list);
    n_items = g_list_model_get_n_items (model);

    if (n_items == 0)
      return;

    message = g_list_model_get_item (model, n_items - 1);
    cm_room_set_read_marker_async (self->cm_room,
                                   chatty_message_get_uid (message),
                                   chatty_message_get_uid (message),
                                   chat_set_read_marker_cb, self);
  } else {
    self->unread_count = unread_count;
    chatty_chat_show_notification (CHATTY_CHAT (chat), NULL);
    g_signal_emit_by_name (self, "changed", 0);
  }
}

static void
ma_chage_send_message_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  ChattyMaChat *self;
  g_autoptr(GTask) task = user_data;
  g_autofree char *event_id = NULL;
  ChattyMessage *message;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  message = g_task_get_task_data (task);
  g_assert (CHATTY_IS_MA_CHAT (self));
  g_assert (CHATTY_IS_MESSAGE (message));

  event_id = cm_room_send_text_finish (self->cm_room, result, NULL);

  /* We add only failed to send messages.  If sending succeeded,
   * we shall get the same via the /sync responses */
  if (event_id) {
    chatty_message_set_status (message, CHATTY_STATUS_SENT, 0);
  } else {
    chatty_message_set_status (message, CHATTY_STATUS_SENDING_FAILED, 0);
  }
}

static void
chatty_ma_chat_send_message_async (ChattyChat          *chat,
                                   ChattyMessage       *message,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  ChattyMaChat *self = (ChattyMaChat *)chat;
  const char *event_id;
  GList *files = NULL;
  GFile *file = NULL;
  GTask *task;

  g_assert (CHATTY_IS_MA_CHAT (self));
  g_assert (CHATTY_IS_MESSAGE (message));

  chatty_message_set_user (message, CHATTY_ITEM (self->self_buddy));
  chatty_message_set_status (message, CHATTY_STATUS_SENDING, 0);

  task = g_task_new (self, NULL, callback, user_data);
  g_task_set_task_data (task, g_object_ref (message), g_object_unref);
  g_list_store_append (self->message_list, message);

  files = chatty_message_get_files (message);

  if (files) {
    ChattyFileInfo *file_info;
    file_info = files->data;

    if (file_info && file_info->path)
      file = g_file_new_for_path (file_info->path);

  }

  if (file)
    event_id = cm_room_send_file_async (self->cm_room, file, NULL,
                                        NULL, NULL, NULL,
                                        ma_chage_send_message_cb, task);
  else
    event_id = cm_room_send_text_async (self->cm_room,
                                        chatty_message_get_text (message),
                                        NULL,
                                        ma_chage_send_message_cb, task);
  g_object_set_data_full (G_OBJECT (message), "event-id", g_strdup (event_id), g_free);
}

static void
ma_download_file_stream_cb (GObject      *obj,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  ChattyMaChat *self;
  g_autoptr(GTask) task = user_data;
  ChattyMessage *message;
  ChattyFileInfo *file;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (CHATTY_IS_MA_CHAT (self));

  g_output_stream_splice_finish (G_OUTPUT_STREAM (obj), result, &error);
  message = g_object_get_data (user_data, "message");
  file = g_object_get_data (user_data, "file");

  if (error) {
    file->status = CHATTY_FILE_ERROR;
    g_task_return_error (task, error);
  } else {
    g_autofree char *file_name = NULL;
    g_autoptr(GFile) parent = NULL;
    GFile *out_file;

    out_file = g_object_get_data (user_data, "out-file");
    parent = g_file_new_build_filename (g_get_user_cache_dir (), "chatty", NULL);

    file_name = g_path_get_basename (file->url);
    file->path = g_file_get_relative_path (parent, out_file);
    file->status = CHATTY_FILE_DOWNLOADED;

    g_task_return_boolean (task, TRUE);
  }

  chatty_history_add_message (self->history_db, CHATTY_CHAT (self), message);
  chatty_message_emit_updated (message);
}

static void
ma_chat_download_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  ChattyMaChat *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GInputStream) istream = NULL;
  ChattyMessage *message;
  ChattyFileInfo *file;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (CHATTY_IS_MA_CHAT (self));

  file = g_object_get_data (G_OBJECT (task), "file");
  message = g_object_get_data (G_OBJECT (task), "message");
  g_return_if_fail (file);
  g_return_if_fail (message);

  istream = cm_client_get_file_finish (self->cm_client, result, &error);

  if (istream) {
    GOutputStream *out_stream;
    g_autofree char *file_name = NULL;
    GFile *out_file;

    file_name = g_path_get_basename (file->url);
    out_file = g_file_new_build_filename (g_get_user_cache_dir (), "chatty", "matrix",
                                          "files", file_name, NULL);
    out_stream = (GOutputStream *)g_file_create (out_file, G_FILE_CREATE_NONE, NULL, &error);
    g_object_set_data_full (user_data, "out-file", out_file, g_object_unref);

    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
      {
        GFileIOStream *io_stream;

        io_stream = g_file_open_readwrite (out_file, NULL, NULL);
        out_stream = g_io_stream_get_output_stream (G_IO_STREAM (io_stream));
      }

    if (out_stream) {
      g_output_stream_splice_async (G_OUTPUT_STREAM (out_stream), istream,
                                    G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
                                    G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                    0, NULL,
                                    ma_download_file_stream_cb,
                                    g_steal_pointer (&task));
    } else {
      g_task_return_boolean (task, FALSE);
    }
  } else {
      file->status = CHATTY_FILE_UNKNOWN;

    chatty_history_add_message (self->history_db, CHATTY_CHAT (self), message);
    chatty_message_emit_updated (message);
  }
}

static void
chatty_ma_chat_get_files_async (ChattyChat          *chat,
                                ChattyMessage       *message,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  ChattyMaChat *self = CHATTY_MA_CHAT (chat);
  ChattyFileInfo *file;
  GList *files;

  g_assert (CHATTY_IS_MA_CHAT (self));
  g_assert (CHATTY_IS_MESSAGE (message));

  task = g_task_new (self, NULL, callback, user_data);
  g_object_set_data_full (G_OBJECT (task), "message", g_object_ref (message), g_object_unref);

  files = chatty_message_get_files (message);
  if (!files)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                               "No file found in message");
      return;
    }


  file = files->data;
  if (!file->url || file->status != CHATTY_FILE_UNKNOWN)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                               "File URL missing or invalid file state");
      return;
    }

  g_object_set_data (G_OBJECT (task), "file", file);

  file->status = CHATTY_FILE_DOWNLOADING;
  chatty_message_emit_updated (message);
  cm_client_get_file_async (self->cm_client, file->url, NULL,
                            ma_chat_download_cb,
                            g_steal_pointer (&task));
}

static gboolean
chatty_ma_chat_get_buddy_typing (ChattyChat *chat)
{
  ChattyMaChat *self = (ChattyMaChat *)chat;

  g_assert (CHATTY_IS_MA_CHAT (self));

  return self->buddy_typing;
}

static void
chatty_ma_chat_set_typing (ChattyChat *chat,
                           gboolean    is_typing)
{
  ChattyMaChat *self = (ChattyMaChat *)chat;

  g_assert (CHATTY_IS_MA_CHAT (self));

  cm_room_set_typing_notice_async (self->cm_room, is_typing, NULL, NULL, NULL);
}

static void
chatty_ma_chat_show_notification (ChattyChat *chat,
                                  const char *name)
{
  ChattyMaChat *self = (ChattyMaChat *)chat;

  g_assert (CHATTY_IS_MA_CHAT (self));

  if (!self->unread_count || self->notification_shown)
    return;

  CHATTY_CHAT_CLASS (chatty_ma_chat_parent_class)->show_notification (chat, name);
}

static const char *
chatty_ma_chat_get_name (ChattyItem *item)
{
  ChattyMaChat *self = (ChattyMaChat *)item;
  const char *name;

  g_assert (CHATTY_IS_MA_CHAT (self));

  name = cm_room_get_name (self->cm_room);
  if (name && *name)
    return name;

  if (self->room_name)
    return self->room_name;

  if (!self->room_name_loaded)
    return self->room_id;

  if (!self->generated_name)
    chatty_mat_chat_update_name (self);

  if (self->generated_name)
    return self->generated_name;

  if (self->room_id)
    return self->room_id;

  return "";
}

static const char *
chatty_ma_chat_get_username (ChattyItem *item)
{
  ChattyMaChat *self = (ChattyMaChat *)item;

  g_assert (CHATTY_IS_MA_CHAT (self));

  return cm_client_get_user_id (self->cm_client);
}

static ChattyItemState
chatty_ma_chat_get_state (ChattyItem *item)
{
  ChattyMaChat *self = (ChattyMaChat *)item;

  g_assert (CHATTY_IS_MA_CHAT (self));

  return self->visibility_state;
}

static void
chatty_ma_chat_set_state (ChattyItem      *item,
                          ChattyItemState  state)
{
  ChattyMaChat *self = (ChattyMaChat *)item;

  g_assert (CHATTY_IS_MA_CHAT (self));

  self->visibility_state = state;
}

static ChattyProtocol
chatty_ma_chat_get_protocols (ChattyItem *item)
{
  return CHATTY_PROTOCOL_MATRIX;
}

static ChattyFileInfo *
chatty_ma_chat_get_avatar_file (ChattyItem *item)
{
  ChattyMaChat *self = (ChattyMaChat *)item;

  g_assert (CHATTY_IS_MA_CHAT (self));

  return self->avatar_file;
}

static GdkPixbuf *
chatty_ma_chat_get_avatar (ChattyItem *item)
{
  ChattyMaChat *self = (ChattyMaChat *)item;
  g_autofree char *path = NULL;

  g_assert (CHATTY_IS_MA_CHAT (self));

  /* If avatar is loading return the past avatar (which may be NULL) */
  if (self->avatar_is_loading || self->avatar)
    return self->avatar;

  if (!self->avatar_file || !self->avatar_file->path)
    return NULL;

  self->avatar_is_loading = TRUE;
  path = g_build_filename (g_get_user_cache_dir (), "chatty",
                           self->avatar_file->path, NULL);
  matrix_utils_get_pixbuf_async (path, self->avatar_cancellable,
                                 ma_chat_get_avatar_pixbuf_cb,
                                 g_object_ref (self));

  return NULL;
}

static void
chatty_ma_chat_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  ChattyMaChat *self = (ChattyMaChat *)object;

  switch (prop_id)
    {
    case PROP_JSON_DATA:
      matrix_chat_set_json_data (self, g_value_dup_boxed (value));
      break;

    case PROP_ROOM_ID:
      self->room_id = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
chatty_ma_chat_finalize (GObject *object)
{
  ChattyMaChat *self = (ChattyMaChat *)object;

  if (self->avatar_cancellable)
    g_cancellable_cancel (self->avatar_cancellable);
  g_clear_object (&self->avatar_cancellable);

  g_list_store_remove_all (self->message_list);
  g_list_store_remove_all (self->buddy_list);
  g_clear_object (&self->message_list);
  g_clear_object (&self->sorted_message_list);
  g_clear_object (&self->buddy_list);
  g_clear_object (&self->self_buddy);
  g_clear_pointer (&self->avatar_file, chatty_file_info_free);
  g_clear_object (&self->avatar);

  g_clear_object (&self->history_db);

  g_clear_pointer (&self->json_data, json_object_unref);

  g_free (self->room_name);
  g_free (self->generated_name);
  g_free (self->room_id);
  g_free (self->last_batch);

  G_OBJECT_CLASS (chatty_ma_chat_parent_class)->finalize (object);
}

static void
chatty_ma_chat_class_init (ChattyMaChatClass *klass)
{
  GObjectClass *object_class  = G_OBJECT_CLASS (klass);
  ChattyItemClass *item_class = CHATTY_ITEM_CLASS (klass);
  ChattyChatClass *chat_class = CHATTY_CHAT_CLASS (klass);

  object_class->set_property = chatty_ma_chat_set_property;
  object_class->finalize = chatty_ma_chat_finalize;

  item_class->get_name = chatty_ma_chat_get_name;
  item_class->get_username = chatty_ma_chat_get_username;
  item_class->get_state = chatty_ma_chat_get_state;
  item_class->set_state = chatty_ma_chat_set_state;
  item_class->get_protocols = chatty_ma_chat_get_protocols;
  item_class->get_avatar_file = chatty_ma_chat_get_avatar_file;
  item_class->get_avatar = chatty_ma_chat_get_avatar;

  chat_class->is_im = chatty_ma_chat_is_im;
  chat_class->has_file_upload = chatty_ma_chat_has_file_upload;
  chat_class->get_chat_name = chatty_ma_chat_get_chat_name;
  chat_class->load_past_messages = chatty_ma_chat_real_past_messages;
  chat_class->is_loading_history = chatty_ma_chat_is_loading_history;
  chat_class->get_messages = chatty_ma_chat_get_messages;
  chat_class->get_account  = chatty_ma_chat_get_account;
  chat_class->get_encryption = chatty_ma_chat_get_encryption;
  chat_class->set_encryption_async = chatty_ma_chat_set_encryption_async;
  chat_class->get_last_message = chatty_ma_chat_get_last_message;
  chat_class->get_unread_count = chatty_ma_chat_get_unread_count;
  chat_class->set_unread_count = chatty_ma_chat_set_unread_count;
  chat_class->send_message_async = chatty_ma_chat_send_message_async;
  chat_class->get_files_async = chatty_ma_chat_get_files_async;
  chat_class->get_buddy_typing = chatty_ma_chat_get_buddy_typing;
  chat_class->set_typing = chatty_ma_chat_set_typing;
  chat_class->show_notification = chatty_ma_chat_show_notification;

  properties[PROP_JSON_DATA] =
    g_param_spec_boxed ("json-data",
                        "json-data",
                        "json-data for the room",
                        JSON_TYPE_OBJECT,
                        G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_ROOM_ID] =
    g_param_spec_string ("room-id",
                         "json-data",
                         "json-data for the room",
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
chatty_ma_chat_init (ChattyMaChat *self)
{
  g_autoptr(GtkSorter) sorter = NULL;

  sorter = gtk_custom_sorter_new (sort_message, NULL, NULL);

  self->message_list = g_list_store_new (CHATTY_TYPE_MESSAGE);
  self->sorted_message_list = gtk_sort_list_model_new (G_LIST_MODEL (self->message_list), sorter);
  self->buddy_list = g_list_store_new (CHATTY_TYPE_MA_BUDDY);
  self->avatar_cancellable = g_cancellable_new ();
}

ChattyMaChat *
chatty_ma_chat_new (const char     *room_id,
                    const char     *name,
                    ChattyFileInfo *avatar,
                    gboolean        encrypted)
{
  ChattyMaChat *self;

  g_return_val_if_fail (room_id && *room_id, NULL);

  self = g_object_new (CHATTY_TYPE_MA_CHAT,
                       "room-id", room_id, NULL);
  self->room_name = g_strdup (name);
  self->avatar_file = avatar;

  return self;
}

static void
ma_chat_name_changed_cb (ChattyMaChat *self)
{
  g_assert (CHATTY_IS_MA_CHAT (self));

  g_object_notify (G_OBJECT (self), "name");
  g_signal_emit_by_name (self, "avatar-changed");
}

static void
ma_chat_encryption_changed_cb (ChattyMaChat *self)
{
  g_assert (CHATTY_IS_MA_CHAT (self));

  g_object_notify (G_OBJECT (self), "encrypt");
}

static void
joined_members_changed_cb (ChattyMaChat *self,
                           guint         position,
                           guint         removed,
                           guint         added,
                           GListModel   *model)
{
  g_autoptr(GPtrArray) items = NULL;

  g_assert (CHATTY_IS_CHAT (self));
  g_assert (G_IS_LIST_MODEL (model));

  for (guint i = position; i < position + added; i++)
    {
      g_autoptr(CmUser) user = NULL;
      ChattyMaBuddy *buddy;

      if (!items)
        items = g_ptr_array_new_with_free_func (g_object_unref);

      user = g_list_model_get_item (model, i);
      buddy = chatty_ma_buddy_new_with_user (user, self->cm_client);
      g_ptr_array_add (items, buddy);
    }

  g_list_store_splice (self->buddy_list, position, removed,
                       items ? items->pdata : NULL, added);
}

ChattyMaChat *
chatty_ma_chat_new_with_room (CmRoom *room)
{
  ChattyMaChat *self;
  GListModel *members;

  g_return_val_if_fail (CM_IS_ROOM (room), NULL);

  self = g_object_new (CHATTY_TYPE_MA_CHAT,
                       "room-id", cm_room_get_id (room), NULL);
  self->cm_room = g_object_ref (room);
  g_signal_connect_object (room, "notify::name",
                           G_CALLBACK (ma_chat_name_changed_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (room, "notify::encrypted",
                           G_CALLBACK (ma_chat_encryption_changed_cb),
                           self, G_CONNECT_SWAPPED);

  members = cm_room_get_joined_members (room);
  g_signal_connect_object (members, "items-changed",
                           G_CALLBACK (joined_members_changed_cb),
                           self, G_CONNECT_SWAPPED);
  joined_members_changed_cb (self, 0, 0,
                             g_list_model_get_n_items (members),
                             members);

  return self;
}

CmRoom *
chatty_ma_chat_get_cm_room (ChattyMaChat *self)
{
  g_return_val_if_fail (CHATTY_IS_MA_CHAT (self), FALSE);

  return self->cm_room;
}

void
chatty_ma_chat_set_history_db (ChattyMaChat *self,
                               gpointer      history_db)
{
  g_return_if_fail (CHATTY_IS_MA_CHAT (self));
  g_return_if_fail (CHATTY_IS_HISTORY (history_db));
  g_return_if_fail (!self->history_db);

  self->history_db = g_object_ref (history_db);
}

/**
 * chatty_ma_chat_set_data:
 * @self: a #ChattyMaChat
 * @account: A #ChattyMaAccount
 * @client: A #CmClient
 *
 * Use this function to set internal data required
 * to connect to a matrix server.
 */
void
chatty_ma_chat_set_data (ChattyMaChat  *self,
                         ChattyAccount *account,
                         gpointer       client)
{
  g_return_if_fail (CHATTY_IS_MA_CHAT (self));
  g_return_if_fail (CM_IS_CLIENT (client));

  g_set_weak_pointer (&self->account, account);
  g_set_object (&self->cm_client, client);
}

gboolean
chatty_ma_chat_matches_id (ChattyMaChat *self,
                           const char   *room_id)
{
  g_return_val_if_fail (CHATTY_IS_MA_CHAT (self), FALSE);

  if (!self->room_id)
    return FALSE;

  return g_strcmp0 (self->room_id, room_id) == 0;
}

void
chatty_ma_chat_add_messages (ChattyMaChat *self,
                             GPtrArray    *messages)
{
  g_return_if_fail (CHATTY_IS_MA_CHAT (self));

  if (messages && messages->len)
    g_list_store_splice (self->message_list, 0, 0,
                         messages->pdata, messages->len);
}
