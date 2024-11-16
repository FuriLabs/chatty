/* chatty-entry.h
 *
 * Copyright 2024 The Chatty Developers
 *
 * Author(s):
 *   Angelo Verlain <hey@vixalien.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-entry"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "chatty-entry.h"

struct _ChattyEntry
{
  AdwBin parent_instance;
};

G_DEFINE_TYPE (ChattyEntry, chatty_entry, ADW_TYPE_BIN)

static void
chatty_entry_class_init (ChattyEntryClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_css_name (widget_class, "entry");
}

static void
chatty_entry_init (ChattyEntry *self)
{
}

ChattyEntry *
chatty_entry_new (void)
{
  return g_object_new (CHATTY_TYPE_ENTRY, NULL);
}
