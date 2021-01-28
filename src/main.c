/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include <time.h>
#include <glib/gi18n.h>
#include "chatty-config.h"
#include "chatty-application.h"
#include "chatty-manager.h"
#include "chatty-log.h"


int
main (int   argc,
      char *argv[])
{
  g_autoptr(ChattyApplication) application = NULL;
  int status;

  chatty_log_init ();
  textdomain (GETTEXT_PACKAGE);
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  g_set_prgname (CHATTY_APP_ID);
  application = chatty_application_new ();

  status = g_application_run (G_APPLICATION (application), argc, argv);
  chatty_log_finalize ();

  return status;
}
