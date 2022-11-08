/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-secret-store.c
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-secret-store"


#include <libsecret/secret.h>
#include <glib/gi18n.h>

#include "chatty-ma-account.h"
#include "chatty-secret-store.h"
#include "chatty-log.h"

#define PROTOCOL_MATRIX_STR  "matrix"

static const SecretSchema *
secret_store_get_schema (void)
{
  static const SecretSchema password_schema = {
    /** SECRET_SCHEMA_DONT_MATCH_NAME is used as a workaround for a bug in gnome-keyring
     *  which prevents cold keyrings from being searched (and hence does not prompt for unlocking)
     *  see https://gitlab.gnome.org/GNOME/gnome-keyring/-/issues/89 and
     *  https://gitlab.gnome.org/GNOME/libsecret/-/issues/7 for more information
     */
    "sm.puri.Chatty", SECRET_SCHEMA_DONT_MATCH_NAME,
    {
      { CHATTY_USERNAME_ATTRIBUTE,  SECRET_SCHEMA_ATTRIBUTE_STRING },
      { CHATTY_SERVER_ATTRIBUTE,    SECRET_SCHEMA_ATTRIBUTE_STRING },
      { CHATTY_PROTOCOL_ATTRIBUTE,  SECRET_SCHEMA_ATTRIBUTE_STRING },
      { NULL, 0 },
    }
  };

  return &password_schema;
}

static void
secret_load_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  GPtrArray *accounts = NULL;
  GError *error = NULL;
  GList *secrets;

  g_assert_true (G_IS_TASK (task));

  secrets = secret_password_search_finish (result, &error);
  CHATTY_DEBUG_MSG ("secret accounts load %s", CHATTY_LOG_SUCESS (!error));

  if (error) {
    g_task_return_error (task, error);
    return;
  }

  for (GList *item = secrets; item; item = item->next) {
    if (!accounts)
      accounts = g_ptr_array_new_full (5, g_object_unref);

    if (item->data)
      g_ptr_array_insert (accounts, -1, item->data);
  }

  if (secrets)
    g_list_free (secrets);

  g_task_return_pointer (task, accounts, (GDestroyNotify)g_ptr_array_unref);
}

void
chatty_secret_load_async  (GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  const SecretSchema *schema;
  GTask *task;

  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  schema = secret_store_get_schema ();
  task = g_task_new (NULL, cancellable, callback, user_data);

  /** With using SECRET_SCHEMA_DONT_MATCH_NAME we need some other attribute
   *  (apart from the schema name itself) to use for the lookup.
   *  The protocol attribute seems like a reasonable choice.
   */
  secret_password_search (schema,
                          SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS,
                          cancellable, secret_load_cb, task,
                          CHATTY_PROTOCOL_ATTRIBUTE, PROTOCOL_MATRIX_STR,
                          NULL);
  CHATTY_DEBUG_MSG ("loading secret accounts");
}

GPtrArray *
chatty_secret_load_finish (GAsyncResult  *result,
                           GError       **error)
{
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

void
chatty_secret_delete_async (ChattyAccount       *account,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  const SecretSchema *schema;
  const char *server, *username;

  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (CHATTY_IS_MA_ACCOUNT (account))
    username = chatty_ma_account_get_login_username (CHATTY_MA_ACCOUNT (account));
  else
    username = chatty_item_get_username (CHATTY_ITEM (account));

  CHATTY_DEBUG (username, "Deleting account");

  schema = secret_store_get_schema ();
  server = chatty_ma_account_get_homeserver (CHATTY_MA_ACCOUNT (account));
  secret_password_clear (schema, cancellable, callback, user_data,
                         CHATTY_USERNAME_ATTRIBUTE, username,
                         CHATTY_SERVER_ATTRIBUTE, server,
                         CHATTY_PROTOCOL_ATTRIBUTE, PROTOCOL_MATRIX_STR,
                         NULL);
}

gboolean
chatty_secret_delete_finish  (GAsyncResult  *result,
                              GError       **error)
{
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return secret_password_clear_finish (result, error);
}
