/* chatty-chat-page.c
 *
 * Copyright 2020-2023 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-chat-page"

#include "config.h"

#include <glib/gi18n.h>

#include "chatty-mm-chat.h"
#include "chatty-purple.h"
#include "chatty-settings.h"
#include "chatty-message-row.h"
#include "chatty-message-bar.h"
#include "chatty-chat-page.h"

struct _ChattyChatPage
{
  AdwBin      parent_instance;

  GtkWidget  *message_view;

  GtkWidget  *scrolled_window;
  GtkWidget  *message_list;
  GtkWidget  *loading_spinner;
  GtkWidget  *typing_revealer;
  GtkWidget  *typing_indicator;
  GtkWidget  *chatty_message_list;
  GtkWidget  *message_bar;
  GtkWidget  *no_message_status;
  GtkWidget  *scroll_down_button;
  GtkAdjustment *vadjustment;
  ChattyHistory *history;

  GDBusProxy *osk_proxy;

  ChattyChat *chat;
  guint       refresh_typing_id;

  guint       scroll_bottom_id;
  guint       osk_id;
  guint       history_load_id;
  gboolean    should_scroll;
};

#define SCROLL_TIMEOUT       60  /* milliseconds */
#define HISTORY_WAIT_TIMEOUT 100 /* milliseconds */
#define INDICATOR_WIDTH   60
#define INDICATOR_HEIGHT  40
#define INDICATOR_MARGIN   2
#define MSG_BUBBLE_MAX_RATIO .3

G_DEFINE_TYPE (ChattyChatPage, chatty_chat_page, ADW_TYPE_BIN)


enum {
  FILE_REQUESTED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static void
chatty_draw_typing_indicator (cairo_t *cr)
{
  double dot_pattern [3][3]= {{0.5, 0.9, 0.9},
                              {0.7, 0.5, 0.9},
                              {0.9, 0.7, 0.5}};
  guint  dot_origins [3] = {15, 30, 45};
  double grey_lev,
    x, y,
    width, height,
    rad, deg;

  static guint i;

  deg = G_PI / 180.0;

  rad = INDICATOR_MARGIN * 5;
  x = y = INDICATOR_MARGIN;
  width = INDICATOR_WIDTH - INDICATOR_MARGIN * 2;
  height = INDICATOR_HEIGHT - INDICATOR_MARGIN * 2;

  if (i > 2)
    i = 0;

  cairo_new_sub_path (cr);
  cairo_arc (cr, x + width - rad, y + rad, rad, -90 * deg, 0 * deg);
  cairo_arc (cr, x + width - rad, y + height - rad, rad, 0 * deg, 90 * deg);
  cairo_arc (cr, x + rad, y + height - rad, rad, 90 * deg, 180 * deg);
  cairo_arc (cr, x + rad, y + rad, rad, 180 * deg, 270 * deg);
  cairo_close_path (cr);

  cairo_set_source_rgb (cr, 0.7, 0.7, 0.7);
  cairo_set_line_width (cr, 1.0);
  cairo_stroke (cr);

  for (guint n = 0; n < 3; n++) {
    cairo_arc (cr, dot_origins[n], 20, 5, 0, 2 * G_PI);
    grey_lev = dot_pattern[i][n];
    cairo_set_source_rgb (cr, grey_lev, grey_lev, grey_lev);
    cairo_fill (cr);
  }

  i++;
}

static void
chat_page_typing_indicator_draw_cb (GtkDrawingArea *drawing_area,
                                    cairo_t        *cr,
                                    int             width,
                                    int             height,
                                    gpointer        user_data)
{
  ChattyChatPage *self = user_data;

  g_assert (CHATTY_IS_CHAT_PAGE (self));

  if (self->refresh_typing_id > 0)
    chatty_draw_typing_indicator (cr);
}

static gboolean
chat_page_indicator_refresh_cb (ChattyChatPage *self)
{
  g_assert (CHATTY_IS_CHAT_PAGE (self));

  gtk_widget_queue_draw (self->typing_indicator);

  return G_SOURCE_CONTINUE;
}

static gboolean
chatty_chat_page_scroll_is_bottom (ChattyChatPage *self)
{
  double upper, value, page_size;

  value = gtk_adjustment_get_value (self->vadjustment);
  upper = gtk_adjustment_get_upper (self->vadjustment);
  page_size = gtk_adjustment_get_page_size (self->vadjustment);

  return ABS (upper - page_size - value) <= DBL_EPSILON;
}

static gboolean
update_view_scroll (gpointer user_data)
{
  ChattyChatPage *self = user_data;
  gdouble size, upper, value;

  g_assert (CHATTY_IS_CHAT_PAGE (self));

  g_clear_handle_id (&self->scroll_bottom_id, g_source_remove);

  size  = gtk_adjustment_get_page_size (self->vadjustment);
  value = gtk_adjustment_get_value (self->vadjustment);
  upper = gtk_adjustment_get_upper (self->vadjustment);

  if (self->should_scroll) {
    self->should_scroll = FALSE;
    gtk_adjustment_set_value (self->vadjustment, upper - size);

    return G_SOURCE_REMOVE;
  }

  if (upper - size <= DBL_EPSILON)
    return G_SOURCE_REMOVE;

  /* If close to bottom, scroll to bottom */
  if (upper - value < (size * 1.75))
    gtk_adjustment_set_value (self->vadjustment, upper - size);

  return G_SOURCE_REMOVE;
}

static void
chatty_chat_page_update (ChattyChatPage *self)
{
  ChattyProtocol protocol;
  g_autofree char *icon = g_strdup_printf ("%s-symbolic", CHATTY_APP_ID);

  g_assert (CHATTY_IS_CHAT_PAGE (self));

  protocol = chatty_item_get_protocols (CHATTY_ITEM (self->chat));

#ifdef PURPLE_ENABLED
  if (chatty_chat_is_im (self->chat) && CHATTY_IS_PP_CHAT (self->chat))
    chatty_pp_chat_load_encryption_status (CHATTY_PP_CHAT (self->chat));
#endif

  adw_status_page_set_icon_name (ADW_STATUS_PAGE (self->no_message_status), icon);
  if (protocol == CHATTY_PROTOCOL_MMS) {
    adw_status_page_set_title (ADW_STATUS_PAGE (self->no_message_status),
                               _("This is an MMS conversation"));
    adw_status_page_set_description (ADW_STATUS_PAGE (self->no_message_status),
                                     _("Your messages are not encrypted, "
                                       "and carrier rates may apply"));
  }
  if (protocol == CHATTY_PROTOCOL_MMS_SMS) {
    /* An SMS/MMS with multiple users is an MMS conversation */
    if (g_list_model_get_n_items (chatty_chat_get_users (self->chat)) > 1)
      adw_status_page_set_title (ADW_STATUS_PAGE (self->no_message_status),
                                 _("This is an MMS conversation"));
    else if (chatty_chat_has_file_upload (self->chat))
      adw_status_page_set_title (ADW_STATUS_PAGE (self->no_message_status),
                                 _("This is an SMS/MMS conversation"));
    else
      adw_status_page_set_title (ADW_STATUS_PAGE (self->no_message_status),
                                 _("This is an SMS conversation"));
    adw_status_page_set_description (ADW_STATUS_PAGE (self->no_message_status),
                                     _("Your messages are not encrypted, "
                                       "and carrier rates may apply"));
  } else {
    adw_status_page_set_title (ADW_STATUS_PAGE (self->no_message_status),
                               _("This is an IM conversation"));
    adw_status_page_set_description (ADW_STATUS_PAGE (self->no_message_status),
                                     _("Your messages are not encrypted, "
                                       "and carrier rates may apply"));
    if (chatty_chat_get_encryption (self->chat) == CHATTY_ENCRYPTION_ENABLED)
      adw_status_page_set_description (ADW_STATUS_PAGE (self->no_message_status),
                                       _("Your messages are encrypted"));
    else
      adw_status_page_set_description (ADW_STATUS_PAGE (self->no_message_status),
                                       _("Your messages are not encrypted"));
  }
}

static void
chat_page_scroll_down_clicked_cb (ChattyChatPage *self)
{
  g_assert (CHATTY_IS_CHAT_PAGE (self));

  /* Temporarily disable kinetic scrolling */
  /* Otherwise, adjustment value isn't updated if kinetic scrolling is active */
  gtk_scrolled_window_set_kinetic_scrolling (GTK_SCROLLED_WINDOW (self->scrolled_window), FALSE);
  gtk_adjustment_set_value (self->vadjustment,
                            gtk_adjustment_get_upper (self->vadjustment) -
                            gtk_adjustment_get_page_size (self->vadjustment));
  gtk_scrolled_window_set_kinetic_scrolling (GTK_SCROLLED_WINDOW (self->scrolled_window), TRUE);
}

static void
chat_page_edge_overshot_cb (ChattyChatPage  *self,
                            GtkPositionType  pos)
{
  g_assert (CHATTY_IS_CHAT_PAGE (self));

  if (pos == GTK_POS_TOP)
    chatty_chat_load_past_messages (self->chat, -1);
}

static GtkWidget *
chat_page_message_row_new (ChattyMessage  *message,
                           ChattyChatPage *self)
{
  GtkWidget *row;
  ChattyProtocol protocol;

  g_assert (CHATTY_IS_MESSAGE (message));
  g_assert (CHATTY_IS_CHAT_PAGE (self));

  protocol = chatty_item_get_protocols (CHATTY_ITEM (self->chat));
  row = chatty_message_row_new (message, protocol, chatty_chat_is_im (self->chat));
  chatty_message_row_set_alias (CHATTY_MESSAGE_ROW (row),
                                chatty_message_get_user_alias (message));

  /* If we are close to bottom, mark as we should scroll after the row is added */
  if (chatty_chat_page_scroll_is_bottom (self))
    self->should_scroll = TRUE;

  return GTK_WIDGET (row);
}

static void
chat_buddy_typing_changed_cb (ChattyChatPage *self)
{
  g_assert (CHATTY_IS_CHAT_PAGE (self));

  if (chatty_chat_get_buddy_typing (self->chat)) {
    gtk_revealer_set_reveal_child (GTK_REVEALER (self->typing_revealer), TRUE);
    self->refresh_typing_id = g_timeout_add (300,
                                             (GSourceFunc)chat_page_indicator_refresh_cb,
                                             self);
  } else {
    gtk_revealer_set_reveal_child (GTK_REVEALER (self->typing_revealer), FALSE);
    g_clear_handle_id (&self->refresh_typing_id, g_source_remove);
  }
}

static void
chat_page_loading_history_cb (ChattyChatPage *self)
{
  gboolean loading;

  g_assert (CHATTY_IS_CHAT_PAGE (self));

  loading = chatty_chat_is_loading_history (self->chat);

  if (loading) {
    gtk_spinner_start (GTK_SPINNER (self->loading_spinner));
    gtk_widget_set_opacity (self->loading_spinner, 1.0);
  } else {
    gtk_spinner_stop (GTK_SPINNER (self->loading_spinner));
    gtk_widget_set_opacity (self->loading_spinner, 0.0);
  }
}

static void
chat_page_message_items_changed (ChattyChatPage *self)
{
  GListModel *messages;

  g_assert (CHATTY_IS_CHAT_PAGE (self));

  if (!self->chat)
    return;

  messages = chatty_chat_get_messages (self->chat);

  if (g_list_model_get_n_items (messages) == 0)
    gtk_widget_set_valign (self->message_list, GTK_ALIGN_FILL);
  else
    gtk_widget_set_valign (self->message_list, GTK_ALIGN_END);
}

static void
chat_page_chat_changed_cb (ChattyChatPage *self)
{
  if (!self->chat)
    return;

  if (CHATTY_IS_MM_CHAT (self->chat))
    chatty_chat_set_unread_count (self->chat, 0);
}

static void
chat_page_adjustment_value_changed_cb (ChattyChatPage *self)
{
  g_assert (CHATTY_IS_CHAT_PAGE (self));

  gtk_widget_set_visible (self->scroll_down_button,
                          !chatty_chat_page_scroll_is_bottom (self));

  if (self->should_scroll) {
    g_clear_handle_id (&self->scroll_bottom_id, g_source_remove);
    self->scroll_bottom_id = g_timeout_add (SCROLL_TIMEOUT, update_view_scroll, self);
  }
}

static void
chat_page_update_header_func (ChattyMessageRow *row,
                              ChattyMessageRow *before,
                              gpointer          user_data)
{
  ChattyChatPage *self = user_data;
  ChattyMessage *a, *b;
  time_t a_time, b_time;

  if (!before || !row)
    return;

  a = chatty_message_row_get_item (before);
  b = chatty_message_row_get_item (row);
  a_time = chatty_message_get_time (a);
  b_time = chatty_message_get_time (b);

  if (chatty_message_user_matches (a, b))
    chatty_message_row_hide_user_detail (row);

  /* Don't hide footers in outgoing SMS as it helps understanding
   * the delivery status of the message
   */
  if (CHATTY_IS_MM_CHAT (self->chat) &&
      chatty_message_get_msg_direction (a) == CHATTY_DIRECTION_OUT)
    return;

  /* Hide footer of the previous message if both have same time (in minutes) */
  if (a_time / 60 == b_time / 60)
    chatty_message_row_hide_footer (before);
}

static void
chat_page_get_files_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  g_autoptr(ChattyChatPage) self = user_data;
}

static void
chat_page_file_requested_cb (ChattyChatPage *self,
                             ChattyMessage  *message)
{
  chatty_chat_get_files_async (self->chat, message,
                               chat_page_get_files_cb,
                               g_object_ref (self));
}

static void
osk_properties_changed_cb (ChattyChatPage *self,
                           GVariant       *changed_properties)
{
  g_autoptr(GVariant) value = NULL;
  GtkWindow *window;

  window = (GtkWindow *)gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);
  if (!window || !chatty_utils_window_has_toplevel_focus (window))
    return;

  value = g_variant_lookup_value (changed_properties, "Visible", NULL);

  if (value && g_variant_get_boolean (value)) {
    g_clear_handle_id (&self->scroll_bottom_id, g_source_remove);
    self->scroll_bottom_id = g_timeout_add (SCROLL_TIMEOUT, update_view_scroll, self);
  }
}

static void
osk_proxy_new_cb (GObject      *service,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  g_autoptr(ChattyChatPage) self = user_data;
  g_autoptr(GError) error = NULL;

  self->osk_proxy = g_dbus_proxy_new_finish (res, &error);

  if (error) {
    g_warning ("Error creating osk proxy: %s", error->message);
    return;
  }

  g_signal_connect_object (self->osk_proxy, "g-properties-changed",
                           G_CALLBACK (osk_properties_changed_cb), self,
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);
}

static void
osk_appeared_cb (GDBusConnection *connection,
                 const char      *name,
                 const char      *name_owner,
                 gpointer         user_data)
{
  ChattyChatPage *self = user_data;

  g_assert (CHATTY_IS_CHAT_PAGE (self));

  g_dbus_proxy_new (connection,
                    G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION |
                    G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START |
                    G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                    NULL,
                    "sm.puri.OSK0",
                    "/sm/puri/OSK0",
                    "sm.puri.OSK0",
                    NULL,
                    osk_proxy_new_cb,
                    g_object_ref (self));
}

static void
osk_vanished_cb (GDBusConnection *connection,
                 const char      *name,
                 gpointer         user_data)
{
  ChattyChatPage *self = user_data;

  g_assert (CHATTY_IS_CHAT_PAGE (self));

  g_clear_object (&self->osk_proxy);
}

static void
chatty_chat_page_finalize (GObject *object)
{
  ChattyChatPage *self = (ChattyChatPage *)object;

  g_clear_handle_id (&self->history_load_id, g_source_remove);
  g_clear_handle_id (&self->osk_id, g_bus_unwatch_name);
  g_clear_handle_id (&self->scroll_bottom_id, g_source_remove);
  g_clear_object (&self->osk_proxy);
  g_clear_object (&self->chat);

  G_OBJECT_CLASS (chatty_chat_page_parent_class)->finalize (object);
}

static void
chatty_chat_page_class_init (ChattyChatPageClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = chatty_chat_page_finalize;

  signals [FILE_REQUESTED] =
    g_signal_new ("file-requested",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, CHATTY_TYPE_MESSAGE);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/Chatty/"
                                               "ui/chatty-chat-page.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyChatPage, message_view);

  gtk_widget_class_bind_template_child (widget_class, ChattyChatPage, scrolled_window);
  gtk_widget_class_bind_template_child (widget_class, ChattyChatPage, scroll_down_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyChatPage, message_list);
  gtk_widget_class_bind_template_child (widget_class, ChattyChatPage, loading_spinner);
  gtk_widget_class_bind_template_child (widget_class, ChattyChatPage, typing_revealer);
  gtk_widget_class_bind_template_child (widget_class, ChattyChatPage, typing_indicator);
  gtk_widget_class_bind_template_child (widget_class, ChattyChatPage, message_bar);
  gtk_widget_class_bind_template_child (widget_class, ChattyChatPage, no_message_status);
  gtk_widget_class_bind_template_child (widget_class, ChattyChatPage, vadjustment);

  gtk_widget_class_bind_template_callback (widget_class, chat_page_scroll_down_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, chat_page_edge_overshot_cb);
  gtk_widget_class_bind_template_callback (widget_class, chat_page_typing_indicator_draw_cb);
  gtk_widget_class_bind_template_callback (widget_class, chat_page_adjustment_value_changed_cb);

  g_type_ensure (CHATTY_TYPE_MESSAGE_BAR);
}

static void
chatty_chat_page_init (ChattyChatPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (self->typing_indicator),
                                  chat_page_typing_indicator_draw_cb,
                                  g_object_ref (self), g_object_unref);
  gtk_list_box_set_placeholder (GTK_LIST_BOX (self->message_list), self->no_message_status);

  g_signal_connect_after (G_OBJECT (self), "file-requested",
                          G_CALLBACK (chat_page_file_requested_cb), self);
  gtk_list_box_set_header_func (GTK_LIST_BOX (self->message_list),
                                (GtkListBoxUpdateHeaderFunc)chat_page_update_header_func,
                                g_object_ref (self), g_object_unref);

  self->osk_id = g_bus_watch_name (G_BUS_TYPE_SESSION, "sm.puri.OSK0",
                                   G_BUS_NAME_WATCHER_FLAGS_NONE,
                                   osk_appeared_cb,
                                   osk_vanished_cb,
                                   g_object_ref (self),
                                   g_object_unref);
}

GtkWidget *
chatty_chat_page_new (void)
{
  return g_object_new (CHATTY_TYPE_CHAT_PAGE, NULL);
}

static gboolean
chat_page_history_wait_timeout_cb (gpointer user_data)
{
  ChattyChatPage *self = user_data;

  g_assert (CHATTY_IS_CHAT_PAGE (self));

  self->history_load_id = 0;
  self->should_scroll = TRUE;

  chat_page_adjustment_value_changed_cb (self);

  return G_SOURCE_REMOVE;
}

void
chatty_chat_page_set_chat (ChattyChatPage *self,
                           ChattyChat     *chat)
{
  GListModel *messages;

  g_return_if_fail (CHATTY_IS_CHAT_PAGE (self));
  g_return_if_fail (!chat || CHATTY_IS_CHAT (chat));

  chatty_message_bar_set_chat (CHATTY_MESSAGE_BAR (self->message_bar), chat);

  if (self->chat && chat != self->chat) {
    g_signal_handlers_disconnect_by_func (self->chat,
                                          chat_buddy_typing_changed_cb,
                                          self);
    g_signal_handlers_disconnect_by_func (self->chat,
                                          chat_page_loading_history_cb,
                                          self);
    g_signal_handlers_disconnect_by_func (chatty_chat_get_messages (self->chat),
                                          chat_page_message_items_changed,
                                          self);
    g_signal_handlers_disconnect_by_func (chatty_chat_get_messages (self->chat),
                                          chat_page_chat_changed_cb,
                                          self);

    gtk_widget_set_visible (self->scroll_down_button, FALSE);
    self->should_scroll = TRUE;
    g_clear_handle_id (&self->history_load_id, g_source_remove);
  }

  gtk_widget_set_visible (self->no_message_status, !!chat);

  if (!g_set_object (&self->chat, chat))
    return;

  if (!chat) {
    gtk_list_box_bind_model (GTK_LIST_BOX (self->message_list),
                             NULL, NULL, NULL, NULL);
    return;
  }

  messages = chatty_chat_get_messages (chat);

  g_signal_connect_object (messages, "items-changed",
                           G_CALLBACK (chat_page_message_items_changed),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (chat, "changed",
                           G_CALLBACK (chat_page_chat_changed_cb),
                           self, G_CONNECT_SWAPPED);
  chat_page_message_items_changed (self);
  chat_page_chat_changed_cb (self);

  if (g_list_model_get_n_items (messages) <= 3)
    chatty_chat_load_past_messages (chat, -1);


  gtk_list_box_bind_model (GTK_LIST_BOX (self->message_list),
                           chatty_chat_get_messages (self->chat),
                           (GtkListBoxCreateWidgetFunc)chat_page_message_row_new,
                           self, NULL);
  g_signal_connect_object (self->chat, "notify::buddy-typing",
                           G_CALLBACK (chat_buddy_typing_changed_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->chat, "notify::loading-history",
                           G_CALLBACK (chat_page_loading_history_cb),
                           self, G_CONNECT_SWAPPED);

  chat_buddy_typing_changed_cb (self);
  chatty_chat_page_update (self);
  chat_page_loading_history_cb (self);
  chat_page_adjustment_value_changed_cb (self);

  self->history_load_id = g_timeout_add (HISTORY_WAIT_TIMEOUT,
                                         chat_page_history_wait_timeout_cb,
                                         self);
}

ChattyChat *
chatty_chat_page_get_chat (ChattyChatPage *self)
{
  g_return_val_if_fail (CHATTY_IS_CHAT_PAGE (self), NULL);

  return self->chat;
}

void
chatty_chat_page_set_db (ChattyChatPage *self,
                         ChattyHistory  *history)
{
  g_return_if_fail (CHATTY_IS_CHAT_PAGE (self));
  g_return_if_fail (CHATTY_IS_HISTORY (history));

  g_set_object (&self->history, history);
  chatty_message_bar_set_db (CHATTY_MESSAGE_BAR (self->message_bar), history);
}
