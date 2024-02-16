/* chatty-message.c
 *
 * Copyright 2021 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>
#include <glib-object.h>

#include "chatty-chat.h"
#include "chatty-message.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_NOTIFICATION (chatty_notification_get_type ())

G_DECLARE_FINAL_TYPE (ChattyNotification, chatty_notification, CHATTY, NOTIFICATION, GObject)

ChattyNotification *chatty_notification_new          (ChattyChat         *chat);
void                chatty_notification_show_message (ChattyNotification *self,
                                                      ChattyMessage      *message,
                                                      const char         *name,
                                                      unsigned int        unread_count,
                                                      gboolean            is_sms);
void                chatty_notification_withdraw_notification (ChattyNotification *self);

G_END_DECLS
