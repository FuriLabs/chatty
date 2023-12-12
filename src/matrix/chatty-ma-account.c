/* chatty-ma-account.c
 *
 * Copyright 2020, 2022 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-ma-account"

#include <libsecret/secret.h>
#include <glib/gi18n.h>

#include "chatty-history.h"
#include "chatty-utils.h"
#include "chatty-ma-chat.h"
#include "chatty-ma-key-chat.h"
#include "chatty-ma-account.h"
#include "chatty-log.h"

/**
 * SECTION: chatty-mat-account
 * @title: ChattyMaAccount
 * @short_description: An abstraction for Matrix accounts
 * @include: "chatty-mat-account.h"
 */

struct _ChattyMaAccount
{
  ChattyAccount   parent_instance;

  char           *name;

  CmMatrix       *cm_matrix;
  CmClient       *cm_client;

  GtkStringObject *device_fp;

  GListStore     *chat_list;
  GdkPixbuf      *avatar;

  ChattyStatus   status;
};

G_DEFINE_TYPE (ChattyMaAccount, chatty_ma_account, CHATTY_TYPE_ACCOUNT)

/* We use macro here so that the debug logs has the right line info */
#define ma_account_update_status(self, _status)                         \
  do {                                                                  \
    if (self->status != _status) {                                      \
      self->status = _status;                                           \
      g_object_notify (G_OBJECT (self), "status");                      \
      CHATTY_TRACE (cm_client_get_user_id (self->cm_client),            \
                    "status changed, connected: %s, user:",             \
                    _status == CHATTY_CONNECTING ? "connecting" :       \
                    CHATTY_LOG_BOOL (_status == CHATTY_CONNECTED));     \
    }                                                                   \
  } while (0)

static void
ma_acccount_password_response_cb (ChattyMaAccount *self,
                                  int              response_id,
                                  GtkDialog       *dialog)
{
  GtkWidget *entry;
  const char *password;

  g_assert (CHATTY_IS_MA_ACCOUNT (self));
  g_assert (GTK_IS_DIALOG (dialog));

  entry = g_object_get_data (G_OBJECT (dialog), "entry");
  password = gtk_editable_get_text (GTK_EDITABLE (entry));

  if (response_id != GTK_RESPONSE_ACCEPT || !password || !*password) {
    chatty_account_set_enabled (CHATTY_ACCOUNT (self), FALSE);
  } else {
    cm_client_set_password (self->cm_client, password);
    chatty_account_set_enabled (CHATTY_ACCOUNT (self), FALSE);
    chatty_account_set_enabled (CHATTY_ACCOUNT (self), TRUE);
  }

  gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
handle_password_login (ChattyMaAccount *self,
                       GError          *error)
{
  g_assert (CHATTY_IS_MA_ACCOUNT (self));

  /* If no error, Api is informing us that logging in succeeded.
   * Let’s update matrix_enc & set device keys to upload */
  if (g_error_matches (error, CM_ERROR, CM_ERROR_BAD_PASSWORD)) {
    GtkWidget *dialog, *content, *header_bar, *label;
    GtkWidget *cancel_btn, *entry;
    g_autofree char *message = NULL;
    CmAccount *cm_account;

    dialog = gtk_dialog_new_with_buttons (_("Incorrect password"),
                                          gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ())),
                                          GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_USE_HEADER_BAR,
                                          _("_OK"), GTK_RESPONSE_ACCEPT,
                                          _("_Cancel"), GTK_RESPONSE_REJECT,
                                          NULL);

    content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    gtk_box_set_spacing (GTK_BOX (content), 12);
    cm_account = cm_client_get_account (self->cm_client);
    message = g_strdup_printf (_("Please enter password for “%s”, homeserver: %s"),
                               cm_account_get_login_id (cm_account),
                               cm_client_get_homeserver (self->cm_client));
    label = gtk_label_new (message);
    gtk_label_set_wrap (GTK_LABEL (label), TRUE);
    gtk_box_append (GTK_BOX (content), label);
    entry = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
    gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE);
    gtk_box_append (GTK_BOX (content), entry);

    header_bar = gtk_dialog_get_header_bar (GTK_DIALOG (dialog));
    gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (header_bar), FALSE);

    cancel_btn = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog),
                                                     GTK_RESPONSE_REJECT);
    g_object_ref (cancel_btn);
    gtk_header_bar_remove (GTK_HEADER_BAR (header_bar), cancel_btn);
    gtk_header_bar_pack_start (GTK_HEADER_BAR (header_bar), cancel_btn);
    g_object_unref (cancel_btn);

    g_object_set_data (G_OBJECT (dialog), "entry", entry);
    g_signal_connect_object (dialog, "response",
                             G_CALLBACK (ma_acccount_password_response_cb),
                             self, G_CONNECT_SWAPPED);
    gtk_window_present (GTK_WINDOW (dialog));
  }

  if (!error)
    ma_account_update_status (self, CHATTY_CONNECTED);
}

static void
cm_account_sync_cb (gpointer   user_data,
                    CmClient  *cm_client,
                    CmRoom    *cm_room,
                    GPtrArray *events,
                    GError    *error)
{
  ChattyMaAccount *self = user_data;

  g_assert (CHATTY_IS_MA_ACCOUNT (self));
  g_assert (CM_IS_CLIENT (self->cm_client));
  g_assert (self->cm_client == cm_client);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  if (error)
    g_debug ("%s Error %d: %s", g_quark_to_string (error->domain),
             error->code, error->message);

  if (g_error_matches (error, CM_ERROR, CM_ERROR_BAD_PASSWORD))
    {
      handle_password_login (self, error);
      return;
    }
}

static const char *
chatty_ma_account_get_protocol_name (ChattyAccount *account)
{
  return "Matrix";
}

static ChattyStatus
chatty_ma_account_get_status (ChattyAccount *account)
{
  ChattyMaAccount *self = (ChattyMaAccount *)account;

  g_assert (CHATTY_IS_MA_ACCOUNT (self));

  return self->status;
}

static gboolean
chatty_ma_account_get_enabled (ChattyAccount *account)
{
  ChattyMaAccount *self = (ChattyMaAccount *)account;

  g_assert (CHATTY_IS_MA_ACCOUNT (self));

  return cm_client_get_enabled (self->cm_client);
}

static void
chatty_ma_account_set_enabled (ChattyAccount *account,
                               gboolean       enable)
{
  ChattyMaAccount *self = (ChattyMaAccount *)account;

  g_assert (CHATTY_IS_MA_ACCOUNT (self));
  g_assert (self->cm_client);

  cm_client_set_enabled (self->cm_client, enable);

  g_object_notify (G_OBJECT (self), "enabled");
  g_object_notify (G_OBJECT (self), "status");
}

static const char *
chatty_ma_account_get_password (ChattyAccount *account)
{
  const char *password;
  ChattyMaAccount *self = (ChattyMaAccount *)account;

  g_assert (CHATTY_IS_MA_ACCOUNT (self));

  password = cm_client_get_password (self->cm_client);

  if (password)
    return password;

  return "";
}

static void
chatty_ma_account_set_password (ChattyAccount *account,
                                const char    *password)
{
  ChattyMaAccount *self = (ChattyMaAccount *)account;

  g_assert (CHATTY_IS_MA_ACCOUNT (self));


  if (self->cm_client)
    {
      if (cm_client_get_logging_in (self->cm_client) ||
          cm_client_get_logged_in (self->cm_client))
        return;
    }

  if (g_strcmp0 (password, cm_client_get_password (self->cm_client)) == 0)
    return;

  cm_client_set_password (self->cm_client, password);
}

static gboolean
chatty_ma_account_get_remember_password (ChattyAccount *self)
{
  /* password is always remembered */
  return TRUE;
}

static GtkStringObject *
chatty_ma_account_get_device_fp (ChattyAccount *account)
{
  ChattyMaAccount *self = (ChattyMaAccount *)account;
  const char *device_id;

  g_assert (CHATTY_IS_MA_ACCOUNT (self));

  device_id = cm_client_get_device_id (self->cm_client);
  g_clear_object (&self->device_fp);

  if (device_id) {
    g_autoptr(GString) fp = NULL;
    const char *str;

    fp = g_string_new (NULL);
    str = cm_client_get_ed25519_key (self->cm_client);

    while (str && *str) {
      g_autofree char *chunk = g_strndup (str, 4);

      g_string_append_printf (fp, "%s ", chunk);
      str = str + strlen (chunk);
    }

    self->device_fp = gtk_string_object_new (fp->str);
    g_object_set_data_full (G_OBJECT (self->device_fp), "device-id",
                            g_strdup (device_id), g_free);
  }

  return self->device_fp;
}

static void
ma_account_leave_chat_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  ChattyMaAccount *self;
  g_autoptr(GTask) task = user_data;
  ChattyChat *chat;
  GError *error = NULL;
  gboolean success;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  chat = g_task_get_task_data (task);
  g_assert (CHATTY_IS_MA_ACCOUNT (self));

  success = cm_room_leave_finish (CM_ROOM (object), result, &error);
  CHATTY_TRACE_MSG ("Leaving chat: %s(%s), success: %d",
                    chatty_item_get_name (CHATTY_ITEM (chat)),
                    chatty_chat_get_chat_name (chat),
                    success);

  if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("Error deleting chat: %s", error->message);

  /* Failed deleting from server, re-add in local chat list */
  if (!success) {
    ChattyItemState old_state;

    g_list_store_append (self->chat_list, chat);
    chatty_item_set_state (CHATTY_ITEM (chat), CHATTY_ITEM_HIDDEN);

    old_state = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (task), "state"));
    chatty_item_set_state (CHATTY_ITEM (chat), old_state);
  }

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, success);
}

static void
chatty_ma_account_leave_chat_async (ChattyAccount       *account,
                                    ChattyChat          *chat,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  ChattyMaAccount *self = (ChattyMaAccount *)account;
  g_autoptr(GTask) task = NULL;

  g_assert (CHATTY_IS_MA_ACCOUNT (self));

  task = g_task_new (self, NULL, callback, user_data);
  g_task_set_task_data (task, g_object_ref (chat), g_object_unref);

  /* Remove the item so that it’s no longer listed in chat list */
  /* TODO: Handle edge case where the item was deleted from two
   * different sessions the same time */
  if (!chatty_utils_remove_list_item (self->chat_list, chat))
    g_return_if_reached ();

  CHATTY_TRACE_MSG ("Leaving chat: %s(%s)",
                    chatty_item_get_name (CHATTY_ITEM (chat)),
                    chatty_chat_get_chat_name (chat));

  g_object_set_data (G_OBJECT (task), "state",
                     GINT_TO_POINTER (chatty_item_get_state (CHATTY_ITEM (chat))));
  chatty_item_set_state (CHATTY_ITEM (chat), CHATTY_ITEM_HIDDEN);
  cm_room_leave_async (chatty_ma_chat_get_cm_room (CHATTY_MA_CHAT (chat)),
                       NULL,
                       ma_account_leave_chat_cb,
                       g_steal_pointer (&task));
}

static ChattyProtocol
chatty_ma_account_get_protocols (ChattyItem *item)
{
  return CHATTY_PROTOCOL_MATRIX;
}

static const char *
chatty_ma_account_get_name (ChattyItem *item)
{
  ChattyMaAccount *self = (ChattyMaAccount *)item;

  g_assert (CHATTY_IS_MA_ACCOUNT (self));

  if (self->name)
    return self->name;

  return "";
}

static void
chatty_ma_account_set_name (ChattyItem *item,
                            const char *name)
{
  ChattyMaAccount *self = (ChattyMaAccount *)item;

  g_assert (CHATTY_IS_MA_ACCOUNT (self));

  g_free (self->name);
  self->name = g_strdup (name);
}

static const char *
chatty_ma_account_get_username (ChattyItem *item)
{
  ChattyMaAccount *self = (ChattyMaAccount *)item;

  g_assert (CHATTY_IS_MA_ACCOUNT (self));

  if (self->cm_client && cm_client_get_user_id (self->cm_client))
    return cm_client_get_user_id (self->cm_client);

  return "";
}

static void
ma_account_get_avatar_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  g_autoptr(ChattyMaAccount) self = user_data;
  g_autoptr(GInputStream) stream = NULL;
  g_autoptr(GError) error = NULL;

  if (self->avatar)
    return;

  stream = cm_user_get_avatar_finish (CM_USER (object), result, &error);

  if (error || !stream)
    return;

  self->avatar = gdk_pixbuf_new_from_stream (stream, NULL, NULL);
  g_signal_emit_by_name (self, "avatar-changed", 0);
}

static GdkPixbuf *
chatty_ma_account_get_avatar (ChattyItem *item)
{
  ChattyMaAccount *self = (ChattyMaAccount *)item;
  CmAccount *account;

  g_assert (CHATTY_IS_MA_ACCOUNT (self));

  if (self->avatar)
    return self->avatar;

  account = cm_client_get_account (self->cm_client);

  cm_user_get_avatar_async (CM_USER (account), NULL,
                            ma_account_get_avatar_cb,
                            g_object_ref (self));
  return NULL;
}

static void
ma_account_set_user_avatar_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  ChattyMaAccount *self;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (CHATTY_MA_ACCOUNT (self));

  cm_account_set_user_avatar_finish (CM_ACCOUNT (object), result, &error);

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);

  if (!error) {
    g_clear_object (&self->avatar);
    g_signal_emit_by_name (self, "avatar-changed");
  }
}

static void
chatty_ma_account_set_avatar_async (ChattyItem          *item,
                                    const char          *file_name,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  ChattyMaAccount *self = (ChattyMaAccount *)item;
  g_autoptr(GTask) task = NULL;
  GFile *file;

  g_assert (CHATTY_IS_MA_ACCOUNT (self));

  task = g_task_new (self, cancellable, callback, user_data);

  if (!file_name)
    {
      g_task_return_boolean (task, TRUE);
      return;
    }

  file = g_file_new_for_path (file_name);
  cm_account_set_user_avatar_async (cm_client_get_account (self->cm_client),
                                    file, cancellable,
                                    ma_account_set_user_avatar_cb,
                                    g_steal_pointer (&task));
}

static void
chatty_ma_account_finalize (GObject *object)
{
  ChattyMaAccount *self = (ChattyMaAccount *)object;

  g_list_store_remove_all (self->chat_list);

  g_clear_object (&self->device_fp);
  g_clear_object (&self->chat_list);
  g_clear_object (&self->avatar);

  g_free (self->name);

  G_OBJECT_CLASS (chatty_ma_account_parent_class)->finalize (object);
}

static void
chatty_ma_account_class_init (ChattyMaAccountClass *klass)
{
  GObjectClass *object_class  = G_OBJECT_CLASS (klass);
  ChattyItemClass *item_class = CHATTY_ITEM_CLASS (klass);
  ChattyAccountClass *account_class = CHATTY_ACCOUNT_CLASS (klass);

  object_class->finalize = chatty_ma_account_finalize;

  item_class->get_protocols = chatty_ma_account_get_protocols;
  item_class->get_name = chatty_ma_account_get_name;
  item_class->set_name = chatty_ma_account_set_name;
  item_class->get_username = chatty_ma_account_get_username;
  item_class->get_avatar = chatty_ma_account_get_avatar;
  item_class->set_avatar_async = chatty_ma_account_set_avatar_async;

  account_class->get_protocol_name = chatty_ma_account_get_protocol_name;
  account_class->get_status   = chatty_ma_account_get_status;
  account_class->get_enabled  = chatty_ma_account_get_enabled;
  account_class->set_enabled  = chatty_ma_account_set_enabled;
  account_class->get_password = chatty_ma_account_get_password;
  account_class->set_password = chatty_ma_account_set_password;
  account_class->get_remember_password = chatty_ma_account_get_remember_password;
  account_class->get_device_fp = chatty_ma_account_get_device_fp;
  account_class->leave_chat_async = chatty_ma_account_leave_chat_async;
}

static void
chatty_ma_account_init (ChattyMaAccount *self)
{
  self->chat_list = g_list_store_new (CHATTY_TYPE_CHAT);
}

static void
joined_rooms_changed (ChattyMaAccount *self,
                      int              position,
                      int              removed,
                      int              added,
                      GListModel      *model)
{
  g_autoptr(GPtrArray) items = NULL;

  g_assert (CHATTY_IS_MA_ACCOUNT (self));
  g_assert (G_IS_LIST_MODEL (model));

  for (guint i = position; i < position + added; i++)
    {
      g_autoptr(CmRoom) room = NULL;
      ChattyMaChat *chat;

      if (!items)
          items = g_ptr_array_new_with_free_func (g_object_unref);

      room = g_list_model_get_item (model, i);
      chat = chatty_ma_chat_new_with_room (room);
      chatty_ma_chat_set_data (chat, CHATTY_ACCOUNT (self), self->cm_client);
      g_ptr_array_add (items, chat);
    }

  g_list_store_splice (self->chat_list, position, removed,
                       items ? items->pdata : NULL, added);
}

static void
key_verifications_changed (ChattyMaAccount *self,
                           int              position,
                           int              removed,
                           int              added,
                           GListModel      *model)
{
  g_autoptr(GPtrArray) items = NULL;

  g_assert (CHATTY_IS_MA_ACCOUNT (self));
  g_assert (G_IS_LIST_MODEL (model));

  for (guint i = position; i < position + added; i++) {
    g_autoptr(CmVerificationEvent) event = NULL;
    ChattyMaKeyChat *chat;

    if (!items)
      items = g_ptr_array_new_with_free_func (g_object_unref);

    event = g_list_model_get_item (model, i);
    chat = chatty_ma_key_chat_new (self, event);
    g_ptr_array_add (items, chat);
  }

  g_list_store_splice (self->chat_list, position, removed,
                       items ? items->pdata : NULL, added);
}

static void
client_status_changed_cb (ChattyMaAccount *self)
{
  ChattyStatus status = CHATTY_DISCONNECTED;

  g_assert (CHATTY_IS_MA_ACCOUNT (self));

  if (!cm_client_get_enabled (self->cm_client))
    status = CHATTY_DISCONNECTED;
  else if (cm_client_is_sync (self->cm_client))
    status = CHATTY_CONNECTED;
  else if (cm_client_get_logging_in (self->cm_client) ||
           cm_client_get_logged_in (self->cm_client))
    status = CHATTY_CONNECTING;

  ma_account_update_status (self, status);
}

static void
ma_account_set_client (ChattyMaAccount *self,
                       CmClient        *client)
{
  GListModel *joined_rooms, *invited_rooms, *key_verifications;

  g_assert (CHATTY_IS_MA_ACCOUNT (self));
  g_assert (CM_IS_CLIENT (client));
  g_assert (!self->cm_client);

  self->cm_client = client;
  cm_client_set_device_name (client, "Chatty");
  cm_client_set_sync_callback (client,
                               cm_account_sync_cb,
                               self, NULL);

  g_signal_connect_object (self->cm_client, "status-changed",
                           G_CALLBACK (client_status_changed_cb),
                           self, G_CONNECT_SWAPPED);

  joined_rooms = cm_client_get_joined_rooms (client);
  g_signal_connect_object (joined_rooms, "items-changed",
                           G_CALLBACK (joined_rooms_changed), self,
                           G_CONNECT_SWAPPED);
  joined_rooms_changed (self, 0, 0, g_list_model_get_n_items (joined_rooms), joined_rooms);

  invited_rooms = cm_client_get_invited_rooms (client);
  g_signal_connect_object (invited_rooms, "items-changed",
                           G_CALLBACK (joined_rooms_changed), self,
                           G_CONNECT_SWAPPED);
  joined_rooms_changed (self, 0, 0, g_list_model_get_n_items (invited_rooms), invited_rooms);

  key_verifications = cm_client_get_key_verifications (client);
  g_signal_connect_object (key_verifications, "items-changed",
                           G_CALLBACK (key_verifications_changed), self,
                           G_CONNECT_SWAPPED);
  key_verifications_changed (self, 0, 0, g_list_model_get_n_items (key_verifications), key_verifications);
}

static void
ma_account_changed_cb (ChattyMaAccount *self)
{
  g_assert (CHATTY_IS_MA_ACCOUNT (self));

  g_clear_object (&self->avatar);
  g_object_notify (G_OBJECT (self), "name");
  g_signal_emit_by_name (self, "avatar-changed", 0);
}

ChattyMaAccount *
chatty_ma_account_new_from_client (CmClient *cm_client)
{
  ChattyMaAccount *self;
  CmAccount *account;

  g_return_val_if_fail (CM_IS_CLIENT (cm_client), NULL);

  self = g_object_new (CHATTY_TYPE_MA_ACCOUNT, NULL);
  ma_account_set_client (self, g_object_ref (cm_client));

  account = cm_client_get_account (cm_client);
  g_signal_connect_object (account, "changed",
                           G_CALLBACK (ma_account_changed_cb),
                           self, G_CONNECT_SWAPPED);

  return self;
}

CmClient *
chatty_ma_account_get_cm_client (ChattyMaAccount *self)
{
  g_return_val_if_fail (CHATTY_IS_MA_ACCOUNT (self), NULL);

  return self->cm_client;
}

/**
 * chatty_ma_account_get_login_username:
 * @self: A #ChattyMaAccount
 *
 * Get the username set when @self was created.  This
 * can be different from chatty_item_get_username().
 *
 * Say for example the user may have logged in using
 * an email address.  So If you want to get the original
 * username (which is the mail) which was used for login,
 * use this method.
 */

const char *
chatty_ma_account_get_login_username (ChattyMaAccount *self)
{
  CmAccount *cm_account;

  g_return_val_if_fail (CHATTY_IS_MA_ACCOUNT (self), "");

  cm_account = cm_client_get_account (self->cm_client);

  return cm_account_get_login_id (cm_account);
}

const char *
chatty_ma_account_get_homeserver (ChattyMaAccount *self)
{
  const char *homeserver;

  g_return_val_if_fail (CHATTY_IS_MA_ACCOUNT (self), "");

  homeserver = cm_client_get_homeserver (self->cm_client);

  if (homeserver)
    return homeserver;

  return "";
}

void
chatty_ma_account_set_homeserver (ChattyMaAccount *self,
                                  const char      *server_url)
{
  g_return_if_fail (CHATTY_IS_MA_ACCOUNT (self));

  cm_client_set_homeserver (self->cm_client, server_url);
}

const char *
chatty_ma_account_get_device_id (ChattyMaAccount *self)
{
  const char *device_id;
  g_return_val_if_fail (CHATTY_IS_MA_ACCOUNT (self), "");

  device_id = cm_client_get_device_id (self->cm_client);

  if (device_id)
    return device_id;

  return "";
}

GListModel *
chatty_ma_account_get_chat_list (ChattyMaAccount *self)
{
  g_return_val_if_fail (CHATTY_IS_MA_ACCOUNT (self), NULL);

  return G_LIST_MODEL (self->chat_list);
}

void
chatty_ma_account_send_file (ChattyMaAccount *self,
                             ChattyChat      *chat,
                             const char      *file_name)
{
  /* TODO */
}

static void
ma_get_details_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  ChattyMaAccount *self;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (CHATTY_IS_MA_ACCOUNT (self));

  cm_user_load_info_finish (CM_USER (object),
                            result, &error);

  if (error)
    g_task_return_error (task, error);
  else {
    CHATTY_TRACE_MSG ("Got user info for %s",
                      cm_client_get_user_id (self->cm_client));

    g_free (self->name);
    self->name = g_strdup (cm_user_get_display_name (CM_USER (object)));

    g_object_notify (G_OBJECT (self), "name");
    g_task_return_boolean (task, TRUE);
  }
}

void
chatty_ma_account_get_details_async (ChattyMaAccount     *self,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  CmAccount *account;

  g_return_if_fail (CHATTY_IS_MA_ACCOUNT (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  account = cm_client_get_account (self->cm_client);

  if (self->name)
    g_task_return_boolean (task, TRUE);
  else
    cm_user_load_info_async (CM_USER (account), cancellable,
                             ma_get_details_cb,
                             g_steal_pointer (&task));
}

gboolean
chatty_ma_account_get_details_finish (ChattyMaAccount  *self,
                                      GAsyncResult     *result,
                                      GError          **error)
{
  g_return_val_if_fail (CHATTY_IS_MA_ACCOUNT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ma_set_name_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  ChattyMaAccount *self;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (CHATTY_IS_MA_ACCOUNT (self));

  cm_account_set_display_name_finish (CM_ACCOUNT (object), result, &error);
  CHATTY_TRACE (chatty_item_get_username (CHATTY_ITEM (self)),
                "Setting name %s user:", CHATTY_LOG_SUCESS (!error));

  if (error)
    g_task_return_error (task, error);
  else {
    char *name;

    name = g_task_get_task_data (task);
    g_free (self->name);
    self->name = g_strdup (name);

    g_object_notify (G_OBJECT (self), "name");
    g_task_return_boolean (task, TRUE);
  }
}

void
chatty_ma_account_set_name_async (ChattyMaAccount     *self,
                                  const char          *name,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  GTask *task = NULL;

  g_return_if_fail (CHATTY_IS_MA_ACCOUNT (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_strdup (name), g_free);

  cm_account_set_display_name_async (cm_client_get_account (self->cm_client),
                                     name, cancellable,
                                     ma_set_name_cb, task);
}

gboolean
chatty_ma_account_set_name_finish (ChattyMaAccount  *self,
                                   GAsyncResult     *result,
                                   GError          **error)
{
  g_return_val_if_fail (CHATTY_IS_MA_ACCOUNT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ma_get_3pid_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  ChattyMaAccount *self;
  g_autoptr(GTask) task = user_data;
  GPtrArray *emails, *phones;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (CHATTY_IS_MA_ACCOUNT (self));

  cm_account_get_3pids_finish (CM_ACCOUNT (object),
                               &emails, &phones,
                               result, &error);

  if (error)
    g_task_return_error (task, error);
  else {
    g_object_set_data_full (G_OBJECT (task), "email", emails,
                            (GDestroyNotify)g_ptr_array_unref);
    g_object_set_data_full (G_OBJECT (task), "phone", phones,
                            (GDestroyNotify)g_ptr_array_unref);

    g_task_return_boolean (task, TRUE);
  }
}

void
chatty_ma_account_get_3pid_async (ChattyMaAccount     *self,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (CHATTY_IS_MA_ACCOUNT (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  cm_account_get_3pids_async (cm_client_get_account (self->cm_client),
                              cancellable,
                              ma_get_3pid_cb,
                              g_steal_pointer (&task));
}

gboolean
chatty_ma_account_get_3pid_finish (ChattyMaAccount  *self,
                                   GPtrArray       **emails,
                                   GPtrArray       **phones,
                                   GAsyncResult     *result,
                                   GError          **error)
{
  g_return_val_if_fail (CHATTY_IS_MA_ACCOUNT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  if (emails)
    *emails = g_object_steal_data (G_OBJECT (result), "email");
  if (phones)
    *phones = g_object_steal_data (G_OBJECT (result), "phone");

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ma_delete_3pid_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  ChattyMaAccount *self;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (CHATTY_IS_MA_ACCOUNT (self));

  cm_account_delete_3pid_finish (CM_ACCOUNT (object), result, &error);

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);
}

void
chatty_ma_account_delete_3pid_async (ChattyMaAccount     *self,
                                     const char          *value,
                                     ChattyIdType         type,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  GTask *task = NULL;
  const char *type_str = NULL;

  g_return_if_fail (CHATTY_IS_MA_ACCOUNT (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  if (type == CHATTY_ID_PHONE)
    type_str = "msisdn";
  else
    type_str = "email";
  cm_account_delete_3pid_async (cm_client_get_account (self->cm_client),
                                value, type_str, cancellable,
                                ma_delete_3pid_cb, task);
}

gboolean
chatty_ma_account_delete_3pid_finish (ChattyMaAccount  *self,
                                      GAsyncResult     *result,
                                      GError          **error)
{
  g_return_val_if_fail (CHATTY_IS_MA_ACCOUNT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
