/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-buddy.h
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
#include <purple.h>

#include "chatty-contact-row.h"
#include "chatty-contact.h"
#include "chatty-item.h"
#include "chatty-enums.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_PP_BUDDY (chatty_pp_buddy_get_type ())

G_DECLARE_FINAL_TYPE (ChattyPpBuddy, chatty_pp_buddy, CHATTY, PP_BUDDY, ChattyItem)

ChattyPpBuddy   *chatty_pp_buddy_get_object    (PurpleBuddy   *buddy);
PurpleAccount   *chatty_pp_buddy_get_account   (ChattyPpBuddy *self);
PurpleConversation *chatty_pp_buddy_get_chat   (ChattyPpBuddy *self);
void             chatty_pp_buddy_set_chat      (ChattyPpBuddy      *self,
                                                PurpleConversation *conv);
PurpleBuddy     *chatty_pp_buddy_get_buddy      (ChattyPpBuddy *self);
const char      *chatty_pp_buddy_get_id        (ChattyPpBuddy *self);
ChattyContact   *chatty_pp_buddy_get_contact   (ChattyPpBuddy *self);
void             chatty_pp_buddy_set_contact   (ChattyPpBuddy *self,
                                                ChattyContact *contact);
ChattyUserFlag   chatty_pp_buddy_get_flags     (ChattyPpBuddy *self);

G_END_DECLS
