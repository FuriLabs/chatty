/* chatty-media.c
 *
 * Copyright 2020, 2021 Purism SPC
 *           2021, Chris Talbot
 *
 * Author(s):
 *   Chris Talbot
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <math.h>
#include <glib-object.h>

#include "chatty-file.h"
#include "chatty-utils.h"

G_BEGIN_DECLS

ChattyFile *chatty_media_scale_image_to_size_sync   (ChattyFile     *input_file,
                                                     gsize           desired_size,
                                                     gboolean        use_temp_file);

G_END_DECLS
