/* chatty-header-bar.c
 *
 * Copyright 2022 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-header-bar"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>

#include "chatty-avatar.h"
#include "chatty-manager.h"
#include "chatty-mm-account.h"
#include "matrix/chatty-ma-chat.h"
#include "mm/chatty-mm-chat.h"
#include "chatty-header-bar.h"

struct _ChattyHeaderBar
{
  GtkBox      parent_instance;

  GtkWidget  *leaflet;

  GtkWidget  *main_header_bar;
  GtkWidget  *back_button;
  GtkWidget  *add_chat_button;
  GtkWidget  *search_button;

  GtkWidget  *content_header_bar;
  GtkWidget  *content_avatar;
  GtkWidget  *content_title;
  GtkWidget  *content_menu_button;
  GtkWidget  *call_button;

  GtkWidget  *sidebar_group;
  GtkWidget  *content_group;
  GtkWidget  *header_group;

  GtkWidget  *new_chat_button;
  GtkWidget  *new_sms_mms_button;
  GtkWidget  *new_group_chat_button;

  GtkWidget  *leave_button;
  GtkWidget  *block_button;
  GtkWidget  *unblock_button;
  GtkWidget  *archive_button;
  GtkWidget  *unarchive_button;
  GtkWidget  *delete_button;

  GBinding   *title_binding;

  ChattyItem *item;

  gulong      content_handler;
};

G_DEFINE_TYPE (ChattyHeaderBar, chatty_header_bar, GTK_TYPE_BOX)

enum {
  BACK_CLICKED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static void
header_bar_update_item_state_button (ChattyHeaderBar *self,
                                     ChattyItem      *item)
{
  ChattyItemState state;

  gtk_widget_hide (self->block_button);
  gtk_widget_hide (self->unblock_button);
  gtk_widget_hide (self->archive_button);
  gtk_widget_hide (self->unarchive_button);

  if (!item || !CHATTY_IS_MM_CHAT (item))
    return;

  state = chatty_item_get_state (item);

  if (state == CHATTY_ITEM_VISIBLE) {
    gtk_widget_show (self->block_button);
    gtk_widget_show (self->archive_button);
  } else if (state == CHATTY_ITEM_ARCHIVED) {
    gtk_widget_show (self->unarchive_button);
  } else if (state == CHATTY_ITEM_BLOCKED) {
    gtk_widget_show (self->unblock_button);
  }
}

static void
header_bar_chat_changed_cb (ChattyHeaderBar *self,
                            ChattyItem      *item)
{
  GListModel *users;

  users = chatty_chat_get_users (CHATTY_CHAT (item));

  /* allow changing state only for 1:1 SMS/MMS chats  */
  if (item == self->item &&
      CHATTY_IS_MM_CHAT (item) &&
      g_list_model_get_n_items (users) == 1)
    header_bar_update_item_state_button (self, item);
}

static void
header_bar_active_protocols_changed_cb (ChattyHeaderBar *self)
{
  ChattyManager *manager;
  ChattyAccount *mm_account;
  ChattyProtocol protocols;
  gboolean has_mms, has_sms, has_im;

  g_assert (CHATTY_IS_HEADER_BAR (self));

  manager = chatty_manager_get_default ();
  mm_account = chatty_manager_get_mm_account (manager);
  protocols = chatty_manager_get_active_protocols (manager);
  has_mms = chatty_mm_account_has_mms_feature (CHATTY_MM_ACCOUNT (mm_account));
  has_sms = !!(protocols & CHATTY_PROTOCOL_MMS_SMS);
  has_im  = !!(protocols & ~CHATTY_PROTOCOL_MMS_SMS);

  gtk_widget_set_sensitive (self->add_chat_button, has_sms || has_im);
  gtk_widget_set_sensitive (self->new_group_chat_button, has_im);
  gtk_widget_set_visible (self->new_sms_mms_button, has_mms && has_sms);
}

static void
header_back_clicked_cb (ChattyHeaderBar *self)
{
  g_assert (CHATTY_IS_HEADER_BAR (self));

  g_signal_emit (self, signals[BACK_CLICKED], 0);
}

static void
chatty_header_bar_map (GtkWidget *widget)
{
  ChattyHeaderBar *self = (ChattyHeaderBar *)widget;
  ChattyManager *manager;

  manager = chatty_manager_get_default ();
  g_signal_connect_object (manager, "notify::active-protocols",
                           G_CALLBACK (header_bar_active_protocols_changed_cb), self,
                           G_CONNECT_SWAPPED);

  header_bar_active_protocols_changed_cb (self);

  GTK_WIDGET_CLASS (chatty_header_bar_parent_class)->map (widget);
}

static void
chatty_header_bar_dispose (GObject *object)
{
  ChattyHeaderBar *self = (ChattyHeaderBar *)object;

  g_clear_object (&self->item);

  G_OBJECT_CLASS (chatty_header_bar_parent_class)->dispose (object);
}

static void
chatty_header_bar_class_init (ChattyHeaderBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = chatty_header_bar_dispose;

  widget_class->map = chatty_header_bar_map;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/Chatty/"
                                               "ui/chatty-header-bar.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyHeaderBar, leaflet);

  gtk_widget_class_bind_template_child (widget_class, ChattyHeaderBar, main_header_bar);
  gtk_widget_class_bind_template_child (widget_class, ChattyHeaderBar, back_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyHeaderBar, add_chat_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyHeaderBar, search_button);

  gtk_widget_class_bind_template_child (widget_class, ChattyHeaderBar, content_header_bar);
  gtk_widget_class_bind_template_child (widget_class, ChattyHeaderBar, content_avatar);
  gtk_widget_class_bind_template_child (widget_class, ChattyHeaderBar, content_title);
  gtk_widget_class_bind_template_child (widget_class, ChattyHeaderBar, content_menu_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyHeaderBar, call_button);

  gtk_widget_class_bind_template_child (widget_class, ChattyHeaderBar, sidebar_group);
  gtk_widget_class_bind_template_child (widget_class, ChattyHeaderBar, content_group);
  gtk_widget_class_bind_template_child (widget_class, ChattyHeaderBar, header_group);

  gtk_widget_class_bind_template_child (widget_class, ChattyHeaderBar, new_chat_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyHeaderBar, new_sms_mms_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyHeaderBar, new_group_chat_button);

  gtk_widget_class_bind_template_child (widget_class, ChattyHeaderBar, leave_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyHeaderBar, block_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyHeaderBar, unblock_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyHeaderBar, archive_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyHeaderBar, unarchive_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyHeaderBar, delete_button);

  gtk_widget_class_bind_template_callback (widget_class, header_back_clicked_cb);

  signals [BACK_CLICKED] =
    g_signal_new ("back-clicked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
chatty_header_bar_init (ChattyHeaderBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

void
chatty_header_bar_set_can_search (ChattyHeaderBar *self,
                                  gboolean         can_search)
{
  g_return_if_fail (CHATTY_IS_HEADER_BAR (self));

  gtk_widget_set_visible (self->search_button, can_search);
}

void
chatty_header_bar_show_archived (ChattyHeaderBar *self,
                                 gboolean         show_archived)
{
  g_return_if_fail (CHATTY_IS_HEADER_BAR (self));

  if (show_archived) {
    hdy_header_bar_set_title (HDY_HEADER_BAR (self->main_header_bar), _("Archived"));
    gtk_widget_show (self->back_button);
    gtk_widget_hide (self->add_chat_button);
  } else {
    hdy_header_bar_set_title (HDY_HEADER_BAR (self->main_header_bar), _("Chats"));
    gtk_widget_hide (self->back_button);
    gtk_widget_show (self->add_chat_button);
  }
}

void
chatty_header_bar_set_item (ChattyHeaderBar *self,
                            ChattyItem      *item)
{
  g_return_if_fail (CHATTY_IS_HEADER_BAR (self));
  g_return_if_fail (!item || CHATTY_IS_ITEM (item));

  if (self->item == item)
    return;

  if (self->item)
    g_clear_signal_handler (&self->content_handler, self->item);

  g_set_object (&self->item, item);

  g_clear_object (&self->title_binding);
  gtk_label_set_label (GTK_LABEL (self->content_title), "");
  gtk_widget_set_visible (self->content_menu_button, !!item);

  header_bar_update_item_state_button (self, item);
  gtk_widget_set_visible (self->content_avatar, !!item);
  gtk_widget_hide (self->call_button);

  if (item) {
    gtk_widget_set_visible (self->leave_button, !CHATTY_IS_MM_CHAT (item));
    /* We can't delete MaChat */
    gtk_widget_set_visible (self->delete_button, !CHATTY_IS_MA_CHAT (item));

    if (CHATTY_IS_MM_CHAT (item)) {
      GListModel *users;
      const char *name;

      users = chatty_chat_get_users (CHATTY_CHAT (item));
      name = chatty_chat_get_chat_name (CHATTY_CHAT (item));

      /* allow changing state only for 1:1 SMS/MMS chats  */
      if (g_list_model_get_n_items (users) == 1)
        header_bar_update_item_state_button (self, item);

      if (g_list_model_get_n_items (users) == 1 &&
          chatty_utils_username_is_valid (name, CHATTY_PROTOCOL_MMS_SMS)) {
        g_autoptr(ChattyMmBuddy) buddy = NULL;
        g_autoptr(GAppInfo) app_info = NULL;

        app_info = g_app_info_get_default_for_uri_scheme ("tel");
        buddy = g_list_model_get_item (users, 0);

        if (app_info)
          gtk_widget_show (self->call_button);
      }
    }

    chatty_avatar_set_item (CHATTY_AVATAR (self->content_avatar), item);
    self->title_binding = g_object_bind_property (item, "name",
                                                  self->content_title, "label",
                                                  G_BINDING_SYNC_CREATE);

    if (CHATTY_IS_CHAT (item))
      self->content_handler = g_signal_connect_object (item, "changed",
                                                       G_CALLBACK (header_bar_chat_changed_cb),
                                                       self, G_CONNECT_SWAPPED);
  }
}

void
chatty_header_bar_set_content_box (ChattyHeaderBar *self,
                                   GtkWidget       *widget)
{
  GtkWidget *child;

  g_return_if_fail (CHATTY_IS_HEADER_BAR (self));
  g_return_if_fail (HDY_IS_LEAFLET (widget));

  g_object_bind_property (widget, "visible-child-name",
                          self->leaflet, "visible-child-name",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (widget, "folded",
                          self->header_group, "decorate-all",
                          G_BINDING_SYNC_CREATE);

  child = hdy_leaflet_get_child_by_name (HDY_LEAFLET (widget), "sidebar");
  gtk_size_group_add_widget (GTK_SIZE_GROUP (self->sidebar_group), child);

  child = hdy_leaflet_get_child_by_name (HDY_LEAFLET (widget), "content");
  gtk_size_group_add_widget (GTK_SIZE_GROUP (self->content_group), child);
}

void
chatty_header_bar_set_search_bar (ChattyHeaderBar *self,
                                  GtkWidget       *widget)
{
  g_return_if_fail (CHATTY_IS_HEADER_BAR (self));
  g_return_if_fail (GTK_WIDGET (widget));

  g_object_bind_property (widget, "search-mode-enabled",
                          self->search_button, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
}
