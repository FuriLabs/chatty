/* chatty-message.h
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#define CMATRIX_USE_EXPERIMENTAL_API
#include <cmatrix.h>
#include <glib-object.h>

#include "chatty-item.h"
#include "chatty-enums.h"

G_BEGIN_DECLS

#define CHATTY_TYPE_MESSAGE (chatty_message_get_type ())

G_DECLARE_FINAL_TYPE (ChattyMessage, chatty_message, CHATTY, MESSAGE, GObject)

ChattyMessage      *chatty_message_new             (ChattyItem         *user,
                                                    const char         *message,
                                                    const char         *uid,
                                                    time_t              time,
                                                    ChattyMsgType       type,
                                                    ChattyMsgDirection  direction,
                                                    ChattyMsgStatus     status);
ChattyMessage      *chatty_message_new_from_event  (ChattyItem         *user,
                                                    CmEvent            *event);
CmEvent            *chatty_message_get_cm_event    (ChattyMessage      *self);

const char         *chatty_message_get_subject     (ChattyMessage      *self);
void                chatty_message_set_subject     (ChattyMessage      *self,
                                                    const char         *subject);

gboolean            chatty_message_get_encrypted   (ChattyMessage      *self);

GList              *chatty_message_get_files       (ChattyMessage      *self);
void                chatty_message_set_files       (ChattyMessage      *self,
                                                    GList              *files);
const char         *chatty_message_get_uid         (ChattyMessage      *self);
void                chatty_message_set_uid         (ChattyMessage      *self,
                                                    const char         *uid);
guint               chatty_message_get_sms_id      (ChattyMessage      *self);
void                chatty_message_set_sms_id      (ChattyMessage      *self,
                                                    guint               id);
const char         *chatty_message_get_text        (ChattyMessage      *self);
void                chatty_message_set_user        (ChattyMessage      *self,
                                                    ChattyItem         *sender);
ChattyItem         *chatty_message_get_user        (ChattyMessage      *self);
const char         *chatty_message_get_user_name   (ChattyMessage      *self);
const char         *chatty_message_get_user_alias  (ChattyMessage      *self);
gboolean            chatty_message_user_matches    (ChattyMessage      *a_message,
                                                    ChattyMessage      *b_message);
time_t              chatty_message_get_time        (ChattyMessage      *self);
ChattyMsgStatus     chatty_message_get_status      (ChattyMessage      *self);
void                chatty_message_set_status      (ChattyMessage      *self,
                                                    ChattyMsgStatus     status,
                                                    time_t              mtime);
ChattyMsgType       chatty_message_get_msg_type    (ChattyMessage      *self);
ChattyMsgDirection  chatty_message_get_msg_direction (ChattyMessage    *self);

G_END_DECLS
