/* library.h
 *
 * Copyright 2022 Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-2-or-later OR CC0-1.0
 */

#include <handy.h>
#include <libgd/gd.h>

#include "dialogs/chatty-chat-info.h"

void __attribute__((constructor)) initialize_libraries (void);

void
__attribute__((constructor)) initialize_libraries (void)
{
  gtk_init (NULL, NULL);
  hdy_init ();
  gd_ensure_types ();

  g_type_ensure (CHATTY_TYPE_CHAT_INFO);
}
