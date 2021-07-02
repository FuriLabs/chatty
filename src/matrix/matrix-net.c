/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* matrix-api.c
 *
 * Copyright 2021 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-matrix-net"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <libsoup/soup.h>
#include <json-glib/json-glib.h>

#include "matrix-utils.h"
#include "matrix-enums.h"
#include "matrix-net.h"
#include "chatty-log.h"

/**
 * SECTION: matrix-net
 * @title: MatrixNet
 * @short_description: Matrix Network related methods
 * @include: "matrix-net.h"
 */

#define MAX_CONNECTIONS     6

struct _MatrixNet
{
  GObject         parent_instance;

  SoupSession    *soup_session;
  GCancellable   *cancellable;
  char           *homeserver;
  char           *access_token;

  guint           full_state_loaded : 1;
  guint           is_sync : 1;

  /* Set when error occurs with sync enabled */
  guint           sync_failed : 1;
  guint           homeserver_verified : 1;
  guint           login_success : 1;
  guint           room_list_loaded : 1;

  guint           resync_id;
};


G_DEFINE_TYPE (MatrixNet, matrix_net, G_TYPE_OBJECT)

static void
net_load_from_stream_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  MatrixNet *self;
  JsonParser *parser = JSON_PARSER (object);
  g_autoptr(GTask) task = user_data;
  JsonNode *root = NULL;
  GError *error = NULL;

  g_assert (JSON_IS_PARSER (parser));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (MATRIX_IS_NET (self));

  json_parser_load_from_stream_finish (parser, result, &error);

  if (!error) {
    root = json_parser_get_root (parser);
    error = matrix_utils_json_node_get_error (root);
  }

  if (error) {
    if (g_error_matches (error, MATRIX_ERROR, M_LIMIT_EXCEEDED) &&
        root &&
        JSON_NODE_HOLDS_OBJECT (root)) {
      JsonObject *obj;
      guint retry;

      obj = json_node_get_object (root);
      retry = matrix_utils_json_object_get_int (obj, "retry_after_ms");
      g_object_set_data (G_OBJECT (task), "retry-after", GINT_TO_POINTER (retry));
    } else {
      CHATTY_DEBUG_MSG ("Error loading from stream: %s", error->message);
    }

    g_task_return_error (task, error);
    return;
  }

  if (JSON_NODE_HOLDS_OBJECT (root))
    g_task_return_pointer (task, json_node_dup_object (root),
                           (GDestroyNotify)json_object_unref);
  else if (JSON_NODE_HOLDS_ARRAY (root))
    g_task_return_pointer (task, json_node_dup_array (root),
                           (GDestroyNotify)json_array_unref);
  else
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                             "Received invalid data");
}

static void
session_send_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  MatrixNet *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GInputStream) stream = NULL;
  g_autoptr(JsonParser) parser = NULL;
  GCancellable *cancellable;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (MATRIX_IS_NET (self));

  stream = soup_session_send_finish (self->soup_session, result, &error);

  if (error) {
    CHATTY_TRACE_MSG ("Error session send: %s", error->message);
    g_task_return_error (task, error);
    return;
  }

  cancellable = g_task_get_cancellable (task);
  parser = json_parser_new ();
  json_parser_load_from_stream_async (parser, stream, cancellable,
                                      net_load_from_stream_cb,
                                      g_steal_pointer (&task));
}

/*
 * queue_data:
 * @data: (transfer full)
 * @size: non-zero if @data is not %NULL
 * @task: (transfer full)
 */
static void
queue_data (MatrixNet  *self,
            char       *data,
            gsize       size,
            const char *uri_path,
            const char *method, /* interned */
            GHashTable *query,
            GTask      *task)
{
  g_autoptr(SoupMessage) message = NULL;
  g_autoptr(SoupURI) uri = NULL;
  GCancellable *cancellable;

  g_assert (MATRIX_IS_NET (self));
  g_assert (uri_path && *uri_path);
  g_assert (method && *method);
  g_return_if_fail (self->homeserver && *self->homeserver);

  g_assert (method == SOUP_METHOD_GET ||
            method == SOUP_METHOD_POST ||
            method == SOUP_METHOD_PUT);

  uri = soup_uri_new (self->homeserver);
  soup_uri_set_path (uri, uri_path);

  if (self->access_token) {
    if (!query)
      query = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

    g_hash_table_replace (query, g_strdup ("access_token"), self->access_token);
    soup_uri_set_query_from_form (uri, query);
  }

  message = soup_message_new_from_uri (method, uri);
  soup_message_headers_append (message->request_headers, "Accept-Encoding", "gzip");

  if (data)
    soup_message_set_request (message, "application/json", SOUP_MEMORY_TAKE, data, size);

  cancellable = g_task_get_cancellable (task);
  g_task_set_task_data (task, g_object_ref (message), g_object_unref);
  soup_session_send_async (self->soup_session, message, cancellable,
                           session_send_cb, task);
}

static void
matrix_net_finalize (GObject *object)
{
  MatrixNet *self = (MatrixNet *)object;

  if (self->cancellable)
    g_cancellable_cancel (self->cancellable);

  soup_session_abort (self->soup_session);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->soup_session);

  G_OBJECT_CLASS (matrix_net_parent_class)->finalize (object);
}

static void
matrix_net_class_init (MatrixNetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = matrix_net_finalize;
}

static void
matrix_net_init (MatrixNet *self)
{
  self->soup_session = g_object_new (SOUP_TYPE_SESSION,
                                     "max-conns-per-host", MAX_CONNECTIONS,
                                     NULL);
  self->cancellable = g_cancellable_new ();
}

MatrixNet *
matrix_net_new (void)
{
  return g_object_new (MATRIX_TYPE_NET, NULL);
}

void
matrix_net_set_homeserver (MatrixNet  *self,
                           const char *homeserver)
{
  g_return_if_fail (MATRIX_IS_NET (self));
  g_return_if_fail (homeserver && *homeserver);

  g_free (self->homeserver);
  self->homeserver = g_strdup (homeserver);
}

void
matrix_net_set_access_token (MatrixNet  *self,
                             const char *access_token)
{
  g_return_if_fail (MATRIX_IS_NET (self));

  g_free (self->access_token);
  self->access_token = g_strdup (access_token);
}

void
matrix_net_send_data_async (MatrixNet           *self,
                            char                *data,
                            gsize                size,
                            const char          *uri_path,
                            const char          *method, /* interned */
                            GHashTable          *query,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  GTask *task;

  g_return_if_fail (MATRIX_IS_NET (self));
  g_return_if_fail (uri_path && *uri_path);
  g_return_if_fail (method && *method);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback);
  g_return_if_fail (self->homeserver && *self->homeserver);

  if (data && *data)
    g_return_if_fail (size);

  /* if (callback == matrix_take_red_pill_cb) */
  /*   soup_message_set_priority (message, SOUP_MESSAGE_PRIORITY_VERY_HIGH); */

  if (!cancellable)
    cancellable = self->cancellable;

  task = g_task_new (self, cancellable, callback, user_data);
  queue_data (self, data, size, uri_path, method, query, task);
}

void
matrix_net_send_json_async (MatrixNet           *self,
                            JsonObject          *object,
                            const char          *uri_path,
                            const char          *method, /* interned */
                            GHashTable          *query,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  GTask *task;
  char *data = NULL;
  gsize size = 0;

  g_return_if_fail (MATRIX_IS_NET (self));
  g_return_if_fail (uri_path && *uri_path);
  g_return_if_fail (method && *method);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback);
  g_return_if_fail (self->homeserver && *self->homeserver);

  if (object) {
    data = matrix_utils_json_object_to_string (object, FALSE);
    g_object_unref (object);
  }

  if (data && *data)
    size = strlen (data);

  if (!cancellable)
    cancellable = self->cancellable;

  task = g_task_new (self, cancellable, callback, user_data);
  queue_data (self, data, size, uri_path, method, query, task);
}
