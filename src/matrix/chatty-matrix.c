/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-matrix.c
 *
 * Copyright 2021 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-matrix"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "contrib/gtk.h"
#define CMATRIX_USE_EXPERIMENTAL_API
#include "cmatrix.h"

#include "chatty-secret-store.h"
#include "chatty-settings.h"
#include "chatty-ma-account.h"
#include "chatty-ma-buddy.h"
#include "chatty-ma-chat.h"
#include "chatty-matrix.h"
#include "chatty-log.h"


struct _ChattyMatrix
{
  GObject        parent_instance;

  GListStore    *account_list;
  GListStore    *list_of_chat_list;
  GtkFlattenListModel *chat_list;

  ChattyHistory *history;
  CmMatrix      *cm_matrix;

  /* The timeout id for the callback on network changes */
  guint          network_change_id;

  gboolean       has_loaded;
  gboolean       disable_auto_login;
  gboolean       network_available;
  gboolean       enabled;
  gboolean       is_ready;
};

G_DEFINE_TYPE (ChattyMatrix, chatty_matrix, G_TYPE_OBJECT)

#define RECONNECT_TIMEOUT    1000 /* milliseconds */

enum {
  PROP_0,
  PROP_ENABLED,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static gboolean
matrix_reconnect (gpointer user_data)
{
  ChattyMatrix *self = user_data;
  GListModel *list;
  guint n_items;

  self->network_change_id = 0;

  list = G_LIST_MODEL (self->account_list);
  n_items = g_list_model_get_n_items (list);

  for (guint i = 0; i < n_items; i++) {
    g_autoptr(ChattyAccount) account = NULL;

    account = g_list_model_get_item (list, i);

    if (chatty_ma_account_can_connect (CHATTY_MA_ACCOUNT (account)))
      chatty_account_connect (account, FALSE);
    else
      chatty_account_disconnect (account);
  }

  return G_SOURCE_REMOVE;
}

static void
matrix_network_changed_cb (ChattyMatrix    *self,
                           gboolean         network_available,
                           GNetworkMonitor *network_monitor)
{
  g_assert (CHATTY_IS_MATRIX (self));
  g_assert (G_IS_NETWORK_MONITOR (network_monitor));

  if (!self->has_loaded)
    return;

  g_clear_handle_id (&self->network_change_id, g_source_remove);
  self->network_change_id = g_timeout_add (RECONNECT_TIMEOUT,
                                           matrix_reconnect, self);

  g_log (G_LOG_DOMAIN, CHATTY_LOG_LEVEL_TRACE,
         "Network changed, has network: %s", CHATTY_LOG_BOOL (network_available));
}

static void
matrix_ma_account_changed_cb (ChattyMatrix    *self,
                              GParamSpec      *pspec,
                              ChattyMaAccount *account)
{
  guint position;

  g_assert (CHATTY_IS_MATRIX (self));
  g_assert (CHATTY_IS_MA_ACCOUNT (account));

  if (chatty_utils_get_item_position (G_LIST_MODEL (self->list_of_chat_list),
                                      chatty_ma_account_get_chat_list (account),
                                      &position))
    g_list_model_items_changed (G_LIST_MODEL (self->list_of_chat_list), position, 1, 1);
}

static void
matrix_secret_load_cb (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  ChattyMatrix *self = user_data;
  g_autoptr(GPtrArray) accounts = NULL;
  g_autoptr(GPtrArray) secrets = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (CHATTY_IS_MATRIX (self));

  secrets = chatty_secret_load_finish (result, &error);
  g_info ("Loading accounts from secrets %s", CHATTY_LOG_SUCESS (!error));

  if (error)
    g_warning ("Error loading secret accounts: %s", error->message);

  for (guint i = 0; secrets && i < secrets->len; i++)
    {
      ChattyMaAccount *account;

      if (!accounts)
        accounts = g_ptr_array_new_full (5, g_object_unref);

      account = chatty_ma_account_new_secret (secrets->pdata[i], self->cm_matrix);

      if (account)
        g_ptr_array_insert (accounts, -1, account);
    }

  if (!accounts)
    return;

  g_info ("Loaded %d matrix accounts", accounts ? accounts->len : 0);

  for (guint i = 0; accounts && i < accounts->len; i++) {
    g_signal_connect_object (accounts->pdata[i], "notify::status",
                             G_CALLBACK (matrix_ma_account_changed_cb),
                             self, G_CONNECT_SWAPPED);
    chatty_ma_account_set_db (accounts->pdata[i], self->history);

    g_list_store_append (self->list_of_chat_list,
                         chatty_ma_account_get_chat_list (accounts->pdata[i]));
  }

  g_list_store_splice (self->account_list, 0, 0, accounts->pdata, accounts->len);

}

static void
chatty_matrix_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  ChattyMatrix *self = (ChattyMatrix *)object;

  switch (prop_id)
    {
    case PROP_ENABLED:
      g_value_set_boolean (value, chatty_matrix_is_enabled (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
chatty_matrix_finalize (GObject *object)
{
  ChattyMatrix *self = (ChattyMatrix *)object;

  g_clear_handle_id (&self->network_change_id, g_source_remove);

  g_list_store_remove_all (self->list_of_chat_list);
  g_list_store_remove_all (self->account_list);

  g_clear_object (&self->account_list);
  g_clear_object (&self->list_of_chat_list);
  g_clear_object (&self->chat_list);

  g_clear_object (&self->history);

  G_OBJECT_CLASS (chatty_matrix_parent_class)->finalize (object);
}

static void
chatty_matrix_class_init (ChattyMatrixClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = chatty_matrix_finalize;
  object_class->get_property = chatty_matrix_get_property;

  /**
   * ChattyMatrix:enabled:
   *
   * Whether matrix is enabled and usable
   */
  properties[PROP_ENABLED] =
    g_param_spec_boolean ("enabled",
                          "matrix enabled",
                          "matrix enabled",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
chatty_matrix_init (ChattyMatrix *self)
{
  GNetworkMonitor *network_monitor;
  GListModel *model;

  self->account_list = g_list_store_new (CHATTY_TYPE_ACCOUNT);
  self->list_of_chat_list = g_list_store_new (G_TYPE_LIST_MODEL);

  model = G_LIST_MODEL (self->list_of_chat_list);
  self->chat_list = gtk_flatten_list_model_new (CHATTY_TYPE_CHAT, model);

  network_monitor = g_network_monitor_get_default ();
  self->network_available = g_network_monitor_get_network_available (network_monitor);

  g_signal_connect_object (network_monitor, "network-changed",
                           G_CALLBACK (matrix_network_changed_cb), self,
                           G_CONNECT_AFTER | G_CONNECT_SWAPPED);

  self->enabled = chatty_settings_get_experimental_features (chatty_settings_get_default ());
}

ChattyMatrix *
chatty_matrix_new (ChattyHistory *history,
                   gboolean       disable_auto_login)
{
  ChattyMatrix *self;

  g_return_val_if_fail (CHATTY_IS_HISTORY (history), NULL);

  self = g_object_new (CHATTY_TYPE_MATRIX, NULL);
  self->history = g_object_ref (history);
  self->disable_auto_login = !!disable_auto_login;

  return self;
}

gboolean
chatty_matrix_is_enabled (ChattyMatrix *self)
{
  g_return_val_if_fail (CHATTY_IS_MATRIX (self), FALSE);

  return self->enabled && self->is_ready;
}

static void
matrix_open_cb (GObject      *obj,
                GAsyncResult *result,
                gpointer      user_data)
{
  g_autoptr(ChattyMatrix) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (CHATTY_IS_MATRIX (self));

  cm_matrix_open_finish (self->cm_matrix, result, &error);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ENABLED]);

  if (error) {
    g_warning ("Error plugging in to the matrix: %s", error->message);
    return;
  }

  chatty_secret_load_async (NULL, matrix_secret_load_cb, self);
}

void
chatty_matrix_load (ChattyMatrix *self)
{
  g_autofree char *db_path = NULL;
  g_autofree char *data_path = NULL;
  g_autofree char *cache_path = NULL;

  if (self->cm_matrix)
    return;

  if (!chatty_settings_get_experimental_features (chatty_settings_get_default ()))
    return;

  data_path = g_build_filename (g_get_user_data_dir (), "chatty", "matrix", NULL);
  cache_path = g_build_filename (g_get_user_cache_dir (), "chatty", "matrix", NULL);
  self->cm_matrix = g_object_new (CM_TYPE_MATRIX, NULL);

  db_path = g_build_filename (chatty_utils_get_purple_dir (), "chatty", "db", NULL);
  cm_matrix_open_async (self->cm_matrix, db_path, "matrix.db", NULL,
                        matrix_open_cb, g_object_ref (self));
}

GListModel *
chatty_matrix_get_account_list (ChattyMatrix *self)
{
  g_return_val_if_fail (CHATTY_IS_MATRIX (self), NULL);

  return G_LIST_MODEL (self->account_list);
}

GListModel *
chatty_matrix_get_chat_list (ChattyMatrix *self)
{
  g_return_val_if_fail (CHATTY_IS_MATRIX (self), NULL);

  return G_LIST_MODEL (self->chat_list);

}

static void
matrix_account_delete_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  ChattyMatrix *self;
  ChattyMaAccount *account;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;
  const char *username;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  account = g_task_get_task_data (task);

  g_assert (CHATTY_IS_MATRIX (self));
  g_assert (CHATTY_IS_MA_ACCOUNT (account));

  chatty_secret_delete_finish (result, &error);

  username = chatty_item_get_username (CHATTY_ITEM (account));
  if (!username || !*username)
    username = chatty_ma_account_get_login_username (account);
  CHATTY_DEBUG_DETAILED (username, "Deleting %s, account:", CHATTY_LOG_SUCESS (!error));

  if (error)
    g_task_return_error (task, error);
  else
    chatty_account_delete (CHATTY_ACCOUNT (account));
}

void
chatty_matrix_delete_account_async (ChattyMatrix        *self,
                                    ChattyAccount       *account,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  GListModel *chat_list;
  GTask *task;

  g_return_if_fail (CHATTY_IS_MATRIX (self));
  g_return_if_fail (CHATTY_IS_MA_ACCOUNT (account));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_object_ref (account), g_object_unref);

  chat_list = chatty_ma_account_get_chat_list (CHATTY_MA_ACCOUNT (account));
  chatty_utils_remove_list_item (self->list_of_chat_list, chat_list);
  chatty_utils_remove_list_item (self->account_list, account);
  chatty_account_set_enabled (account, FALSE);

  CHATTY_DEBUG (chatty_item_get_username (CHATTY_ITEM (account)), "Deleting account");

  chatty_secret_delete_async (account, NULL, matrix_account_delete_cb, task);
}

gboolean
chatty_matrix_delete_account_finish (ChattyMatrix  *self,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  g_return_val_if_fail (CHATTY_IS_MATRIX (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}


static void
matrix_save_account_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  ChattyMatrix *self;
  ChattyMaAccount *account = (gpointer)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  const char *username;
  gboolean saved;

  g_assert (G_IS_TASK (task));
  g_assert (CHATTY_IS_MA_ACCOUNT (account));

  self = g_task_get_source_object (task);
  g_assert (CHATTY_IS_MATRIX (self));

  saved = chatty_ma_account_save_finish (account, result, &error);

  username = chatty_item_get_username (CHATTY_ITEM (account));
  if (!username || !*username)
    username = chatty_ma_account_get_login_username (account);
  CHATTY_DEBUG_DETAILED (username, "Saving %s, account:", CHATTY_LOG_SUCESS (!error));

  if (error) {
    g_task_return_error (task, error);
    return;
  }

  if (saved) {
    g_list_store_append (self->account_list, account);
    g_list_store_append (self->list_of_chat_list,
                         chatty_ma_account_get_chat_list (account));

    g_signal_connect_object (account, "notify::status",
                             G_CALLBACK (matrix_ma_account_changed_cb),
                             self, G_CONNECT_SWAPPED);

    if (!self->disable_auto_login)
      chatty_account_set_enabled (CHATTY_ACCOUNT (account), TRUE);
  }

  g_task_return_boolean (task, saved);
}

void
chatty_matrix_save_account_async (ChattyMatrix        *self,
                                  ChattyAccount       *account,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  GTask *task;

  g_return_if_fail (CHATTY_IS_MATRIX (self));
  g_return_if_fail (CHATTY_IS_MA_ACCOUNT (account));

  CHATTY_DEBUG (chatty_item_get_username (CHATTY_ITEM (account)), "Saving account");

  task = g_task_new (self, cancellable, callback, user_data);
  chatty_ma_account_set_matrix (CHATTY_MA_ACCOUNT (account), self->cm_matrix);
  chatty_ma_account_set_db (CHATTY_MA_ACCOUNT (account), self->history);
  chatty_ma_account_save_async (CHATTY_MA_ACCOUNT (account), TRUE, NULL,
                                matrix_save_account_cb, task);
}

gboolean
chatty_matrix_save_account_finish (ChattyMatrix  *self,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  g_return_val_if_fail (CHATTY_IS_MATRIX (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

ChattyAccount *
chatty_matrix_find_account_with_name  (ChattyMatrix *self,
                                       const char   *account_id)
{
  GListModel *account_list;
  guint n_items;

  g_return_val_if_fail (CHATTY_IS_MATRIX (self), NULL);
  g_return_val_if_fail (account_id && *account_id, NULL);

  account_list = G_LIST_MODEL (self->account_list);
  n_items = g_list_model_get_n_items (account_list);

  for (guint i = 0; i < n_items; i++) {
    g_autoptr(ChattyMaAccount) account = NULL;

    account = g_list_model_get_item (account_list, i);

    /* The account matches if either login name or username matches */
    if (g_strcmp0 (chatty_ma_account_get_login_username (account), account_id) == 0 ||
        g_strcmp0 (chatty_item_get_username (CHATTY_ITEM (account)), account_id) == 0)
      return CHATTY_ACCOUNT (account);
  }

  return NULL;
}

ChattyChat *
chatty_matrix_find_chat_with_name (ChattyMatrix   *self,
                                   ChattyProtocol  protocol,
                                   const char     *account_id,
                                   const char     *chat_id)
{
  GListModel *accounts, *chat_list;
  guint n_accounts, n_items;

  g_return_val_if_fail (CHATTY_IS_MATRIX (self), NULL);
  g_return_val_if_fail (account_id && *account_id, NULL);
  g_return_val_if_fail (chat_id && *chat_id, NULL);

  if (!chatty_settings_get_experimental_features (chatty_settings_get_default ()) ||
      protocol != CHATTY_PROTOCOL_MATRIX)
    return NULL;

  accounts = G_LIST_MODEL (self->account_list);
  n_accounts = g_list_model_get_n_items (accounts);

  for (guint i = 0; i < n_accounts; i++) {
    g_autoptr(ChattyAccount) account = NULL;

    account = g_list_model_get_item (accounts, i);

    if (g_strcmp0 (chatty_item_get_username (CHATTY_ITEM (account)), account_id) != 0)
      continue;

    chat_list = chatty_ma_account_get_chat_list (CHATTY_MA_ACCOUNT (account));
    n_items = g_list_model_get_n_items (chat_list);

    for (guint j = 0; j < n_items; j++) {
      g_autoptr(ChattyMaChat) chat = NULL;

      chat = g_list_model_get_item (chat_list, j);

      if (chatty_ma_chat_matches_id (chat, chat_id))
        return CHATTY_CHAT (chat);
    }
  }

  return NULL;
}

CmClient *
chatty_matrix_client_new (ChattyMatrix *self)
{
  g_return_val_if_fail (CHATTY_IS_MATRIX (self), NULL);

  return cm_matrix_client_new (self->cm_matrix);
}
