/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* message-text-item.c
 *
 * Copyright 2021 Purism SPC
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef NDEBUG
#undef G_DISABLE_ASSERT
#undef G_DISABLE_CHECKS
#undef G_DISABLE_CAST_CHECKS
#undef G_LOG_DOMAIN

#include "chatty-message-row.c"

static void
test_message_text_markup (void)
{
  typedef struct data {
    char *text;
    char *markup;
  } data;
  data array[] = {
    {"",""},
    {"abc","abc"},
    {".abc",".abc"},
    {"www.","www."},
    {"www. ","www. "},
    {"www.a ","<a href=\"www.a\">www.a</a> "},
    {"http:// ","http:// "},
    {"http://w ","<a href=\"http://w\">http://w</a> "},
    {"::: ","::: "},
    {
     "അത് http://www.example.com/മലയാളം ആണ്",
     "അത് <a href=\"http://www.example.com/മലയാളം\">http://www.example.com/മലയാളം</a> ആണ്"},
    {
     "http://www.example.com/user's-image.png",
     "<a href=\"http://www.example.com/user&apos;s-image.png\">http://www.example.com/user&apos;s-image.png</a>"},
    {
     "www.puri.sm www.gnu.org www.fsf.org ",
     "<a href=\"www.puri.sm\">www.puri.sm</a> "
     "<a href=\"www.gnu.org\">www.gnu.org</a> "
     "<a href=\"www.fsf.org\">www.fsf.org</a> "
    },
    {
     "www.puri.sm\n",
     "<a href=\"www.puri.sm\">www.puri.sm</a>\n"
    },
    {
     "www.puri.sm\nwww.gnu.org",
     "<a href=\"www.puri.sm\">www.puri.sm</a>\n"
     "<a href=\"www.gnu.org\">www.gnu.org</a>"
    },
    {
     "www.puri.sm, ",
     "<a href=\"www.puri.sm\">www.puri.sm</a>, "
    },
    {
     "file:///home/user/good&bad-file.png ",
     "<a href=\"file:///home/user/good&amp;bad-file.png\">"
     "file:///home/user/good&amp;bad-file.png</a> "
    },
  };

  for (guint i = 0; i < G_N_ELEMENTS (array); i++) {
    g_autofree char *content = NULL;
    GtkLabel *label;
    const char *str;

    label = GTK_LABEL (gtk_label_new (""));
    content = text_item_linkify (array[i].text);

    if (content && *content)
      gtk_label_set_markup (label, content);
    else
      gtk_label_set_text (label, array[i].text);

    str = gtk_label_get_text (label);
    g_assert_cmpstr (str, ==, array[i].text);

    str = gtk_label_get_label (label);
    g_assert_cmpstr (str, ==, array[i].markup);
  }
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/message-text/markup", test_message_text_markup);

  return g_test_run ();
}
