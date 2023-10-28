/*
 * Copyright (C) 1999-2008 Novell, Inc.
 *               2023 Purism SPC
 *               2023 Chris Talbot
 *
 * Author(s):
 *   Chris Talbot <chris@talbothome.com>
 *
 * Adapted from:
 * https://gitlab.gnome.org/GNOME/evolution-data-server/-/blob/master/src/camel/tests/smime/
 * https://gitlab.gnome.org/GNOME/evolution/-/tree/master/src/composer
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "chatty-pgp"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "chatty-log.h"
#include "chatty-pgp.h"

#define DEFAULT_SIGNING_HASH "SHA512"

static CamelCipherHash
chatty_pgp_algo_to_camel_hash (const char *hash_algo)
{
  CamelCipherHash res = CAMEL_CIPHER_HASH_DEFAULT;

  if (hash_algo && *hash_algo) {
    if (g_ascii_strcasecmp (hash_algo, "SHA1") == 0)
      res = CAMEL_CIPHER_HASH_SHA1;
    else if (g_ascii_strcasecmp (hash_algo, "SHA256") == 0)
      res = CAMEL_CIPHER_HASH_SHA256;
    else if (g_ascii_strcasecmp (hash_algo, "SHA384") == 0)
      res = CAMEL_CIPHER_HASH_SHA384;
    else if (g_ascii_strcasecmp (hash_algo, "SHA512") == 0)
      res = CAMEL_CIPHER_HASH_SHA512;
  }

  return res;
}

static char *
chatty_pgp_get_data_dir (void)
{
  char *chatty_data_dir = NULL;

  chatty_data_dir = g_build_filename (g_get_user_data_dir (), "chatty", "pgp", NULL);
  g_mkdir_with_parents (chatty_data_dir, 0700);

  return chatty_data_dir;
}

static char *
chatty_pgp_get_tmp_dir (void)
{
  char *chatty_tmp_dir = NULL;

  chatty_tmp_dir = g_build_filename (g_get_tmp_dir (), "chatty", "pgp", NULL);
  g_mkdir_with_parents (chatty_tmp_dir, 0700);

  return chatty_tmp_dir;
}

static CamelSession *
chatty_pgp_create_camel_session (void)
{
  g_autofree char *chatty_cache_dir = NULL;
  g_autofree char *chatty_data_dir = NULL;

  chatty_cache_dir = chatty_pgp_get_tmp_dir ();
  chatty_data_dir = chatty_pgp_get_data_dir ();

  return g_object_new (CAMEL_TYPE_SESSION,
                          "user-data-dir", chatty_cache_dir,
                          "user-cache-dir", chatty_data_dir,
                          NULL);
}

static CamelCipherContext *
chatty_pgp_create_camel_ctx (CamelSession *session)
{
  CamelCipherContext *ctx;

  ctx = camel_gpg_context_new (session);
  camel_gpg_context_set_always_trust (CAMEL_GPG_CONTEXT (ctx), TRUE);

  return ctx;
}

CamelMimePart *
chatty_pgp_create_mime_part (const char  *contents,
                             GList       *files,
                             char       **recipients,
                             gboolean     bind_recipients)
{
  CamelMimePart *conpart = NULL;
  CamelMultipart *body = NULL;

  if (!files && !contents)
    return NULL;

  if (bind_recipients)
    if (!recipients || !*recipients)
      return NULL;

  if (contents) {
    CamelDataWrapper *plaintext_dw;
    CamelStream *plaintext_stream;

    conpart = camel_mime_part_new ();
    plaintext_stream = camel_stream_mem_new ();
    plaintext_dw = camel_data_wrapper_new ();

    camel_stream_write (plaintext_stream, contents, strlen(contents), NULL, NULL);
    g_seekable_seek (G_SEEKABLE (plaintext_stream), 0, G_SEEK_SET, NULL, NULL);

    camel_data_wrapper_construct_from_stream_sync (plaintext_dw, plaintext_stream, NULL, NULL);
    camel_medium_set_content ((CamelMedium *) conpart, plaintext_dw);
    camel_mime_part_set_content_type (conpart, "text/plain");
    camel_mime_part_set_encoding (conpart, CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE);
    g_clear_object (&plaintext_dw);
    g_clear_object (&plaintext_stream);
  }

  if (files) {
    body = camel_multipart_new ();
    camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (body), "multipart/mixed");
    camel_multipart_set_boundary (body, NULL);
    if (contents) {
      camel_multipart_add_part (body, conpart);
      g_clear_object (&conpart);
    }

    for (GList *l = files; l != NULL; l = l->next) {
      g_autoptr(GFile) file_ref = NULL;
      CamelMimePart *part;
      g_autoptr(CamelDataWrapper) file_dw = NULL;
      g_autofree char *file_contents = NULL;
      gsize content_length = 0;
      g_autoptr(GError) error = NULL;
      g_autoptr(CamelStream) file_stream = NULL;

      ChattyFile *attachment = l->data;

      file_dw = camel_data_wrapper_new ();
      part = camel_mime_part_new ();
      file_ref = g_file_new_for_uri (chatty_file_get_url (attachment));
      g_file_load_contents (file_ref, NULL, &file_contents, &content_length, NULL, &error);
      if (error) {
        g_warning ("Getting file contents failed: '%s'", error->message);
        continue;
      }

      file_stream = camel_stream_mem_new ();
      camel_stream_write (file_stream,
                          file_contents,
                          content_length, NULL, NULL);

      g_seekable_seek (G_SEEKABLE (file_stream), 0, G_SEEK_SET, NULL, NULL);

      camel_data_wrapper_construct_from_stream_sync (file_dw, file_stream, NULL, NULL);
      camel_medium_set_content ((CamelMedium *) part, file_dw);

      /*
       * Though you have not encoded them in base64 yet, when they are signed/encrypted
       * They will be base64 encoded.
       */
      camel_mime_part_set_encoding (part, CAMEL_TRANSFER_ENCODING_BASE64);

      if (chatty_file_get_mime_type (attachment))
        camel_mime_part_set_content_type (part, chatty_file_get_mime_type (attachment));

      if (chatty_file_get_name (attachment))
        camel_mime_part_set_filename (part, chatty_file_get_name (attachment));

      camel_multipart_add_part (body, part);
      g_object_unref (part);
    }
    conpart = camel_mime_part_new ();
    camel_medium_set_content (CAMEL_MEDIUM (conpart), CAMEL_DATA_WRAPPER (body));
    g_object_unref (body);
  }

  /*
   * Bind the recipients to signed messages.
   * Reasoning: https://theworld.com/~dtd/sign_encrypt/sign_encrypt7.html
   */
  if (bind_recipients) {
    g_autofree char *recipient_list = NULL;
    recipient_list = g_strjoinv ("," , recipients);
    camel_medium_add_header ((CamelMedium *) conpart,
                             "Recipients",
                             recipient_list);
  }

  return conpart;
}

CamelMimePart *
chatty_pgp_sign_stream (const char   *contents_to_sign,
                        GList        *files,
                        const char   *signing_id,
                        char        **recipients)
{
  CamelSession *session;
  CamelCipherContext *ctx;
  CamelMimePart *sigpart = NULL;
  CamelMimePart *conpart = NULL;
  g_autoptr(GError) error = NULL;

  if (!signing_id || !*signing_id)
    return NULL;

  if (!recipients || !*recipients)
    return NULL;

  session = chatty_pgp_create_camel_session ();

  ctx = chatty_pgp_create_camel_ctx (session);

  /*
   * Bind the recipients to the signed message.
   * Reasoning: https://theworld.com/~dtd/sign_encrypt/sign_encrypt7.html
   */
  conpart = chatty_pgp_create_mime_part (contents_to_sign,
                                         files,
                                         recipients,
                                         TRUE);

  if (!conpart)
    goto out;

  sigpart = camel_mime_part_new ();
  camel_cipher_context_sign_sync (ctx, signing_id, chatty_pgp_algo_to_camel_hash(DEFAULT_SIGNING_HASH),
                                  conpart, sigpart, NULL, &error);

  if (error != NULL) {
    g_warning ("PGP signing failed: '%s'", error->message);
    g_object_unref (sigpart);
    sigpart = NULL;
    goto out;
  }

out:
  g_clear_object (&conpart);
  g_clear_object (&session);
  g_clear_object (&ctx);

  return sigpart;
}

CamelMimePart *
chatty_pgp_encrypt_stream (const char     *contents_to_encrypt,
                           GList          *files,
                           const char     *userid,
                           char          **recipients,
                           CamelMimePart  *sigpart)
{
  CamelSession *session;
  CamelStream *plaintext_stream = NULL;
  CamelDataWrapper *plaintext_dw = NULL;
  CamelDataWrapper *signed_dw = NULL;
  CamelCipherContext *ctx;
  CamelMimePart *encpart = NULL;
  CamelMimePart *conpart = NULL;
  CamelMimePart *ptpart = NULL;
  g_autoptr(GError) error = NULL;
  GPtrArray *recipients_arr;

  if (!userid || !*userid)
    return NULL;

  if (!recipients || !*recipients)
    return NULL;

  session = chatty_pgp_create_camel_session ();

  ctx = chatty_pgp_create_camel_ctx (session);

  /* camel_medium_get_content () is transfer_none */
  if (sigpart && CAMEL_IS_MIME_PART (sigpart))
    signed_dw = camel_medium_get_content (CAMEL_MEDIUM (sigpart));

  if (signed_dw || CAMEL_IS_MULTIPART_SIGNED (signed_dw)) {
    ptpart = sigpart;
  } else {
    conpart = chatty_pgp_create_mime_part (contents_to_encrypt,
                                           files,
                                           recipients,
                                           FALSE);
    if (!conpart)
      goto out;

    ptpart = conpart;
  }
  encpart = camel_mime_part_new ();

  recipients_arr = g_ptr_array_new ();
  for (int i = 0; recipients[i] != NULL; i++) {
    g_ptr_array_add (recipients_arr, (unsigned char *) recipients[i]);
  }
  g_ptr_array_add (recipients_arr, (unsigned char *) userid);

  camel_cipher_context_encrypt_sync (ctx, userid, recipients_arr,
                                     ptpart, encpart, NULL, &error);
  g_ptr_array_free (recipients_arr, TRUE);

  if (error != NULL) {
    g_warning ("PGP encryption failed: '%s'", error->message);
    g_clear_object (&encpart);
    goto out;
  }

out:
  g_clear_object (&session);
  g_clear_object (&ctx);
  g_clear_object (&plaintext_stream);
  g_clear_object (&plaintext_dw);
  g_clear_object (&conpart);

  return encpart;
}

CamelMimePart *
chatty_pgp_sign_and_encrypt_stream (const char  *contents_to_sign_and_encrypt,
                                    GList       *files,
                                    const char  *signing_id,
                                    char       **recipients)
{
  g_autoptr(CamelMimePart) sigpart = NULL;
  CamelMimePart *encpart;
  CamelDataWrapper *signed_dw = NULL;

  /*
   * If you are combining sign & encrypt, you want to do sign THEN Encrypt.
   * Reasoning: https://theworld.com/~dtd/sign_encrypt/sign_encrypt7.html
   */
  sigpart = chatty_pgp_sign_stream (contents_to_sign_and_encrypt,
                                    files,
                                    signing_id,
                                    recipients);

  if (!sigpart)
    return NULL;

  /* camel_medium_get_content () is transfer_none */
  signed_dw = camel_medium_get_content (CAMEL_MEDIUM (sigpart));
  if (!signed_dw) {
    g_clear_object (&sigpart);
    return NULL;
  }

  if (!CAMEL_IS_MULTIPART_SIGNED (signed_dw)) {
    g_clear_object (&sigpart);
    return NULL;
  }

  encpart = chatty_pgp_encrypt_stream (NULL,
                                       NULL,
                                       signing_id,
                                       recipients,
                                       sigpart);

  return encpart;
}

CamelMimePart *
chatty_pgp_decrypt_mime_part (CamelMimePart *encpart)
{
  CamelSession *session;
  CamelCipherContext *ctx;
  g_autoptr(GError) error = NULL;
  CamelDataWrapper *encrypted_dw = NULL;
  CamelMimePart *outpart = NULL;
  CamelCipherValidity *valid = NULL;

  if (!encpart)
    return NULL;

  /* camel_medium_get_content () is transfer_none */
  encrypted_dw = camel_medium_get_content (CAMEL_MEDIUM (encpart));
  if (!encrypted_dw)
    return NULL;

  if (!CAMEL_IS_MULTIPART_ENCRYPTED (encrypted_dw))
    return NULL;

  session = chatty_pgp_create_camel_session ();

  ctx = chatty_pgp_create_camel_ctx (session);

  outpart = camel_mime_part_new ();
  valid = camel_cipher_context_decrypt_sync (ctx, encpart, outpart, NULL, &error);
  if (error != NULL) {
    g_warning ("PGP decryption failed: '%s'", error->message);
    g_object_unref (outpart);
    encpart = NULL;
    goto out;
  }

  /*
   * Valid doesn't tell us anything useful, but we need to clear it to prevent
   * a memory leak.
   */
  camel_cipher_validity_clear (valid);

out:
  g_clear_object (&session);
  g_clear_object (&ctx);

  return outpart;
}

CamelMimePart *
chatty_pgp_decrypt_stream (const char *data_to_check)
{
  CamelStream *plaintext_stream;
  CamelMimePart *conpart, *outpart;

  if (!data_to_check || !*data_to_check)
    return NULL;

  plaintext_stream = camel_stream_mem_new ();
  conpart = camel_mime_part_new ();
  outpart = camel_mime_part_new ();
  camel_stream_write (plaintext_stream, data_to_check, strlen(data_to_check), NULL, NULL);
  g_seekable_seek (G_SEEKABLE (plaintext_stream), 0, G_SEEK_SET, NULL, NULL);

  camel_data_wrapper_construct_from_stream_sync ((CamelDataWrapper *) conpart, plaintext_stream, NULL, NULL);

  outpart = chatty_pgp_decrypt_mime_part (conpart);

  g_clear_object (&plaintext_stream);
  g_clear_object (&conpart);

  return outpart;
}

CamelCipherValidity *
chatty_pgp_check_sig_mime_part (CamelMimePart *sigpart)
{
  CamelSession *session;
  CamelCipherContext *ctx;
  CamelDataWrapper *signed_dw = NULL;
  CamelCipherValidity *valid = NULL;
  g_autoptr(GError) error = NULL;

  if (!sigpart)
    return NULL;

  /* camel_medium_get_content () is transfer_none */
  signed_dw = camel_medium_get_content (CAMEL_MEDIUM (sigpart));

  if (!signed_dw)
    return NULL;

  if (!CAMEL_IS_MULTIPART_SIGNED (signed_dw))
    return NULL;

  session = chatty_pgp_create_camel_session ();

  ctx = chatty_pgp_create_camel_ctx (session);

  valid = camel_cipher_context_verify_sync (ctx, sigpart, NULL, &error);

  if (error != NULL) {
    g_warning ("PGP signature verification failed: '%s'", error->message);
    goto out;
  }

out:
  g_clear_object (&session);
  g_clear_object (&ctx);

  return valid;
}

CamelCipherValidity *
chatty_pgp_check_sig_stream (const char     *data_to_check,
                             CamelMimePart **output_part)
{
  CamelStream *plaintext_stream;
  CamelCipherValidity *valid = NULL;

  if (!data_to_check || !*data_to_check)
    return NULL;

  plaintext_stream = camel_stream_mem_new ();
  (*output_part) = camel_mime_part_new ();
  camel_stream_write (plaintext_stream, data_to_check, strlen(data_to_check), NULL, NULL);
  g_seekable_seek (G_SEEKABLE (plaintext_stream), 0, G_SEEK_SET, NULL, NULL);

  camel_data_wrapper_construct_from_stream_sync ((CamelDataWrapper *) (*output_part), plaintext_stream, NULL, NULL);

  valid = chatty_pgp_check_sig_mime_part (*output_part);
  g_clear_object (&plaintext_stream);
  return valid;
}

ChattyPgpMessage
chatty_pgp_check_pgp_type (const char *data_to_check)
{
  CamelStream *plaintext_stream;
  CamelMimePart *conpart;
  ChattyPgpMessage msg_type = CHATTY_PGP_UNKNOWN;
  CamelDataWrapper *dw = NULL;
  g_autofree char *data_to_check_truncated = NULL;

  if (!data_to_check || !*data_to_check)
    return msg_type;

  /*
   * Quick check to see if a message is signed or encrypted
   * Since the PGP message is embedded in the MIME, we cannot
   * just check to see if the header is at the beginning.
   *
   * However, "-----BEGIN PGP" will be embedded close to the beginning
   * so we can truncate the data and check.
   */
  data_to_check_truncated = strndup (data_to_check, 10000);
  if (!strstr(data_to_check_truncated, "-----BEGIN PGP"))
    return msg_type;

  plaintext_stream = camel_stream_mem_new ();
  conpart = camel_mime_part_new ();
  camel_stream_write (plaintext_stream, data_to_check, strlen(data_to_check), NULL, NULL);
  g_seekable_seek (G_SEEKABLE (plaintext_stream), 0, G_SEEK_SET, NULL, NULL);

  camel_data_wrapper_construct_from_stream_sync ((CamelDataWrapper *) conpart, plaintext_stream, NULL, NULL);

  /* camel_medium_get_content () is transfer_none */
  dw = camel_medium_get_content (CAMEL_MEDIUM (conpart));

  if (CAMEL_IS_MULTIPART_SIGNED (dw))
    msg_type = CHATTY_PGP_SIGNED;
  else if (CAMEL_IS_MULTIPART_ENCRYPTED (dw))
    msg_type = CHATTY_PGP_ENCRYPTED;

  /* https://gitlab.freedesktop.org/xdg/shared-mime-info/-/blob/master/data/freedesktop.org.xml.in#L280 */
  if (msg_type == CHATTY_PGP_UNKNOWN && g_str_has_prefix(data_to_check_truncated, "-----BEGIN PGP PUBLIC KEY BLOCK-----"))
    msg_type = CHATTY_PGP_PUBLIC_KEY;

  if (msg_type == CHATTY_PGP_UNKNOWN && g_str_has_prefix(data_to_check_truncated, "-----BEGIN PGP PRIVATE KEY BLOCK-----"))
    msg_type = CHATTY_PGP_PRIVATE_KEY;

  g_clear_object (&plaintext_stream);
  g_clear_object (&conpart);

  return msg_type;
}

static GByteArray *
chatty_pgp_get_message (CamelDataWrapper *dw)
{
  GByteArray *buffer, *to_return;
  CamelStream *stream;

  buffer = g_byte_array_new ();
  to_return = g_byte_array_new ();
  stream = camel_stream_mem_new_with_byte_array (buffer);
  camel_data_wrapper_write_to_stream_sync (dw, stream, NULL, NULL);
  /*
   * Then the stream is freed, so is the underlying data, so you have to
   * Manually copy it.
   *
   * There will not be a `\0` in the data wrapper.
   */
  g_byte_array_append (to_return, (unsigned char *) buffer->data, buffer->len);
  g_object_unref (stream);
  return to_return;
}

char *
chatty_pgp_decode_mime_part (CamelMimePart *mime_part)
{
  GByteArray *buffer;
  char *stream_decoded = NULL;

  buffer = chatty_pgp_get_message (CAMEL_DATA_WRAPPER (mime_part));

  stream_decoded = g_strndup ((char *) buffer->data, buffer->len);
  g_byte_array_unref (buffer);

  return stream_decoded;
}

static const char *
chatty_gpg_get_exec (void)
{
  const char *names[] = {
    "gpg2", /* Prefer gpg2, which the seahorse might use too */
    "gpg",
    NULL
  };

  for (guint i = 0; names[i]; i++) {
    g_autofree char *path = NULL;

    path = g_find_program_in_path (names[i]);
    if (path)
      return names[i];
  }

  return NULL;
}

/* This functionality doesn't exist in libcamel, so we have to make it ourselves */
char *
chatty_pgp_get_pub_fingerprint (const char *signing_id)
{
  g_autoptr(GError) error = NULL;
  g_autofree char *directory = NULL;
  g_autofree char *path = NULL;
  g_autoptr(GFile) fingerprint_file = NULL;
  g_autofree char *system_args = NULL;
  g_autofree char *file_contents = NULL;
  gsize content_length = 0;
  const char *gpg_exec = NULL;
  char **fpr_tokens = NULL;
  char *fingerprint = NULL;
  int i = 0;
  gboolean success = FALSE;

  gpg_exec = chatty_gpg_get_exec ();
  if (!gpg_exec || !*gpg_exec)
    return NULL;

  directory = chatty_pgp_get_tmp_dir ();
  path = g_build_filename (directory, "fingerprint.txt", NULL);
  unlink (path);

  /* https://devhints.io/gnupg */
  system_args = g_strdup_printf ("%s --with-colons --list-keys %s > %s", gpg_exec, signing_id, path);
  success = system (system_args);

  if (success != 0) {
    g_warning ("Exporting getting fingerprint error: %d", success);
    return NULL;
  }

  fingerprint_file = g_file_new_for_path (path);
  g_file_load_contents (fingerprint_file, NULL, &file_contents, &content_length, NULL, &error);
  if (error) {
    g_warning ("Getting file contents failed: '%s'", error->message);
    return NULL;
  }

  fpr_tokens = g_strsplit_set (file_contents, ":\r\n", -1);
  /* First look for "pub": Public Key */
  for (i = 0; fpr_tokens[i] != NULL; i++) {
      if (strstr (fpr_tokens[i], "pub"))
        break;
  }

  /* Then look for "fpr": fingerprint */
  for ( ; fpr_tokens[i] != NULL; i++) {
     if (strstr (fpr_tokens[i], "fpr")) {
       i++;
       break;
     }
  }

  /* The next non-empty value is the fingerprint */
  for ( ; fpr_tokens[i] != NULL; i++) {
     if (*fpr_tokens[i]) {
       fingerprint = g_strdup (fpr_tokens[i]);
       break;
     }
  }
  g_strfreev (fpr_tokens);

  g_file_delete (fingerprint_file, NULL, &error);
  if (error)
    g_warning ("Error deleting file: %s", error->message);

  return fingerprint;
}

/* This functionality doesn't exist in libcamel, so we have to make it ourselves */
GFile *
chatty_pgp_get_pub_key (const char *signing_id,
                        const char *save_directory)
{
  g_autofree char *system_args = NULL;
  g_autofree char *path = NULL;
  g_autofree char *directory = NULL;
  const char *gpg_exec = NULL;
  int return_code = 0;

  if (!signing_id || !*signing_id)
    return NULL;

  if (save_directory || *save_directory)
    directory = g_strdup (save_directory);
  else
    directory = chatty_pgp_get_tmp_dir ();

  g_mkdir_with_parents (directory, 0700);

  path = g_build_filename (directory, "pub_key.asc", NULL);

  if (g_file_test (path, G_FILE_TEST_EXISTS)) {
    g_warning ("File already exists at %s", path);
    return NULL;
  }

  gpg_exec = chatty_gpg_get_exec ();
  if (!gpg_exec || !*gpg_exec)
    return NULL;

  system_args = g_strdup_printf ("%s --armor --output %s --export %s", gpg_exec, path, signing_id);
  return_code = system (system_args);

  if (return_code != 0) {
    g_warning ("Exporting Public key error: %d", return_code);
    return NULL;
  }

  return g_file_new_for_path (path);
}

char *
chatty_pgp_get_recipients (CamelMimePart *mime_part)
{
  char *recipients = NULL;
  CamelDataWrapper *dw = NULL;
  CamelMimePart *mime;

  dw = camel_medium_get_content ((CamelMedium *) mime_part);
  if (CAMEL_IS_MULTIPART_SIGNED (dw)) {
    mime = camel_multipart_get_part ((CamelMultipart *) dw, CAMEL_MULTIPART_SIGNED_CONTENT);
  } else
    mime = mime_part;

  recipients = g_strdup (camel_medium_get_header ((CamelMedium *) mime, "Recipients"));

  return recipients;
}

char *
chatty_pgp_get_content (CamelMimePart *mime_part,
                        GList         *files,
                        const char     *directory_to_save_in)
{
  GByteArray *stream_decoded = NULL;
  char *message_to_return = NULL;
  CamelDataWrapper *dw = NULL;
  CamelContentType *content_type;

  dw = camel_medium_get_content ((CamelMedium *) mime_part);

  if (CAMEL_IS_MULTIPART_SIGNED (dw)) {
    mime_part = camel_multipart_get_part ((CamelMultipart *) dw, CAMEL_MULTIPART_SIGNED_CONTENT);
    dw = camel_medium_get_content ((CamelMedium *) mime_part);
  } else if (CAMEL_IS_MULTIPART_ENCRYPTED (dw)) {
    g_warning ("Content is encrypted. Not attempting to decode.");
    return NULL;
  }

  content_type = camel_mime_part_get_content_type (mime_part);

  if (camel_content_type_is (content_type, "text", "*")) {
    stream_decoded = chatty_pgp_get_message (dw);
    message_to_return = g_strndup ((char *) stream_decoded->data, stream_decoded->len);
    g_byte_array_unref (stream_decoded);

  } else if (CAMEL_IS_MULTIPART (dw)) {
    for (int i = 0; camel_multipart_get_part ((CamelMultipart *) dw, i) != NULL; i++) {
      g_autoptr(GError) error = NULL;
      g_autoptr(GFile) file_to_save = NULL;
      g_autoptr(GFileOutputStream) out = NULL;
      g_autofree char *decoded = NULL;
      g_autofree char *file_to_save_location = NULL;
      const char *filename = NULL;
      gsize len, written = 0;
      CamelMimePart *part = camel_multipart_get_part ((CamelMultipart *) dw, i);
      CamelDataWrapper *part_dw = camel_medium_get_content ((CamelMedium *) part);
      /* The message is encoded as the first part */
      content_type = camel_mime_part_get_content_type (part);
      filename = camel_mime_part_get_filename (part);

      if (i == 0 && (camel_content_type_is (content_type,"text","plain"))) {
        stream_decoded = chatty_pgp_get_message (part_dw);
        message_to_return = g_strndup ((char *) stream_decoded->data, stream_decoded->len);
        g_byte_array_unref (stream_decoded);
        continue;
      }

      if (camel_mime_part_get_encoding ((CamelMimePart *) part) == CAMEL_TRANSFER_ENCODING_BASE64) {
        g_autofree char *temp = NULL;
        stream_decoded = chatty_pgp_get_message (part_dw);
        /*
         * stream_decoded may have extra data at the end.
         * Make a temporary buffer and decode that.
         */
        temp = g_strndup ((char *) stream_decoded->data, stream_decoded->len);
        decoded = (char *) g_base64_decode (temp, &len);
        g_byte_array_unref (stream_decoded);
      } else if (camel_mime_part_get_encoding ((CamelMimePart *) part) == CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE ||
                 camel_mime_part_get_encoding ((CamelMimePart *) part) == CAMEL_TRANSFER_ENCODING_DEFAULT) {
        stream_decoded = chatty_pgp_get_message (part_dw);
        decoded = g_strndup ((char *) stream_decoded->data, stream_decoded->len);
        g_byte_array_unref (stream_decoded);
      } else {
        g_warning ("I don't know how to decode this!");
        continue;
      }

      g_mkdir_with_parents (directory_to_save_in, 0700);
      if (filename)
        file_to_save_location = g_build_filename (directory_to_save_in, filename, NULL);
      else
        file_to_save_location = g_build_filename (directory_to_save_in, i, NULL);

      file_to_save = g_file_new_for_path (file_to_save_location);
      out = g_file_create (file_to_save, G_FILE_CREATE_PRIVATE, NULL, &error);
      if (error) {
        g_warning ("Error getting file info: %s", error->message);
        continue;
      }
      if (!g_output_stream_write_all ((GOutputStream *) out,
                                      decoded,
                                      len,
                                      &written,
                                      NULL,
                                      &error)) {
        g_warning ("Failed to write to file %s: %s",
                   g_file_peek_path (file_to_save), error->message);
        g_clear_error (&error);
      } else {
        files = g_list_append (files, chatty_file_new_for_path(file_to_save_location));
      }
    }
    return message_to_return;
  } else {
    g_warning ("I don't know how to handle this!");
    return NULL;
  }
  return message_to_return;
}
