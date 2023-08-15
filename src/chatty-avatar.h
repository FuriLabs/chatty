/* chatty-avatar.h
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>

#include "chatty-item.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_AVATAR (chatty_avatar_get_type ())

G_DECLARE_FINAL_TYPE (ChattyAvatar, chatty_avatar, CHATTY, AVATAR, AdwBin)

void       chatty_avatar_set_title (ChattyAvatar *item,
                                    const char   *title);
void       chatty_avatar_set_item (ChattyAvatar *self,
                                   ChattyItem   *item);

G_END_DECLS
