/* chatty-invite-view.h
 *
 * Copyright 2022 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#pragma once

#include <gtk/gtk.h>

#include "chatty-chat.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_INVITE_PAGE (chatty_invite_page_get_type ())

G_DECLARE_FINAL_TYPE (ChattyInvitePage, chatty_invite_page, CHATTY, INVITE_PAGE, GtkBox)

void          chatty_invite_page_set_chat       (ChattyInvitePage *self,
                                                 ChattyChat       *chat);

G_END_DECLS
