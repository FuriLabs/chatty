/*
 * Copyright (C) 2023 Purism SPC
 *               2023 Chris Talbot
 * Author(s):
 *   Chris Talbot <chris@talbothome.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib.h>
#include <gio/gio.h>
#include <camel/camel.h>
#include <libedataserver/libedataserver.h>

#include "chatty-enums.h"
#include "chatty-file.h"

G_BEGIN_DECLS

typedef enum
{
  CHATTY_PGP_UNKNOWN,
  CHATTY_PGP_SIGNED,
  CHATTY_PGP_ENCRYPTED,
  CHATTY_PGP_PUBLIC_KEY,
  CHATTY_PGP_PRIVATE_KEY
} ChattyPgpMessage;

CamelMimePart     *chatty_pgp_create_mime_part (const char  *contents,
                                                GList       *files,
                                                char       **recipients,
                                                gboolean     bind_recipients);

CamelMimePart     *chatty_pgp_sign_stream        (const char  *contents_to_sign,
                                                  GList       *files,
                                                  const char  *signing_id,
                                                  char       **recipients);

CamelMimePart     *chatty_pgp_encrypt_stream (const char    *contents_to_encrypt,
                                              GList         *files,
                                              const char    *signing_id,
                                              char         **recipients,
                                              CamelMimePart *sigpart);

CamelMimePart     *chatty_pgp_sign_and_encrypt_stream (const char  *contents_to_sign_and_encrypt,
                                                       GList       *files,
                                                       const char  *signing_id,
                                                       char       **recipients);

CamelCipherValidity *chatty_pgp_check_sig_mime_part (CamelMimePart  *sigpart);
CamelCipherValidity *chatty_pgp_check_sig_stream (const char        *data_to_check,
                                                  CamelMimePart    **output_part);

CamelMimePart       *chatty_pgp_decrypt_mime_part (CamelMimePart *encpart);
CamelMimePart       *chatty_pgp_decrypt_stream (const char *data_to_check);

ChattyPgpMessage     chatty_pgp_check_pgp_type (const char *data_to_check);
char                *chatty_pgp_decode_mime_part (CamelMimePart *mime_part);
char                *chatty_pgp_get_content (CamelMimePart *mime_part,
                                             GList        **files,
                                             const char    *directory_to_save_in);

char                *chatty_pgp_get_recipients (CamelMimePart *mime_part);
GFile               *chatty_pgp_get_pub_key    (const char *signing_id,
                                                const char *save_directory);
char                *chatty_pgp_get_pub_fingerprint (const char *signing_id);
G_END_DECLS
