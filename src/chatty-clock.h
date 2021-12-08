/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-clock.h
 *
 * Copyright 2021 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define CHATTY_TYPE_CLOCK (chatty_clock_get_type ())

G_DECLARE_FINAL_TYPE (ChattyClock, chatty_clock, CHATTY, CLOCK, GObject)

ChattyClock *chatty_clock_get_default         (void);
char        *chatty_clock_get_human_time      (ChattyClock *self,
                                               time_t       unix_time,
                                               gboolean     detailed);
void         chatty_clock_start               (ChattyClock *self);
void         chatty_clock_stop                (ChattyClock *self);

G_END_DECLS
