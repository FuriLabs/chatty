/* cm-enums.h
 *
 * Copyright 2022 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once


/**
 * CmError:
 *
 * The Error returned by the Matrix Server
 * See https://matrix.org/docs/spec/client_server/r0.6.1#api-standards
 * for details.
 */
/* The order and value of members here and the error strings
 * in the array error_codes in cm-utils.c should match.
 */
typedef enum {
  CM_ERROR_FORBIDDEN = 1,
  CM_ERROR_UNKNOWN_TOKEN,
  CM_ERROR_MISSING_TOKEN,
  CM_ERROR_BAD_JSON,
  CM_ERROR_NOT_JSON,
  CM_ERROR_NOT_FOUND,
  CM_ERROR_LIMIT_EXCEEDED,
  CM_ERROR_UNKNOWN,
  CM_ERROR_UNRECOGNIZED,
  CM_ERROR_UNAUTHORIZED,
  CM_ERROR_USER_DEACTIVATED,
  CM_ERROR_USER_IN_USE,
  CM_ERROR_INVALID_USERNAME,
  CM_ERROR_ROOM_IN_USE,
  CM_ERROR_INVALID_ROOM_STATE,
  CM_ERROR_THREEPID_IN_USE,
  CM_ERROR_THREEPID_NOT_FOUND,
  CM_ERROR_THREEPID_AUTH_FAILED,
  CM_ERROR_THREEPID_DENIED,
  CM_ERROR_SERVER_NOT_TRUSTED,
  CM_ERROR_UNSUPPORTED_ROOM_VERSION,
  CM_ERROR_INCOMPATIBLE_ROOM_VERSION,
  CM_ERROR_BAD_STATE,
  CM_ERROR_GUEST_ACCESS_FORBIDDEN,
  CM_ERROR_CAPTCHA_NEEDED,
  CM_ERROR_CAPTCHA_INVALID,
  CM_ERROR_MISSING_PARAM,
  CM_ERROR_INVALID_PARAM,
  CM_ERROR_TOO_LARGE,
  CM_ERROR_EXCLUSIVE,
  CM_ERROR_RESOURCE_LIMIT_EXCEEDED,
  CM_ERROR_CANNOT_LEAVE_SERVER_NOTICE_ROOM,

  /* Local errors */
  CM_ERROR_BAD_PASSWORD,
  CM_ERROR_NO_HOME_SERVER,
  CM_ERROR_BAD_HOME_SERVER,
  CM_ERROR_USER_DEVICE_CHANGED,
} CmError;

typedef enum {
  CM_STATUS_UNKNOWN,
  CM_STATUS_JOIN,
  CM_STATUS_LEAVE,
  CM_STATUS_INVITE,
  CM_STATUS_BAN,
  CM_STATUS_KNOCK
} CmStatus;

typedef enum {
  CM_CONTENT_TYPE_UNKNOWN,
  CM_CONTENT_TYPE_TEXT,
  CM_CONTENT_TYPE_EMOTE,
  CM_CONTENT_TYPE_NOTICE,
  CM_CONTENT_TYPE_IMAGE,
  CM_CONTENT_TYPE_FILE,
  CM_CONTENT_TYPE_LOCATION,
  CM_CONTENT_TYPE_AUDIO,
  CM_CONTENT_TYPE_VIDEO,
  CM_CONTENT_TYPE_SERVER_NOTICE,
} CmContentType;

/*
 * The order of the enum items SHOULD NEVER
 * be changed as they are used in database.
 * New items should be appended to the end.
 */
typedef enum {
  CM_M_UNKNOWN,
  CM_M_CALL_ANSWER,
  CM_M_CALL_ASSERTED_IDENTITY,
  CM_M_CALL_ASSERTED_IDENTITY_PREFIX,
  CM_M_CALL_CANDIDATES,
  CM_M_CALL_HANGUP,
  CM_M_CALL_INVITE,
  CM_M_CALL_NEGOTIATE,
  CM_M_CALL_REJECT,
  CM_M_CALL_REPLACES,
  CM_M_CALL_SELECT_ANSWER,
  CM_M_DIRECT,
  CM_M_DUMMY,
  CM_M_FORWARDED_ROOM_KEY,
  CM_M_FULLY_READ,
  CM_M_IGNORED_USER_LIST,
  CM_M_KEY_VERIFICATION_ACCEPT,
  CM_M_KEY_VERIFICATION_CANCEL,
  CM_M_KEY_VERIFICATION_DONE,
  CM_M_KEY_VERIFICATION_KEY,
  CM_M_KEY_VERIFICATION_MAC,
  CM_M_KEY_VERIFICATION_READY,
  CM_M_KEY_VERIFICATION_REQUEST,
  CM_M_KEY_VERIFICATION_START,
  CM_M_PRESENCE,
  CM_M_PUSH_RULES,
  CM_M_REACTION,
  CM_M_RECEIPT,
  CM_M_ROOM_ALIASES,
  CM_M_ROOM_AVATAR,
  CM_M_ROOM_BOT_OPTIONS,
  CM_M_ROOM_CANONICAL_ALIAS,
  CM_M_ROOM_CREATE,
  CM_M_ROOM_ENCRYPTED,
  CM_M_ROOM_ENCRYPTION,
  CM_M_ROOM_GUEST_ACCESS,
  CM_M_ROOM_HISTORY_VISIBILITY,
  CM_M_ROOM_JOIN_RULES,
  CM_M_ROOM_KEY,
  CM_M_ROOM_KEY_REQUEST,
  CM_M_ROOM_MEMBER,
  CM_M_ROOM_MESSAGE,
  CM_M_ROOM_MESSAGE_FEEDBACK,
  CM_M_ROOM_NAME,
  CM_M_ROOM_PINNED_EVENTS,
  CM_M_ROOM_PLUMBING,
  CM_M_ROOM_POWER_LEVELS,
  CM_M_ROOM_REDACTION,
  CM_M_ROOM_RELATED_GROUPS,
  CM_M_ROOM_SERVER_ACL,
  CM_M_ROOM_THIRD_PARTY_INVITE,
  CM_M_ROOM_TOMBSTONE,
  CM_M_ROOM_TOPIC,
  CM_M_SECRET_REQUEST,
  CM_M_SECRET_SEND,
  CM_M_SECRET_STORAGE_DEFAULT_KEY,
  CM_M_SPACE_CHILD,
  CM_M_SPACE_PARENT,
  CM_M_STICKER,
  CM_M_TAG,
  CM_M_TYPING,

  /* Custom */
  CM_M_USER_STATUS = 256,
  CM_M_ROOM_INVITE,
  CM_M_ROOM_BAN,
  CM_M_ROOM_KICK,
} CmEventType;

typedef enum
{
  CM_EVENT_STATE_UNKNOWN,
  CM_EVENT_STATE_DRAFT,
  /* Messages that are queued to be sent */
  CM_EVENT_STATE_WAITING,
  /* When saving to db consider this as failed until sent? */
  CM_EVENT_STATE_SENDING,
  CM_EVENT_STATE_SENDING_FAILED,
  CM_EVENT_STATE_SENT,
  CM_EVENT_STATE_RECEIVED,
} CmEventState;
