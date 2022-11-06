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
#include <libgd/gd.h>

#include "chatty-application.h"
#include "chatty-manager.h"
#include "chatty-log.h"

static void
show_backtrace (int signum)
{
  /* Log only if we have set some verbosity so that the trace
   * shall be shown only if the user have explicitly asked for.
   * Thus avoid logging sensitive information to system log
   * without user's knowledge.
   */
  if (chatty_log_get_verbosity () > 0)
    g_on_error_stack_trace (g_get_prgname ());

  g_print ("signum %d: %s\n", signum, g_strsignal (signum));

  exit (128 + signum);
}

static void
enable_backtrace (void)
{
  const char *env;

  env = g_getenv ("LD_PRELOAD");

  /* Don't log backtrace if run inside valgrind */
  if (env && (strstr (env, "/valgrind/") || strstr (env, "/vgpreload")))
    return;

  signal (SIGABRT, show_backtrace);
  signal (SIGTRAP, show_backtrace);

#ifndef __has_feature
#  define __has_feature(x) (0)
#endif

#if __has_feature (address_sanitizer) ||        \
  defined(__SANITIZE_ADDRESS__) ||              \
  defined(__SANITIZE_THREAD__)
  return;
#endif

  /* Trap SIGSEGV only if not compiled with sanitizers */
  /* as sanitizers shall handle this better. */
  /* fixme: How to check if leak sanitizer is enabled? */
  signal (SIGSEGV, show_backtrace);
}


int
main (int   argc,
      char *argv[])
{
  g_autoptr(ChattyApplication) application = NULL;

  g_set_prgname (CHATTY_APP_ID);
  enable_backtrace ();
  chatty_log_init ();
  gd_ensure_types ();

  textdomain (GETTEXT_PACKAGE);
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  application = chatty_application_new ();

  return g_application_run (G_APPLICATION (application), argc, argv);
}
