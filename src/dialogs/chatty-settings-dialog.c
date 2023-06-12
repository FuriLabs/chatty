/* chatty-settings-dialog.c
 *
 * Copyright 2020 Purism SPC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s):
 *   Andrea Schäfer <mosibasu@me.com>
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-settings-dialog"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>
#define CMATRIX_USE_EXPERIMENTAL_API
#include "cmatrix.h"

#include "gtk3-to-4.h"
#include "chatty-utils.h"
#include "chatty-ma-account.h"
#include "chatty-mm-account.h"
#include "chatty-mm-account-private.h"
#include "chatty-manager.h"
#include "chatty-purple.h"
#include "chatty-fp-row.h"
#include "chatty-avatar.h"
#include "chatty-settings.h"
#include "chatty-phone-utils.h"
#include "chatty-ma-account-details.h"
#include "chatty-pp-account-details.h"
#include "chatty-settings-dialog.h"
#include "chatty-log.h"

/**
 * @short_description: Chatty settings Dialog
 */

/* Several code has been copied from chatty-dialogs.c with modifications
 * which was written by Andrea Schäfer. */
struct _ChattySettingsDialog
{
  AdwWindow      parent_instance;

  GtkWidget      *back_button;
  GtkWidget      *cancel_button;
  GtkWidget      *add_button;
  GtkWidget      *matrix_spinner;
  GtkWidget      *save_button;
  GtkWidget      *mms_cancel_button;
  GtkWidget      *mms_save_button;

  GtkWidget      *notification_revealer;
  GtkWidget      *notification_label;

  GtkWidget      *main_stack;
  GtkWidget      *accounts_list_box;
  GtkWidget      *add_account_row;

  GtkWidget      *enable_purple_row;
  GtkWidget      *enable_purple_switch;

  GtkWidget      *account_details_stack;
  GtkWidget      *pp_account_details;
  GtkWidget      *ma_account_details;

  GtkWidget      *protocol_list_group;
  GtkWidget      *protocol_list;
  GtkWidget      *xmpp_radio_button;
  GtkWidget      *matrix_row;
  GtkWidget      *matrix_radio_button;
  GtkWidget      *telegram_row;
  GtkWidget      *telegram_radio_button;
  GtkWidget      *new_account_settings_list;
  GtkWidget      *new_account_id_entry;
  GtkWidget      *new_password_entry;

  GtkWidget      *delivery_reports_switch;
  GtkWidget      *handle_smil_switch;
  GtkWidget      *blocked_list;
  GtkWidget      *phone_number_entry;
  GtkWidget      *carrier_mmsc_entry;
  GtkWidget      *mms_apn_entry;
  GtkWidget      *mms_proxy_entry;

  GtkWidget      *send_receipts_switch;
  GtkWidget      *message_archive_switch;
  GtkWidget      *message_carbons_row;
  GtkWidget      *message_carbons_switch;
  GtkWidget      *typing_notification_switch;
  GtkWidget      *strip_url_tracking_id_switch;

  GtkWidget      *convert_smileys_switch;
  GtkWidget      *return_sends_switch;

  GtkWidget      *purple_settings_row;

  GtkWidget      *matrix_homeserver_entry;

  ChattyMmAccount *mm_account;
  ChattySettings *settings;
  ChattyAccount  *selected_account;
  GCancellable   *cancellable;

  gboolean visible;
  gboolean mms_settings_loaded;
  guint    revealer_timeout_id;
};

G_DEFINE_TYPE (ChattySettingsDialog, chatty_settings_dialog, ADW_TYPE_WINDOW)

static void
settings_apply_style (GtkWidget  *widget,
                      const char *style)
{
  GtkStyleContext *context;

  g_assert (style && *style);

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_add_class (context, style);
}

static void
settings_remove_style (GtkWidget  *widget,
                       const char *style)
{
  GtkStyleContext *context;

  g_assert (style && *style);

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_remove_class (context, style);
}

static void
settings_check_librem_one (ChattySettingsDialog *self)
{
  GtkEditable *editable;
  const char *server, *user_id;

  editable = GTK_EDITABLE (self->matrix_homeserver_entry);
  server = gtk_editable_get_text (editable);
  user_id = gtk_editable_get_text (GTK_EDITABLE (self->new_account_id_entry));

  if (!server || !*server) {
    if (g_str_has_suffix (user_id, ":librem.one") ||
        g_str_has_suffix (user_id, "@librem.one"))
      gtk_editable_set_text (editable, "https://chat.librem.one");
    else if (g_str_has_suffix (user_id, "talk.puri.sm") ||
             g_str_has_suffix (user_id, "@puri.sm"))
      gtk_editable_set_text (editable, "https://talk.puri.sm");
  }
}

static void
settings_dialog_set_save_state (ChattySettingsDialog *self,
                                gboolean              in_progress)
{
  g_object_set (self->matrix_spinner, "spinning", in_progress, NULL);
  gtk_widget_set_sensitive (self->main_stack, !in_progress);
  gtk_widget_set_sensitive (self->add_button, !in_progress);
  gtk_widget_set_visible (self->back_button, !in_progress);
  gtk_widget_set_visible (self->cancel_button, in_progress);
}

static void
settings_save_account_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  ChattySettingsDialog *self = user_data;
  ChattyManager *manager = (gpointer)object;
  g_autoptr(GError) error = NULL;

  chatty_manager_save_account_finish (manager, result, &error);
  settings_dialog_set_save_state (self, FALSE);

  if (error) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (GTK_WINDOW (self),
                                       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_WARNING,
                                       GTK_BUTTONS_CLOSE,
                                       "Error saving account: %s", error->message);
      g_signal_connect_swapped (dialog, "response",
                                G_CALLBACK (gtk_window_destroy), dialog);
      gtk_window_present (GTK_WINDOW (dialog));
    }
  } else {
    gtk_widget_hide (self->add_button);
    gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack), "main-settings");
  }
}

static gboolean
dialog_notification_timeout_cb (gpointer user_data)
{
  ChattySettingsDialog *self = user_data;

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->notification_revealer), FALSE);
  g_clear_handle_id (&self->revealer_timeout_id, g_source_remove);

  return G_SOURCE_REMOVE;
}

static void
matrix_home_server_verify_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  ChattySettingsDialog *self = user_data;
  g_autoptr(ChattyMaAccount) account = NULL;
  g_autoptr(GError) error = NULL;
  const char *homeserver, *server;

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  settings_dialog_set_save_state (self, FALSE);
  /* verified homeserver URL */
  homeserver = cm_client_get_homeserver_finish (CM_CLIENT (object), result, &error);
  /* homeserver URL entered by the user */
  server = gtk_editable_get_text (GTK_EDITABLE (self->matrix_homeserver_entry));

  if (!homeserver) {
    gtk_widget_set_sensitive (self->add_button, FALSE);
    g_clear_handle_id (&self->revealer_timeout_id, g_source_remove);

    if (error)
      g_dbus_error_strip_remote_error (error);

    if (server && *server) {
      g_autofree char *label = NULL;

      if (error)
        label = g_strdup_printf (_("Failed to verify server: %s"), error->message);
      else
        label = g_strdup (_("Failed to verify server"));

      gtk_label_set_text (GTK_LABEL (self->notification_label), label);
    } else {
      settings_dialog_set_save_state (self, FALSE);
      gtk_label_set_text (GTK_LABEL (self->notification_label),
                          _("Couldn't get Home server address"));
      gtk_widget_show (self->matrix_homeserver_entry);
      gtk_entry_grab_focus_without_selecting (GTK_ENTRY (self->matrix_homeserver_entry));
      gtk_editable_set_position (GTK_EDITABLE (self->matrix_homeserver_entry), -1);
    }

    gtk_revealer_set_reveal_child (GTK_REVEALER (self->notification_revealer), TRUE);
    self->revealer_timeout_id = g_timeout_add_seconds (5, dialog_notification_timeout_cb, self);
    return;
  }

  account = chatty_ma_account_new_from_client (CM_CLIENT (object));
  chatty_manager_save_account_async (chatty_manager_get_default (), CHATTY_ACCOUNT (account),
                                     NULL, settings_save_account_cb, self);
}

static void
chatty_settings_save_matrix (ChattySettingsDialog *self,
                             const char           *user_id,
                             const char           *password)
{
  g_autoptr(CmClient) cm_client = NULL;
  g_autofree char *uri_prefixed = NULL;
  CmAccount *cm_account;
  GtkEditable *editable;
  const char *uri;
  gboolean uri_has_prefix;

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));
  g_return_if_fail (user_id && *user_id);
  g_return_if_fail (password && *password);

  settings_dialog_set_save_state (self, TRUE);

  g_clear_object (&self->cancellable);
  self->cancellable = g_cancellable_new ();
  editable = GTK_EDITABLE (self->matrix_homeserver_entry);
  uri = gtk_editable_get_text (editable);

  uri_has_prefix = g_str_has_prefix (uri, "http");

  /* Assume https by default */
  if (!uri_has_prefix)
    uri_prefixed = g_strdup_printf ("https://%s", uri);

  cm_client = chatty_manager_matrix_client_new (chatty_manager_get_default ());
  cm_account = cm_client_get_account (cm_client);
  cm_account_set_login_id (cm_account, user_id);
  cm_client_set_homeserver (cm_client, uri_has_prefix ? uri : uri_prefixed);
  cm_client_set_password (cm_client, password);
  cm_client_get_homeserver_async (cm_client, self->cancellable,
                                  matrix_home_server_verify_cb,
                                  self);
}

static void
chatty_settings_add_clicked_cb (ChattySettingsDialog *self)
{
  ChattyManager *manager;
  g_autoptr (ChattyAccount) account = NULL;
  const char *user_id, *password;
  gboolean is_matrix, is_telegram;

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  manager  = chatty_manager_get_default ();
  user_id  = gtk_editable_get_text (GTK_EDITABLE (self->new_account_id_entry));
  password = gtk_editable_get_text (GTK_EDITABLE (self->new_password_entry));

  is_matrix = gtk_check_button_get_active (GTK_CHECK_BUTTON (self->matrix_radio_button));
  is_telegram = gtk_check_button_get_active (GTK_CHECK_BUTTON (self->telegram_radio_button));

  if (is_matrix)
    settings_check_librem_one (self);

  if (is_matrix) {
    chatty_settings_save_matrix (self, user_id, password);
    return;
  }

#ifdef PURPLE_ENABLED
  if (is_telegram) {
    account = (ChattyAccount *)chatty_pp_account_new (CHATTY_PROTOCOL_TELEGRAM, user_id, NULL, FALSE);
  } else {/* XMPP */
    gboolean has_encryption;

    has_encryption = chatty_purple_has_encryption (chatty_purple_get_default ());
    account = (ChattyAccount *)chatty_pp_account_new (CHATTY_PROTOCOL_XMPP,
                                                      user_id, NULL, has_encryption);
  }
#endif

  g_return_if_fail (account);

  if (password)
    {
      chatty_account_set_password (CHATTY_ACCOUNT (account), password);

      if (!is_telegram)
        chatty_account_set_remember_password (CHATTY_ACCOUNT (account), TRUE);
    }

  chatty_account_save (CHATTY_ACCOUNT (account));

  if (!chatty_manager_get_disable_auto_login (manager))
    chatty_account_set_enabled (CHATTY_ACCOUNT (account), TRUE);

  gtk_widget_hide (self->add_button);
  gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack), "main-settings");
}

static void
pp_account_save_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  g_autoptr(ChattySettingsDialog) self = user_data;

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  g_object_set (self->matrix_spinner, "spinning", FALSE, NULL);
  gtk_widget_set_sensitive (self->account_details_stack, TRUE);

  gtk_widget_hide (self->save_button);
  gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack), "main-settings");
}

static void
chatty_settings_save_clicked_cb (ChattySettingsDialog *self)
{
  GtkStack *stack;

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  stack = GTK_STACK (self->account_details_stack);

  g_object_set (self->matrix_spinner, "spinning", TRUE, NULL);
  gtk_widget_set_sensitive (self->account_details_stack, FALSE);

  if (gtk_stack_get_visible_child (stack) == self->pp_account_details)
    chatty_pp_account_save_async (CHATTY_PP_ACCOUNT_DETAILS (self->pp_account_details),
                                  pp_account_save_cb, g_object_ref (self));
  else if (gtk_stack_get_visible_child (stack) == self->ma_account_details)
    chatty_ma_account_details_save_async (CHATTY_MA_ACCOUNT_DETAILS (self->ma_account_details),
                                          pp_account_save_cb, g_object_ref (self));
}

static void
settings_pp_details_changed_cb (ChattySettingsDialog *self,
                                GParamSpec           *pspec,
                                GtkWidget            *details)
{
  gboolean modified;

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));
  g_assert (GTK_IS_WIDGET (details));

  g_object_get (details, "modified", &modified, NULL);
  gtk_widget_set_sensitive (self->save_button, modified);
}

static void
settings_delete_account_response_cb (ChattySettingsDialog *self,
                                     int                   response_id,
                                     GtkDialog            *dialog)
{
  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));
  g_assert (GTK_IS_DIALOG (dialog));

  if (response_id == GTK_RESPONSE_OK) {
    chatty_manager_delete_account_async (chatty_manager_get_default (),
                                         g_steal_pointer (&self->selected_account),
                                         NULL, NULL, NULL);

    gtk_widget_hide (self->save_button);
    gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack), "main-settings");
  }

  gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
settings_delete_account_clicked_cb (ChattySettingsDialog *self)
{
  GtkWidget *dialog;
  const char *username;

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  if (CHATTY_IS_MA_ACCOUNT (self->selected_account))
    username = chatty_ma_account_get_login_username (CHATTY_MA_ACCOUNT (self->selected_account));
  else
    username = chatty_item_get_username (CHATTY_ITEM (self->selected_account));

  dialog = gtk_message_dialog_new ((GtkWindow*)self,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_OK_CANCEL,
                                   _("Delete Account"));

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("Delete account %s?"), username);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

  g_signal_connect_object (dialog, "response",
                           G_CALLBACK (settings_delete_account_response_cb),
                           self, G_CONNECT_SWAPPED);
  gtk_window_present (GTK_WINDOW (dialog));
}

static void
settings_pp_details_delete_cb (ChattySettingsDialog *self)
{
  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  settings_delete_account_clicked_cb (self);
}

static void
settings_dialog_load_mms_settings (ChattySettingsDialog *self)
{
  g_autoptr(ChattyMmDevice) device = NULL;
  g_autofree char *device_number = NULL;
  GListModel *devices;
  const char *apn, *mmsc, *proxy, *number;
  gboolean use_smil;

  devices = chatty_mm_account_get_devices (self->mm_account);
  device = g_list_model_get_item (devices, 0);

  if (device)
    device_number = chatty_mm_device_get_number (device);

  gtk_widget_set_visible (self->phone_number_entry, !device_number);

  if (chatty_mm_account_get_mms_settings (self->mm_account, &apn, &mmsc, &proxy, &number, &use_smil)) {
    gtk_editable_set_text (GTK_EDITABLE (self->mms_apn_entry), apn);
    gtk_editable_set_text (GTK_EDITABLE (self->carrier_mmsc_entry), mmsc);
    gtk_editable_set_text (GTK_EDITABLE (self->mms_proxy_entry), proxy);
    gtk_editable_set_text (GTK_EDITABLE (self->phone_number_entry), number);
    gtk_switch_set_state (GTK_SWITCH (self->handle_smil_switch), use_smil);

    self->mms_settings_loaded = TRUE;
  }
}

static void
sms_mms_settings_row_activated_cb (ChattySettingsDialog *self)
{
  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  gtk_widget_show (self->mms_save_button);
  gtk_widget_show (self->mms_cancel_button);
  gtk_widget_hide (self->back_button);

  settings_dialog_load_mms_settings (self);

  gtk_switch_set_state (GTK_SWITCH (self->delivery_reports_switch),
                        chatty_settings_request_sms_delivery_reports (self->settings));

  gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack), "message-settings-view");
}

static void
purple_settings_row_activated_cb (ChattySettingsDialog *self)
{
  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack), "purple-settings-view");
}

static void
settings_phone_number_entry_changed_cb (ChattySettingsDialog *self,
                                        GtkEntry             *number_entry)
{
  GtkStyleContext *context;
  const char *number;
  gboolean valid = TRUE;

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));
  g_assert (GTK_IS_EDITABLE (number_entry));

  if (!gtk_widget_get_visible (GTK_WIDGET (number_entry)))
    goto end;

  number = gtk_editable_get_text (GTK_EDITABLE (number_entry));

  if (!number || !*number)
    goto end;

  if (number && *number == '+')
    valid = TRUE;

  /* Since the number is supposed to be in E164 fomat, country code shouldn't matter */
  if (valid && number && *number)
    valid = chatty_phone_utils_is_valid (number, "+1");

 end:
  context = gtk_widget_get_style_context (GTK_WIDGET (number_entry));

  if (valid)
    gtk_style_context_remove_class (context, "error");
  else
    gtk_style_context_add_class (context, "error");


  gtk_widget_set_sensitive (self->mms_save_button, valid);
}

static void
settings_homeserver_entry_changed (ChattySettingsDialog *self,
                                   GtkEntry             *entry)
{
  const char *server;
  gboolean valid = FALSE;

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));
  g_assert (GTK_IS_EDITABLE (entry));

  if (!gtk_widget_is_visible (GTK_WIDGET (entry)))
    return;

  server = gtk_editable_get_text (GTK_EDITABLE (entry));

  if (server && *server) {
    g_autoptr(GUri) uri = NULL;
    g_autofree char *server_prefixed = NULL;
    const char *scheme = NULL;
    const char *path = NULL;
    const char *host = NULL;
    gboolean server_has_prefix = g_str_has_prefix (server, "http");

    /* Assume https by default */
    if (!server_has_prefix)
      server_prefixed = g_strdup_printf ("https://%s", server);

    uri = g_uri_parse (server_has_prefix ? server : server_prefixed, G_URI_FLAGS_NONE, NULL);
    if (uri) {
      scheme = g_uri_get_scheme (uri);
      path = g_uri_get_path (uri);
      host = g_uri_get_host (uri);
    }

    valid = scheme && *scheme;
    valid = valid && (g_str_equal (scheme, "http") || g_str_equal (scheme, "https"));
    valid = valid && host && *host;
    valid = valid && !g_str_has_suffix (host, ".");
    valid = valid && (!path || !*path);
  }

  if (valid)
    settings_remove_style (GTK_WIDGET (entry), "error");
  else
    settings_apply_style (GTK_WIDGET (entry), "error");

  gtk_widget_set_sensitive (self->add_button, valid);
}

static void
settings_dialog_purple_changed_cb (ChattySettingsDialog *self)
{
#ifdef PURPLE_ENABLED
  ChattyPurple *purple;
  AdwActionRow *row;
  gboolean active;

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  row = ADW_ACTION_ROW (self->enable_purple_row);
  purple = chatty_purple_get_default ();
  active = gtk_switch_get_active (GTK_SWITCH (self->enable_purple_switch));

  if (!active && chatty_purple_is_loaded (purple))
    adw_action_row_set_subtitle (row, _("Restart chatty to disable purple"));
  else
    adw_action_row_set_subtitle (row, _("Enable purple plugin"));
#endif
}

static void
settings_dialog_page_changed_cb (ChattySettingsDialog *self)
{
  const char *name;

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  name = gtk_stack_get_visible_child_name (GTK_STACK (self->main_stack));

  if (g_strcmp0 (name, "message-settings-view") == 0)
    gtk_window_set_title (GTK_WINDOW (self), _("SMS and MMS Settings"));
  else if (g_strcmp0 (name, "purple-settings-view") == 0)
    gtk_window_set_title (GTK_WINDOW (self), _("Purple Settings"));
  else if (g_strcmp0 (name, "add-account-view") == 0)
    gtk_window_set_title (GTK_WINDOW (self), _("New Account"));
  else if (g_strcmp0 (name, "blocked-list-view") == 0)
    gtk_window_set_title (GTK_WINDOW (self), _("Blocked Contacts"));
  else
    gtk_window_set_title (GTK_WINDOW (self), _("Preferences"));
}

static void
settings_update_new_account_view (ChattySettingsDialog *self)
{
  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  gtk_widget_hide (self->matrix_homeserver_entry);

  gtk_editable_set_text (GTK_EDITABLE (self->matrix_homeserver_entry), "");
  gtk_editable_set_text (GTK_EDITABLE (self->new_account_id_entry), "");
  gtk_editable_set_text (GTK_EDITABLE (self->new_password_entry), "");

  self->selected_account = NULL;
  gtk_widget_grab_focus (self->new_account_id_entry);
  gtk_widget_show (self->add_button);

  gtk_widget_set_visible (self->matrix_row, TRUE);

#ifdef PURPLE_ENABLED
  /* if (purple_find_prpl ("prpl-matrix")) */
  /*   gtk_widget_set_visible (self->matrix_row, TRUE); */

  gtk_widget_set_visible (self->telegram_row, purple_find_prpl ("prpl-telegram") != NULL);
#endif

  adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (self->protocol_list_group),
                                   _("Select Protocol"));
  gtk_widget_show (self->protocol_list);

  if (gtk_widget_is_visible (self->xmpp_radio_button))
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->xmpp_radio_button), TRUE);
  else if (gtk_widget_is_visible (self->matrix_radio_button))
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->matrix_radio_button), TRUE);

  gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack), "add-account-view");
}

static void
chatty_settings_dialog_update_status (GtkListBoxRow *row)
{
  ChattyAccount *account;
  GtkSpinner *spinner;
  ChattyStatus status;

  g_assert (GTK_IS_LIST_BOX_ROW (row));

  spinner = g_object_get_data (G_OBJECT(row), "row-prefix");
  account = g_object_get_data (G_OBJECT (row), "row-account");
  status  = chatty_account_get_status (CHATTY_ACCOUNT (account));

  g_object_set (spinner,
                "spinning", status == CHATTY_CONNECTING,
                NULL);
}

static void
mms_carrier_settings_apply_button_clicked_cb (ChattySettingsDialog *self)
{
  ChattyAccount *mm_account;
  const char *apn, *mmsc, *proxy, *phone_number;
  gboolean use_smil;

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  mm_account = chatty_manager_get_mm_account (chatty_manager_get_default ());
  apn = gtk_editable_get_text (GTK_EDITABLE (self->mms_apn_entry));
  mmsc = gtk_editable_get_text (GTK_EDITABLE (self->carrier_mmsc_entry));
  proxy = gtk_editable_get_text (GTK_EDITABLE (self->mms_proxy_entry));
  phone_number = gtk_editable_get_text (GTK_EDITABLE (self->phone_number_entry));
  use_smil = gtk_switch_get_state (GTK_SWITCH (self->handle_smil_switch));

  chatty_mm_account_set_mms_settings_async (CHATTY_MM_ACCOUNT (mm_account),
                                            apn, mmsc, proxy, phone_number, use_smil,
                                            NULL, NULL, NULL);

  g_object_set (self->settings,
                "request-sms-delivery-reports",
                gtk_switch_get_state (GTK_SWITCH (self->delivery_reports_switch)),
                NULL);

  gtk_widget_hide (self->mms_cancel_button);
  gtk_widget_hide (self->mms_save_button);
  gtk_widget_show (self->back_button);
  gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack), "main-settings");
}

static void
mms_carrier_settings_cancel_button_clicked_cb (ChattySettingsDialog *self)
{
  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  gtk_widget_hide (self->mms_cancel_button);
  gtk_widget_hide (self->mms_save_button);
  gtk_widget_show (self->back_button);
  gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack), "main-settings");
}

static void
account_list_row_activated_cb (ChattySettingsDialog *self,
                               GtkListBoxRow        *row,
                               GtkListBox           *box)
{
  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (box));

  gtk_widget_set_sensitive (self->add_button, FALSE);
  gtk_widget_set_sensitive (self->save_button, FALSE);

  if (GTK_WIDGET (row) == self->add_account_row)
    {
      settings_update_new_account_view (self);
    }
  else
    {
      gtk_widget_show (self->save_button);
      self->selected_account = g_object_get_data (G_OBJECT (row), "row-account");
      g_assert (self->selected_account != NULL);

      if (CHATTY_IS_MA_ACCOUNT (self->selected_account)) {
        chatty_ma_account_details_set_item (CHATTY_MA_ACCOUNT_DETAILS (self->ma_account_details),
                                            self->selected_account);
        chatty_pp_account_details_set_item (CHATTY_PP_ACCOUNT_DETAILS (self->pp_account_details), NULL);
        gtk_stack_set_visible_child (GTK_STACK (self->account_details_stack), self->ma_account_details);
      } else {
        chatty_pp_account_details_set_item (CHATTY_PP_ACCOUNT_DETAILS (self->pp_account_details),
                                            self->selected_account);
        chatty_ma_account_details_set_item (CHATTY_MA_ACCOUNT_DETAILS (self->ma_account_details), NULL);
        gtk_stack_set_visible_child (GTK_STACK (self->account_details_stack), self->pp_account_details);
      }

      chatty_settings_dialog_update_status (row);
      gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack),
                                        "edit-account-view");
    }
}

static void
blocked_list_action_activated_cb (ChattySettingsDialog *self)
{
  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  gtk_widget_hide (self->mms_save_button);
  gtk_widget_hide (self->mms_cancel_button);
  gtk_widget_show (self->back_button);

  gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack), "blocked-list-view");
}

static void
chatty_settings_back_clicked_cb (ChattySettingsDialog *self)
{
  const gchar *visible_child;

  visible_child = gtk_stack_get_visible_child_name (GTK_STACK (self->main_stack));

  if (g_str_equal (visible_child, "blocked-list-view"))
    {
      gtk_widget_show (self->mms_save_button);
      gtk_widget_show (self->mms_cancel_button);
      gtk_widget_hide (self->back_button);

      gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack), "message-settings-view");
    }
  else if (g_str_equal (visible_child, "main-settings"))
    {
        gtk_widget_hide (GTK_WIDGET (self));
    }
  else
    {
      gtk_widget_hide (self->add_button);
      gtk_widget_hide (self->save_button);
      gtk_stack_set_visible_child_name (GTK_STACK (self->main_stack), "main-settings");
    }
}

static void
chatty_settings_cancel_clicked_cb (ChattySettingsDialog *self)
{
  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  if (self->cancellable)
    g_cancellable_cancel (self->cancellable);

  g_object_set (self->matrix_spinner, "spinning", FALSE, NULL);
  gtk_widget_set_sensitive (self->main_stack, TRUE);
  gtk_widget_set_sensitive (self->add_button, TRUE);
  gtk_widget_hide (self->cancel_button);
  gtk_widget_show (self->back_button);
}

static void
settings_new_detail_changed_cb (ChattySettingsDialog *self)
{
  const gchar *id, *password;
  ChattyProtocol protocol, valid_protocol;
  gboolean valid = TRUE;

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  id = gtk_editable_get_text (GTK_EDITABLE (self->new_account_id_entry));
  password = gtk_editable_get_text (GTK_EDITABLE (self->new_password_entry));

  if (password && *password)
    settings_remove_style (GTK_WIDGET (self->new_password_entry), "error");
  else
    settings_apply_style (GTK_WIDGET (self->new_password_entry), "error");

  if (!id || !*id) {
    gtk_editable_set_text (GTK_EDITABLE (self->matrix_homeserver_entry), "");
    gtk_widget_show (self->matrix_homeserver_entry);
    gtk_widget_hide (self->matrix_homeserver_entry);
  }

  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (self->matrix_radio_button)))
    protocol = CHATTY_PROTOCOL_MATRIX;
  else if (gtk_check_button_get_active (GTK_CHECK_BUTTON (self->telegram_radio_button)))
    protocol = CHATTY_PROTOCOL_TELEGRAM;
  else
    protocol = CHATTY_PROTOCOL_XMPP;

  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (self->matrix_radio_button)))
    protocol = CHATTY_PROTOCOL_MATRIX | CHATTY_PROTOCOL_EMAIL;

  valid_protocol = chatty_utils_username_is_valid (id, protocol);
  valid = valid && valid_protocol;

  if (valid_protocol &&
      gtk_check_button_get_active (GTK_CHECK_BUTTON (self->matrix_radio_button)))
    valid_protocol = !chatty_manager_has_matrix_with_id (chatty_manager_get_default (), id);

  if (valid_protocol)
    settings_remove_style (GTK_WIDGET (self->new_account_id_entry), "error");
  else
    settings_apply_style (GTK_WIDGET (self->new_account_id_entry), "error");

  /* If user tried to login to matrix via email, ask for homeserver details */
  if (protocol & CHATTY_PROTOCOL_MATRIX &&
      valid_protocol & CHATTY_PROTOCOL_EMAIL) {
    gtk_widget_show (self->matrix_homeserver_entry);
    settings_check_librem_one (self);
    settings_homeserver_entry_changed (self, GTK_ENTRY (self->matrix_homeserver_entry));
  }

  /* Allow empty passwords for telegram accounts */
  if (protocol != CHATTY_PROTOCOL_TELEGRAM)
    valid = valid && password && *password;

  /* Don’t allow adding if an account with same id exists */
  if (valid &&
      chatty_manager_find_account_with_name (chatty_manager_get_default (), protocol, id))
    valid = FALSE;

  gtk_widget_set_sensitive (self->add_button, valid);
}

static void
settings_protocol_changed_cb (ChattySettingsDialog *self,
                              GtkWidget            *button)
{
  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));
  g_assert (GTK_IS_CHECK_BUTTON (button));

  gtk_widget_grab_focus (self->new_account_id_entry);

  if (button == self->xmpp_radio_button)
    gtk_entry_set_placeholder_text (GTK_ENTRY (self->new_account_id_entry),
                                    "user@example.com");
  else if (button == self->matrix_radio_button)
    gtk_entry_set_placeholder_text (GTK_ENTRY (self->new_account_id_entry),
                                    /* TRANSLATORS: Only translate 'or' */
                                    _("@user:matrix.org or user@example.com"));
  else /* Telegram */
    gtk_entry_set_placeholder_text (GTK_ENTRY (self->new_account_id_entry),
                                    "+1123456789");

  /* Force re-check if id is valid */
  settings_new_detail_changed_cb (self);
}

static GtkWidget *
chatty_account_row_new (ChattyAccount *account)
{
  AdwActionRow   *row;
  GtkWidget      *account_enabled_switch;
  GtkWidget      *spinner;
  const char     *username;

  row = ADW_ACTION_ROW (adw_action_row_new ());
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);
  g_object_set_data (G_OBJECT(row),
                     "row-account",
                     (gpointer) account);

  spinner = gtk_spinner_new ();
  gtk_widget_show (spinner);
  adw_action_row_add_prefix (row, spinner);

  g_object_set_data (G_OBJECT(row),
                     "row-prefix",
                     (gpointer)spinner);

  account_enabled_switch = gtk_switch_new ();

  g_object_set  (G_OBJECT(account_enabled_switch),
                 "valign", GTK_ALIGN_CENTER,
                 "halign", GTK_ALIGN_END,
                 NULL);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), account_enabled_switch);

  g_object_bind_property (account, "enabled",
                          account_enabled_switch, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  if (CHATTY_IS_MA_ACCOUNT (account))
    username = chatty_ma_account_get_login_username (CHATTY_MA_ACCOUNT (account));
  else
    username = chatty_item_get_username (CHATTY_ITEM (account));

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), username);
  adw_action_row_set_subtitle (row, chatty_account_get_protocol_name (CHATTY_ACCOUNT (account)));
  adw_action_row_set_activatable_widget (row, NULL);

  g_signal_connect_object (account, "notify::status",
                           G_CALLBACK (chatty_settings_dialog_update_status),
                           row, G_CONNECT_SWAPPED);
  chatty_settings_dialog_update_status (GTK_LIST_BOX_ROW (row));

  return GTK_WIDGET (row);
}

static void
blocked_list_items_changed_cb (ChattySettingsDialog *self)
{
  GListModel *list;
  gboolean empty;

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  list = chatty_mm_account_get_blocked_chat_list (self->mm_account);
  empty = g_list_model_get_n_items (list) == 0;

  gtk_widget_set_visible (self->blocked_list, !empty);
}

static void
row_unblock_button_clicked_cb (AdwActionRow *row)
{
  ChattyChat *chat;

  g_assert (ADW_IS_ACTION_ROW (row));

  chat = g_object_get_data (G_OBJECT (row), "chat");
  g_assert (chat);

  chatty_item_set_state (CHATTY_ITEM (chat), CHATTY_ITEM_VISIBLE);
}

static GtkWidget *
chatty_settings_blocked_row_new (ChattyChat           *chat,
                                 ChattySettingsDialog *self)
{
  AdwActionRow *row;
  GtkWidget *button;
  const char *username;

  g_assert (CHATTY_IS_CHAT (chat));
  row = ADW_ACTION_ROW (adw_action_row_new ());
  g_object_set_data (G_OBJECT (row), "chat", chat);

  button = gtk_button_new_from_icon_name ("user-trash-symbolic");
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), button);

  g_signal_connect_object (button, "clicked",
                           G_CALLBACK (row_unblock_button_clicked_cb),
                           row, G_CONNECT_SWAPPED);

  username = chatty_chat_get_chat_name (chat);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), username);

  return GTK_WIDGET (row);
}

static GtkWidget *
chatty_settings_account_row_new (ChattyAccount        *account,
                                 ChattySettingsDialog *self)
{
  /* mm account is the last item, and we use the row to show new account row */
  if (CHATTY_IS_MM_ACCOUNT (account))
    return self->add_account_row;
  else
    return chatty_account_row_new (account);
}

static void
settings_dialog_mm_status_changed (ChattySettingsDialog *self)
{
  gboolean has_mms;

  g_assert (CHATTY_IS_SETTINGS_DIALOG (self));

  has_mms = chatty_mm_account_has_mms_feature (self->mm_account);
  gtk_widget_set_sensitive (self->handle_smil_switch, has_mms);
  gtk_widget_set_sensitive (self->carrier_mmsc_entry, has_mms);
  gtk_widget_set_sensitive (self->mms_apn_entry, has_mms);
  gtk_widget_set_sensitive (self->mms_proxy_entry, has_mms);

  if (has_mms && !self->mms_settings_loaded)
    settings_dialog_load_mms_settings (self);
}

static void
chatty_settings_dialog_constructed (GObject *object)
{
  ChattySettingsDialog *self = (ChattySettingsDialog *)object;
  ChattySettings *settings;

  G_OBJECT_CLASS (chatty_settings_dialog_parent_class)->constructed (object);

  settings = chatty_settings_get_default ();
  self->settings = g_object_ref (settings);

  g_object_bind_property (settings, "message-carbons",
                          self->message_carbons_switch, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (settings, "purple-enabled",
                          self->enable_purple_switch, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (settings, "send-receipts",
                          self->send_receipts_switch, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (settings, "mam-enabled",
                          self->message_archive_switch, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (settings, "send-typing",
                          self->typing_notification_switch, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (settings, "strip-url-tracking-id",
                          self->strip_url_tracking_id_switch, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  g_object_bind_property (settings, "convert-emoticons",
                          self->convert_smileys_switch, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property (settings, "return-sends-message",
                          self->return_sends_switch, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
}

static void
chatty_settings_dialog_finalize (GObject *object)
{
  ChattySettingsDialog *self = (ChattySettingsDialog *)object;

  g_clear_handle_id (&self->revealer_timeout_id, g_source_remove);
  if (self->cancellable)
    g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (chatty_settings_dialog_parent_class)->finalize (object);
}

static void
chatty_settings_dialog_class_init (ChattySettingsDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed  = chatty_settings_dialog_constructed;
  object_class->finalize = chatty_settings_dialog_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/Chatty/"
                                               "ui/chatty-settings-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, back_button);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, add_button);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, matrix_spinner);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, save_button);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, mms_cancel_button);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, mms_save_button);

  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, notification_revealer);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, notification_label);

  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, main_stack);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, accounts_list_box);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, add_account_row);

  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, enable_purple_row);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, enable_purple_switch);

  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, account_details_stack);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, pp_account_details);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, ma_account_details);

  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, protocol_list_group);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, protocol_list);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, xmpp_radio_button);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, matrix_row);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, matrix_radio_button);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, telegram_row);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, telegram_radio_button);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, new_account_settings_list);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, new_account_id_entry);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, new_password_entry);

  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, delivery_reports_switch);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, handle_smil_switch);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, blocked_list);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, phone_number_entry);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, carrier_mmsc_entry);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, mms_apn_entry);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, mms_proxy_entry);

  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, send_receipts_switch);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, message_archive_switch);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, message_carbons_row);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, message_carbons_switch);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, typing_notification_switch);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, strip_url_tracking_id_switch);

  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, convert_smileys_switch);
  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, return_sends_switch);

  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, purple_settings_row);

  gtk_widget_class_bind_template_child (widget_class, ChattySettingsDialog, matrix_homeserver_entry);

  gtk_widget_class_bind_template_callback (widget_class, chatty_settings_add_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, chatty_settings_save_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, settings_pp_details_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, settings_pp_details_delete_cb);
  gtk_widget_class_bind_template_callback (widget_class, sms_mms_settings_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, purple_settings_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, settings_phone_number_entry_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, mms_carrier_settings_cancel_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, mms_carrier_settings_apply_button_clicked_cb);

  gtk_widget_class_bind_template_callback (widget_class, account_list_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, blocked_list_action_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, chatty_settings_back_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, chatty_settings_cancel_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, settings_new_detail_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, settings_protocol_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, settings_homeserver_entry_changed);

  gtk_widget_class_bind_template_callback (widget_class, settings_dialog_purple_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, settings_dialog_page_changed_cb);
}

static void
chatty_settings_dialog_init (ChattySettingsDialog *self)
{
  ChattyManager *manager;
  gboolean show_account_box = FALSE;

  manager = chatty_manager_get_default ();
  self->mm_account = (ChattyMmAccount *)chatty_manager_get_mm_account (manager);

  gtk_widget_init_template (GTK_WIDGET (self));
  settings_dialog_page_changed_cb (self);

#ifdef PURPLE_ENABLED
  {
    ChattyPurple *purple;

    purple = chatty_purple_get_default ();
    gtk_widget_show (self->purple_settings_row);
    gtk_widget_set_visible (self->message_carbons_row,
                            chatty_purple_has_carbon_plugin (purple));
    g_object_bind_property (purple, "enabled",
                            self->xmpp_radio_button, "visible",
                            G_BINDING_SYNC_CREATE);
    g_object_bind_property (purple, "enabled",
                            self->accounts_list_box, "visible",
                            G_BINDING_SYNC_CREATE);
    if (chatty_purple_is_loaded (purple))
      show_account_box = TRUE;
  }
#else
  gtk_widget_hide (self->xmpp_radio_button);
#endif

  show_account_box = TRUE;

  gtk_widget_set_visible (self->accounts_list_box, show_account_box);

  g_signal_connect_object (chatty_mm_account_get_blocked_chat_list (self->mm_account),
                           "items-changed",
                           G_CALLBACK (blocked_list_items_changed_cb),
                           self, G_CONNECT_SWAPPED);
  gtk_list_box_bind_model (GTK_LIST_BOX (self->blocked_list),
                           chatty_mm_account_get_blocked_chat_list (self->mm_account),
                           (GtkListBoxCreateWidgetFunc)chatty_settings_blocked_row_new,
                           g_object_ref (self), g_object_unref);
  gtk_list_box_bind_model (GTK_LIST_BOX (self->accounts_list_box),
                           chatty_manager_get_accounts (manager),
                           (GtkListBoxCreateWidgetFunc)chatty_settings_account_row_new,
                           g_object_ref (self), g_object_unref);
  g_signal_connect_object (self->mm_account, "notify::status",
                           G_CALLBACK (settings_dialog_mm_status_changed),
                           self, G_CONNECT_SWAPPED);
  settings_dialog_mm_status_changed (self);
}

GtkWidget *
chatty_settings_dialog_new (GtkWindow *parent_window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (parent_window), NULL);

  return g_object_new (CHATTY_TYPE_SETTINGS_DIALOG,
                       "transient-for", parent_window,
                       "modal", TRUE,
                       NULL);
}
