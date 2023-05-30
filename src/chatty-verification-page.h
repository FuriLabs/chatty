/* chatty-verification-page.h
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

#define CHATTY_TYPE_VERIFICATION_PAGE (chatty_verification_page_get_type ())

G_DECLARE_FINAL_TYPE (ChattyVerificationPage, chatty_verification_page, CHATTY, VERIFICATION_PAGE, GtkBox)

void        chatty_verification_page_set_item     (ChattyVerificationPage *self,
                                                   ChattyItem             *item);

G_END_DECLS

