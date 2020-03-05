/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-folks.h
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>
#include <gdk/gdk.h>

#include "users/chatty-contact.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_EDS (chatty_eds_get_type ())

G_DECLARE_FINAL_TYPE (ChattyEds, chatty_eds, CHATTY, EDS, GObject)

ChattyEds     *chatty_eds_new            (ChattyProtocol protocols);
gboolean       chatty_eds_is_ready       (ChattyEds  *self);
GListModel    *chatty_eds_get_model      (ChattyEds  *self);
ChattyContact *chatty_eds_find_by_number (ChattyEds  *self,
                                          const char *phone_number);

G_END_DECLS
