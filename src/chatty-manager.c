/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-manager.c
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-manager"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define LIBFEEDBACK_USE_UNSTABLE_API
#include <libfeedback.h>
#include <glib/gi18n.h>

#include "chatty-settings.h"
#include "contrib/gtk.h"
#include "chatty-contact-provider.h"
#include "chatty-utils.h"
#include "chatty-application.h"
#include "chatty-window.h"
#include "chatty-chat-view.h"
#include "users/chatty-mm-account.h"
#include "users/chatty-pp-account.h"
#include "matrix/chatty-ma-account.h"
#include "matrix/chatty-ma-chat.h"
#include "matrix/matrix-db.h"
#include "chatty-secret-store.h"
#include "chatty-chat.h"
#include "chatty-mm-chat.h"
#include "chatty-pp-chat.h"
#include "chatty-history.h"
#include "chatty-purple.h"
#include "chatty-manager.h"
#include "chatty-log.h"

/**
 * SECTION: chatty-manager
 * @title: ChattyManager
 * @short_description: A class to manage various providers and accounts
 * @include: "chatty-manager.h"
 */

struct _ChattyManager
{
  GObject          parent_instance;

  ChattyHistory   *history;
  ChattyEds       *chatty_eds;

  GtkFlattenListModel *accounts;
  GListStore          *account_list;
  GListStore          *list_of_account_list;

  GListStore      *list_of_chat_list;
  GListStore      *list_of_user_list;
  GtkFlattenListModel *contact_list;
  GtkSortListModel    *sorted_chat_list;
  GtkSorter           *chat_sorter;

  ChattyPurple    *purple;
  MatrixDb        *matrix_db;
  /* We have exactly one MM account */
  ChattyMmAccount *mm_account;

  gboolean         disable_auto_login;
  gboolean         network_available;
  gboolean         has_loaded;
};

G_DEFINE_TYPE (ChattyManager, chatty_manager, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ACTIVE_PROTOCOLS,
  N_PROPS
};

enum {
  CHAT_DELETED,
  OPEN_CHAT,
  N_SIGNALS
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

static int
manager_sort_chat_item (ChattyChat *a,
                        ChattyChat *b,
                        gpointer    user_data)
{
  time_t a_time, b_time;

  g_assert (CHATTY_IS_CHAT (a));
  g_assert (CHATTY_IS_CHAT (b));

  a_time = chatty_chat_get_last_msg_time (a);
  b_time = chatty_chat_get_last_msg_time (b);

  return difftime (b_time, a_time);
}

static gboolean
manager_mm_set_eds (gpointer user_data)
{
  g_autoptr(ChattyManager) self = user_data;

  chatty_mm_account_set_eds (self->mm_account, self->chatty_eds);

  return G_SOURCE_REMOVE;
}

static void
manager_eds_is_ready (ChattyManager *self)
{
  g_assert (CHATTY_IS_MANAGER (self));

  /* Set eds after some timeout so that most contacts are loaded */
  g_timeout_add (200, manager_mm_set_eds, g_object_ref (self));
}

static void
manager_network_changed_cb (GNetworkMonitor *network_monitor,
                            gboolean         network_available,
                            ChattyManager   *self)
{
  GListModel *list;
  guint n_items;

  g_assert (G_IS_NETWORK_MONITOR (network_monitor));
  g_assert (CHATTY_IS_MANAGER (self));

  if (network_available == self->network_available ||
      !self->has_loaded)
    return;

  self->network_available = network_available;
  list = G_LIST_MODEL (self->account_list);
  n_items = g_list_model_get_n_items (list);

  g_log (G_LOG_DOMAIN, CHATTY_LOG_LEVEL_TRACE,
         "Network changed, has network: %s", CHATTY_LOG_BOOL (network_available));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(ChattyAccount) account = NULL;

      account = g_list_model_get_item (list, i);

      if (network_available)
        chatty_account_connect (account, FALSE);
      else
        chatty_account_disconnect (account);
    }
}

static void
chatty_manager_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  ChattyManager *self = (ChattyManager *)object;

  switch (prop_id)
    {
    case PROP_ACTIVE_PROTOCOLS:
      g_value_set_int (value, chatty_manager_get_active_protocols (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
chatty_manager_dispose (GObject *object)
{
  ChattyManager *self = (ChattyManager *)object;

  g_clear_object (&self->chatty_eds);
  g_clear_object (&self->list_of_chat_list);
  g_clear_object (&self->list_of_user_list);
  g_clear_object (&self->contact_list);
  g_clear_object (&self->list_of_user_list);
  g_clear_object (&self->account_list);
  g_clear_object (&self->accounts);

  G_OBJECT_CLASS (chatty_manager_parent_class)->dispose (object);
}

static void
chatty_manager_finalize (GObject *object)
{
  ChattyManager *self = (ChattyManager *)object;

  g_clear_object (&self->history);

  G_OBJECT_CLASS (chatty_manager_parent_class)->finalize (object);
}

static void
chatty_manager_class_init (ChattyManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = chatty_manager_get_property;
  object_class->dispose = chatty_manager_dispose;
  object_class->finalize = chatty_manager_finalize;

  /**
   * ChattyUser:active-protocols:
   *
   * Protocols currently available for use.  This is a
   * flag of protocols currently connected and available
   * for use.
   */
  properties[PROP_ACTIVE_PROTOCOLS] =
    g_param_spec_int ("active-protocols",
                      "Active protocols",
                      "Protocols currently active and connected",
                      CHATTY_PROTOCOL_NONE,
                      CHATTY_PROTOCOL_TELEGRAM,
                      CHATTY_PROTOCOL_NONE,
                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * ChattyManager::chat-deleted:
   * @self: a #ChattyManager
   * @chat: A #ChattyChat
   *
   * Emitted when a chat is deleted.  ‘chat-deleted’ is
   * emitted just before the chat is actually deleted
   * and thus @chat will still point to a valid memory.
   */
  signals [CHAT_DELETED] =
    g_signal_new ("chat-deleted",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1, CHATTY_TYPE_CHAT);

  /**
   * ChattyManager::open-chat:
   * @self: A #ChattyManager
   * @chat: A #ChattyChat
   *
   * Emitted when user requests to open a chat.  UI can hook
   * to this signal to do whatever appropriate.
   */
  signals [OPEN_CHAT] =
    g_signal_new ("open-chat",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1, CHATTY_TYPE_CHAT);
}

static void
manager_mm_account_changed_cb (ChattyManager *self)
{
  g_assert (CHATTY_IS_MANAGER (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE_PROTOCOLS]);
}

static void
chatty_manager_init (ChattyManager *self)
{
  g_autoptr(GtkFlattenListModel) flatten_list = NULL;
  GNetworkMonitor *network_monitor;

  self->list_of_account_list = g_list_store_new (G_TYPE_LIST_MODEL);
  self->accounts = gtk_flatten_list_model_new (CHATTY_TYPE_ACCOUNT,
                                               G_LIST_MODEL (self->list_of_account_list));
  self->account_list = g_list_store_new (CHATTY_TYPE_ACCOUNT);
  g_list_store_append (self->list_of_account_list, self->account_list);

  self->chatty_eds = chatty_eds_new (CHATTY_PROTOCOL_MMS_SMS);
  self->mm_account = chatty_mm_account_new ();

  g_signal_connect_object (self->mm_account, "notify::status",
                           G_CALLBACK (manager_mm_account_changed_cb), self,
                           G_CONNECT_SWAPPED);

  self->list_of_chat_list = g_list_store_new (G_TYPE_LIST_MODEL);
  self->list_of_user_list = g_list_store_new (G_TYPE_LIST_MODEL);

  self->contact_list = gtk_flatten_list_model_new (G_TYPE_OBJECT,
                                                   G_LIST_MODEL (self->list_of_user_list));
  g_list_store_append (self->list_of_user_list,
                       chatty_eds_get_model (self->chatty_eds));

  self->chat_sorter = gtk_custom_sorter_new ((GCompareDataFunc)manager_sort_chat_item,
                                             NULL, NULL);
  flatten_list = gtk_flatten_list_model_new (G_TYPE_OBJECT,
                                             G_LIST_MODEL (self->list_of_chat_list));
  self->sorted_chat_list = gtk_sort_list_model_new (G_LIST_MODEL (flatten_list),
                                                    self->chat_sorter);

  g_signal_connect_object (self->chatty_eds, "notify::is-ready",
                           G_CALLBACK (manager_eds_is_ready), self,
                           G_CONNECT_SWAPPED);

  network_monitor = g_network_monitor_get_default ();
  self->network_available = g_network_monitor_get_network_available (network_monitor);

  /* Required for matrix */
  g_signal_connect_object (network_monitor, "network-changed",
                           G_CALLBACK (manager_network_changed_cb), self,
                           G_CONNECT_AFTER);
}

ChattyManager *
chatty_manager_get_default (void)
{
  static ChattyManager *self;

  if (!self)
    {
      self = g_object_new (CHATTY_TYPE_MANAGER, NULL);
      g_object_add_weak_pointer (G_OBJECT (self), (gpointer *)&self);
    }

  return self;
}

static void
manager_ma_account_changed_cb (ChattyMaAccount *account)
{
  ChattyManager *self;
  GListModel *chat_list;
  ChattyStatus status;

  g_assert (CHATTY_IS_MA_ACCOUNT (account));

  self = chatty_manager_get_default ();
  status = chatty_account_get_status (CHATTY_ACCOUNT (account));
  chat_list = chatty_ma_account_get_chat_list (account);

  if (status == CHATTY_CONNECTED)
    g_list_store_append (self->list_of_chat_list, chat_list);
  else
    chatty_utils_remove_list_item (self->list_of_chat_list, chat_list);
}

static void
manager_secret_load_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  ChattyManager *self = user_data;
  g_autoptr(GPtrArray) accounts = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (CHATTY_IS_MANAGER (self));

  accounts = chatty_secret_load_finish (result, &error);
  g_info ("Loading accounts from secrets %s", CHATTY_LOG_SUCESS (!error));

  if (error)
    g_warning ("Error loading secret accounts: %s", error->message);

  if (!accounts)
    return;

  g_info ("Loaded %d matrix accounts", accounts ? accounts->len : 0);

  for (guint i = 0; i < accounts->len; i++) {
    g_signal_connect_object (accounts->pdata[i], "notify::status",
                             G_CALLBACK (manager_ma_account_changed_cb),
                             accounts->pdata[i], 0);

    chatty_ma_account_set_history_db (accounts->pdata[i], self->history);
    chatty_ma_account_set_db (accounts->pdata[i], self->matrix_db);
  }

  g_list_store_splice (self->account_list, 0, 0, accounts->pdata, accounts->len);
}

static void
matrix_db_open_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  ChattyManager *self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (CHATTY_IS_MANAGER (self));

  if (matrix_db_open_finish (self->matrix_db, result, &error))
    chatty_secret_load_async (NULL, manager_secret_load_cb, self);
  else if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("Failed to open Matrix DB: %s", error->message);

  g_info ("Opening matrix db %s", CHATTY_LOG_SUCESS (!error));
}

static void
manager_mm_account_load_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  g_autoptr(ChattyManager) self = user_data;
  GListModel *chat_list;

  g_assert (CHATTY_IS_MANAGER (self));

  chat_list = chatty_mm_account_get_chat_list (self->mm_account);
  g_list_store_append (self->list_of_chat_list, chat_list);
}

void
chatty_manager_load (ChattyManager *self)
{
  g_return_if_fail (CHATTY_IS_MANAGER (self));

  if (self->has_loaded)
    return;

  self->has_loaded = TRUE;

  if (!self->purple) {
    self->purple = chatty_purple_get_default ();
    g_list_store_append (self->list_of_account_list,
                         chatty_purple_get_accounts (self->purple));
    g_list_store_append (self->list_of_chat_list,
                         chatty_purple_get_chat_list (self->purple));
    g_list_store_append (self->list_of_user_list,
                         chatty_purple_get_chat_list (self->purple));
    g_list_store_append (self->list_of_user_list,
                         chatty_purple_get_user_list (self->purple));
    chatty_purple_set_history_db (self->purple, self->history);
    chatty_purple_load (self->purple, self->disable_auto_login);
  }

  chatty_mm_account_set_history_db (self->mm_account,
                                    chatty_manager_get_history (self));
  chatty_mm_account_load_async (self->mm_account,
                                manager_mm_account_load_cb,
                                g_object_ref (self));

  if (chatty_settings_get_experimental_features (chatty_settings_get_default ())) {
    char *db_path;

    self->matrix_db = matrix_db_new ();
    db_path =  g_build_filename (purple_user_dir(), "chatty", "db", NULL);
    matrix_db_open_async (self->matrix_db, db_path, "matrix.db",
                          matrix_db_open_cb, self);
    g_info ("Opening matrix db");
  }
}

GListModel *
chatty_manager_get_accounts (ChattyManager *self)
{
  g_return_val_if_fail (CHATTY_IS_MANAGER (self), NULL);

  return G_LIST_MODEL (self->accounts);
}

GListModel *
chatty_manager_get_contact_list (ChattyManager *self)
{
  g_return_val_if_fail (CHATTY_IS_MANAGER (self), NULL);

  return G_LIST_MODEL (self->contact_list);
}


GListModel *
chatty_manager_get_chat_list (ChattyManager *self)
{
  g_return_val_if_fail (CHATTY_IS_MANAGER (self), NULL);

  return G_LIST_MODEL (self->sorted_chat_list);
}

/**
 * chatty_manager_disable_auto_login:
 * @self: A #ChattyManager
 * @disable: whether to disable auto-login
 *
 * Set whether to disable automatic login when accounts are
 * loaded/added.  By default, auto-login is enabled if the
 * account is enabled with chatty_pp_account_set_enabled().
 *
 * This is not applicable to SMS accounts.
 */
void
chatty_manager_disable_auto_login (ChattyManager *self,
                                   gboolean       disable)
{
  g_return_if_fail (CHATTY_IS_MANAGER (self));

  self->disable_auto_login = !!disable;
}

gboolean
chatty_manager_get_disable_auto_login (ChattyManager *self)
{
  g_return_val_if_fail (CHATTY_IS_MANAGER (self), TRUE);

  return self->disable_auto_login;
}

ChattyProtocol
chatty_manager_get_active_protocols (ChattyManager *self)
{
  ChattyProtocol protocols = CHATTY_PROTOCOL_NONE;

  g_return_val_if_fail (CHATTY_IS_MANAGER (self), CHATTY_PROTOCOL_NONE);

  if (chatty_account_get_status (CHATTY_ACCOUNT (self->mm_account)) == CHATTY_CONNECTED)
    protocols = protocols | CHATTY_PROTOCOL_MMS_SMS;

  if (self->purple)
    protocols |= chatty_purple_get_protocols (self->purple);

  return protocols;
}


ChattyEds *
chatty_manager_get_eds (ChattyManager *self)
{
  g_return_val_if_fail (CHATTY_IS_MANAGER (self), NULL);

  return self->chatty_eds;
}

static void
matrix_db_account_delete_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;
  gboolean status;

  g_assert (G_IS_TASK (task));

  status = matrix_db_delete_account_finish (MATRIX_DB (object),
                                            result, &error);

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, status);
}

static void
manager_account_delete_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  ChattyManager *self;
  ChattyMaAccount *account;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  account = g_task_get_task_data (task);

  g_assert (CHATTY_IS_MANAGER (self));
  g_assert (CHATTY_IS_MA_ACCOUNT (account));

  if (!chatty_secret_delete_finish (result, &error)) {
    g_task_return_error (task, error);
    return;
  }

  matrix_db_delete_account_async (self->matrix_db, CHATTY_ACCOUNT (account),
                                  matrix_db_account_delete_cb,
                                  g_steal_pointer (&task));
}

void
chatty_manager_delete_account_async (ChattyManager       *self,
                                     ChattyAccount       *account,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  GListModel *chat_list;
  GTask *task;

  g_return_if_fail (CHATTY_IS_MANAGER (self));
  /* We now handle only matrix accounts */
  g_return_if_fail (CHATTY_IS_MA_ACCOUNT (account));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_object_ref (account), g_object_unref);

  chat_list = chatty_ma_account_get_chat_list (CHATTY_MA_ACCOUNT (account));
  chatty_utils_remove_list_item (self->list_of_chat_list, chat_list);
  chatty_utils_remove_list_item (self->account_list, account);

  chatty_secret_delete_async (account, NULL, manager_account_delete_cb, task);
}

gboolean
chatty_manager_delete_account_finish  (ChattyManager  *self,
                                       GAsyncResult   *result,
                                       GError        **error)
{
  g_return_val_if_fail (CHATTY_IS_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
manager_save_account_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  ChattyManager *self;
  ChattyMaAccount *account = (gpointer)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  gboolean saved;

  g_assert (G_IS_TASK (task));
  g_assert (CHATTY_IS_MA_ACCOUNT (account));

  self = g_task_get_source_object (task);
  g_assert (CHATTY_IS_MANAGER (self));

  saved = chatty_ma_account_save_finish (account, result, &error);

  if (error) {
    g_task_return_error (task, error);
    return;
  }

  if (saved) {
    g_list_store_append (self->account_list, account);

    g_signal_connect_object (account, "notify::status",
                             G_CALLBACK (manager_ma_account_changed_cb),
                             account, 0);

    if (!chatty_manager_get_disable_auto_login (self))
      chatty_account_set_enabled (CHATTY_ACCOUNT (account), TRUE);
  }

  g_task_return_boolean (task, saved);
}

void
chatty_manager_save_account_async (ChattyManager       *self,
                                   ChattyAccount       *account,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  GTask *task;

  g_return_if_fail (CHATTY_IS_MANAGER (self));
  /* We now handle only matrix accounts */
  g_return_if_fail (CHATTY_IS_MA_ACCOUNT (account));

  task = g_task_new (self, cancellable, callback, user_data);
  chatty_ma_account_set_history_db (CHATTY_MA_ACCOUNT (account), self->history);
  chatty_ma_account_set_db (CHATTY_MA_ACCOUNT (account), self->matrix_db);
  chatty_ma_account_save_async (CHATTY_MA_ACCOUNT (account), TRUE, NULL,
                                manager_save_account_cb, task);
}

gboolean
chatty_manager_save_account_finish (ChattyManager  *self,
                                    GAsyncResult   *result,
                                    GError        **error)
{
  g_return_val_if_fail (CHATTY_IS_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

ChattyAccount *
chatty_manager_get_mm_account (ChattyManager *self)
{
  g_return_val_if_fail (CHATTY_IS_MANAGER (self), NULL);

  return CHATTY_ACCOUNT (self->mm_account);
}

ChattyChat *
chatty_manager_find_chat_with_name (ChattyManager *self,
                                    const char    *account_id,
                                    const char    *chat_id)
{
  ChattyAccount *mm_account;
  GListModel *accounts, *chat_list;
  const char *id;
  guint n_accounts, n_items;

  g_return_val_if_fail (CHATTY_IS_MANAGER (self), NULL);
  g_return_val_if_fail (chat_id && *chat_id, NULL);

  mm_account = chatty_manager_get_mm_account (self);
  chat_list = chatty_mm_account_get_chat_list (CHATTY_MM_ACCOUNT (mm_account));
  n_items = g_list_model_get_n_items (chat_list);

  for (guint i = 0; i < n_items; i++) {
    g_autoptr(ChattyChat) chat = NULL;

    chat = g_list_model_get_item (chat_list, i);
    id = chatty_chat_get_chat_name (chat);

    if (g_strcmp0 (id, chat_id) == 0)
      return chat;
  }

  accounts = G_LIST_MODEL (self->account_list);
  n_accounts = g_list_model_get_n_items (accounts);

  for (guint i = 0; i < n_accounts; i++) {
    g_autoptr(ChattyAccount) account = NULL;

    account = g_list_model_get_item (accounts, i);

    if (!CHATTY_IS_MA_ACCOUNT (account) ||
        g_strcmp0 (chatty_item_get_username (CHATTY_ITEM (account)), account_id) != 0)
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

gboolean
chatty_manager_set_uri (ChattyManager *self,
                        const char    *uri)
{
  g_autoptr(ChattyChat) chat = NULL;
  g_autofree char *who = NULL;
  ChattyAccount *account;
  const char *country_code;

  if (!uri || !*uri)
    return FALSE;

  account = chatty_manager_get_mm_account (self);
  if (chatty_account_get_status (account) != CHATTY_CONNECTED)
    return FALSE;

  country_code = chatty_settings_get_country_iso_code (chatty_settings_get_default ());
  who = chatty_utils_check_phonenumber (uri, country_code);
  if (!who)
    return FALSE;

  chat = chatty_mm_account_start_chat (CHATTY_MM_ACCOUNT (account), who);
  g_signal_emit (self, signals[OPEN_CHAT], 0, chat);

  return TRUE;
}

ChattyHistory *
chatty_manager_get_history (ChattyManager *self)
{
  g_return_val_if_fail (CHATTY_IS_MANAGER (self), NULL);

  if (!self->history)
    self->history = chatty_history_new ();

  return self->history;
}
