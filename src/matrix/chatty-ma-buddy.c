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

  CmUser         *cm_user;
  GdkPixbuf      *avatar;

  /* generated using g_str_hash for faster comparison */
  guint           id_hash;
  gboolean        is_self;
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

  if (!self->cm_user || !cm_user_get_id (self->cm_user))
    return FALSE;

  if (needle == cm_user_get_id (self->cm_user))
    return TRUE;

  return strcasestr (needle, cm_user_get_id (self->cm_user)) != NULL;
}

static const char *
chatty_ma_buddy_get_name (ChattyItem *item)
{
  ChattyMaBuddy *self = (ChattyMaBuddy *)item;
  const char *name = NULL;

  g_assert (CHATTY_IS_MA_BUDDY (self));

  if (!self->cm_user)
    return "";

  name = cm_user_get_display_name (self->cm_user);

  if (name)
    return name;

  return cm_user_get_id (self->cm_user);
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

  return "";
}

static void
ma_buddy_get_avatar_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  g_autoptr(ChattyMaBuddy) self = user_data;
  g_autoptr(GInputStream) stream = NULL;
  g_autoptr(GError) error = NULL;

  if (self->avatar)
    return;

  stream = cm_user_get_avatar_finish (self->cm_user, result, &error);

  if (error || !stream)
    return;

  self->avatar = gdk_pixbuf_new_from_stream (stream, NULL, NULL);
  g_signal_emit_by_name (self, "avatar-changed", 0);
}

static GdkPixbuf *
chatty_ma_buddy_get_avatar (ChattyItem *item)
{
  ChattyMaBuddy *self = (ChattyMaBuddy *)item;

  g_assert (CHATTY_IS_MA_BUDDY (self));

  if (self->avatar)
    return self->avatar;

  if (!self->cm_user)
    return NULL;

  cm_user_get_avatar_async (self->cm_user, NULL,
                            ma_buddy_get_avatar_cb,
                            g_object_ref (self));

  return NULL;
}

static void
chatty_ma_buddy_dispose (GObject *object)
{
  ChattyMaBuddy *self = (ChattyMaBuddy *)object;

  g_clear_object (&self->cm_user);

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

static void
ma_buddy_changed_cb (ChattyMaBuddy *self)
{
  g_assert (CHATTY_IS_MA_BUDDY (self));

  g_clear_object (&self->avatar);
  g_signal_emit_by_name (self, "changed", 0);
  g_signal_emit_by_name (self, "avatar-changed", 0);
}

ChattyMaBuddy *
chatty_ma_buddy_new_with_user (CmUser *user)
{
  ChattyMaBuddy *self;

  g_return_val_if_fail (CM_IS_USER (user), NULL);

  self = g_object_new (CHATTY_TYPE_MA_BUDDY, NULL);
  self->cm_user = g_object_ref (user);
  g_signal_connect_swapped (self->cm_user, "changed",
                            G_CALLBACK (ma_buddy_changed_cb), self);

  return self;
}

gboolean
chatty_ma_buddy_matches_cm_user (ChattyMaBuddy *self,
                                 CmUser        *user)
{
  g_return_val_if_fail (CHATTY_IS_MA_BUDDY (self), FALSE);
  g_return_val_if_fail (CM_IS_USER (user), FALSE);

  return user == self->cm_user;
}

guint
chatty_ma_buddy_get_id_hash (ChattyMaBuddy *self)
{
  g_return_val_if_fail (CHATTY_IS_MA_BUDDY (self), 0);

  if (!self->id_hash && self->cm_user)
    self->id_hash = g_str_hash (cm_user_get_id (self->cm_user));

  return self->id_hash;
}
