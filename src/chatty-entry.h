/* chatty-entry.h
 *
 * Copyright 2024 The Chatty Developers
 *
 * Author(s):
 *   Angelo Verlain <hey@vixalien.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define CHATTY_TYPE_ENTRY (chatty_entry_get_type())

G_DECLARE_FINAL_TYPE (ChattyEntry, chatty_entry, CHATTY, CUSTOM_ENTRY, AdwBin)

ChattyEntry *chatty_entry_new (void);

G_END_DECLS

