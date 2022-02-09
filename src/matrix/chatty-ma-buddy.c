/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* chatty-ma-buddy.c
 *
 * Copyright 2020, 2022 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-ma-buddy"

#define _GNU_SOURCE
#include <string.h>
#include <glib/gi18n.h>

#include "matrix-utils.h"
#include "chatty-ma-buddy.h"

struct _ChattyMaBuddy
{
  ChattyItem      parent_instance;

  char           *matrix_id;
  char           *name;

  CmUser         *cm_user;
  CmClient       *cm_client;

  /* generated using g_str_hash for faster comparison */
  guint           id_hash;
  gboolean        is_self;
};

struct _BuddyDevice
{

  char *device_id;
  char *device_name;
  char *curve_key; /* Public part Curve25519 identity key pair */
  char *ed_key;    /* Public part of Ed25519 fingerprint key pair */
  char *one_time_key;

  gboolean meagolm_v1;
  gboolean olm_v1;
};


G_DEFINE_TYPE (ChattyMaBuddy, chatty_ma_buddy, CHATTY_TYPE_ITEM)

enum {
  CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static ChattyProtocol
chatty_ma_buddy_get_protocols (ChattyItem *item)
{
  return CHATTY_PROTOCOL_MATRIX;
}

static gboolean
chatty_ma_buddy_matches (ChattyItem     *item,
                         const char     *needle,
                         ChattyProtocol  protocols,
                         gboolean        match_name)
{
  ChattyMaBuddy *self = (ChattyMaBuddy *)item;

  if (needle == self->matrix_id)
    return TRUE;

  if (!needle || !self->matrix_id)
    return FALSE;

  return strcasestr (needle, self->matrix_id) != NULL;
}

static const char *
chatty_ma_buddy_get_name (ChattyItem *item)
{
  ChattyMaBuddy *self = (ChattyMaBuddy *)item;

  g_assert (CHATTY_IS_MA_BUDDY (self));

  if (self->name)
    return self->name;

  if (self->matrix_id)
    return self->matrix_id;

  return "";
}

static void
chatty_ma_buddy_set_name (ChattyItem *item,
                          const char *name)
{
  ChattyMaBuddy *self = (ChattyMaBuddy *)item;

  g_assert (CHATTY_IS_MA_BUDDY (self));

  g_free (self->name);

  if (!name || !*name)
    self->name = NULL;
  else
    self->name = g_strdup (name);
}

/*
 * chatty_ma_buddy_get_username:
 *
 * Get the user id of @item. The id is usually a
 * fully qualified Matrix ID (@user:example.com),
 * but it can also be the username alone (user).
 *
 * Returns: (transfer none): the id of Buddy.
 * or an empty string if not found or on error.
 */
static const char *
chatty_ma_buddy_get_username (ChattyItem *item)
{
  ChattyMaBuddy *self = (ChattyMaBuddy *)item;

  g_assert (CHATTY_IS_MA_BUDDY (self));

  if (self->cm_user)
    return cm_user_get_id (self->cm_user);

  if (self->matrix_id)
    return self->matrix_id;

  return "";
}

static GdkPixbuf *
chatty_ma_buddy_get_avatar (ChattyItem *item)
{
  ChattyMaBuddy *self = (ChattyMaBuddy *)item;

  g_assert (CHATTY_IS_MA_BUDDY (self));

  return NULL;
}

static void
chatty_ma_buddy_dispose (GObject *object)
{
  ChattyMaBuddy *self = (ChattyMaBuddy *)object;

  g_clear_pointer (&self->matrix_id, g_free);
  g_clear_pointer (&self->name, g_free);

  g_clear_object (&self->cm_user);
  g_clear_object (&self->cm_client);

  G_OBJECT_CLASS (chatty_ma_buddy_parent_class)->dispose (object);
}

static void
chatty_ma_buddy_class_init (ChattyMaBuddyClass *klass)
{
  GObjectClass *object_class  = G_OBJECT_CLASS (klass);
  ChattyItemClass *item_class = CHATTY_ITEM_CLASS (klass);

  object_class->dispose = chatty_ma_buddy_dispose;

  item_class->get_protocols = chatty_ma_buddy_get_protocols;
  item_class->matches  = chatty_ma_buddy_matches;
  item_class->get_name = chatty_ma_buddy_get_name;
  item_class->set_name = chatty_ma_buddy_set_name;
  item_class->get_username = chatty_ma_buddy_get_username;
  item_class->get_avatar = chatty_ma_buddy_get_avatar;

  /**
   * ChattyMaBuddy::changed:
   * @self: a #ChattyMaBuddy
   *
   * changed signal is emitted when any detail
   * of the buddy changes.
   */
  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
chatty_ma_buddy_init (ChattyMaBuddy *self)
{
}

ChattyMaBuddy *
chatty_ma_buddy_new (const char *matrix_id,
                     CmClient   *client)
{
  ChattyMaBuddy *self;

  g_return_val_if_fail (matrix_id && *matrix_id == '@', NULL);
  g_return_val_if_fail (CM_IS_CLIENT (client), NULL);

  self = g_object_new (CHATTY_TYPE_MA_BUDDY, NULL);
  self->matrix_id = g_strdup (matrix_id);
  self->cm_client = g_object_ref (client);

  if (g_str_equal (matrix_id, cm_client_get_user_id (client)))
    self->is_self = TRUE;

  return self;
}

ChattyMaBuddy *
chatty_ma_buddy_new_with_user (CmUser   *user,
                               CmClient *client)
{
  ChattyMaBuddy *self;

  g_return_val_if_fail (CM_IS_USER (user), NULL);
  g_return_val_if_fail (CM_IS_CLIENT (client), NULL);

  self = g_object_new (CHATTY_TYPE_MA_BUDDY, NULL);
  self->cm_user = g_object_ref (user);
  self->cm_client = g_object_ref (client);

  if (g_strcmp0 (cm_user_get_id (user),
                 cm_client_get_user_id (client)) == 0)
    self->is_self = TRUE;

  return self;
}

guint
chatty_ma_buddy_get_id_hash (ChattyMaBuddy *self)
{
  g_return_val_if_fail (CHATTY_IS_MA_BUDDY (self), 0);

  if (!self->id_hash && self->matrix_id)
    self->id_hash = g_str_hash (self->matrix_id);

  return self->id_hash;
}

const char *
chatty_ma_device_get_id (BuddyDevice *device)
{
  g_return_val_if_fail (device, "");

  return device->device_id;
}

const char *
chatty_ma_device_get_ed_key (BuddyDevice *device)
{
  g_return_val_if_fail (device, "");

  return device->ed_key;
}

const char *
chatty_ma_device_get_curve_key (BuddyDevice *device)
{
  g_return_val_if_fail (device, "");

  return device->curve_key;
}

char *
chatty_ma_device_get_one_time_key (BuddyDevice *device)
{
  g_return_val_if_fail (device, g_strdup (""));

  return g_steal_pointer (&device->one_time_key);
}
