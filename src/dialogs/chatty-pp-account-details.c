/* chatty-pp-account-details.c
 *
 * Copyright 2021 Purism SPC
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
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-pp-account-details"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>

#include "chatty-avatar.h"
#include "chatty-fp-row.h"
#include "chatty-pp-account-details.h"
#include "chatty-log.h"

struct _ChattyPpAccountDetails
{
  AdwPreferencesPage parent_instance;

  ChattyAccount *account;

  GtkWidget     *avatar_image;
  GtkWidget     *delete_avatar_button;
  GtkWidget     *edit_avatar_button;

  GtkWidget     *account_id_row;
  GtkWidget     *account_protocol_row;
  GtkWidget     *status_row;
  GtkWidget     *password_row;
  GtkWidget     *delete_account_button;

  GtkWidget     *device_fp;
  GtkWidget     *device_fp_list;

  guint          modified : 1;
  gulong         status_id;
  gulong         avatar_changed_id;
};

enum {
  DELETE_CLICKED,
  N_SIGNALS
};

enum {
  PROP_0,
  PROP_MODIFIED,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

G_DEFINE_TYPE (ChattyPpAccountDetails, chatty_pp_account_details, ADW_TYPE_PREFERENCES_PAGE)

static void
pp_account_load_avatar_response_cb (GObject         *dialog,
                                    GAsyncResult    *response,
                                    gpointer        user_data)
{
  ChattyPpAccountDetails *self = CHATTY_PP_ACCOUNT_DETAILS (user_data);
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree char *file_name = NULL;

  g_assert (CHATTY_IS_PP_ACCOUNT_DETAILS (self));
  g_assert (GTK_IS_DIALOG (dialog));

  file = gtk_file_dialog_open_finish (GTK_FILE_DIALOG (dialog), response, &error);
  g_clear_object (&dialog);

  if (!file) {
    if (!g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
      g_warning ("Error getting file: %s", error->message);
    return;
  }

  file_name = g_file_get_path (file);

  if (file_name)
    chatty_item_set_avatar_async (CHATTY_ITEM (self->account),
                                  file_name, NULL, NULL, NULL);
}

static void
pp_account_show_dialog_load_avatar (ChattyPpAccountDetails *self)
{
  GtkFileFilter *filter;
  GtkWidget *window;
  GtkFileDialog *dialog;

  window = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);
  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, _("Set Avatar"));

  gtk_native_dialog_set_transient_for (GTK_NATIVE_DIALOG (dialog), GTK_WINDOW (window));
  gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (dialog), TRUE);

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_mime_type (filter, "image/*");
  gtk_file_filter_set_name (filter, _("Images"));
  gtk_file_dialog_set_default_filter (dialog, filter);

  gtk_file_dialog_open (dialog, GTK_WINDOW (window), NULL, pp_account_load_avatar_response_cb, self);
}

static void
pp_details_delete_avatar_button_clicked_cb (ChattyPpAccountDetails *self)
{
  g_assert (CHATTY_IS_PP_ACCOUNT_DETAILS (self));

  chatty_item_set_avatar_async (CHATTY_ITEM (self->account),
                                NULL, NULL, NULL, NULL);
}

static void
pp_details_edit_avatar_button_clicked_cb (ChattyPpAccountDetails *self)
{
  pp_account_show_dialog_load_avatar (self);
}

static void
pa_details_pw_entry_changed_cb (ChattyPpAccountDetails *self)
{
  g_assert (CHATTY_IS_PP_ACCOUNT_DETAILS (self));

  self->modified = TRUE;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODIFIED]);
}

static void
pa_details_delete_account_clicked_cb (ChattyPpAccountDetails *self)
{
  g_assert (CHATTY_IS_PP_ACCOUNT_DETAILS (self));

  g_signal_emit (self, signals[DELETE_CLICKED], 0);
}

static void
pp_details_status_changed_cb (ChattyPpAccountDetails *self)
{
  const char *status_text;
  ChattyStatus status;

  g_assert (CHATTY_IS_PP_ACCOUNT_DETAILS (self));

  status = chatty_account_get_status (self->account);

  if (status == CHATTY_CONNECTED)
    status_text = _("connected");
  else if (status == CHATTY_CONNECTING)
    status_text = _("connecting…");
  else
    status_text = _("disconnected");

  adw_action_row_set_subtitle (ADW_ACTION_ROW (self->status_row), status_text);
}

static void
pp_details_avatar_changed_cb (ChattyPpAccountDetails *self)
{
  GdkPixbuf *avatar;

  g_assert (CHATTY_IS_PP_ACCOUNT_DETAILS (self));

  if (!self->account)
    return;

  avatar = chatty_item_get_avatar (CHATTY_ITEM (self->account));
  gtk_widget_set_visible (self->delete_avatar_button, !!avatar);
}

static void
pp_details_get_fingerprints_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  g_autoptr(ChattyPpAccountDetails) self = user_data;
  ChattyAccount *account = CHATTY_ACCOUNT (object);
  g_autoptr(GError) error = NULL;
  GListModel *fp_list;
  GtkStringObject *device_fp;

  chatty_account_load_fp_finish (account, result, &error);

  if (error) {
    g_warning ("error: %s", error->message);
    return;
  }

  device_fp = chatty_account_get_device_fp (account);
  fp_list = chatty_account_get_fp_list (account);

  gtk_widget_set_visible (self->device_fp_list, !!device_fp);
  gtk_widget_set_visible (self->device_fp, !!device_fp);

  gtk_widget_set_visible (self->device_fp_list,
                          fp_list && g_list_model_get_n_items (fp_list));

  gtk_list_box_bind_model (GTK_LIST_BOX (self->device_fp_list),
                           fp_list,
                           (GtkListBoxCreateWidgetFunc) chatty_fp_row_new,
                           NULL, NULL);
  if (device_fp)
    chatty_fp_row_set_item (CHATTY_FP_ROW (self->device_fp), device_fp);
}

static void
chatty_pp_account_details_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  ChattyPpAccountDetails *self = (ChattyPpAccountDetails *)object;

  switch (prop_id)
    {
    case PROP_MODIFIED:
      g_value_set_boolean (value, self->modified);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
chatty_pp_account_details_finalize (GObject *object)
{
  ChattyPpAccountDetails *self = (ChattyPpAccountDetails *)object;

  g_clear_object (&self->account);

  G_OBJECT_CLASS (chatty_pp_account_details_parent_class)->finalize (object);
}

static void
chatty_pp_account_details_class_init (ChattyPpAccountDetailsClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = chatty_pp_account_details_get_property;
  object_class->finalize = chatty_pp_account_details_finalize;

  signals [DELETE_CLICKED] =
    g_signal_new ("delete-clicked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  properties[PROP_MODIFIED] =
    g_param_spec_boolean ("modified",
                          "Modified",
                          "If Settings is modified",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/Chatty/"
                                               "ui/chatty-pp-account-details.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyPpAccountDetails, avatar_image);
  gtk_widget_class_bind_template_child (widget_class, ChattyPpAccountDetails, delete_avatar_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyPpAccountDetails, edit_avatar_button);

  gtk_widget_class_bind_template_child (widget_class, ChattyPpAccountDetails, account_id_row);
  gtk_widget_class_bind_template_child (widget_class, ChattyPpAccountDetails, account_protocol_row);
  gtk_widget_class_bind_template_child (widget_class, ChattyPpAccountDetails, status_row);
  gtk_widget_class_bind_template_child (widget_class, ChattyPpAccountDetails, password_row);
  gtk_widget_class_bind_template_child (widget_class, ChattyPpAccountDetails, delete_account_button);

  gtk_widget_class_bind_template_child (widget_class, ChattyPpAccountDetails, device_fp);
  gtk_widget_class_bind_template_child (widget_class, ChattyPpAccountDetails, device_fp_list);

  gtk_widget_class_bind_template_callback (widget_class, pp_details_delete_avatar_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, pp_details_edit_avatar_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, pa_details_pw_entry_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, pa_details_delete_account_clicked_cb);
}

static void
chatty_pp_account_details_init (ChattyPpAccountDetails *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
chatty_pp_account_details_new (void)
{
  return g_object_new (CHATTY_TYPE_PP_ACCOUNT_DETAILS, NULL);
}

void
chatty_pp_account_save_async (ChattyPpAccountDetails *self,
                              GAsyncReadyCallback     callback,
                              gpointer                user_data)
{
  g_autoptr(GTask) task = NULL;
  AdwPasswordEntryRow *row;

  g_return_if_fail (CHATTY_IS_PP_ACCOUNT_DETAILS (self));
  g_return_if_fail (callback);

  row = (AdwPasswordEntryRow *)self->password_row;
  chatty_account_set_password (self->account, gtk_editable_get_text (GTK_EDITABLE (row)));

  chatty_account_set_remember_password (self->account, TRUE);
  chatty_account_set_enabled (self->account, TRUE);

  gtk_editable_set_text (GTK_EDITABLE (row), "");

  self->modified = FALSE;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODIFIED]);

  task = g_task_new (self, NULL, callback, user_data);
  g_task_return_boolean (task, TRUE);
}

gboolean
chatty_pp_account_save_finish (ChattyPpAccountDetails  *self,
                               GAsyncResult            *result,
                               GError                 **error)
{
  g_return_val_if_fail (CHATTY_IS_PP_ACCOUNT_DETAILS (self), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

ChattyAccount *
chatty_pp_account_details_get_item (ChattyPpAccountDetails *self)
{
  g_return_val_if_fail (CHATTY_IS_PP_ACCOUNT_DETAILS (self), NULL);

  return self->account;
}

void
chatty_pp_account_details_set_item (ChattyPpAccountDetails *self,
                                    ChattyAccount          *account)
{
  const char *account_name, *protocol_name;

  g_return_if_fail (CHATTY_IS_PP_ACCOUNT_DETAILS (self));
  g_return_if_fail (!account || CHATTY_IS_ACCOUNT (account));

  gtk_editable_set_text (GTK_EDITABLE (self->password_row), "");

  if (self->account != account) {
    g_clear_signal_handler (&self->status_id, self->account);
    g_clear_signal_handler (&self->avatar_changed_id, self->account);
    gtk_list_box_bind_model (GTK_LIST_BOX (self->device_fp_list),
                             NULL, NULL, NULL, NULL);
    gtk_widget_set_visible (self->device_fp_list, FALSE);
    gtk_widget_set_visible (self->device_fp, FALSE);
  }

  if (!g_set_object (&self->account, account) || !account)
    return;

  account_name = chatty_item_get_username (CHATTY_ITEM (account));
  protocol_name = chatty_account_get_protocol_name (account);

  adw_action_row_set_subtitle (ADW_ACTION_ROW (self->account_id_row), account_name);
  adw_action_row_set_subtitle (ADW_ACTION_ROW (self->account_protocol_row), protocol_name);

  chatty_avatar_set_item (CHATTY_AVATAR (self->avatar_image), CHATTY_ITEM (account));

  self->status_id = g_signal_connect_object (self->account, "notify::status",
                                             G_CALLBACK (pp_details_status_changed_cb),
                                             self, G_CONNECT_SWAPPED);
  self->avatar_changed_id = g_signal_connect_object (self->account, "avatar-changed",
                                                     G_CALLBACK (pp_details_avatar_changed_cb),
                                                     self, G_CONNECT_SWAPPED);
  pp_details_status_changed_cb (self);
  pp_details_avatar_changed_cb (self);

  if (chatty_item_get_protocols (CHATTY_ITEM (self->account)) == CHATTY_PROTOCOL_XMPP)
    chatty_account_load_fp_async (self->account,
                                  pp_details_get_fingerprints_cb,
                                  g_object_ref (self));
}
