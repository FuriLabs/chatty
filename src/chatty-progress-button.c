#define G_LOG_DOMAIN "chatty-progress-button"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "chatty-progress-button.h"

struct _ChattyProgressButton
{
  AdwBin      parent_instance;

  GtkWidget  *action_button;

  char       *action_icon;
  double      fraction;

  double      start_fraction;
  double      end_fraction;
  gboolean    is_clockwise;

  guint       pulse_id;
  guint       update_id;
};


G_DEFINE_TYPE (ChattyProgressButton, chatty_progress_button, ADW_TYPE_BIN)

enum {
  ACTION_CLICKED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static gboolean
progress_button_update_pulse (gpointer user_data)
{
  ChattyProgressButton *self = user_data;

  if (self->start_fraction <= 0.0)
    self->is_clockwise = TRUE;
  else if (self->end_fraction >= 1.0)
    self->is_clockwise = FALSE;

  if (self->is_clockwise)
    {
      self->start_fraction += 0.01;
      self->end_fraction += 0.01;
    }
  else
    {
      self->start_fraction -= 0.01;
      self->end_fraction -= 0.01;
    }

  gtk_widget_queue_draw (user_data);

  return G_SOURCE_CONTINUE;
}

static gboolean
progress_button_update_fraction (gpointer user_data)
{
  ChattyProgressButton *self = user_data;

  if (self->end_fraction >= self->fraction) {
    self->update_id = 0;

    return G_SOURCE_REMOVE;
  }

  /* todo: Do some non-linear value change */
  self->end_fraction += 0.01;
  gtk_widget_queue_draw (user_data);

  return G_SOURCE_CONTINUE;
}


static void
chatty_progress_button_clicked_cb (ChattyProgressButton *self)
{
  g_assert (CHATTY_IS_PROGRESS_BUTTON (self));

  g_signal_emit (self, signals[ACTION_CLICKED], 0);
}

static void
chatty_progress_button_snapshot (GtkWidget   *widget,
                                 GtkSnapshot *snapshot)
{
  ChattyProgressButton *self = (ChattyProgressButton *)widget;
  GtkStyleContext *sc;
  cairo_t *cr;
  int width, height;
  GdkRGBA color = {0};

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  cr = gtk_snapshot_append_cairo (snapshot,
                                  &GRAPHENE_RECT_INIT (0, 0, width, height));
  cairo_save (cr);

  sc = gtk_widget_get_style_context (widget);
  if (gtk_style_context_lookup_color (sc, "accent_color", &color))
    cairo_set_source_rgb (cr, color.red, color.green, color.blue);
  else
    cairo_set_source_rgb (cr, 0.2078, 0.5176, 0.894);
  cairo_set_line_width (cr, 6);

  cairo_arc (cr, width / 2., height / 2., width / 2. - 4.,
             2 * (self->start_fraction - 0.25) * G_PI, 2 * (self->end_fraction - 0.25) * G_PI);
  cairo_stroke_preserve (cr);
  cairo_restore (cr);
  cairo_destroy (cr);

  return GTK_WIDGET_CLASS (chatty_progress_button_parent_class)->snapshot (widget, snapshot);
}


static void
chatty_progress_button_finalize (GObject *object)
{
  ChattyProgressButton *self = (ChattyProgressButton *)object;

  g_clear_handle_id (&self->pulse_id, g_source_remove);
  g_clear_handle_id (&self->update_id, g_source_remove);

  g_clear_pointer (&self->action_icon, g_free);

  G_OBJECT_CLASS (chatty_progress_button_parent_class)->finalize (object);
}

static void
chatty_progress_button_class_init (ChattyProgressButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = chatty_progress_button_finalize;
  widget_class->snapshot = chatty_progress_button_snapshot;

  signals [ACTION_CLICKED] =
    g_signal_new ("action-clicked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/sm/puri/Chatty/"
                                               "ui/chatty-progress-button.ui");

  gtk_widget_class_bind_template_child (widget_class, ChattyProgressButton, action_button);

  gtk_widget_class_bind_template_callback (widget_class, chatty_progress_button_clicked_cb);
}

static void
chatty_progress_button_init (ChattyProgressButton *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

void
chatty_progress_button_pulse (ChattyProgressButton *self)
{
  g_return_if_fail (CHATTY_IS_PROGRESS_BUTTON (self));

  if (self->pulse_id)
    return;

  g_clear_handle_id (&self->update_id, g_source_remove);

  self->start_fraction = 0.0;
  self->end_fraction = 0.35;
  self->pulse_id = g_timeout_add (30, progress_button_update_pulse, self);
}

void
chatty_progress_button_set_fraction (ChattyProgressButton *self,
                                     double                fraction)
{
  g_return_if_fail (CHATTY_IS_PROGRESS_BUTTON (self));

  g_clear_handle_id (&self->pulse_id, g_source_remove);

  self->start_fraction = 0.0;
  self->end_fraction = CLAMP (fraction, 0.0, 1.0);

  if (!self->update_id)
    self->update_id = g_timeout_add (30, progress_button_update_fraction, self);
}
