#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define CHATTY_TYPE_PROGRESS_BUTTON (chatty_progress_button_get_type ())
G_DECLARE_FINAL_TYPE (ChattyProgressButton, chatty_progress_button, CHATTY, PROGRESS_BUTTON, AdwBin)

void       chatty_progress_button_pulse        (ChattyProgressButton *self);
void       chatty_progress_button_set_fraction (ChattyProgressButton *self,
                                                double                fraction);
void       chatty_progress_button_set_icon     (ChattyProgressButton *self,
                                                const char           *action_button_icon);

G_END_DECLS
