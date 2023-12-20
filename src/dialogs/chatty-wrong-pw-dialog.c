/*
 * Copyright (C) 2023 Chris Talbot
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-wrong-pw-dialog"

#include "config.h"

#define _GNU_SOURCE
#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>

#include "chatty-ma-account.h"
#include "chatty-wrong-pw-dialog.h"

/**
 * ChattyWrongPwDialog:
 *
 * An AdwWindow dialog to allow a user to reinput a password
 *
 */
struct _ChattyWrongPwDialog
{
  AdwWindow        parent_instance;
  ChattyMaAccount *ma_account;
  CmClient        *cm_client;

  GtkWidget       *description;
  GtkWidget       *password_text_buffer;
  GtkWidget       *ok_button;
};

G_DEFINE_TYPE (ChattyWrongPwDialog, chatty_wrong_pw_dialog, ADW_TYPE_WINDOW)

static void
chatty_wrong_pw_dialog_apply_button_clicked_cb (ChattyWrongPwDialog *self)
{
  const char *password = NULL;

  g_assert (CHATTY_IS_WRONG_PW_DIALOG (self));

  password = gtk_entry_buffer_get_text (GTK_ENTRY_BUFFER (self->password_text_buffer));

  if (!password || !*password)
    return;

  if (!self->ma_account)
    g_warning ("Cannot set password for unknown ma_account");
  else if (!self->cm_client)
    g_warning ("Cannot set password for unknown cm_client");
  else {
    cm_client_set_password (self->cm_client, password);
    chatty_account_set_enabled (CHATTY_ACCOUNT (self->ma_account), FALSE);
    chatty_account_set_enabled (CHATTY_ACCOUNT (self->ma_account), TRUE);
  }

  gtk_window_destroy (GTK_WINDOW (self));
}

static void
chatty_wrong_pw_dialog_cancel_button_clicked_cb (ChattyWrongPwDialog *self)
{
  g_assert (CHATTY_IS_WRONG_PW_DIALOG (self));

  if (!self->ma_account)
    g_warning ("Cannot cancel for unknown ma_account");
  else {
    chatty_account_set_enabled (CHATTY_ACCOUNT (self->ma_account), FALSE);
  }

  gtk_window_destroy (GTK_WINDOW (self));
}

static void
chatty_wrong_pw_dialog_password_text_buffer_changed_cb (ChattyWrongPwDialog *self)
{
  unsigned int buffer_size;

  g_assert (CHATTY_IS_WRONG_PW_DIALOG (self));
  buffer_size = gtk_entry_buffer_get_length (GTK_ENTRY_BUFFER (self->password_text_buffer));

  /* Once the buffer goes above 1, it never goes below 1 for some reason */
  if (buffer_size > 1)
    gtk_widget_set_sensitive (self->ok_button, TRUE);
  else
    gtk_widget_set_sensitive (self->ok_button, FALSE);
}

void
chatty_wrong_pw_dialog_set_description (ChattyWrongPwDialog *self,
                                        const char          *description)
{
  g_assert (CHATTY_IS_WRONG_PW_DIALOG (self));
  adw_preferences_group_set_description (ADW_PREFERENCES_GROUP (self->description),
                                                                description);
}

void
chatty_wrong_pw_dialog_set_ma_account (ChattyWrongPwDialog *self,
                                       ChattyMaAccount     *account)
{
  g_assert (CHATTY_IS_WRONG_PW_DIALOG (self));

  self->ma_account = account;
}

void
chatty_wrong_pw_dialog_set_cm_client (ChattyWrongPwDialog *self,
                                      CmClient            *client)
{
  g_assert (CHATTY_IS_WRONG_PW_DIALOG (self));

  self->cm_client = client;
}

static void
chatty_wrong_pw_dialog_class_init (ChattyWrongPwDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/Chatty/"
                                               "ui/chatty-dialog-wrong-pw.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyWrongPwDialog, ok_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyWrongPwDialog, description);
  gtk_widget_class_bind_template_child (widget_class, ChattyWrongPwDialog, password_text_buffer);

  gtk_widget_class_bind_template_callback (widget_class, chatty_wrong_pw_dialog_cancel_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, chatty_wrong_pw_dialog_apply_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, chatty_wrong_pw_dialog_password_text_buffer_changed_cb);
}


static void
chatty_wrong_pw_dialog_init (ChattyWrongPwDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}


GtkWidget *
chatty_wrong_pw_dialog_new (GtkWindow *parent_window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (parent_window), NULL);

  return g_object_new (CHATTY_TYPE_WRONG_PW_DIALOG,
                       "transient-for", parent_window,
                       NULL);
}
