/* chatty-file-item.c
 *
 * Copyright 2023 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-file-item"

#include <glib/gi18n.h>

#include "gtk3-to-4.h"
#include "chatty-enums.h"
#include "chatty-file.h"
#include "chatty-chat-view.h"
#include "chatty-progress-button.h"
#include "chatty-message.h"
#include "chatty-file-item.h"


struct _ChattyFileItem
{
  AdwBin         parent_instance;

  GtkWidget     *file_overlay;
  GtkWidget     *progress_button;
  GtkWidget     *file_title;

  GtkGesture    *activate_gesture;
  ChattyMessage *message;
  ChattyFile    *file;
};

G_DEFINE_TYPE (ChattyFileItem, chatty_file_item, ADW_TYPE_BIN)


static void
file_item_get_stream_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  g_autoptr(ChattyFileItem) self = user_data;
  g_autoptr(GInputStream) stream = NULL;

  if (gtk_widget_in_destruction (GTK_WIDGET (self)))
    return;

  stream = chatty_file_get_stream_finish (self->file, result, NULL);

  if (!stream)
    return;
}

static gboolean
item_set_file (gpointer user_data)
{
  ChattyFileItem *self = user_data;

  /* It's possible that we get signals after dispose().
   * Fix warning in those cases
   */
  if (!self->file)
    return G_SOURCE_REMOVE;

  chatty_file_get_stream_async (self->file, NULL,
                                file_item_get_stream_cb,
                                g_object_ref (self));
  return G_SOURCE_REMOVE;
}

static void
file_item_update_message (ChattyFileItem *self)
{
  ChattyFileStatus status;

  g_assert (CHATTY_IS_FILE_ITEM (self));
  g_assert (self->message);
  g_assert (self->file);

  status = chatty_file_get_status (self->file);

  if (status == CHATTY_FILE_UNKNOWN)
    chatty_progress_button_set_fraction (CHATTY_PROGRESS_BUTTON (self->progress_button), 0.0);
  else if (status == CHATTY_FILE_DOWNLOADING)
    chatty_progress_button_pulse (CHATTY_PROGRESS_BUTTON (self->progress_button));
  else
    gtk_widget_hide (self->progress_button);

  /* Update in idle so that self is added to the parent container */
  if (status == CHATTY_FILE_DOWNLOADED)
    g_idle_add (item_set_file, self);
}

static void
file_progress_button_action_clicked_cb (ChattyFileItem *self)
{
  GtkWidget *view;

  g_assert (CHATTY_IS_FILE_ITEM (self));

  view = gtk_widget_get_ancestor (GTK_WIDGET (self), CHATTY_TYPE_CHAT_VIEW);

  g_signal_emit_by_name (view, "file-requested", self->message);
}

static void
file_item_button_clicked (ChattyFileItem *self)
{
  g_autoptr(GFile) file = NULL;
  g_autofree char *uri = NULL;
  GList *file_list;
  const char *path;

  g_assert (CHATTY_IS_FILE_ITEM (self));
  g_assert (self->file);

  file_list = chatty_message_get_files (self->message);

  if (!file_list || !file_list->data)
    return;

  path = chatty_file_get_path (file_list->data);
  if (!path)
    return;

  if (chatty_message_get_cm_event (self->message))
    file = g_file_new_build_filename (path, NULL);
  else
    file = g_file_new_build_filename (g_get_user_data_dir (), "chatty", path, NULL);

  uri = g_file_get_uri (file);

  gtk_show_uri (NULL, uri, GDK_CURRENT_TIME);
}

static void
chatty_file_item_dispose (GObject *object)
{
  ChattyFileItem *self = (ChattyFileItem *)object;

  g_clear_object (&self->message);
  g_clear_object (&self->file);

  G_OBJECT_CLASS (chatty_file_item_parent_class)->dispose (object);
}

static void
chatty_file_item_class_init (ChattyFileItemClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = chatty_file_item_dispose;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/Chatty/"
                                               "ui/chatty-file-item.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyFileItem, file_overlay);
  gtk_widget_class_bind_template_child (widget_class, ChattyFileItem, progress_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyFileItem, file_title);

  gtk_widget_class_bind_template_callback (widget_class, file_progress_button_action_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, file_item_button_clicked);

  g_type_ensure (CHATTY_TYPE_PROGRESS_BUTTON);
}

static void
chatty_file_item_init (ChattyFileItem *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
chatty_file_item_new (ChattyMessage *message,
                      ChattyFile    *file)
{
  ChattyFileItem *self;
  const char *file_name;

  g_return_val_if_fail (CHATTY_IS_MESSAGE (message), NULL);

  if (!file) {
    GList *files;

    files = chatty_message_get_files (message);
    if (files)
      file = files->data;
  }

  g_return_val_if_fail (CHATTY_IS_FILE (file), NULL);

  self = g_object_new (CHATTY_TYPE_FILE_ITEM, NULL);
  self->message = g_object_ref (message);
  self->file = g_object_ref (file);

  file_name = chatty_file_get_name (file);
  gtk_widget_set_visible (self->file_title, file_name && *file_name);

  if (file_name)
    gtk_label_set_text (GTK_LABEL (self->file_title), file_name);

  g_signal_connect_object (file, "status-changed",
                           G_CALLBACK (file_item_update_message),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self, "notify::scale-factor",
                           G_CALLBACK (file_item_update_message),
                           self, G_CONNECT_SWAPPED);
  file_item_update_message (self);

  return GTK_WIDGET (self);
}

GtkStyleContext *
chatty_file_item_get_style (ChattyFileItem *self)
{
  g_return_val_if_fail (CHATTY_IS_FILE_ITEM (self), NULL);

  return gtk_widget_get_style_context (self->file_overlay);
}

ChattyMessage *
chatty_file_item_get_item (ChattyFileItem *self)
{
  g_return_val_if_fail (CHATTY_IS_FILE_ITEM (self), NULL);

  return self->message;
}
