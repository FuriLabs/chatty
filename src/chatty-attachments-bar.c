/* chatty-attachments-bar.c
 *
 * Copyright 2021 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-attachments-bar"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "chatty-file.h"
#include "chatty-attachment.h"
#include "chatty-attachments-bar.h"

struct _ChattyAttachmentsBar
{
  GtkBox parent_instance;

  GtkWidget *scrolled_window;
  GtkWidget *files_grid;

  GListStore *files_store;        /* unowned */
  GtkSelectionModel *files_model; /* unowned */
};

G_DEFINE_TYPE (ChattyAttachmentsBar, chatty_attachments_bar, GTK_TYPE_BOX)

static void
attachment_bar_file_deleted_cb (ChattyAttachmentsBar *self,
                                GtkWidget            *child)
{
  GFile *file;
  guint position;

  g_assert (CHATTY_IS_ATTACHMENTS_BAR (self));
  g_assert (GTK_IS_WIDGET (child));

  if (gtk_widget_in_destruction (GTK_WIDGET (self)))
    return;

  file = chatty_attachment_get_file (CHATTY_ATTACHMENT (child));
  if (g_list_store_find (self->files_store, file, &position))
    g_list_store_remove (self->files_store, position);

  g_debug ("Removed file: %p", file);
}

static void
setup_list_item_cb (GtkListItemFactory   *factory,
                    GtkListItem          *list_item,
                    ChattyAttachmentsBar *self)
{
  GtkWidget *child;

  g_assert (CHATTY_IS_ATTACHMENTS_BAR (self));

  child = g_object_new (CHATTY_TYPE_ATTACHMENT, NULL);
  g_signal_connect_object (child, "deleted",
                           G_CALLBACK (attachment_bar_file_deleted_cb),
                           self, G_CONNECT_SWAPPED);

  gtk_list_item_set_child (list_item, child);
  gtk_list_item_set_activatable (list_item, FALSE);
}

static void
bind_list_item_cb (GtkListItemFactory *factory,
                   GtkListItem        *list_item)
{
  GtkWidget *attachment;
  GFile *file;

  attachment = gtk_list_item_get_child (list_item);
  file = gtk_list_item_get_item (list_item);
  chatty_attachment_set_file (CHATTY_ATTACHMENT (attachment), file);
}

static void
chatty_attachments_bar_class_init (ChattyAttachmentsBarClass *klass)
{
}

static void
chatty_attachments_bar_init (ChattyAttachmentsBar *self)
{
  GtkListItemFactory *factory;
  GListModel *files;

  self->files_store = g_list_store_new (G_TYPE_FILE);
  files = G_LIST_MODEL (self->files_store);
  self->files_model = GTK_SELECTION_MODEL (gtk_no_selection_new (files));

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_list_item_cb), self);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_list_item_cb), NULL);

  self->files_grid = gtk_grid_view_new (self->files_model, factory);
  gtk_grid_view_set_min_columns (GTK_GRID_VIEW (self->files_grid), 2);
  gtk_widget_set_margin_top (self->files_grid, 6);
  gtk_widget_set_margin_bottom (self->files_grid, 6);
  gtk_widget_set_margin_start (self->files_grid, 6);
  gtk_widget_set_margin_end (self->files_grid, 6);
  gtk_widget_add_css_class (self->files_grid, "frame");

  gtk_box_append (GTK_BOX (self), self->files_grid);

}

GtkWidget *
chatty_attachments_bar_new (void)
{
  return g_object_new (CHATTY_TYPE_ATTACHMENTS_BAR, NULL);
}

void
chatty_attachments_bar_reset (ChattyAttachmentsBar *self)
{
  g_return_if_fail (CHATTY_IS_ATTACHMENTS_BAR (self));

  g_list_store_remove_all (self->files_store);
}

void
chatty_attachments_bar_add_file (ChattyAttachmentsBar *self,
                                 GFile                *file)
{
  g_return_if_fail (CHATTY_IS_ATTACHMENTS_BAR (self));
  g_return_if_fail (G_IS_FILE (file));

  g_list_store_append (self->files_store, file);
}

/**
 * chatty_attachments_bar_get_files:
 * @self: A #ChattyAttachmentsBar
 *
 * Get the list of files attached. The list contains
 * ChattyFileInfo and the list should be freed with
 * g_list_free_full(list, (GDestroyNotify)chatty_file_info_free)
 *
 * Returns: (transfer full) (nullable): A List of strings.
 */
GList *
chatty_attachments_bar_get_files (ChattyAttachmentsBar *self)
{
  GList *files = NULL;
  guint n_items;

  g_return_val_if_fail (CHATTY_IS_ATTACHMENTS_BAR (self), NULL);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->files_store));

  for (guint i = 0; i < n_items; i++) {
    g_autoptr(GFile) file = NULL;
    ChattyFile *chatty_file;

    file = g_list_model_get_item (G_LIST_MODEL (self->files_store), i);
    chatty_file = chatty_file_new_for_path (g_file_peek_path (file));
    files = g_list_append (files, chatty_file);
  }

  return files;
}

GListModel *
chatty_attachments_bar_get_files_list (ChattyAttachmentsBar *self)
{
  g_return_val_if_fail (CHATTY_IS_ATTACHMENTS_BAR (self), NULL);

  return G_LIST_MODEL (self->files_store);
}
