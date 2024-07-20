/*
 * Copyright 2023 Purism SPC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-side-bar"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <adwaita.h>
#include <glib/gi18n.h>

#include "contrib/contrib.h"
#include "chatty-manager.h"
#include "chatty-mm-account.h"
#include "chatty-purple.h"
#include "chatty-chat-list.h"
#include "chatty-selectable-row.h"
#include "chatty-side-bar.h"

struct _ChattySideBar
{
  AdwNavigationPage            parent_instance;

  GtkWidget         *header_bar;
  GtkWidget         *back_button;
  GtkWidget         *add_chat_button;
  GtkWidget         *search_button;
  GtkWidget         *chats_search_bar;
  GtkWidget         *chats_search_entry;

  GtkWidget         *chat_list;

  GtkWidget         *main_menu_popover;

  GtkWidget         *new_message_popover;
  GtkWidget         *new_chat_button;
  GtkWidget         *new_sms_mms_button;
  GtkWidget         *new_group_chat_button;

  GtkWidget         *protocol_list_popover;
  GtkWidget         *protocol_list;
  GtkWidget         *protocol_any_row;

  DemoTaggedEntryTag *protocol_tag;

  GtkWidget         *selected_protocol_row;
  ChattyProtocol     protocol_filter;
};


G_DEFINE_TYPE (ChattySideBar, chatty_side_bar, ADW_TYPE_NAVIGATION_PAGE)

enum {
  SELECTION_CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

void
chatty_side_bar_set_show_end_title_buttons (ChattySideBar *self,
                                            gboolean       visible)
{
  adw_header_bar_set_show_end_title_buttons (ADW_HEADER_BAR (self->header_bar), visible);
}

static void
side_bar_update_search_mode (ChattySideBar *self)
{
  GListModel *model;
  gboolean has_child;

  g_assert (CHATTY_IS_SIDE_BAR (self));

  model = chatty_chat_list_get_filter_model (CHATTY_CHAT_LIST (self->chat_list));
  has_child = g_list_model_get_n_items (model) > 0;

  gtk_widget_set_visible (self->search_button, has_child);

  if (!has_child)
    gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (self->chats_search_bar), FALSE);
}

static void
side_bar_active_protocols_changed_cb (ChattySideBar *self)
{
  ChattyManager *manager;
  ChattyAccount *mm_account;
  ChattyProtocol protocols;
  gboolean has_mms, has_sms, has_im;

  g_assert (CHATTY_IS_SIDE_BAR (self));

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
side_bar_back_clicked_cb (ChattySideBar *self)
{
  g_assert (CHATTY_IS_SIDE_BAR (self));

  if (chatty_chat_list_is_archived (CHATTY_CHAT_LIST (self->chat_list))) {
    chatty_side_bar_set_show_archived (self, FALSE);
    side_bar_update_search_mode (self);
  } else {
    /* window_set_item (self, NULL); */
  }
}

static void
side_bar_search_enable_changed_cb (ChattySideBar *self)
{
  gboolean enabled;

  g_assert (CHATTY_IS_SIDE_BAR (self));

  enabled = gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (self->chats_search_bar));

  /* Reset protocol filter */
  if (!enabled &&
      self->protocol_any_row != self->selected_protocol_row)
    gtk_widget_activate (self->protocol_any_row);

  if (enabled)
    gtk_widget_grab_focus (self->chats_search_entry);
}

static void
side_bar_search_changed_cb (ChattySideBar *self,
                            GtkEntry      *entry)
{
  g_assert (CHATTY_IS_SIDE_BAR (self));
  g_assert (GTK_IS_EDITABLE (entry));

  chatty_chat_list_filter_string (CHATTY_CHAT_LIST (self->chat_list),
                                  gtk_editable_get_text (GTK_EDITABLE (entry)));
}

static void
side_bar_chat_list_selection_changed_cb (ChattySideBar  *self,
                                         ChattyChatList *list)
{
  g_assert (CHATTY_IS_SIDE_BAR (self));
  g_assert (CHATTY_IS_CHAT_LIST (list));

  g_signal_emit (self, signals[SELECTION_CHANGED], 0, list);
}

static void
side_bar_protocol_changed_cb (ChattySideBar *self,
                              GtkWidget     *selected_row,
                              GtkListBox    *box)
{
  DemoTaggedEntry *entry;
  GtkWidget *old_row;

  g_assert (CHATTY_IS_SIDE_BAR (self));
  g_assert (GTK_IS_LIST_BOX (box));

  entry = DEMO_TAGGED_ENTRY (self->chats_search_entry);
  old_row = self->selected_protocol_row;

  if (old_row == selected_row)
    return;

  self->protocol_filter = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (selected_row), "protocol"));
  chatty_chat_list_filter_protocol (CHATTY_CHAT_LIST (self->chat_list), self->protocol_filter);
  chatty_selectable_row_set_selected (CHATTY_SELECTABLE_ROW (old_row), FALSE);
  chatty_selectable_row_set_selected (CHATTY_SELECTABLE_ROW (selected_row), TRUE);
  self->selected_protocol_row = selected_row;

  if (selected_row == self->protocol_any_row) {
    demo_tagged_entry_remove_tag (entry, GTK_WIDGET (self->protocol_tag));
  } else {
    const char *title;

    if (!gtk_widget_get_parent (GTK_WIDGET (self->protocol_tag)))
      demo_tagged_entry_add_tag (entry, GTK_WIDGET (self->protocol_tag));
    title = chatty_selectable_row_get_title (CHATTY_SELECTABLE_ROW (selected_row));
    demo_tagged_entry_tag_set_label (self->protocol_tag, title);
  }
}

static void
chatty_side_bar_map (GtkWidget *widget)
{
  ChattySideBar *self = (ChattySideBar *)widget;
  ChattyManager *manager;

  manager = chatty_manager_get_default ();
  g_signal_connect_object (manager, "notify::active-protocols",
                           G_CALLBACK (side_bar_active_protocols_changed_cb), self,
                           G_CONNECT_SWAPPED);

  side_bar_active_protocols_changed_cb (self);

  g_signal_connect_object (chatty_chat_list_get_filter_model (CHATTY_CHAT_LIST (self->chat_list)),
                           "items-changed",
                           G_CALLBACK (side_bar_update_search_mode), self,
                           G_CONNECT_SWAPPED);

  side_bar_update_search_mode (self);

  GTK_WIDGET_CLASS (chatty_side_bar_parent_class)->map (widget);
}


static void
chatty_side_bar_finalize (GObject *object)
{
  ChattySideBar *self = (ChattySideBar *)object;

  g_clear_object (&self->protocol_tag);

  G_OBJECT_CLASS (chatty_side_bar_parent_class)->finalize (object);
}

static void
chatty_side_bar_class_init (ChattySideBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = chatty_side_bar_finalize;

  widget_class->map = chatty_side_bar_map;

  signals [SELECTION_CHANGED] =
    g_signal_new ("selection-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, CHATTY_TYPE_CHAT_LIST);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/Chatty/"
                                               "ui/chatty-side-bar.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattySideBar, header_bar);
  gtk_widget_class_bind_template_child (widget_class, ChattySideBar, back_button);
  gtk_widget_class_bind_template_child (widget_class, ChattySideBar, add_chat_button);
  gtk_widget_class_bind_template_child (widget_class, ChattySideBar, search_button);
  gtk_widget_class_bind_template_child (widget_class, ChattySideBar, chats_search_bar);
  gtk_widget_class_bind_template_child (widget_class, ChattySideBar, chats_search_entry);

  gtk_widget_class_bind_template_child (widget_class, ChattySideBar, chat_list);

  gtk_widget_class_bind_template_child (widget_class, ChattySideBar, main_menu_popover);

  gtk_widget_class_bind_template_child (widget_class, ChattySideBar, new_message_popover);
  gtk_widget_class_bind_template_child (widget_class, ChattySideBar, new_chat_button);
  gtk_widget_class_bind_template_child (widget_class, ChattySideBar, new_sms_mms_button);

  gtk_widget_class_bind_template_child (widget_class, ChattySideBar, protocol_list_popover);
  gtk_widget_class_bind_template_child (widget_class, ChattySideBar, protocol_list);
  gtk_widget_class_bind_template_child (widget_class, ChattySideBar, new_group_chat_button);

  gtk_widget_class_bind_template_callback (widget_class, side_bar_back_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, side_bar_search_enable_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, side_bar_search_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, side_bar_chat_list_selection_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, side_bar_protocol_changed_cb);

  g_type_ensure (CHATTY_TYPE_SELECTABLE_ROW);
}

static void
protocol_list_header_func (GtkListBoxRow *row,
                           GtkListBoxRow *before,
                           gpointer       user_data)
{
  if (!before) {
    gtk_list_box_row_set_header (row, NULL);
  } else if (!gtk_list_box_row_get_header (row)) {
    GtkWidget *separator;

    separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_visible (separator, TRUE);
    gtk_list_box_row_set_header (row, separator);
  }
}

static void
side_bar_add_selectable_row (ChattySideBar  *self,
                             const char     *name,
                             ChattyProtocol  protocol,
                             gboolean        selected)
{
  GtkWidget *row;

  row = chatty_selectable_row_new (name);
  g_object_set_data (G_OBJECT (row), "protocol", GINT_TO_POINTER (protocol));
  chatty_selectable_row_set_selected (CHATTY_SELECTABLE_ROW (row), selected);
  gtk_list_box_append (GTK_LIST_BOX (self->protocol_list), row);

  if (protocol == CHATTY_PROTOCOL_ANY)
    self->protocol_any_row = self->selected_protocol_row = row;
}

static void
chatty_side_bar_init (ChattySideBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->protocol_filter = CHATTY_PROTOCOL_ANY;
  gtk_search_bar_connect_entry (GTK_SEARCH_BAR (self->chats_search_bar),
                                GTK_EDITABLE (self->chats_search_entry));
  gtk_list_box_set_header_func (GTK_LIST_BOX (self->protocol_list),
                                protocol_list_header_func,
                                NULL, NULL);
  self->protocol_tag = demo_tagged_entry_tag_new ("");
  g_object_ref_sink (self->protocol_tag);

  side_bar_add_selectable_row (self, _("Any Protocol"), CHATTY_PROTOCOL_ANY, TRUE);
  side_bar_add_selectable_row (self, _("Matrix"), CHATTY_PROTOCOL_MATRIX, FALSE);
  side_bar_add_selectable_row (self, _("SMS/MMS"), CHATTY_PROTOCOL_MMS_SMS, FALSE);

#ifdef PURPLE_ENABLED
  side_bar_add_selectable_row (self, _("XMPP"), CHATTY_PROTOCOL_XMPP, FALSE);

  if (chatty_purple_has_telegram_loaded (chatty_purple_get_default ()))
    side_bar_add_selectable_row (self, _("Telegram"), CHATTY_PROTOCOL_TELEGRAM, FALSE);
#endif
}

GtkWidget *
chatty_side_bar_get_chat_list (ChattySideBar *self)
{
  g_return_val_if_fail (CHATTY_IS_SIDE_BAR (self), NULL);

  return self->chat_list;
}

void
chatty_side_bar_set_show_archived (ChattySideBar *self,
                                   gboolean       show_archived)
{
  GtkWidget *title;

  g_return_if_fail (CHATTY_IS_SIDE_BAR (self));

  title = adw_header_bar_get_title_widget (ADW_HEADER_BAR (self->header_bar));
  adw_window_title_set_title (ADW_WINDOW_TITLE (title),
                              show_archived ? _("Archived") : _("Chats"));
  gtk_widget_set_visible (self->back_button, show_archived);
  gtk_widget_set_visible (self->add_chat_button, !show_archived);
  chatty_chat_list_show_archived (CHATTY_CHAT_LIST (self->chat_list), show_archived);
}


void
chatty_side_bar_toggle_search (ChattySideBar *self)
{
  gboolean active;

  g_return_if_fail (CHATTY_IS_SIDE_BAR (self));

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->search_button));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->search_button), !active);
}
