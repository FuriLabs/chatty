/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */



#include <glib/gi18n.h>

#include "chatty-purple-notify.h"
#include "chatty-application.h"
#include "chatty-utils.h"


static void *
chatty_notify_message (PurpleNotifyMsgType  type,
                       const char          *title,
                       const char          *primary,
                       const char          *secondary)
{
  GtkWindow *window;
  g_autoptr(GtkAlertDialog) dialog = NULL;

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
  dialog = gtk_alert_dialog_new ("%s", primary ? primary : title);
  gtk_alert_dialog_set_detail (dialog, secondary);
  gtk_alert_dialog_show (GTK_ALERT_DIALOG (dialog), window);
  purple_notify_close (PURPLE_NOTIFY_MESSAGE, GTK_WIDGET (dialog));

  return dialog;
}



static void
chatty_close_notify (PurpleNotifyType  type,
                     void             *ui_handle)
{
  gtk_window_destroy (GTK_WINDOW (ui_handle));
}


static PurpleNotifyUiOps ops =
{
  chatty_notify_message,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  chatty_close_notify,
  NULL,
  NULL,
  NULL,
  NULL
};


PurpleNotifyUiOps *
chatty_notify_get_ui_ops (void)
{
  return &ops;
}
