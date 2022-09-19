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
  char                *room_id;
  GdkPixbuf           *avatar;
  ChattyFileInfo      *avatar_file;
  GCancellable        *avatar_cancellable;
  ChattyMaBuddy       *self_buddy;
  GListStore          *buddy_list;
  GListStore          *message_list;
  GtkFilterListModel  *filtered_event_list;

  ChattyAccount    *account;
  CmClient         *cm_client;
  CmRoom           *cm_room;

  ChattyItemState visibility_state;
  int             unread_count;

  guint          notification_shown : 1;

  guint          history_is_loading : 1;
  guint          avatar_is_loading : 1;

  guint          room_name_loaded : 1;
  guint          buddy_typing : 1;
};

G_DEFINE_TYPE (ChattyMaChat, chatty_ma_chat, CHATTY_TYPE_CHAT)

enum {
  PROP_0,
  PROP_ROOM_ID,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static gboolean
ma_chat_filter_event_list (ChattyMessage *message,
                           ChattyMaChat  *self)
{
  g_assert (CHATTY_IS_MESSAGE (message));
  g_assert (CHATTY_IS_MA_CHAT (self));

  return !!chatty_message_get_cm_event (message);
}

static ChattyMaBuddy *
ma_chat_find_cm_user (ChattyMaChat *self,
                      GListModel   *model,
                      CmUser       *user,
                      guint        *index,
                      gboolean      add_if_missing)
{
  guint n_items;

  g_assert (CHATTY_IS_MA_CHAT (self));
  g_assert (G_IS_LIST_MODEL (model));

  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++) {
    g_autoptr(ChattyMaBuddy) buddy = NULL;

    buddy = g_list_model_get_item (model, i);
    if (chatty_ma_buddy_matches_cm_user (buddy, user)) {
      if (index)
        *index = i;

      return g_steal_pointer (&buddy);
    }
  }

  if (add_if_missing)
    {
      ChattyMaBuddy *buddy;

      buddy = chatty_ma_buddy_new_with_user (user);
      g_list_store_append (self->buddy_list, buddy);

      return buddy;
    }

  return NULL;
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

static void
ma_chat_get_past_events_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  g_autoptr(ChattyMaChat) self = user_data;
  /* g_autoptr(GPtrArray) events = NULL; */
  g_autoptr(GError) error = NULL;

  g_assert (CHATTY_IS_MA_CHAT (self));

  cm_room_load_past_events_finish (CM_ROOM (object), result, &error);

  self->history_is_loading = FALSE;
  g_object_notify (G_OBJECT (self), "loading-history");

  if (error)
    g_warning ("Error: %s", error->message);
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

  g_assert (CHATTY_IS_MA_CHAT (self));
  g_assert (count > 0);

  CHATTY_TRACE (self->room_id, "Loading %d past messages from", count);

  self->history_is_loading = TRUE;
  g_object_notify (G_OBJECT (self), "loading-history");

  cm_room_load_past_events_async (self->cm_room,
                                  ma_chat_get_past_events_cb,
                                  g_object_ref (self));
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

  return G_LIST_MODEL (self->filtered_event_list);
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

  model = G_LIST_MODEL (self->filtered_event_list);
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

  return cm_room_get_unread_notification_counts (self->cm_room);
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

  if (unread_count == 0) {
    g_autoptr(CmEvent) event = NULL;
    GListModel *model;
    guint n_items;

    model = cm_room_get_events_list (self->cm_room);
    n_items = g_list_model_get_n_items (model);

    if (n_items == 0)
      return;

    event = g_list_model_get_item (model, n_items - 1);
    cm_room_set_read_marker_async (self->cm_room,
                                   event, event,
                                   chat_set_read_marker_cb, self);
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
ma_chat_download_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  ChattyMaChat *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GInputStream) istream = NULL;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (CHATTY_IS_MA_CHAT (self));

  istream = chatty_message_get_file_stream_finish (CHATTY_MESSAGE (object), result, &error);
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
  if (file->status != CHATTY_FILE_UNKNOWN)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                               "File URL missing or invalid file state");
      return;
    }

  g_object_set_data (G_OBJECT (task), "file", file);

  chatty_message_get_file_stream_async (message, NULL, CHATTY_PROTOCOL_MATRIX, NULL,
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

  if (name && cm_room_get_past_name (self->cm_room)) {
    g_free (self->room_name);
    self->room_name = g_strdup_printf ("%s (was %s)", name,
                                       cm_room_get_past_name (self->cm_room));
  }

  if (self->room_name)
    return self->room_name;

  if (name && *name)
    return name;

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
  g_clear_object (&self->buddy_list);
  g_clear_object (&self->self_buddy);
  g_clear_pointer (&self->avatar_file, chatty_file_info_free);
  g_clear_object (&self->avatar);

  g_free (self->room_name);
  g_free (self->room_id);

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

  properties[PROP_ROOM_ID] =
    g_param_spec_string ("room-id",
                         "room id",
                         "Matrix room id the room",
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
chatty_ma_chat_init (ChattyMaChat *self)
{
  g_autoptr(GtkFilter) filter = NULL;

  self->message_list = g_list_store_new (CHATTY_TYPE_MESSAGE);

  filter = gtk_custom_filter_new ((GtkCustomFilterFunc)ma_chat_filter_event_list,
                                  g_object_ref (self),
                                  g_object_unref);
  self->filtered_event_list = gtk_filter_list_model_new (G_LIST_MODEL (self->message_list),
                                                         filter);
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
      buddy = chatty_ma_buddy_new_with_user (user);
      g_ptr_array_add (items, buddy);
    }

  g_list_store_splice (self->buddy_list, position, removed,
                       items ? items->pdata : NULL, added);
}

static void
events_list_changed_cb (ChattyMaChat *self,
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
      g_autoptr(CmEvent) event = NULL;
      ChattyMessage *message;

      if (!items)
        items = g_ptr_array_new_with_free_func (g_object_unref);

      event = g_list_model_get_item (model, i);

      if (CM_IS_ROOM_MESSAGE_EVENT (event) ||
          cm_event_is_encrypted (event)) {
        g_autoptr(ChattyMaBuddy) user = NULL;

        user = ma_chat_find_cm_user (self, G_LIST_MODEL (self->buddy_list),
                                     cm_event_get_sender (event), NULL, TRUE);
        message = chatty_message_new_from_event (CHATTY_ITEM (user), event);
      } else {
        message = g_object_new (CHATTY_TYPE_MESSAGE, NULL);
      }

      g_object_set_data_full (G_OBJECT (message), "cm-event", g_object_ref (event), g_object_unref);
      g_ptr_array_add (items, message);
    }

  g_list_store_splice (self->message_list, position, removed,
                       items ? items->pdata : NULL, added);
  g_signal_emit_by_name (self, "changed", 0);
}

ChattyMaChat *
chatty_ma_chat_new_with_room (CmRoom *room)
{
  ChattyMaChat *self;
  GListModel *members, *events;

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

  events = cm_room_get_events_list (room);
  g_signal_connect_object (events, "items-changed",
                           G_CALLBACK (events_list_changed_cb),
                           self, G_CONNECT_SWAPPED);
  events_list_changed_cb (self, 0, 0,
                          g_list_model_get_n_items (events),
                          events);

  return self;
}

CmRoom *
chatty_ma_chat_get_cm_room (ChattyMaChat *self)
{
  g_return_val_if_fail (CHATTY_IS_MA_CHAT (self), FALSE);

  return self->cm_room;
}

gboolean
chatty_ma_chat_can_set_encryption (ChattyMaChat *self)
{
  g_return_val_if_fail (CHATTY_IS_MA_CHAT (self), FALSE);

  if (!self->cm_room)
    return FALSE;

  return cm_room_self_has_power_for_event (self->cm_room, CM_M_ROOM_ENCRYPTION);
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
                             GPtrArray    *messages,
                             gboolean      append)
{
  guint position = 0;

  g_return_if_fail (CHATTY_IS_MA_CHAT (self));

  if (append)
    position = g_list_model_get_n_items (G_LIST_MODEL (self->message_list));

  if (messages && messages->len)
    g_list_store_splice (self->message_list, position, 0,
                         messages->pdata, messages->len);
}
