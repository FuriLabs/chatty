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
  GtkWidget *files_box;
};

G_DEFINE_TYPE (ChattyAttachmentsBar, chatty_attachments_bar, GTK_TYPE_BOX)

static void
chatty_attachments_bar_class_init (ChattyAttachmentsBarClass *klass)
{
}

static void
chatty_attachments_bar_init (ChattyAttachmentsBar *self)
{
  self->scrolled_window = gtk_scrolled_window_new ();
  gtk_widget_set_size_request (self->scrolled_window, -1, 194);
  gtk_widget_set_hexpand (self->scrolled_window, TRUE);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scrolled_window),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);

  self->files_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (self->scrolled_window), self->files_box);
  gtk_box_append (GTK_BOX (self), self->scrolled_window);

  gtk_widget_add_css_class (self->scrolled_window, "content");
  gtk_widget_add_css_class (self->scrolled_window, "view");
  gtk_widget_add_css_class (self->scrolled_window, "frame");
}

GtkWidget *
chatty_attachments_bar_new (void)
{
  return g_object_new (CHATTY_TYPE_ATTACHMENTS_BAR, NULL);
}

static void
chatty_attachemnts_bar_update_bar (ChattyAttachmentsBar *self)
{
  g_assert (CHATTY_IS_ATTACHMENTS_BAR (self));

  if (!gtk_widget_get_first_child (self->files_box)) {
    GtkWidget *parent;

    parent = gtk_widget_get_parent (GTK_WIDGET (self));

    if (GTK_IS_REVEALER (parent))
      gtk_revealer_set_reveal_child (GTK_REVEALER (parent), FALSE);
    else
      gtk_widget_hide (parent);
  }
}

void
chatty_attachments_bar_reset (ChattyAttachmentsBar *self)
{
  GtkWidget *child;

  g_return_if_fail (CHATTY_IS_ATTACHMENTS_BAR (self));

  do {
    child = gtk_widget_get_first_child (GTK_WIDGET (self->files_box));

    if (child)
      gtk_box_remove (GTK_BOX (self->files_box), child);
  } while (child);

  chatty_attachemnts_bar_update_bar (self);
}

static void
attachment_bar_file_deleted_cb (ChattyAttachmentsBar *self,
                                GtkWidget            *child)
{
  g_assert (CHATTY_IS_ATTACHMENTS_BAR (self));
  g_assert (GTK_IS_WIDGET (child));

  if (gtk_widget_in_destruction (GTK_WIDGET (self)))
    return;

  gtk_box_remove (GTK_BOX (self->files_box), child);
  chatty_attachemnts_bar_update_bar (self);

  g_debug ("Remove file: %p", child);

}

void
chatty_attachments_bar_add_file (ChattyAttachmentsBar *self,
                                 GFile                *file)
{
  GtkWidget *child;

  g_return_if_fail (CHATTY_IS_ATTACHMENTS_BAR (self));
  g_return_if_fail (G_IS_FILE (file));

  child = chatty_attachment_new (file);
  g_debug ("Add file: %p", child);

  gtk_box_append (GTK_BOX (self->files_box), child);
  g_signal_connect_object (child, "deleted",
                           G_CALLBACK (attachment_bar_file_deleted_cb),
                           self, G_CONNECT_SWAPPED);
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
  GtkWidget *child;
  GList *files = NULL;

  g_return_val_if_fail (CHATTY_IS_ATTACHMENTS_BAR (self), NULL);

  child = gtk_widget_get_first_child (self->files_box);

  while (child)
    {
      ChattyFile *chatty_file;
      GFile *file;

      file = chatty_attachment_get_file (CHATTY_ATTACHMENT (child));
      chatty_file = chatty_file_new_for_path (g_file_peek_path (file));
      files = g_list_append (files, chatty_file);

      child = gtk_widget_get_next_sibling (child);
    }

  return files;
}
