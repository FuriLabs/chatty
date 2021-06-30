/* -*- mode: c; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "chatty-fp-row.h"
#include "matrix/chatty-ma-account.h"
#include "chatty-ma-account-details.h"
#include "chatty-log.h"

struct _ChattyMaAccountDetails
{
  HdyPreferencesPage parent_instance;

  ChattyAccount *account;

  GtkWidget     *avatar_image;
  GtkWidget     *status_label;
  GtkWidget     *name_entry;
  GtkWidget     *email_box;
  GtkWidget     *phone_box;

  GtkWidget     *homeserver_label;
  GtkWidget     *matrix_id_label;
  GtkWidget     *device_id_label;

  gulong         status_id;
};

enum {
  CHANGED,
  DELETE_CLICKED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

G_DEFINE_TYPE (ChattyMaAccountDetails, chatty_ma_account_details, HDY_TYPE_PREFERENCES_PAGE)

static char *
ma_account_show_dialog_load_avatar (ChattyMaAccountDetails *self)
{
  g_autoptr(GtkFileChooserNative) dialog = NULL;
  GtkWidget *window;
  int response;

  window = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);
  dialog = gtk_file_chooser_native_new (_("Set Avatar"),
                                        GTK_WINDOW (window),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        _("Open"),
                                        _("Cancel"));

  response = gtk_native_dialog_run (GTK_NATIVE_DIALOG (dialog));

  if (response == GTK_RESPONSE_ACCEPT)
    return gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

  return NULL;
}

static void
ma_details_avatar_button_clicked_cb (ChattyMaAccountDetails *self)
{
  g_autofree char *file_name = NULL;

  file_name = ma_account_show_dialog_load_avatar (self);

  if (file_name)
    chatty_item_set_avatar_async (CHATTY_ITEM (self->account),
                                  file_name, NULL, NULL, NULL);
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

  if (g_strcmp0 (name, gtk_entry_get_text (GTK_ENTRY (self->name_entry))) != 0)
    gtk_entry_set_text (GTK_ENTRY (self->name_entry), name);
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

  if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("Error getting details: %s", error->message);

  ma_account_details_update (self);
}

static void
ma_account_details_add_entry (ChattyMaAccountDetails *self,
                              GtkWidget              *box,
                              const char             *value)
{
  GtkWidget *entry;

  entry = gtk_entry_new ();
  gtk_widget_show (entry);
  gtk_widget_set_hexpand (entry, TRUE);
  gtk_widget_set_sensitive (entry, FALSE);
  gtk_entry_set_text (GTK_ENTRY (entry), value);

  gtk_container_add (GTK_CONTAINER (box), entry);
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
    ma_account_details_add_entry (self, self->email_box, emails->pdata[i]);

  for (guint i = 0; phones && i < phones->len; i++)
    ma_account_details_add_entry (self, self->phone_box, phones->pdata[i]);

  gtk_widget_set_visible (self->email_box, emails && emails->len > 0);
  gtk_widget_set_visible (self->phone_box, phones && phones->len > 0);
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

  gtk_label_set_text (GTK_LABEL (self->status_label), status_text);

  if (status == CHATTY_CONNECTED) {
    chatty_ma_account_get_details_async (CHATTY_MA_ACCOUNT (self->account), NULL,
                                         ma_account_details_get_cb,
                                         g_object_ref (self));
    chatty_ma_account_get_3pid_async (CHATTY_MA_ACCOUNT (self->account), NULL,
                                      ma_account_get_3pid_cb,
                                      g_object_ref (self));
  }

  gtk_label_set_text (GTK_LABEL (self->device_id_label),
                      chatty_ma_account_get_device_id (CHATTY_MA_ACCOUNT (self->account)));

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

  object_class->finalize = chatty_ma_account_details_finalize;

  signals [DELETE_CLICKED] =
    g_signal_new ("delete-clicked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/Chatty/"
                                               "ui/chatty-ma-account-details.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyMaAccountDetails, avatar_image);
  gtk_widget_class_bind_template_child (widget_class, ChattyMaAccountDetails, status_label);
  gtk_widget_class_bind_template_child (widget_class, ChattyMaAccountDetails, name_entry);
  gtk_widget_class_bind_template_child (widget_class, ChattyMaAccountDetails, email_box);
  gtk_widget_class_bind_template_child (widget_class, ChattyMaAccountDetails, phone_box);

  gtk_widget_class_bind_template_child (widget_class, ChattyMaAccountDetails, homeserver_label);
  gtk_widget_class_bind_template_child (widget_class, ChattyMaAccountDetails, matrix_id_label);
  gtk_widget_class_bind_template_child (widget_class, ChattyMaAccountDetails, device_id_label);

  gtk_widget_class_bind_template_callback (widget_class, ma_details_avatar_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, ma_details_delete_account_clicked_cb);
}

static void
chatty_ma_account_details_init (ChattyMaAccountDetails *self)
{
  GtkWidget *clamp;

  gtk_widget_init_template (GTK_WIDGET (self));

  clamp = gtk_widget_get_ancestor (self->avatar_image, HDY_TYPE_CLAMP);

  if (clamp) {
    hdy_clamp_set_maximum_size (HDY_CLAMP (clamp), 360);
    hdy_clamp_set_tightening_threshold (HDY_CLAMP (clamp), 320);
  }
}

GtkWidget *
chatty_ma_account_details_new (void)
{
  return g_object_new (CHATTY_TYPE_MA_ACCOUNT_DETAILS, NULL);
}

void
chatty_ma_account_save (ChattyMaAccountDetails *self)
{
  g_return_if_fail (CHATTY_IS_MA_ACCOUNT_DETAILS (self));
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
    gtk_entry_set_text (GTK_ENTRY (self->name_entry), "");
    gtk_widget_hide (self->email_box);
    gtk_widget_hide (self->phone_box);
  }

  if (!g_set_object (&self->account, account) || !account)
    return;

  gtk_label_set_text (GTK_LABEL (self->homeserver_label),
                      chatty_ma_account_get_homeserver (CHATTY_MA_ACCOUNT (self->account)));
  gtk_label_set_text (GTK_LABEL (self->matrix_id_label),
                      chatty_account_get_username (account));

  chatty_avatar_set_item (CHATTY_AVATAR (self->avatar_image), CHATTY_ITEM (account));

  self->status_id = g_signal_connect_object (self->account, "notify::status",
                                             G_CALLBACK (ma_details_status_changed_cb),
                                             self, G_CONNECT_SWAPPED);
  ma_details_status_changed_cb (self);
}
