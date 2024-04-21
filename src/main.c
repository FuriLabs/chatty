/*
 * Copyright (C) 2018 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <time.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

#include "chatty-application.h"
#include "chatty-manager.h"
#include "chatty-log.h"

int
main (int   argc,
      char *argv[])
{
  g_autoptr(ChattyApplication) application = NULL;

  g_set_prgname (CHATTY_APP_ID);
  chatty_log_init ();
  /* You have to do this for gtk_source if libspelling is not enabled */
  gtk_source_init();

  textdomain (GETTEXT_PACKAGE);
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  application = chatty_application_new ();

  return g_application_run (G_APPLICATION (application), argc, argv);
}
