/* chatty-message.c
 *
 * Copyright 2020 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-message"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>

#include "chatty-enums.h"
#include "chatty-file.h"
#include "chatty-mm-buddy.h"
#include "chatty-message.h"
#include "chatty-utils.h"

/**
 * SECTION: chatty-message
 * @title: ChattyMessage
 * @short_description: An abstraction for chat messages
 * @include: "chatty-message.h"
 */

struct _ChattyMessage
{
  GObject          parent_instance;

  ChattyItem      *user;
  char            *user_name;
  char            *subject;
  char            *message;
  char            *uid;
  CmEvent         *cm_event;

  GList           *files;

  ChattyMsgType    type;
  ChattyMsgStatus  status;
  ChattyMsgDirection direction;
  time_t           time;

  guint            encrypted : 1;
  /* Set if files are created with file path string */
  guint            files_are_path : 1;
  guint            sms_id;
};

G_DEFINE_TYPE (ChattyMessage, chatty_message, G_TYPE_OBJECT)

enum {
  UPDATED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static void
chatty_message_finalize (GObject *object)
{
  ChattyMessage *self = (ChattyMessage *)object;

  g_clear_object (&self->user);
  g_clear_object (&self->cm_event);
  g_free (self->message);
  g_free (self->subject);
  g_free (self->uid);
  g_free (self->user_name);

  if (self->files)
    g_list_free_full (self->files, (GDestroyNotify)g_object_unref);

  G_OBJECT_CLASS (chatty_message_parent_class)->finalize (object);
}

static void
chatty_message_class_init (ChattyMessageClass *klass)
{
  GObjectClass *object_class  = G_OBJECT_CLASS (klass);

  object_class->finalize = chatty_message_finalize;

  /**
   * ChattyMessage::updated:
   * @self: a #ChattyMessage
   *
   * Emitted when the message or any of its property
   * is updated.
   */
  signals [UPDATED] =
    g_signal_new ("updated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
chatty_message_init (ChattyMessage *self)
{
}


ChattyMessage *
chatty_message_new (ChattyItem         *user,
                    const char         *message,
                    const char         *uid,
                    time_t              timestamp,
                    ChattyMsgType       type,
                    ChattyMsgDirection  direction,
                    ChattyMsgStatus     status)
{
  ChattyMessage *self;

  if (!timestamp)
    timestamp = time (NULL);

  self = g_object_new (CHATTY_TYPE_MESSAGE, NULL);
  g_set_object (&self->user, user);
  self->message = g_strdup (message);
  self->uid = g_strdup (uid);
  self->status = status;
  self->direction = direction;
  self->time = timestamp;
  self->type = type;

  return self;
}

ChattyMessage *
chatty_message_new_from_event (ChattyItem *user,
                               CmEvent    *event)
{
  ChattyMessage *self;
  const char *body = NULL;
  ChattyMsgType type = CHATTY_MESSAGE_UNKNOWN;
  ChattyMsgDirection direction;
  ChattyMsgStatus status;

  g_return_val_if_fail (CM_IS_EVENT (event), NULL);
  g_return_val_if_fail (CHATTY_IS_ITEM (user), NULL);

  if (CM_IS_ROOM_MESSAGE_EVENT (event)) {
    switch (cm_room_message_event_get_msg_type ((gpointer)event))
      {
      case CM_CONTENT_TYPE_TEXT:
      case CM_CONTENT_TYPE_EMOTE:
      case CM_CONTENT_TYPE_NOTICE:
      case CM_CONTENT_TYPE_SERVER_NOTICE:
        type = CHATTY_MESSAGE_TEXT;
        break;

      case CM_CONTENT_TYPE_IMAGE:
        type = CHATTY_MESSAGE_IMAGE;
        break;

      case CM_CONTENT_TYPE_FILE:
        type = CHATTY_MESSAGE_FILE;
        break;

      case CM_CONTENT_TYPE_AUDIO:
        type = CHATTY_MESSAGE_AUDIO;
        break;

      case CM_CONTENT_TYPE_LOCATION:
        type = CHATTY_MESSAGE_LOCATION;
        break;

      case CM_CONTENT_TYPE_VIDEO:
        type = CHATTY_MESSAGE_VIDEO;
        break;

      case CM_CONTENT_TYPE_UNKNOWN:
      default:
        break;
      }
  }

  switch (cm_event_get_state (event))
    {
    case CM_EVENT_STATE_DRAFT:
      status = CHATTY_STATUS_DRAFT;
      break;

    case CM_EVENT_STATE_RECEIVED:
      status = CHATTY_STATUS_RECEIVED;
      break;

    case CM_EVENT_STATE_WAITING:
    case CM_EVENT_STATE_SENDING:
      status = CHATTY_STATUS_SENDING;
      break;

    case CM_EVENT_STATE_SENDING_FAILED:
      status = CHATTY_STATUS_SENDING_FAILED;
      break;

    case CM_EVENT_STATE_SENT:
      status = CHATTY_STATUS_SENT;
      break;

    case CM_EVENT_STATE_UNKNOWN:
    default:
      status = CHATTY_STATUS_UNKNOWN;
      break;
    }

  if (CM_IS_ROOM_MESSAGE_EVENT (event))
    body = cm_room_message_event_get_body ((gpointer)event);

  if ((!body || !*body) &&
      cm_event_get_m_type (event) == CM_M_ROOM_ENCRYPTED)
    direction = CHATTY_DIRECTION_SYSTEM;
  else if (cm_event_get_state (event) == CM_EVENT_STATE_WAITING ||
           cm_event_get_state (event) == CM_EVENT_STATE_SENT ||
           cm_event_get_state (event) == CM_EVENT_STATE_SENDING ||
           cm_event_get_state (event) == CM_EVENT_STATE_SENDING_FAILED)
    direction = CHATTY_DIRECTION_OUT;
  else
    direction = CHATTY_DIRECTION_IN;

  if (direction == CHATTY_DIRECTION_SYSTEM)
    body = _("Got an encrypted message, but couldn't decrypt due to missing keys");
  else if (CM_IS_ROOM_MESSAGE_EVENT (event))
    body = cm_room_message_event_get_body ((gpointer)event);
  else
    body = "";

  self = chatty_message_new (user, body,
                             cm_event_get_id (event),
                             cm_event_get_time_stamp (event) / 1000,
                             type, direction, status);

  self->cm_event = g_object_ref (event);
  self->user = g_object_ref (user);

  if (type == CHATTY_MESSAGE_IMAGE ||
      type == CHATTY_MESSAGE_FILE ||
      type == CHATTY_MESSAGE_AUDIO ||
      type == CHATTY_MESSAGE_VIDEO) {
    self->files = g_list_append (self->files, chatty_file_new_for_cm_event (self->cm_event));
  }

  return self;
}

CmEvent *
chatty_message_get_cm_event (ChattyMessage *self)
{
  g_return_val_if_fail (CHATTY_IS_MESSAGE (self), NULL);

  return self->cm_event;
}

const char *
chatty_message_get_subject (ChattyMessage *self)
{
  g_return_val_if_fail (CHATTY_IS_MESSAGE (self), NULL);

  return self->subject;
}

void
chatty_message_set_subject (ChattyMessage *self,
                            const char    *subject)
{
  g_return_if_fail (CHATTY_IS_MESSAGE (self));

  g_free (self->subject);
  self->subject = g_strdup (subject);
}

gboolean
chatty_message_get_encrypted (ChattyMessage *self)
{
  g_return_val_if_fail (CHATTY_IS_MESSAGE (self), FALSE);

  return self->encrypted;
}

/**
 * chatty_message_set_files:
 * @self: A #ChattyMessage
 *
 * Get List of files
 *
 * Returns: (transfer none) (nullable): Get the list
 * of files or %NULL if no file is set.
 */
GList *
chatty_message_get_files (ChattyMessage *self)
{
  g_return_val_if_fail (CHATTY_IS_MESSAGE (self), NULL);

  return self->files;
}

/**
 * chatty_message_set_files:
 * @self: A #ChattyMessage
 * @files: (transfer full): A #GList of #ChattyFileInfo
 */
void
chatty_message_set_files (ChattyMessage *self,
                          GList         *files)
{
  g_return_if_fail (CHATTY_IS_MESSAGE (self));
  g_return_if_fail (!self->files);

  self->files = files;
}

const char *
chatty_message_get_uid (ChattyMessage *self)
{
  g_return_val_if_fail (CHATTY_IS_MESSAGE (self), NULL);

  return self->uid;
}

void
chatty_message_set_uid (ChattyMessage *self,
                        const char    *uid)
{
  g_return_if_fail (CHATTY_IS_MESSAGE (self));
  g_return_if_fail (!self->uid);

  self->uid = g_strdup (uid);
}

guint
chatty_message_get_sms_id (ChattyMessage *self)
{
  g_return_val_if_fail (CHATTY_IS_MESSAGE (self), 0);

  return self->sms_id;
}

void
chatty_message_set_sms_id (ChattyMessage *self,
                           guint          id)
{
  g_return_if_fail (CHATTY_IS_MESSAGE (self));

  if (id)
    self->sms_id = id;
}

const char *
chatty_message_get_text (ChattyMessage *self)
{
  g_return_val_if_fail (CHATTY_IS_MESSAGE (self), "");

  if (!self->message)
    return "";

  return self->message;
}

void
chatty_message_set_user (ChattyMessage *self,
                         ChattyItem    *sender)
{
  g_return_if_fail (CHATTY_IS_MESSAGE (self));
  g_return_if_fail (!sender || CHATTY_IS_ITEM (sender));
  g_return_if_fail (!self->user);

  g_set_object (&self->user, sender);
}

ChattyItem *
chatty_message_get_user (ChattyMessage *self)
{
  g_return_val_if_fail (CHATTY_IS_MESSAGE (self), NULL);

  return self->user;
}

const char *
chatty_message_get_user_name (ChattyMessage *self)
{
  const char *user_name = NULL;

  g_return_val_if_fail (CHATTY_IS_MESSAGE (self), "");

  if (!self->user_name && self->user) {
    /* TODO: chatty_item_get_username() is supposed to return phone number,
       but it's not implemented.  Check if doing so would break something.
     */
    if (CHATTY_IS_MM_BUDDY (self->user))
      user_name = chatty_mm_buddy_get_number (CHATTY_MM_BUDDY (self->user));
    else
      user_name = chatty_item_get_username (self->user);

    if (!user_name || !*user_name)
      user_name = chatty_item_get_name (self->user);
  }

  if (user_name &&
      chatty_item_get_protocols (self->user) == CHATTY_PROTOCOL_XMPP)
    self->user_name = chatty_utils_jabber_id_strip (user_name);
  else if (user_name)
    self->user_name = g_strdup (user_name);

  if (self->user_name)
    return self->user_name;

  return "";
}

const char *
chatty_message_get_user_alias (ChattyMessage *self)
{
  const char *name = NULL;

  g_return_val_if_fail (CHATTY_IS_MESSAGE (self), NULL);

  if (self->user)
    name = chatty_item_get_name (self->user);

  if (name && *name)
    return name;

  return NULL;
}

gboolean
chatty_message_user_matches (ChattyMessage *a,
                             ChattyMessage *b)
{
  g_return_val_if_fail (CHATTY_IS_MESSAGE (a), FALSE);
  g_return_val_if_fail (CHATTY_IS_MESSAGE (b), FALSE);

  if (a == b)
    return TRUE;

  if (a->user && a->user == b->user)
    return TRUE;

  if (g_strcmp0 (chatty_message_get_user_name (a),
                 chatty_message_get_user_name (b)) == 0)
    return TRUE;
  else if (a->user_name && b->user_name)
    return FALSE;

  return FALSE;
}

time_t
chatty_message_get_time (ChattyMessage *self)
{
  g_return_val_if_fail (CHATTY_IS_MESSAGE (self), 0);

  return self->time;
}

ChattyMsgStatus
chatty_message_get_status (ChattyMessage *self)
{
  g_return_val_if_fail (CHATTY_IS_MESSAGE (self), CHATTY_STATUS_UNKNOWN);

  return self->status;
}

void
chatty_message_set_status (ChattyMessage   *self,
                           ChattyMsgStatus  status,
                           time_t           mtime)
{
  g_return_if_fail (CHATTY_IS_MESSAGE (self));

  g_atomic_int_set (&self->status, status);

  if (mtime)
    self->time = mtime;

  g_signal_emit (self, signals[UPDATED], 0);
}

ChattyMsgType
chatty_message_get_msg_type (ChattyMessage *self)
{
  g_return_val_if_fail (CHATTY_IS_MESSAGE (self), CHATTY_MESSAGE_UNKNOWN);

  return self->type;
}

ChattyMsgDirection
chatty_message_get_msg_direction (ChattyMessage *self)
{
  g_return_val_if_fail (CHATTY_IS_MESSAGE (self), CHATTY_DIRECTION_UNKNOWN);

  return self->direction;
}
