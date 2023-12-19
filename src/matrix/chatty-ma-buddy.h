/* chatty-ma-buddy.h
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
#define CMATRIX_USE_EXPERIMENTAL_API
#include "cmatrix.h"

#include "chatty-item.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_MA_BUDDY (chatty_ma_buddy_get_type ())

G_DECLARE_FINAL_TYPE (ChattyMaBuddy, chatty_ma_buddy, CHATTY, MA_BUDDY, ChattyItem)

ChattyMaBuddy   *chatty_ma_buddy_new_with_user     (CmUser        *user);
gboolean         chatty_ma_buddy_matches_cm_user   (ChattyMaBuddy *self,
                                                    CmUser        *user);

G_END_DECLS
