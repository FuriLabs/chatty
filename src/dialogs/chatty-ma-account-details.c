/* chatty-ma-account-details.c
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

#define G_LOG_DOMAIN "chatty-ma-account-details"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>

#include "chatty-avatar.h"
#include "chatty-ma-account.h"
#include "chatty-ma-account-details.h"
#include "chatty-log.h"

struct _ChattyMaAccountDetails
{
  AdwPreferencesPage parent_instance;

  ChattyAccount *account;

  GtkWidget     *avatar_image;
  GtkWidget     *delete_avatar_button;
  GtkWidget     *delete_button_stack;
  GtkWidget     *delete_button_image;
  GtkWidget     *delete_avatar_spinner;

  GtkWidget     *status_row;
  GtkWidget     *name_row;
  GtkWidget     *email_row;
  GtkWidget     *phone_row;

  GtkWidget     *homeserver_row;
  GtkWidget     *matrix_id_row;
  GtkWidget     *device_id_row;
  GtkWidget     *access_token_row;

  guint          modified : 1;
  guint          is_deleting_avatar : 1;
  gulong         status_id;
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

G_DEFINE_TYPE (ChattyMaAccountDetails, chatty_ma_account_details, ADW_TYPE_PREFERENCES_PAGE)



static void
on_copy_action_activated (GtkWidget *widget, const char *action_name, GVariant *target)
{
  ChattyMaAccountDetails *self = CHATTY_MA_ACCOUNT_DETAILS (widget);
  GdkClipboard *clipboard =  gdk_display_get_clipboard (gdk_display_get_default());
  const char *copy_text = NULL;
  const char *target_str = g_variant_get_string (target, NULL);

  if (g_strcmp0 (target_str, "access-token") == 0)
    copy_text = adw_action_row_get_subtitle (ADW_ACTION_ROW (self->access_token_row));
  else if (g_strcmp0 (target_str, "homeserver") == 0)
    copy_text = adw_action_row_get_subtitle (ADW_ACTION_ROW (self->homeserver_row));
  else if (g_strcmp0 (target_str, "matrix-id") == 0)
    copy_text = adw_action_row_get_subtitle (ADW_ACTION_ROW (self->matrix_id_row));
  else if (g_strcmp0 (target_str, "device-id") == 0)
    copy_text = adw_action_row_get_subtitle (ADW_ACTION_ROW (self->device_id_row));

  if (!copy_text) {
    g_warning ("Unknown target for copy action: '%s'", target_str);
    return;
  }

  gdk_clipboard_set_text (clipboard, copy_text);
}


static void
update_delete_avatar_button_state (ChattyMaAccountDetails *self)
{
  GtkStack *button_stack;
  ChattyStatus status;
  gboolean has_avatar = FALSE, can_delete;

  status = chatty_account_get_status (CHATTY_ACCOUNT (self->account));
  can_delete = has_avatar && !self->is_deleting_avatar && status == CHATTY_CONNECTED;
  button_stack = GTK_STACK (self->delete_button_stack);

  if (self->is_deleting_avatar)
    gtk_stack_set_visible_child (button_stack, self->delete_avatar_spinner);
  else
    gtk_stack_set_visible_child (button_stack, self->delete_button_image);

  gtk_widget_set_visible (self->delete_avatar_button, can_delete || self->is_deleting_avatar);
  gtk_widget_set_sensitive (self->delete_avatar_button, can_delete);

  g_object_set (self->delete_avatar_spinner, "spinning", self->is_deleting_avatar, NULL);
}

static void
ma_account_load_avatar_response_cb (GObject         *dialog,
                                    GAsyncResult    *response,
                                    gpointer        user_data)
{
  ChattyMaAccountDetails *self = CHATTY_MA_ACCOUNT_DETAILS (user_data);
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree char *file_name = NULL;

  g_assert (CHATTY_IS_MA_ACCOUNT_DETAILS (self));
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
ma_account_show_dialog_load_avatar (ChattyMaAccountDetails *self)
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

  gtk_file_dialog_open (dialog, GTK_WINDOW (window), NULL, ma_account_load_avatar_response_cb, self);
}

static void
ma_details_avatar_button_clicked_cb (ChattyMaAccountDetails *self)
{
  ma_account_show_dialog_load_avatar (self);
}

static void
ma_details_delete_avatar_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  g_autoptr(ChattyMaAccountDetails) self = user_data;

  self->is_deleting_avatar = FALSE;
  update_delete_avatar_button_state (self);
}

static void
ma_details_delete_avatar_button_clicked_cb (ChattyMaAccountDetails *self)
{
  g_assert (CHATTY_IS_MA_ACCOUNT_DETAILS (self));

  if (self->is_deleting_avatar)
    return;

  self->is_deleting_avatar = TRUE;
  update_delete_avatar_button_state (self);
  chatty_item_set_avatar_async (CHATTY_ITEM (self->account), NULL, NULL,
                                ma_details_delete_avatar_cb,
                                g_object_ref (self));
}

static void
ma_details_name_row_changed_cb (ChattyMaAccountDetails *self,
                                AdwEntryRow            *entry)
{
  const char *old, *new;

  g_assert (CHATTY_IS_MA_ACCOUNT_DETAILS (self));
  g_assert (ADW_IS_ENTRY_ROW (entry));

  old = g_object_get_data (G_OBJECT (entry), "name");
  new = gtk_editable_get_text (GTK_EDITABLE (entry));

  if (g_strcmp0 (old, new) == 0)
    self->modified = FALSE;
  else
    self->modified = TRUE;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODIFIED]);
}

static void
ma_details_delete_account_clicked_cb (ChattyMaAccountDetails *self)
{
  g_assert (CHATTY_IS_MA_ACCOUNT_DETAILS (self));

  g_signal_emit (self, signals[DELETE_CLICKED], 0);
}

static void
ma_account_details_update (ChattyMaAccountDetails *self)
{
  const char *name;

  g_assert (CHATTY_IS_MA_ACCOUNT_DETAILS (self));

  name = chatty_item_get_name (CHATTY_ITEM (self->account));

  g_object_set_data_full (G_OBJECT (self->name_row),
                          "name", g_strdup (name), g_free);
  gtk_editable_set_text (GTK_EDITABLE (self->name_row), name);
  gtk_widget_set_sensitive (self->name_row, TRUE);
}

static void
ma_account_details_get_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  g_autoptr(ChattyMaAccountDetails) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (CHATTY_IS_MA_ACCOUNT_DETAILS (self));

  chatty_ma_account_get_details_finish (CHATTY_MA_ACCOUNT (self->account),
                                        result, &error);

  if (error &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_PENDING))
    g_warning ("Error getting details: %s", error->message);

  ma_account_details_update (self);
}


static void
ma_details_delete_3pid_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  ChattyMaAccountDetails *self;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GtkWidget *button, *row;
  ChattyMaAccount *account;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (CHATTY_IS_MA_ACCOUNT_DETAILS (self));

  button = g_task_get_task_data (task);
  row = gtk_widget_get_parent (button);
  account = CHATTY_MA_ACCOUNT (self->account);

  if (chatty_ma_account_delete_3pid_finish (account, result, &error)) {
    GtkWidget *parent;

    parent = gtk_widget_get_parent (row);
    gtk_box_remove (GTK_BOX (parent), row);
  } else {
    GtkWidget *stack, *child;

    stack = gtk_button_get_child (GTK_BUTTON (button));
    child = gtk_stack_get_child_by_name (GTK_STACK (stack), "spinner");
    gtk_spinner_stop (GTK_SPINNER (child));

    gtk_stack_set_visible_child_name (GTK_STACK (stack), "trash-image");
    gtk_widget_set_sensitive (GTK_WIDGET (button), TRUE);
  }
}

static void
ma_details_delete_3pid_clicked (ChattyMaAccountDetails *self,
                                GObject                *button)
{
  GtkWidget *stack, *child;
  const char *value;
  GTask *task;
  ChattyIdType type;

  g_assert (CHATTY_IS_MA_ACCOUNT_DETAILS (self));
  g_assert (GTK_IS_BUTTON (button));

  value = g_object_get_data (button, "value");
  type = GPOINTER_TO_INT (g_object_get_data (button, "type"));

  g_assert (value);
  g_assert (type);

  stack = gtk_button_get_child (GTK_BUTTON (button));
  child = gtk_stack_get_child_by_name (GTK_STACK (stack), "spinner");
  gtk_stack_set_visible_child_name (GTK_STACK (stack), "trash-image");

  gtk_spinner_start (GTK_SPINNER (child));
  gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);

  task = g_task_new (self, NULL, NULL, NULL);
  g_task_set_task_data (task, g_object_ref (button), g_object_unref);
  chatty_ma_account_delete_3pid_async (CHATTY_MA_ACCOUNT (self->account),
                                       value, type, NULL,
                                       ma_details_delete_3pid_cb,
                                       task);
}

static void
ma_account_details_add_entry (ChattyMaAccountDetails *self,
                              GtkWidget              *row,
                              const char             *value)
{
  GtkWidget *button, *stack, *spinner, *image;

  adw_action_row_set_subtitle (ADW_ACTION_ROW (row), value);

  stack = gtk_stack_new ();
  image = gtk_image_new_from_icon_name ("user-trash-symbolic");
  gtk_stack_add_named (GTK_STACK (stack), image, "trash-image");

  spinner = gtk_spinner_new ();
  gtk_stack_add_named (GTK_STACK (stack), spinner, "spinner");

  button = gtk_button_new ();
  g_object_set_data_full (G_OBJECT (button), "value", g_strdup (value), g_free);
  if (row == self->phone_row)
    g_object_set_data (G_OBJECT (button), "type", GINT_TO_POINTER (CHATTY_ID_PHONE));
  else
    g_object_set_data (G_OBJECT (button), "type", GINT_TO_POINTER (CHATTY_ID_EMAIL));

  gtk_button_set_child (GTK_BUTTON (button), stack);

  g_signal_connect_swapped (button, "clicked",
                            (GCallback)ma_details_delete_3pid_clicked, self);

  adw_action_row_add_suffix (ADW_ACTION_ROW (row), button);
}

static void
ma_account_get_3pid_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  g_autoptr(ChattyMaAccountDetails) self = user_data;
  g_autoptr(GPtrArray) emails = NULL;
  g_autoptr(GPtrArray) phones = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (CHATTY_IS_MA_ACCOUNT_DETAILS (self));

  chatty_ma_account_get_3pid_finish (CHATTY_MA_ACCOUNT (self->account),
                                     &emails, &phones, result, &error);

  if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("Error getting 3pid: %s", error->message);

  for (guint i = 0; emails && i < emails->len; i++)
    ma_account_details_add_entry (self, self->email_row, emails->pdata[i]);

  for (guint i = 0; phones && i < phones->len; i++)
    ma_account_details_add_entry (self, self->phone_row, phones->pdata[i]);

  gtk_widget_set_visible (self->email_row, emails && emails->len > 0);
  gtk_widget_set_visible (self->phone_row, phones && phones->len > 0);
}

static void
ma_details_status_changed_cb (ChattyMaAccountDetails *self)
{
  const char *status_text;
  ChattyStatus status;

  g_assert (CHATTY_IS_MA_ACCOUNT_DETAILS (self));

  status = chatty_account_get_status (self->account);

  if (status == CHATTY_CONNECTED)
    status_text = _("connected");
  else if (status == CHATTY_CONNECTING)
    status_text = _("connecting…");
  else
    status_text = _("disconnected");

  adw_action_row_set_subtitle (ADW_ACTION_ROW (self->status_row), status_text);
  update_delete_avatar_button_state (self);

  if (status == CHATTY_CONNECTED) {
    chatty_ma_account_get_details_async (CHATTY_MA_ACCOUNT (self->account), NULL,
                                         ma_account_details_get_cb,
                                         g_object_ref (self));
    chatty_ma_account_get_3pid_async (CHATTY_MA_ACCOUNT (self->account), NULL,
                                      ma_account_get_3pid_cb,
                                      g_object_ref (self));
  }

  adw_action_row_set_subtitle (ADW_ACTION_ROW (self->device_id_row),
                               chatty_ma_account_get_device_id (CHATTY_MA_ACCOUNT (self->account)));

  adw_action_row_set_subtitle (ADW_ACTION_ROW (self->access_token_row),
                               chatty_ma_account_get_access_token (CHATTY_MA_ACCOUNT (self->account)));

}

static void
chatty_ma_account_details_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  ChattyMaAccountDetails *self = (ChattyMaAccountDetails *)object;

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
chatty_ma_account_details_finalize (GObject *object)
{
  ChattyMaAccountDetails *self = (ChattyMaAccountDetails *)object;

  g_clear_object (&self->account);

  G_OBJECT_CLASS (chatty_ma_account_details_parent_class)->finalize (object);
}


static void
chatty_ma_account_details_class_init (ChattyMaAccountDetailsClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = chatty_ma_account_details_get_property;
  object_class->finalize = chatty_ma_account_details_finalize;

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
                                               "ui/chatty-ma-account-details.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyMaAccountDetails, avatar_image);
  gtk_widget_class_bind_template_child (widget_class, ChattyMaAccountDetails, delete_avatar_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyMaAccountDetails, delete_button_stack);
  gtk_widget_class_bind_template_child (widget_class, ChattyMaAccountDetails, delete_button_image);
  gtk_widget_class_bind_template_child (widget_class, ChattyMaAccountDetails, delete_avatar_spinner);

  gtk_widget_class_bind_template_child (widget_class, ChattyMaAccountDetails, status_row);
  gtk_widget_class_bind_template_child (widget_class, ChattyMaAccountDetails, name_row);
  gtk_widget_class_bind_template_child (widget_class, ChattyMaAccountDetails, email_row);
  gtk_widget_class_bind_template_child (widget_class, ChattyMaAccountDetails, phone_row);

  gtk_widget_class_bind_template_child (widget_class, ChattyMaAccountDetails, homeserver_row);
  gtk_widget_class_bind_template_child (widget_class, ChattyMaAccountDetails, matrix_id_row);
  gtk_widget_class_bind_template_child (widget_class, ChattyMaAccountDetails, device_id_row);
  gtk_widget_class_bind_template_child (widget_class, ChattyMaAccountDetails, access_token_row);

  gtk_widget_class_bind_template_callback (widget_class, ma_details_avatar_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, ma_details_delete_avatar_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, ma_details_name_row_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, ma_details_delete_account_clicked_cb);

  gtk_widget_class_install_action (widget_class, "ma-account-details.copy", "s",
                                   on_copy_action_activated);
}

static void
chatty_ma_account_details_init (ChattyMaAccountDetails *self)
{
  GtkWidget *clamp;

  gtk_widget_init_template (GTK_WIDGET (self));

  clamp = gtk_widget_get_ancestor (self->avatar_image, ADW_TYPE_CLAMP);

  if (clamp) {
    adw_clamp_set_maximum_size (ADW_CLAMP (clamp), 360);
    adw_clamp_set_tightening_threshold (ADW_CLAMP (clamp), 320);
  }
}

GtkWidget *
chatty_ma_account_details_new (void)
{
  return g_object_new (CHATTY_TYPE_MA_ACCOUNT_DETAILS, NULL);
}

static void
ma_details_set_name_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  ChattyMaAccountDetails *self;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;
  const char *name;

  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (CHATTY_IS_MA_ACCOUNT_DETAILS (self));

  chatty_ma_account_set_name_finish (CHATTY_MA_ACCOUNT (self->account),
                                     result, &error);

  if (error)
    g_warning ("Error setting name: %s", error->message);

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);

  name = chatty_item_get_name (CHATTY_ITEM (self->account));
  g_object_set_data_full (G_OBJECT (self->name_row),
                          "name", g_strdup (name), g_free);
  self->modified = FALSE;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODIFIED]);
}

void
chatty_ma_account_details_save_async (ChattyMaAccountDetails *self,
                                      GAsyncReadyCallback     callback,
                                      gpointer                user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (CHATTY_IS_MA_ACCOUNT_DETAILS (self));
  g_return_if_fail (callback);

  task = g_task_new (self, NULL, callback, user_data);

  if (self->modified) {
    const char *name;

    name = gtk_editable_get_text (GTK_EDITABLE (self->name_row));
    chatty_ma_account_set_name_async (CHATTY_MA_ACCOUNT (self->account),
                                      name, NULL,
                                      ma_details_set_name_cb,
                                      g_steal_pointer (&task));
  } else
    g_task_return_boolean (task, TRUE);
}

gboolean
chatty_ma_account_details_save_finish (ChattyMaAccountDetails *self,
                                       GAsyncResult           *result,
                                       GError                 **error)
{
  g_return_val_if_fail (CHATTY_IS_MA_ACCOUNT_DETAILS (self), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

ChattyAccount *
chatty_ma_account_details_get_item (ChattyMaAccountDetails *self)
{
  g_return_val_if_fail (CHATTY_IS_MA_ACCOUNT_DETAILS (self), NULL);

  return self->account;
}

void
chatty_ma_account_details_set_item (ChattyMaAccountDetails *self,
                                    ChattyAccount          *account)
{
  g_return_if_fail (CHATTY_IS_MA_ACCOUNT_DETAILS (self));
  g_return_if_fail (!account || CHATTY_IS_MA_ACCOUNT (account));

  if (self->account != account) {
    g_clear_signal_handler (&self->status_id, self->account);
    gtk_editable_set_text (GTK_EDITABLE (self->name_row), "");
    gtk_widget_set_visible (self->email_row, FALSE);
    gtk_widget_set_visible (self->phone_row, FALSE);
  }

  if (!g_set_object (&self->account, account) || !account)
    return;

  adw_action_row_set_subtitle (ADW_ACTION_ROW (self->homeserver_row),
                               chatty_ma_account_get_homeserver (CHATTY_MA_ACCOUNT (self->account)));
  adw_action_row_set_subtitle (ADW_ACTION_ROW (self->matrix_id_row),
                               chatty_item_get_username (CHATTY_ITEM (account)));

  chatty_avatar_set_item (CHATTY_AVATAR (self->avatar_image), CHATTY_ITEM (account));

  self->status_id = g_signal_connect_object (self->account, "notify::status",
                                             G_CALLBACK (ma_details_status_changed_cb),
                                             self, G_CONNECT_SWAPPED);
  ma_details_status_changed_cb (self);
}
