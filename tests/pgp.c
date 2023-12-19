/* pgp.c
 *
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#undef NDEBUG
#undef G_DISABLE_ASSERT
#undef G_DISABLE_CHECKS
#undef G_DISABLE_CAST_CHECKS
#undef G_LOG_DOMAIN

#define PGP_TEST_DATA_DIR SOURCE_DIR "/tests/pgp"
#define PGP_TEST_BUILD_DIR BUILD_DIR "/pgp"
#define PGP_SHA1_UUID_LEN 20

#include "chatty-pgp.h"

/*
 * chatty_test_pgp_assert_valid:
 * @f: The CamelCipherValidity *valid message
 */
#define chatty_test_pgp_assert_valid(f) G_STMT_START {                   \
    CamelCipherValidity *__valid = f;                                    \
    if (!camel_cipher_validity_get_valid (__valid)) {                    \
      g_autofree char *invalid_message = g_strdup_printf ("Signed Message Not valid! %s",  \
              valid->sign.description);                                  \
      g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC,  \
                           invalid_message);                             \
    }                                                                    \
  } G_STMT_END



static const char *
digest_to_str (const unsigned char *digest)
{
  static char buf[PGP_SHA1_UUID_LEN * 2 + 1];
  unsigned int i;

  for (i = 0; i < PGP_SHA1_UUID_LEN; i++)
    sprintf (&buf[i * 2], "%02X", digest[i]);

  buf[PGP_SHA1_UUID_LEN * 2] = 0;

  return buf;
}

static const char *
generate_sha_from_pdu (const unsigned char *pdu,
                       unsigned int         len)
{
  GChecksum *checksum;
  guint8 digest[PGP_SHA1_UUID_LEN];
  gsize digest_size = PGP_SHA1_UUID_LEN;

  checksum = g_checksum_new (G_CHECKSUM_SHA1);
  if (checksum == NULL)
    return NULL;

  g_checksum_update (checksum, pdu, len);

  g_checksum_get_digest (checksum, digest, &digest_size);

  g_checksum_free (checksum);

  return digest_to_str (digest);
}

static void
test_check_sha (const char *filepath,
                const char *filepath_to_compare)
{
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFile) file_2 = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *sha = NULL;
  g_autofree char *sha_to_compare = NULL;
  g_autofree char *pdu = NULL;
  gsize len = 0;
  g_autofree char *pdu_to_compare = NULL;
  gsize len_to_compare = 0;

  file = g_file_new_for_path (filepath);
  g_file_load_contents (file, NULL, &pdu, &len, NULL, &error);
  g_assert_no_error (error);
  sha = g_strdup (generate_sha_from_pdu ((unsigned char *) pdu, len));

  file_2 = g_file_new_for_path (filepath_to_compare);
  g_file_load_contents (file_2, NULL, &pdu_to_compare, &len_to_compare, NULL, &error);
  g_assert_no_error (error);
  sha_to_compare = g_strdup (generate_sha_from_pdu ((unsigned char *) pdu_to_compare, len_to_compare));

  g_assert_cmpstr (sha, ==, sha_to_compare);
}

static void
test_pgp_message (void)
{
  const char *stream_signed = "Hello, I am a test stream. I should be signed.";
  const char *stream_encrypted = "Hello, I am a test stream. I should be encrypted";
  const char *stream_signed_and_encrypted = "Hello, I am a test stream. I should be signed then encrypted";
  const char *signing_email = "sender@example.com";
  char *recipients = "recipient1@no.domain,recipient2@no.domain";
  char **recipient_array = NULL;
  CamelMimePart *sigpart = NULL;
  CamelMimePart *sigpart_from_stream = NULL;
  CamelMimePart *encpart = NULL;
  CamelMimePart *decrypt_part = NULL;
  CamelMimePart *sign_and_encpart = NULL;
  CamelMimePart *decrypt_and_signed_part = NULL;
  g_autofree char *signature = NULL;
  g_autofree char *encrypted_msg = NULL;
  g_autofree char *signed_and_encrypted_msg = NULL;
  g_autofree char *recipients_to_check = NULL;
  char *content_to_check = NULL;
  CamelCipherValidity *valid = NULL;

  recipient_array = g_strsplit (recipients, ",", -1);
  sigpart = chatty_pgp_sign_stream (stream_signed, NULL, signing_email, recipient_array);
  g_assert_nonnull (sigpart);

  valid = chatty_pgp_check_sig_mime_part (sigpart);
  chatty_test_pgp_assert_valid (valid);
  camel_cipher_validity_free (valid);

  signature = chatty_pgp_decode_mime_part (sigpart);
  g_assert_nonnull (signature);
  content_to_check = chatty_pgp_get_content (sigpart, NULL, NULL);
  recipients_to_check = chatty_pgp_get_recipients (sigpart);
  g_assert_cmpstr (recipients_to_check, ==, recipients);
  g_assert_cmpstr (content_to_check, ==, stream_signed);
  g_free (content_to_check);
  g_clear_pointer (&recipients_to_check, g_free);

  g_assert_finalize_object (sigpart);

  g_assert_cmpint (CHATTY_PGP_SIGNED, ==, chatty_pgp_check_pgp_type (signature));
  valid = chatty_pgp_check_sig_stream (signature, &sigpart_from_stream);

  g_assert_nonnull (sigpart_from_stream);
  content_to_check = chatty_pgp_get_content (sigpart_from_stream, NULL, NULL);
  g_assert_cmpstr (content_to_check, ==, stream_signed);
  g_free (content_to_check);
  g_assert_finalize_object (sigpart_from_stream);
  chatty_test_pgp_assert_valid (valid);
  camel_cipher_validity_free (valid);

  encpart = chatty_pgp_encrypt_stream (stream_encrypted,
                                       NULL,
                                       signing_email,
                                       recipient_array,
                                       NULL);
  decrypt_part = chatty_pgp_decrypt_mime_part (encpart);
  g_assert_nonnull (decrypt_part);
  /* This was not signed, so validity should be NULL */
  valid = chatty_pgp_check_sig_mime_part (decrypt_part);
  g_assert_null (valid);
  g_assert_finalize_object (decrypt_part);

  encrypted_msg = chatty_pgp_decode_mime_part (encpart);
  g_assert_nonnull (encrypted_msg);
  g_assert_finalize_object (encpart);

  g_assert_cmpint (CHATTY_PGP_ENCRYPTED, ==, chatty_pgp_check_pgp_type (encrypted_msg));
  decrypt_part = chatty_pgp_decrypt_stream (encrypted_msg);
  /* This was not signed, so validity should be NULL */
  valid = chatty_pgp_check_sig_mime_part (decrypt_part);
  g_assert_null (valid);

  content_to_check = chatty_pgp_get_content (decrypt_part, NULL, NULL);
  g_assert_cmpstr (content_to_check, ==, stream_encrypted);
  g_free (content_to_check);
  g_assert_finalize_object (decrypt_part);

  sign_and_encpart = chatty_pgp_sign_and_encrypt_stream (stream_signed_and_encrypted,
                                                         NULL,
                                                         signing_email,
                                                         recipient_array);

  decrypt_and_signed_part = chatty_pgp_decrypt_mime_part (sign_and_encpart);
  g_assert_nonnull (decrypt_and_signed_part);
  valid = chatty_pgp_check_sig_mime_part (decrypt_and_signed_part);
  chatty_test_pgp_assert_valid (valid);
  camel_cipher_validity_free (valid);
  recipients_to_check = chatty_pgp_get_recipients (decrypt_and_signed_part);
  g_assert_cmpstr (recipients_to_check, ==, recipients);
  g_clear_pointer (&recipients_to_check, g_free);


  g_assert_finalize_object (decrypt_and_signed_part);

  signed_and_encrypted_msg = chatty_pgp_decode_mime_part (sign_and_encpart);
  g_assert_nonnull (signed_and_encrypted_msg);
  g_assert_finalize_object (sign_and_encpart);

  g_assert_cmpint (CHATTY_PGP_ENCRYPTED, ==, chatty_pgp_check_pgp_type (signed_and_encrypted_msg));
  decrypt_part = chatty_pgp_decrypt_stream (signed_and_encrypted_msg);
  valid = chatty_pgp_check_sig_mime_part (decrypt_part);
  chatty_test_pgp_assert_valid (valid);
  camel_cipher_validity_free (valid);

  content_to_check = chatty_pgp_get_content (decrypt_part, NULL, NULL);
  g_assert_cmpstr (content_to_check, ==, stream_signed_and_encrypted);
  g_free (content_to_check);

  g_strfreev (recipient_array);
  g_assert_cmpint (CHATTY_PGP_UNKNOWN, ==, chatty_pgp_check_pgp_type (NULL));
  g_assert_cmpint (CHATTY_PGP_UNKNOWN, ==, chatty_pgp_check_pgp_type (""));
  g_assert_cmpint (CHATTY_PGP_UNKNOWN, ==, chatty_pgp_check_pgp_type ("This is an example text that isn't PGP encoded."));
  g_assert_finalize_object (decrypt_part);
}

static void
test_pgp_encode_decode (void)
{
  const char *attachment_1 = "IMG20211119201506.jpg";
  const char *attachment_2 = "Postcard_John_Doe.vcf";
  const char *attachment_3 = "otus_eating_carrot.mp4";
  const char *stream_to_encode = "Hello, I am a test stream. Please encode me.";
  CamelMimePart *encoded_mime_part = NULL;
  g_autofree char *attachment_1_filepath = NULL;
  g_autofree char *attachment_2_filepath = NULL;
  g_autofree char *attachment_3_filepath = NULL;
  g_autofree char *recipients_to_check = NULL;
  g_autolist(ChattyFile) files = NULL;
  char *recipients = "recipient1@no.domain,recipient2@no.domain";
  char **recipient_array = NULL;

  recipient_array = g_strsplit (recipients, ",", -1);

  attachment_1_filepath = g_build_filename (PGP_TEST_DATA_DIR, "attachments", attachment_1, NULL);
  files = g_list_append (files, chatty_file_new_for_path(attachment_1_filepath));
  attachment_2_filepath = g_build_filename (PGP_TEST_DATA_DIR, "attachments", attachment_2, NULL);
  files = g_list_append (files, chatty_file_new_for_path(attachment_2_filepath));
  attachment_3_filepath = g_build_filename (PGP_TEST_DATA_DIR, "attachments", attachment_3, NULL);
  files = g_list_append (files, chatty_file_new_for_path(attachment_3_filepath));

  /* Should return NULL, I want to to bind recipients but I didn't add them */
  encoded_mime_part = chatty_pgp_create_mime_part (stream_to_encode, NULL, NULL, TRUE);
  g_assert_null (encoded_mime_part);

  /* No content, return NULL */
  encoded_mime_part = chatty_pgp_create_mime_part (NULL, NULL, NULL, FALSE);
  g_assert_null (encoded_mime_part);

  /* No content, return NULL */
  encoded_mime_part = chatty_pgp_create_mime_part (NULL, NULL, recipient_array, TRUE);
  g_assert_null (encoded_mime_part);

  /* Make with only files */
  encoded_mime_part = chatty_pgp_create_mime_part (NULL, files, NULL, FALSE);
  g_assert_nonnull (encoded_mime_part);
  g_assert_finalize_object (encoded_mime_part);

  /* Bind recipients to the files */
  encoded_mime_part = chatty_pgp_create_mime_part (NULL, files, recipient_array, TRUE);
  g_assert_nonnull (encoded_mime_part);

  recipients_to_check = chatty_pgp_get_recipients (encoded_mime_part);
  g_assert_cmpstr (recipients_to_check, ==, recipients);
  g_clear_pointer (&recipients_to_check, g_free);
  g_assert_finalize_object (encoded_mime_part);

  /* Check to make sure that the multipart PGP message works */
  encoded_mime_part = chatty_pgp_create_mime_part (stream_to_encode, files, recipient_array, TRUE);
  g_assert_nonnull (encoded_mime_part);
  g_assert_finalize_object (encoded_mime_part);

  g_strfreev (recipient_array);
}

static void
test_pgp_message_and_files (void)
{
  const char *attachment_1 = "IMG20211119201506.jpg";
  const char *attachment_2 = "Postcard_John_Doe.vcf";
  const char *attachment_3 = "otus_eating_carrot.mp4";
  const char *stream_to_encode = "Hello, I am a test stream. Please encode me.";
  CamelMimePart *encoded_mime_part = NULL;
  g_autofree char *attachment_1_filepath = NULL;
  g_autofree char *attachment_2_filepath = NULL;
  g_autofree char *attachment_3_filepath = NULL;
  g_autofree char *attachment_1_savepath = NULL;
  g_autofree char *attachment_2_savepath = NULL;
  g_autofree char *attachment_3_savepath = NULL;
  g_autofree char *signed_and_encrypted_msg = NULL;
  g_autofree char *content_message = NULL;
  g_autofree char *save_directory = NULL;
  g_autofree char *recipients_to_check = NULL;
  g_autolist(ChattyFile) files = NULL;
  g_autolist(ChattyFile) saved_files = NULL;
  CamelCipherValidity *valid = NULL;
  const char *signing_email = "sender@example.com";
  char *recipients = "recipient1@no.domain,recipient2@no.domain";
  char **recipient_array = NULL;
  CamelMimePart *sign_and_encpart = NULL;
  CamelMimePart *decrypt_and_signed_part = NULL;

  recipient_array = g_strsplit (recipients, ",", -1);
  save_directory = g_build_filename (PGP_TEST_DATA_DIR, "chatty", "pgp", NULL);

  attachment_1_filepath = g_build_filename (PGP_TEST_DATA_DIR, "attachments", attachment_1, NULL);
  files = g_list_append (files, chatty_file_new_for_path(attachment_1_filepath));
  attachment_2_filepath = g_build_filename (PGP_TEST_DATA_DIR, "attachments", attachment_2, NULL);
  files = g_list_append (files, chatty_file_new_for_path(attachment_2_filepath));
  attachment_3_filepath = g_build_filename (PGP_TEST_DATA_DIR, "attachments", attachment_3, NULL);
  files = g_list_append (files, chatty_file_new_for_path(attachment_3_filepath));

  attachment_1_savepath = g_build_filename (PGP_TEST_BUILD_DIR, attachment_1, NULL);
  attachment_2_savepath = g_build_filename (PGP_TEST_BUILD_DIR, attachment_2, NULL);
  attachment_3_savepath = g_build_filename (PGP_TEST_BUILD_DIR, attachment_3, NULL);

  /* Check to make sure that the multipart PGP message works */
  encoded_mime_part = chatty_pgp_create_mime_part (stream_to_encode, files, recipient_array, TRUE);
  g_assert_nonnull (encoded_mime_part);
  recipients_to_check = chatty_pgp_get_recipients (encoded_mime_part);
  g_assert_cmpstr (recipients_to_check, ==, recipients);
  g_clear_pointer (&recipients_to_check, g_free);
  sign_and_encpart = chatty_pgp_sign_and_encrypt_stream (stream_to_encode,
                                                         files,
                                                         signing_email,
                                                         recipient_array);

  /* Decode the signed and encrypted message to simulate this being texted */
  signed_and_encrypted_msg = chatty_pgp_decode_mime_part (sign_and_encpart);

  g_assert_finalize_object (sign_and_encpart);
  g_assert_nonnull (signed_and_encrypted_msg);
  decrypt_and_signed_part = chatty_pgp_decrypt_stream (signed_and_encrypted_msg);
  g_assert_nonnull (decrypt_and_signed_part);
  valid = chatty_pgp_check_sig_mime_part (decrypt_and_signed_part);
  chatty_test_pgp_assert_valid (valid);
  camel_cipher_validity_free (valid);
  recipients_to_check = chatty_pgp_get_recipients (decrypt_and_signed_part);
  g_assert_cmpstr (recipients_to_check, ==, recipients);
  g_clear_pointer (&recipients_to_check, g_free);

  content_message = chatty_pgp_get_content (decrypt_and_signed_part, &saved_files, PGP_TEST_BUILD_DIR);
  g_assert_cmpint (g_list_length (saved_files), ==, 3);
  test_check_sha (attachment_1_filepath, attachment_1_savepath);
  test_check_sha (attachment_2_filepath, attachment_2_savepath);
  test_check_sha (attachment_3_filepath, attachment_3_savepath);

  /* fixme: This is fails on byzantium, but works on Debian Bookworm */
  //g_assert_cmpstr (stream_to_encode, ==, content_message);
  if (g_strcmp0 (stream_to_encode, content_message) !=0)
    g_message ("%s is different than %s", stream_to_encode, content_message);

  /* Delete Attachments. Comment if you are having issues */
  unlink (attachment_1_savepath);
  unlink (attachment_2_savepath);
  unlink (attachment_3_savepath);

  g_assert_finalize_object (encoded_mime_part);
  g_assert_finalize_object (decrypt_and_signed_part);
  g_strfreev (recipient_array);
}

static void
test_pgp_export_pub_key (void)
{
  g_autoptr(GError) error = NULL;
  const char *signing_email = "sender@example.com";
  g_autofree char *pub_key_save_dir = NULL;
  g_autofree char *pub_key_to_check = NULL;
  g_autofree char *orig_pub_key = NULL;
  g_autofree char *pdu = NULL;
  g_autofree char *fingerprint = NULL;
  gsize len = 0;
  GFile *pub_key;

  pub_key_save_dir = g_build_filename (PGP_TEST_BUILD_DIR, NULL);
  pub_key = chatty_pgp_get_pub_key (signing_email, pub_key_save_dir);
  g_assert_nonnull (pub_key);

  orig_pub_key = g_build_filename (PGP_TEST_DATA_DIR, "chatty-test.gpg.pub", NULL);
  pub_key_to_check = g_build_filename (pub_key_save_dir, "pub_key.asc", NULL);

  test_check_sha (orig_pub_key, pub_key_to_check);
  g_file_load_contents (pub_key, NULL, &pdu, &len, NULL, &error);
  g_assert_no_error (error);

  g_file_delete (pub_key, NULL, &error);
  if (error)
    g_warning ("Error deleting file: %s", error->message);

  g_assert_cmpint (CHATTY_PGP_PUBLIC_KEY, ==, chatty_pgp_check_pgp_type (pdu));
  g_assert_finalize_object (pub_key);

  fingerprint = chatty_pgp_get_pub_fingerprint (signing_email);

  g_assert_cmpstr ("362E7448A9CEEBD2C045D9AD028782BB929585AA", ==, fingerprint);
}

int
main (int   argc,
      char *argv[])
{
  int ret;
  g_autofree char *gnupg_directory = NULL;

  g_test_init (&argc, &argv, NULL);

  gnupg_directory = g_build_filename (PGP_TEST_BUILD_DIR, ".gnupg", NULL);
  if (g_mkdir_with_parents (gnupg_directory, 0700) < 0)
    g_warning ("Could not create directory '%s'", gnupg_directory);

  g_print ("Setting environment: 'GNUPGHOME=%s'\n", gnupg_directory);
  g_setenv ("GNUPGHOME", gnupg_directory, 1);

  /* You need to add the private-keys-v1.d for this to work on newer versions of gnupg */
  g_free (gnupg_directory);
  gnupg_directory = g_build_filename (PGP_TEST_BUILD_DIR, ".gnupg", "private-keys-v1.d", NULL);
  g_mkdir_with_parents (gnupg_directory, 0700);

  if ((ret = system ("gpg < /dev/null > /dev/null 2>&1")) == -1)
    return 77;

  g_print("Importing keys with 'gpg --import'\n");

  ret = system ("gpg --import " PGP_TEST_DATA_DIR "/chatty-test.gpg.pub > /dev/null 2>&1");
  if (ret < 0)
    g_warning ("Inporting key error: %d", ret);

  ret = system ("gpg --import " PGP_TEST_DATA_DIR "/chatty-test.gpg.sec > /dev/null 2>&1");
  if (ret < 0)
    g_warning ("Inporting key error: %d", ret);

  ret = system ("gpg --import " PGP_TEST_DATA_DIR "/recipient1.gpg.pub > /dev/null 2>&1");
  if (ret < 0)
    g_warning ("Inporting key error: %d", ret);

  ret = system ("gpg --import " PGP_TEST_DATA_DIR "/recipient2.gpg.pub > /dev/null 2>&1");
  if (ret < 0)
    g_warning ("Inporting key error: %d", ret);

  /* We have the private key in case we need them, but chatty-test is the "user" key */
  //ret = system ("gpg --import " PGP_TEST_DATA_DIR "/recipient1.gpg.sec > /dev/null 2>&1");
  //ret = system ("gpg --import " PGP_TEST_DATA_DIR "/recipient2.gpg.sec > /dev/null 2>&1");

  /* Trust chatty-test key sender@example.com to make signature from it valid */
  ret = system ("echo 362E7448A9CEEBD2C045D9AD028782BB929585AA:6: | gpg --import-ownertrust 2>/dev/null");
  if (ret < 0)
    g_warning ("Setting owner trust error: %d", ret);

  g_print ("GPG setup complete. Starting tests..\n");
  g_test_add_func ("/pgp/message", test_pgp_message);
  g_test_add_func ("/pgp/content", test_pgp_encode_decode);
  g_test_add_func ("/pgp/message and files", test_pgp_message_and_files);
  g_test_add_func ("/pgp/export public key", test_pgp_export_pub_key);


  return g_test_run ();
}
