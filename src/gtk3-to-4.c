/*
 * Author: Mohammed Sadiq <www.sadiqpk.org>
 *
 * glue code to be used when migrating from GTK3 to GTK4.
 * should never be used for long, use it only as
 * a helper to migrate to GTK4, and drop this once
 * done
 *
 * Please note that these API may not do the right thing.
 * Some API also makes your application run slower.
 *
 * Consider this code only as a helper to avoid compiler
 * warnings.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later OR CC0-1.0
 */

#define G_LOG_DOMAIN "custom-gtk3-to-4"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "gtk3-to-4.h"

static void
dialog_response_cb (GObject  *object,
                    int       response_id,
                    gpointer  user_data)
{
  GTask *task = user_data;

  g_assert (GTK_IS_DIALOG (object) || GTK_IS_NATIVE_DIALOG (object));
  g_assert (G_IS_TASK (task));

  g_task_return_int (task, response_id);
}

int
gtk_dialog_run (GtkDialog *dialog)
{
  g_autoptr(GTask) task = NULL;

  g_assert (GTK_IS_DIALOG (dialog));

  task = g_task_new (dialog, NULL, NULL, NULL);
  gtk_widget_set_visible (GTK_WIDGET (dialog), TRUE);

  g_signal_connect_object (dialog, "response",
                           G_CALLBACK (dialog_response_cb),
                           task, G_CONNECT_AFTER);

  /* Wait until the task is completed */
  while (!g_task_get_completed (task))
    g_main_context_iteration (NULL, TRUE);

  return g_task_propagate_int (task, NULL);
}
