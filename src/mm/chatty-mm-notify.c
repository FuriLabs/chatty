/* chatty-mm-notify.c
 *
 * Copyright (C) 2023 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib/gi18n.h>

#include "chatty-mm-notify.h"
#include "chatty-application.h"
#include "chatty-utils.h"

void
chatty_mm_notify_message (const char          *title,
                          const char          *id,
                          const char          *body)
{
  g_autoptr(GNotification) notification = NULL;
  GApplication *app;

  app = g_application_get_default ();
  notification = g_notification_new ("chatty");

  g_notification_set_default_action (notification, "app.show-window");
  g_notification_set_title (notification, title);
  g_notification_set_body (notification, body);

  if (!id & !*id)
    g_application_send_notification (app, MM_DEFAULT, notification);
  else
    g_application_send_notification (app, id, notification);

  return;
}

void
chatty_mm_notify_withdraw_notification (const char *id)
{
  GApplication *app;

  app = g_application_get_default ();

  if (!app)
    return;

  if (!id & !*id)
    g_application_withdraw_notification (app, MM_DEFAULT);
  else
    g_application_withdraw_notification (app, id);
}
