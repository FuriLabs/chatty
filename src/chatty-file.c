/* chatty-file.c
 *
 * Copyright 2023 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-file"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define CMATRIX_USE_EXPERIMENTAL_API
#include <cmatrix.h>

#include "chatty-enums.h"
#include "chatty-file.h"

/**
 * SECTION: chatty-file
 * @title: ChattyFile
 * @short_description: An abstraction for attachments
 * @include: "chatty-file.h"
 *
 * An abstraction for message attachments.
 */

struct _ChattyFile
{
  GObject             parent_instance;

  CmRoomMessageEvent *cm_event;
  GInputStream       *stream;
  GFile              *file;
  char               *file_name;
  char               *url;
  char               *path;
  char               *mime_type;

  gsize               size;
  /* for image and video files */
  gsize               width;
  gsize               height;
  /* for audio and video files */
  gsize               duration;

  ChattyFileStatus    status;
};

G_DEFINE_TYPE (ChattyFile, chatty_file, G_TYPE_OBJECT)

enum {
  STATUS_CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static void
chatty_file_finalize (GObject *object)
{
  ChattyFile *self = (ChattyFile *)object;

  g_clear_pointer (&self->file_name, g_free);
  g_clear_pointer (&self->mime_type, g_free);
  g_clear_pointer (&self->path, g_free);
  g_clear_pointer (&self->url, g_free);

  G_OBJECT_CLASS (chatty_file_parent_class)->finalize (object);
}

static void
chatty_file_class_init (ChattyFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = chatty_file_finalize;

  signals [STATUS_CHANGED] =
    g_signal_new ("status-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
chatty_file_init (ChattyFile *self)
{
}

ChattyFile *
chatty_file_new_for_cm_event (gpointer cm_event)
{
  ChattyFile *self;

  g_return_val_if_fail (CM_IS_ROOM_MESSAGE_EVENT (cm_event), NULL);

  self = g_object_new (CHATTY_TYPE_FILE, NULL);
  self->cm_event = g_object_ref (cm_event);

  return self;
}

ChattyFile *
chatty_file_new_for_path (const char *path)
{
  g_autoptr(ChattyFile) self = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFileInfo) file_info = NULL;

  self = g_object_new (CHATTY_TYPE_FILE, NULL);
  file = g_file_new_for_path (path);
  file_info = g_file_query_info (file,
                                 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE "," G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                 G_FILE_QUERY_INFO_NONE,
                                 NULL,
                                 &error);



  if (error) {
    g_warning ("Error getting file info: %s", error->message);
    return NULL;
  }

  /*
   *  https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types
   */
  self->file_name = g_file_get_basename (file);
  if (g_file_info_get_content_type (file_info))
    self->mime_type = g_content_type_get_mime_type (g_file_info_get_content_type (file_info));
  else
    self->mime_type = g_strdup ("application/octet-stream");


  if (!self->mime_type) {
    g_warning ("Could not get MIME type! Trying Content Type instead");
    if (g_file_info_get_content_type (file_info)) {
      self->mime_type = g_strdup (g_file_info_get_content_type (file_info));
    } else {
      g_warning ("Could not figure out Content Type! Using a Generic one");
      self->mime_type = g_strdup ("application/octet-stream");
    }
  }

  self->size = g_file_info_get_size (file_info);
  self->path = g_file_get_path (file);
  self->url  = g_file_get_uri (file);

  return g_steal_pointer (&self);
}

ChattyFile *
chatty_file_new_full (const char *file_name,
                      const char *url,
                      const char *path,
                      const char *mime_type,
                      gsize       size,
                      gsize       width,
                      gsize       height,
                      gsize       duration)
{
  ChattyFile *self;

  self = g_object_new (CHATTY_TYPE_FILE, NULL);
  self->file_name = g_strdup (file_name);
  self->url = g_strdup (url);
  self->path = g_strdup (path);
  self->mime_type = g_strdup (mime_type);

  self->duration = duration;
  self->height = height;
  self->width = width;
  self->size = size;

  return self;
}

const char *
chatty_file_get_name (ChattyFile *self)
{
  g_return_val_if_fail (CHATTY_IS_FILE (self), NULL);

  return self->file_name;
}

const char *
chatty_file_get_url (ChattyFile *self)
{
  g_return_val_if_fail (CHATTY_IS_FILE (self), NULL);

  return self->url;
}

const char *
chatty_file_get_path (ChattyFile *self)
{
  g_return_val_if_fail (CHATTY_IS_FILE (self), NULL);

  if (self->cm_event)
    return cm_room_message_event_get_file_path (self->cm_event);

  return self->path;
}

const char *
chatty_file_get_mime_type (ChattyFile *self)
{
  g_return_val_if_fail (CHATTY_IS_FILE (self), NULL);

  return self->mime_type;
}

gsize
chatty_file_get_size (ChattyFile *self)
{
  g_return_val_if_fail (CHATTY_IS_FILE (self), 0);

  return self->size;
}

void
chatty_file_set_status (ChattyFile       *self,
                        ChattyFileStatus  status)
{
  g_return_if_fail (CHATTY_IS_FILE (self));

  if (self->status == status)
    return;

  g_atomic_int_set (&self->status, status);
  g_signal_emit (self, signals[STATUS_CHANGED], 0);
}

ChattyFileStatus
chatty_file_get_status (ChattyFile *self)
{
  g_return_val_if_fail (CHATTY_IS_FILE (self), CHATTY_FILE_UNKNOWN);

  return self->status;
}

void
chatty_file_set_file (ChattyFile *self,
                      GFile      *file)
{
  g_return_if_fail (CHATTY_IS_FILE (self));
  g_return_if_fail (G_IS_FILE (file));

  g_set_object (&self->file, file);
}

GFile *
chatty_file_get_file (ChattyFile *self)
{
  g_return_val_if_fail (CHATTY_IS_FILE (self), NULL);

  return self->file;
}

static void
file_get_file_stream_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  ChattyFile *self;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (CHATTY_IS_FILE (self));

  g_clear_object (&self->stream);
  self->stream = cm_room_message_event_get_file_finish (self->cm_event, result, &error);

  if (self->stream)
    chatty_file_set_status (self, CHATTY_FILE_DOWNLOADED);
  else if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) ||
           g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NETWORK_UNREACHABLE) ||
           g_error_matches (error, G_IO_ERROR, G_IO_ERROR_HOST_UNREACHABLE))
    chatty_file_set_status (self, CHATTY_FILE_UNKNOWN);
  else
    chatty_file_set_status (self, CHATTY_FILE_ERROR);

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, g_object_ref (self->stream), g_object_unref);
}

void
chatty_file_get_stream_async (ChattyFile          *self,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (CHATTY_IS_FILE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  if (chatty_file_get_status (self) == CHATTY_FILE_ERROR) {
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "File download failed");
    return;
  }

  if (chatty_file_get_status (self) == CHATTY_FILE_DOWNLOADING) {
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_PENDING,
                             "File download already in progress");
    return;
  }

  if (self->stream) {
    g_seekable_seek (G_SEEKABLE (self->stream), 0, G_SEEK_SET, NULL, NULL);
    g_task_return_pointer (task, g_object_ref (self->stream), g_object_unref);
    return;
  }

  chatty_file_set_status (self, CHATTY_FILE_DOWNLOADING);

  if (self->cm_event) {
    cm_room_message_event_get_file_async (self->cm_event,
                                          cancellable,
                                          NULL, NULL,
                                          file_get_file_stream_cb,
                                          g_steal_pointer (&task));
  } else {
    GError *error = NULL;

    if (!self->file) {
      g_autofree char *path = NULL;

      path = g_build_filename (g_get_user_data_dir (), "chatty", self->path, NULL);
      self->file = g_file_new_for_path (path);
    }

    self->stream = (gpointer)g_file_read (self->file, NULL, &error);
    if (self->stream) {
      g_task_return_pointer (task, g_object_ref (self->stream), g_object_unref);
      chatty_file_set_status (self, CHATTY_FILE_DOWNLOADED);
    } else {
      chatty_file_set_status (self, CHATTY_FILE_ERROR);
      g_task_return_error (task, error);
    }
  }
}

GInputStream *
chatty_file_get_stream_finish (ChattyFile    *self,
                               GAsyncResult  *result,
                               GError       **error)
{
  g_return_val_if_fail (CHATTY_IS_FILE (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);
  g_return_val_if_fail (!error || !*error, NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

void
chatty_file_set_istream (ChattyFile   *self,
                         GInputStream *stream)
{
  g_return_if_fail (CHATTY_IS_FILE (self));
  g_return_if_fail (G_IS_INPUT_STREAM (stream));

  g_set_object (&self->stream, stream);
}

GInputStream *
chatty_file_get_istream (ChattyFile *self)
{
  g_return_val_if_fail (CHATTY_IS_FILE (self), NULL);

  return self->stream;
}
gsize
chatty_file_get_width (ChattyFile *self)
{
  g_return_val_if_fail (CHATTY_IS_FILE (self), 0);

  return self->width;
}

gsize
chatty_file_get_height (ChattyFile *self)
{
  g_return_val_if_fail (CHATTY_IS_FILE (self), 0);

  return self->height;
}

gsize
chatty_file_get_duration (ChattyFile *self)
{
  g_return_val_if_fail (CHATTY_IS_FILE (self), 0);

  return self->duration;
}
