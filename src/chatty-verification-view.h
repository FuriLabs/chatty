/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-verification-view.h
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

#include "chatty-item.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_VERIFICATION_VIEW (chatty_verification_view_get_type ())

G_DECLARE_FINAL_TYPE (ChattyVerificationView, chatty_verification_view, CHATTY, VERIFICATION_VIEW, GtkBox)

void        chatty_verification_view_set_item     (ChattyVerificationView *self,
                                                   ChattyItem             *item);

G_END_DECLS

