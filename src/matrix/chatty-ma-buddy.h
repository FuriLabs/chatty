/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
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
#include <json-glib/json-glib.h>
#define CMATRIX_USE_EXPERIMENTAL_API
#include "cmatrix.h"

#include "chatty-item.h"

G_BEGIN_DECLS

typedef struct _BuddyDevice BuddyDevice;

#define CHATTY_TYPE_MA_BUDDY (chatty_ma_buddy_get_type ())

G_DECLARE_FINAL_TYPE (ChattyMaBuddy, chatty_ma_buddy, CHATTY, MA_BUDDY, ChattyItem)

ChattyMaBuddy   *chatty_ma_buddy_new               (const char    *matrix_id,
                                                    CmClient      *client);
ChattyMaBuddy   *chatty_ma_buddy_new_with_user     (CmUser        *user,
                                                    CmClient      *client);
const char      *chatty_ma_buddy_get_id            (ChattyMaBuddy *self);
guint            chatty_ma_buddy_get_id_hash       (ChattyMaBuddy *self);

const char      *chatty_ma_device_get_id           (BuddyDevice   *device);
const char      *chatty_ma_device_get_ed_key       (BuddyDevice   *device);
const char      *chatty_ma_device_get_curve_key    (BuddyDevice   *device);
char            *chatty_ma_device_get_one_time_key (BuddyDevice   *device);

G_END_DECLS
