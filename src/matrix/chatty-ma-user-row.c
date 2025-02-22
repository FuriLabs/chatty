/* chatty-list-row.h
 *
 * Copyright 2025 Chatty Developers
 *
 * Author(s):
 *   Evangelos Ribeiro Tzaras <devrtz@fortysixandtwo.eu>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "chatty-ma-user-row.h"

#include "chatty-avatar.h"

struct _ChattyMaUserRow
{
  GtkListBoxRow  parent_instance;

  ChattyAvatar  *avatar;
  GtkLabel      *display_name;
  GtkLabel      *user_id;

  ChattyMaBuddy *buddy;
};

G_DEFINE_FINAL_TYPE (ChattyMaUserRow, chatty_ma_user_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_BUDDY,
  N_PROPS
};
static GParamSpec *properties[N_PROPS];


static void
chatty_ma_user_row_update (ChattyMaUserRow *self)
{
  ChattyItem *item;

  g_assert (CHATTY_IS_MA_BUDDY (self->buddy));

  item = CHATTY_ITEM (self->buddy);

  gtk_label_set_text (self->display_name, chatty_item_get_name (item));
  gtk_label_set_text (self->user_id, chatty_item_get_username (item));
}

static void
on_copy_id_action_activated (GtkWidget *widget, const char *action_name, GVariant *target)
{
  ChattyMaUserRow *self = CHATTY_MA_USER_ROW (widget);
  GdkClipboard *clipboard =  gdk_display_get_clipboard (gdk_display_get_default());

  gdk_clipboard_set_text (clipboard,
                          chatty_item_get_username (CHATTY_ITEM (self->buddy)));
}

static void
chatty_ma_user_row_set_property (GObject *object,
                                 guint prop_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
  ChattyMaUserRow *self = CHATTY_MA_USER_ROW (object);

  switch (prop_id) {
  case PROP_BUDDY:
    self->buddy = g_value_dup_object (value);
    chatty_avatar_set_item (self->avatar, CHATTY_ITEM (self->buddy));

    g_signal_connect_object (self->buddy, "changed",
                             G_CALLBACK (chatty_ma_user_row_update),
                             self, G_CONNECT_SWAPPED);

    chatty_ma_user_row_update (self);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
chatty_ma_user_row_dispose (GObject *object)
{
  ChattyMaUserRow *self = CHATTY_MA_USER_ROW (object);

  g_clear_object (&self->buddy);

  G_OBJECT_CLASS (chatty_ma_user_row_parent_class)->dispose (object);
}

static void
chatty_ma_user_row_class_init (ChattyMaUserRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = chatty_ma_user_row_dispose;
  object_class->set_property = chatty_ma_user_row_set_property;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/Chatty/"
                                               "ui/chatty-ma-user-row.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyMaUserRow, avatar);
  gtk_widget_class_bind_template_child (widget_class, ChattyMaUserRow, display_name);
  gtk_widget_class_bind_template_child (widget_class, ChattyMaUserRow, user_id);

  gtk_widget_class_install_action (widget_class, "ma-user-row.copy_id", NULL,
                                   on_copy_id_action_activated);
  /**
   * ChattyMaUserRow:buddy:
   *
   * The user for whom to display details.
   */
  properties[PROP_BUDDY] =
    g_param_spec_object ("buddy",
                         "",
                         "",
                         CHATTY_TYPE_MA_BUDDY,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
chatty_ma_user_row_init (ChattyMaUserRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}


GtkWidget *
chatty_ma_user_row_new (ChattyMaBuddy *buddy)
{
  return g_object_new (CHATTY_TYPE_MA_USER_ROW, "buddy", buddy, NULL);
}
