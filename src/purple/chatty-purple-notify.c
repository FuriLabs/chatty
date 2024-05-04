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
  AdwDialog *dialog;
  GtkWindow *window;

  window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));
  dialog = adw_alert_dialog_new (_("Error"), NULL);

  adw_alert_dialog_format_heading (ADW_ALERT_DIALOG (dialog),
                                   "%s",
                                   primary ? primary : title);

  adw_alert_dialog_format_body (ADW_ALERT_DIALOG (dialog),
                                "%s",
                                secondary);
  adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "close", _("Close"));

  adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "close");
  adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (dialog), "close");

  adw_dialog_present (dialog, GTK_WIDGET (window));

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
