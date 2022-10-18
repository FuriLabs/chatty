/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
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

#define CHATTY_TYPE_INVITE_VIEW (chatty_invite_view_get_type ())

G_DECLARE_FINAL_TYPE (ChattyInviteView, chatty_invite_view, CHATTY, INVITE_VIEW, GtkBox)

void          chatty_invite_view_set_chat       (ChattyInviteView *self,
                                                 ChattyChat       *chat);

G_END_DECLS
