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

#define CMATRIX_USE_EXPERIMENTAL_API
#include "cmatrix.h"

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

  CmMatrix      *cm_matrix;

  gboolean       disable_auto_login;
  gboolean       is_ready;
};

G_DEFINE_TYPE (ChattyMatrix, chatty_matrix, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ENABLED,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

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

  g_list_store_remove_all (self->list_of_chat_list);
  g_list_store_remove_all (self->account_list);

  g_clear_object (&self->account_list);
  g_clear_object (&self->list_of_chat_list);
  g_clear_object (&self->chat_list);

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
  GListModel *model;

  self->account_list = g_list_store_new (CHATTY_TYPE_ACCOUNT);
  self->list_of_chat_list = g_list_store_new (G_TYPE_LIST_MODEL);

  model = G_LIST_MODEL (self->list_of_chat_list);
  self->chat_list = gtk_flatten_list_model_new (g_object_ref (model));
}

ChattyMatrix *
chatty_matrix_new (gboolean disable_auto_login)
{
  ChattyMatrix *self;

  self = g_object_new (CHATTY_TYPE_MATRIX, NULL);
  self->disable_auto_login = !!disable_auto_login;

  return self;
}

gboolean
chatty_matrix_is_enabled (ChattyMatrix *self)
{
  g_return_val_if_fail (CHATTY_IS_MATRIX (self), FALSE);

  return self->is_ready;
}

static void
matrix_open_cb (GObject      *obj,
                GAsyncResult *result,
                gpointer      user_data)
{
  g_autoptr(ChattyMatrix) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (CHATTY_IS_MATRIX (self));
  g_assert (CM_IS_MATRIX (obj));

  cm_matrix_open_finish (CM_MATRIX (obj), result, &error);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ENABLED]);

  if (error)
    g_warning ("Error plugging in to the matrix: %s", error->message);
}

static void
matrix_client_list_changed_cb (ChattyMatrix *self,
                               int              position,
                               int              removed,
                               int              added,
                               GListModel      *model)
{
  g_autoptr(GPtrArray) items = NULL;

  g_assert (CHATTY_IS_MATRIX (self));
  g_assert (G_IS_LIST_MODEL (model));

  for (guint i = position; i < position + added; i++)
    {
      g_autoptr(CmClient) client = NULL;
      ChattyMaAccount *account;

      if (!items)
        items = g_ptr_array_new_with_free_func (g_object_unref);

      client = g_list_model_get_item (model, i);
      account = chatty_ma_account_new_from_client (client);
      g_list_store_append (self->list_of_chat_list,
                           chatty_ma_account_get_chat_list (account));
      g_signal_connect_object (account, "notify::status",
                               G_CALLBACK (matrix_ma_account_changed_cb),
                               self, G_CONNECT_SWAPPED);
      if (g_object_get_data (G_OBJECT (client), "enable"))
        cm_client_set_enabled (client, TRUE);
      g_ptr_array_add (items, account);
    }

  g_list_store_splice (self->account_list, position, removed,
                       items ? items->pdata : NULL, added);
}

void
chatty_matrix_load (ChattyMatrix *self)
{
  g_autofree char *db_path = NULL;
  g_autofree char *data_path = NULL;
  g_autofree char *cache_path = NULL;
  GListModel *client_list;

  if (self->cm_matrix)
    return;

  data_path = g_build_filename (g_get_user_data_dir (), "chatty", NULL);
  cache_path = g_build_filename (g_get_user_cache_dir (), "chatty", NULL);
  self->cm_matrix = cm_matrix_new (data_path, cache_path, "sm.puri.Chatty",
                                   self->disable_auto_login);
  client_list = cm_matrix_get_clients_list (self->cm_matrix);
  g_signal_connect_object (client_list, "items-changed",
                           G_CALLBACK (matrix_client_list_changed_cb),
                           self, G_CONNECT_SWAPPED);

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

  cm_matrix_delete_client_finish (self->cm_matrix, result, &error);

  username = chatty_item_get_username (CHATTY_ITEM (account));
  if (!username || !*username)
    username = chatty_ma_account_get_login_username (account);
  CHATTY_DEBUG_DETAILED (username, "Deleting %s, account:", CHATTY_LOG_SUCESS (!error));

  if (error)
    g_task_return_error (task, error);
}

void
chatty_matrix_delete_account_async (ChattyMatrix        *self,
                                    ChattyAccount       *account,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  GTask *task;

  g_return_if_fail (CHATTY_IS_MATRIX (self));
  g_return_if_fail (CHATTY_IS_MA_ACCOUNT (account));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_object_ref (account), g_object_unref);

  chatty_account_set_enabled (account, FALSE);

  CHATTY_DEBUG (chatty_item_get_username (CHATTY_ITEM (account)), "Deleting account");
  cm_matrix_delete_client_async (self->cm_matrix,
                                 chatty_ma_account_get_cm_client (CHATTY_MA_ACCOUNT (account)),
                                 matrix_account_delete_cb, task);
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
  ChattyMaAccount *account;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  const char *username;
  gboolean saved;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  account = g_task_get_task_data (task);
  g_assert (CHATTY_IS_MATRIX (self));
  g_assert (CHATTY_IS_MA_ACCOUNT (account));

  saved = cm_matrix_save_client_finish (self->cm_matrix, result, &error);

  username = chatty_item_get_username (CHATTY_ITEM (account));
  if (!username || !*username)
    username = chatty_ma_account_get_login_username (account);
  CHATTY_DEBUG_DETAILED (username, "Saving %s, account:", CHATTY_LOG_SUCESS (!error));

  if (error) {
    g_task_return_error (task, error);
    return;
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
  CmClient *cm_client;
  GTask *task;

  g_return_if_fail (CHATTY_IS_MATRIX (self));
  g_return_if_fail (CHATTY_IS_MA_ACCOUNT (account));

  CHATTY_DEBUG (chatty_item_get_username (CHATTY_ITEM (account)), "Saving account");

  cm_client = chatty_ma_account_get_cm_client (CHATTY_MA_ACCOUNT (account));
  g_return_if_fail (CM_IS_CLIENT (cm_client));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_object_ref (account), g_object_unref);
  cm_matrix_save_client_async (self->cm_matrix, cm_client,
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

  if (protocol != CHATTY_PROTOCOL_MATRIX)
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

gboolean
chatty_matrix_has_user_id (ChattyMatrix *self,
                           const char   *user_id)
{
  g_return_val_if_fail (CHATTY_IS_MATRIX (self), FALSE);

  return cm_matrix_has_client_with_id (self->cm_matrix, user_id);
}
