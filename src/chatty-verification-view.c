/* chatty-verification-view.c
 *
 * Copyright 2022 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-verification-view"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>

#include "chatty-manager.h"
#include "chatty-avatar.h"
#include "chatty-ma-key-chat.h"
#include "chatty-verification-view.h"

static const char *emojis[][2] = {
  /* TRANSLATORS: You may copy translations from https://github.com/matrix-org/matrix-spec-proposals/blob/old_master/data-definitions/sas-emoji.json
   * if available */
  {"🐶", N_("Dog")}, /* "U+1F436" */
  {"🐱", N_("Cat")}, /* "U+1F431" */
  {"🦁", N_("Lion")}, /* "U+1F981" */
  {"🐎", N_("Horse")}, /* "U+1F40E" */
  {"🦄", N_("Unicorn")}, /* "U+1F984" */
  {"🐷", N_("Pig")}, /* "U+1F437" */
  {"🐘", N_("Elephant")}, /* "U+1F418" */
  {"🐰", N_("Rabbit")}, /* "U+1F430" */
  {"🐼", N_("Panda")}, /* "U+1F43C" */
  {"🐓", N_("Rooster")}, /* "U+1F413" */
  {"🐧", N_("Penguin")}, /* "U+1F427" */
  {"🐢", N_("Turtle")}, /* "U+1F422" */
  {"🐟", N_("Fish")}, /* "U+1F41F" */
  {"🐙", N_("Octopus")}, /* "U+1F419" */
  {"🦋", N_("Butterfly")}, /* "U+1F98B" */
  {"🌷", N_("Flower")}, /* "U+1F337" */
  {"🌳", N_("Tree")}, /* "U+1F333" */
  {"🌵", N_("Cactus")}, /* "U+1F335" */
  {"🍄", N_("Mushroom")}, /* "U+1F344" */
  {"🌏", N_("Globe")}, /* "U+1F30F" */
  {"🌙", N_("Moon")}, /* "U+1F319" */
  {"☁️", N_("Cloud")}, /* "U+2601U+FE0F" */
  {"🔥", N_("Fire")}, /* "U+1F525" */
  {"🍌", N_("Banana")}, /* "U+1F34C" */
  {"🍎", N_("Apple")}, /* "U+1F34E" */
  {"🍓", N_("Strawberry")}, /* "U+1F353" */
  {"🌽", N_("Corn")}, /* "U+1F33D" */
  {"🍕", N_("Pizza")}, /* "U+1F355" */
  {"🎂", N_("Cake")}, /* "U+1F382" */
  {"❤️", N_("Heart")}, /* "U+2764U+FE0F" */
  {"😀", N_("Smiley")}, /* "U+1F600" */
  {"🤖", N_("Robot")}, /* "U+1F916" */
  {"🎩", N_("Hat")}, /* "U+1F3A9" */
  {"👓", N_("Glasses")}, /* "U+1F453" */
  {"🔧", N_("Spanner")}, /* "U+1F527" */
  {"🎅", N_("Santa")}, /* "U+1F385" */
  {"👍", N_("Thumbs Up")}, /* "U+1F44D" */
  {"☂️", N_("Umbrella")}, /* "U+2602U+FE0F" */
  {"⌛", N_("Hourglass")}, /* "U+231B" */
  {"⏰", N_("Clock")}, /* "U+23F0" */
  {"🎁", N_("Gift")}, /* "U+1F381" */
  {"💡", N_("Light Bulb")}, /* "U+1F4A1" */
  {"📕", N_("Book")}, /* "U+1F4D5" */
  {"✏️", N_("Pencil")}, /* "U+270FU+FE0F" */
  {"📎", N_("Paperclip")}, /* "U+1F4CE" */
  {"✂️", N_("Scissors")}, /* "U+2702U+FE0F" */
  {"🔒", N_("Lock")}, /* "U+1F512" */
  {"🔑", N_("Key")}, /* "U+1F511" */
  {"🔨", N_("Hammer")}, /* "U+1F528" */
  {"☎️", N_("Telephone")}, /* "U+260EU+FE0F" */
  {"🏁", N_("Flag")}, /* "U+1F3C1" */
  {"🚂", N_("Train")}, /* "U+1F682" */
  {"🚲", N_("Bicycle")}, /* "U+1F6B2" */
  {"✈️", N_("Aeroplane")}, /* "U+2708U+FE0F" */
  {"🚀", N_("Rocket")}, /* "U+1F680" */
  {"🏆", N_("Trophy")}, /* "U+1F3C6" */
  {"⚽", N_("Ball")}, /* "U+26BD" */
  {"🎸", N_("Guitar")}, /* "U+1F3B8" */
  {"🎺", N_("Trumpet")}, /* "U+1F3BA" */
  {"🔔", N_("Bell")}, /* "U+1F514" */
  {"⚓", N_("Anchor")}, /* "U+2693" */
  {"🎧", N_("Headphones")}, /* "U+1F3A7" */
  {"📁", N_("Folder")}, /* "U+1F4C1" */
  {"📌", N_("Pin")}, /* "U+1F4CC" */
};

struct _ChattyVerificationView
{
  GtkBox            parent_instance;

  GtkWidget        *user_avatar;
  GtkWidget        *name_label;
  GtkWidget        *username_label;

  GtkWidget        *continue_button;
  GtkWidget        *continue_spinner;
  GtkWidget        *cancel_button;
  GtkWidget        *cancel_spinner;

  GtkWidget        *verification_dialog;
  GtkWidget        *verification_type_button;
  GtkWidget        *content_stack;

  GtkWidget        *decimal_content;
  GtkWidget        *decimal1_label;
  GtkWidget        *decimal2_label;
  GtkWidget        *decimal3_label;

  GtkWidget        *emoji_content;
  GtkWidget        *emoji1_label;
  GtkWidget        *emoji1_title;
  GtkWidget        *emoji2_label;
  GtkWidget        *emoji2_title;
  GtkWidget        *emoji3_label;
  GtkWidget        *emoji3_title;
  GtkWidget        *emoji4_label;
  GtkWidget        *emoji4_title;
  GtkWidget        *emoji5_label;
  GtkWidget        *emoji5_title;
  GtkWidget        *emoji6_label;
  GtkWidget        *emoji6_title;
  GtkWidget        *emoji7_label;
  GtkWidget        *emoji7_title;

  ChattyMaKeyChat *item;

  GBinding        *name_binding;

  gulong           update_handler;
  gulong           delete_handler;

  gboolean         emoji_set;
  gboolean         emoji_shown;
};

G_DEFINE_TYPE (ChattyVerificationView, chatty_verification_view, GTK_TYPE_BOX)

static void
show_verification_dailog (ChattyVerificationView *self)
{
  GtkWidget *window;

  g_assert (CHATTY_IS_VERIFICATION_VIEW (self));

  window = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);
  gtk_window_set_transient_for (GTK_WINDOW (self->verification_dialog), GTK_WINDOW (window));
  gtk_window_present (GTK_WINDOW (self->verification_dialog));
  gtk_spinner_stop (GTK_SPINNER (self->continue_spinner));

  self->emoji_shown = TRUE;
}

static void
verification_update_emoji (ChattyVerificationView *self)
{
  g_autoptr(GPtrArray) labels = NULL;
  g_autoptr(GPtrArray) titles = NULL;
  char *decimal_str;
  GPtrArray *emoji;
  guint16 *decimal;

  g_assert (CHATTY_IS_VERIFICATION_VIEW (self));

  if (self->emoji_set)
    return;

  emoji = chatty_ma_key_get_emoji (self->item);
  decimal = chatty_ma_key_get_decimal (self->item);

  if (!emoji || !decimal)
    return;

  self->emoji_set = TRUE;
  labels = g_ptr_array_new ();
  g_ptr_array_add (labels, self->emoji1_label);
  g_ptr_array_add (labels, self->emoji2_label);
  g_ptr_array_add (labels, self->emoji3_label);
  g_ptr_array_add (labels, self->emoji4_label);
  g_ptr_array_add (labels, self->emoji5_label);
  g_ptr_array_add (labels, self->emoji6_label);
  g_ptr_array_add (labels, self->emoji7_label);

  titles = g_ptr_array_new ();
  g_ptr_array_add (titles, self->emoji1_title);
  g_ptr_array_add (titles, self->emoji2_title);
  g_ptr_array_add (titles, self->emoji3_title);
  g_ptr_array_add (titles, self->emoji4_title);
  g_ptr_array_add (titles, self->emoji5_title);
  g_ptr_array_add (titles, self->emoji6_title);
  g_ptr_array_add (titles, self->emoji7_title);

  for (guint i = 0; i < emoji->len; i++) {
    GtkLabel *label_widget, *title_widget;
    const char *title = "";
    char *label;

    label = emoji->pdata[i];

    for (guint j = 0; j < G_N_ELEMENTS (emojis); j++) {
      if (g_strcmp0 (label, emojis[j][0]) != 0)
        continue;

      title = emojis[j][1];
      break;
    }

    label_widget = labels->pdata[i];
    title_widget = titles->pdata[i];

    gtk_label_set_label (label_widget, label);
    gtk_label_set_label (title_widget, gettext (title));
  }

  decimal_str = g_strdup_printf ("%4u", (int)decimal[0]);
  gtk_label_set_label (GTK_LABEL (self->decimal1_label), decimal_str);
  g_free (decimal_str);

  decimal_str = g_strdup_printf ("%4u", (int)decimal[1]);
  gtk_label_set_label (GTK_LABEL (self->decimal2_label), decimal_str);
  g_free (decimal_str);

  decimal_str = g_strdup_printf ("%4u", (int)decimal[2]);
  gtk_label_set_label (GTK_LABEL (self->decimal3_label), decimal_str);
  g_free (decimal_str);
}

static void
verification_item_updated_cb (ChattyVerificationView *self)
{
  CmEvent *event;

  g_assert (CHATTY_IS_VERIFICATION_VIEW (self));

  event = chatty_ma_key_chat_get_event (self->item);

  if (g_object_get_data (G_OBJECT (event), "cancel") ||
      g_object_get_data (G_OBJECT (event), "completed")) {
    ChattyManager *manager;

    manager = chatty_manager_get_default ();
    g_signal_emit_by_name (manager, "chat-deleted", self->item);
    return;
  }

  if (!self->emoji_shown &&
      g_object_get_data (G_OBJECT (event), "key")) {
    verification_update_emoji (self);
    show_verification_dailog (self);
  }
}

static void
verification_item_deleted_cb (ChattyVerificationView *self)
{
  g_assert (CHATTY_IS_VERIFICATION_VIEW (self));
}

static void
verification_key_continue_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  g_autoptr(ChattyVerificationView) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (CHATTY_IS_MA_KEY_CHAT (object));

  chatty_ma_key_cancel_finish (CHATTY_MA_KEY_CHAT (object), result, &error);

  if (error) {
    gtk_spinner_stop (GTK_SPINNER (self->continue_spinner));
    gtk_widget_set_sensitive (self->continue_button, TRUE);
    g_warning ("Error: %s", error->message);
  }
}

static void
verification_continue_clicked_cb (ChattyVerificationView *self)
{
  g_assert (CHATTY_IS_VERIFICATION_VIEW (self));

  gtk_spinner_start (GTK_SPINNER (self->continue_spinner));
  gtk_widget_set_sensitive (self->continue_button, FALSE);

  chatty_ma_key_accept_async (CHATTY_MA_KEY_CHAT (self->item),
                              verification_key_continue_cb,
                              g_object_ref (self));
}

static void
verification_key_cancel_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  g_autoptr(ChattyVerificationView) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (CHATTY_IS_MA_KEY_CHAT (object));

  gtk_spinner_stop (GTK_SPINNER (self->cancel_spinner));
  if (chatty_ma_key_cancel_finish (CHATTY_MA_KEY_CHAT (object), result, &error)) {
    ChattyManager *manager;

    manager = chatty_manager_get_default ();
    g_signal_emit_by_name (manager, "chat-deleted", object);
  }

  if (error)
    g_warning ("Error: %s", error->message);
}

static void
verification_cancel_clicked_cb (ChattyVerificationView *self,
                                GtkWidget              *widget)
{
  g_assert (CHATTY_IS_VERIFICATION_VIEW (self));

  gtk_spinner_start (GTK_SPINNER (self->cancel_spinner));
  gtk_widget_set_sensitive (self->continue_button, FALSE);
  gtk_widget_hide (self->verification_dialog);

  chatty_ma_key_cancel_async (CHATTY_MA_KEY_CHAT (self->item),
                              verification_key_cancel_cb,
                              g_object_ref (self));
}

static void
verification_key_match_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  g_autoptr(ChattyVerificationView) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (CHATTY_IS_MA_KEY_CHAT (object));

  gtk_widget_hide (self->verification_dialog);
  chatty_ma_key_match_finish (CHATTY_MA_KEY_CHAT (object), result, &error);

  if (error)
    g_warning ("Error: %s", error->message);
}

static void
verification_type_clicked_cb (ChattyVerificationView *self)
{
  GtkWidget *visible_child;

  g_assert (CHATTY_IS_VERIFICATION_VIEW (self));

  visible_child = gtk_stack_get_visible_child (GTK_STACK (self->content_stack));

  if (visible_child == self->decimal_content)
    visible_child = self->emoji_content;
  else
    visible_child = self->decimal_content;

  gtk_stack_set_visible_child (GTK_STACK (self->content_stack), visible_child);
}

static void
verification_match_clicked_cb (ChattyVerificationView *self)
{
  g_assert (CHATTY_IS_VERIFICATION_VIEW (self));

  gtk_widget_set_sensitive (self->continue_button, FALSE);
  gtk_spinner_start (GTK_SPINNER (self->continue_spinner));
  chatty_ma_key_match_async (CHATTY_MA_KEY_CHAT (self->item),
                             verification_key_match_cb,
                             g_object_ref (self));
}

static void
verification_content_child_changed_cb (ChattyVerificationView *self)
{
  const char *button_label;

  g_assert (CHATTY_IS_VERIFICATION_VIEW (self));

  if (gtk_stack_get_visible_child (GTK_STACK (self->content_stack)) == self->decimal_content)
    button_label = _("Show Emojis");
  else
    button_label = _("Show Numbers");

  gtk_button_set_label (GTK_BUTTON (self->verification_type_button), button_label);
}

static void
chatty_verification_view_dispose (GObject *object)
{
  ChattyVerificationView *self = (ChattyVerificationView *)object;

  g_clear_object (&self->item);
  g_clear_object (&self->name_binding);

  G_OBJECT_CLASS (chatty_verification_view_parent_class)->dispose (object);
}

static void
chatty_verification_view_class_init (ChattyVerificationViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = chatty_verification_view_dispose;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/Chatty/"
                                               "ui/chatty-verification-view.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, user_avatar);
  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, name_label);
  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, username_label);

  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, continue_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, continue_spinner);
  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, cancel_spinner);

  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, verification_dialog);
  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, verification_type_button);
  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, content_stack);

  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, decimal_content);
  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, decimal1_label);
  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, decimal2_label);
  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, decimal3_label);

  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, emoji_content);
  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, emoji1_label);
  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, emoji1_title);
  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, emoji2_label);
  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, emoji2_title);
  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, emoji3_label);
  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, emoji3_title);
  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, emoji4_label);
  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, emoji4_title);
  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, emoji5_label);
  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, emoji5_title);
  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, emoji6_label);
  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, emoji6_title);
  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, emoji7_label);
  gtk_widget_class_bind_template_child (widget_class, ChattyVerificationView, emoji7_title);

  gtk_widget_class_bind_template_callback (widget_class, verification_continue_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, verification_cancel_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, verification_type_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, verification_match_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, verification_content_child_changed_cb);
}

static void
chatty_verification_view_init (ChattyVerificationView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  verification_content_child_changed_cb (self);
}

void
chatty_verification_view_set_item (ChattyVerificationView *self,
                                   ChattyItem             *item)
{
  ChattyItem *sender;

  g_return_if_fail (CHATTY_IS_VERIFICATION_VIEW (self));

  if (self->item) {
    self->emoji_set = FALSE;
    self->emoji_shown = FALSE;
    g_clear_object (&self->name_binding);
    chatty_avatar_set_item (CHATTY_AVATAR (self->user_avatar), NULL);
    g_clear_signal_handler (&self->update_handler, self->item);
    g_clear_signal_handler (&self->delete_handler, self->item);
    gtk_spinner_stop (GTK_SPINNER (self->continue_spinner));
    gtk_spinner_stop (GTK_SPINNER (self->cancel_spinner));
    gtk_widget_hide (self->verification_dialog);
    g_clear_object (&self->item);
  }

  if (!CHATTY_IS_MA_KEY_CHAT (item))
    return;

  g_set_object (&self->item, (ChattyMaKeyChat *)item);

  if (!item)
    return;

  gtk_widget_set_sensitive (self->continue_button, TRUE);
  self->update_handler = g_signal_connect_object (item, "changed",
                                                  G_CALLBACK (verification_item_updated_cb),
                                                  self, G_CONNECT_SWAPPED);
  self->delete_handler = g_signal_connect_object (item, "deleted",
                                                  G_CALLBACK (verification_item_deleted_cb),
                                                  self, G_CONNECT_SWAPPED);
  sender = chatty_ma_key_chat_get_sender (CHATTY_MA_KEY_CHAT (item));
  chatty_avatar_set_item (CHATTY_AVATAR (self->user_avatar), sender);

  self->name_binding = g_object_bind_property (sender, "name",
                                               self->name_label, "label",
                                               G_BINDING_SYNC_CREATE);
  gtk_label_set_label (GTK_LABEL (self->username_label),
                       chatty_item_get_username (CHATTY_ITEM (sender)));
}
