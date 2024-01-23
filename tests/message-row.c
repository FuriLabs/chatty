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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wbidi-chars"
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
    {
     "http://www.example.com/user's-image.png?blah",
     "<a href=\"http://www.example.com/user&apos;s-image.png?blah\">http://www.example.com/user&apos;s-image.png?blah</a>"},
    {
     "https://en.wikipedia.org/wiki/Sphere_(venue)",
     "<a href=\"https://en.wikipedia.org/wiki/Sphere_(venue)\">https://en.wikipedia.org/wiki/Sphere_(venue)</a>"
    },
    {
     "https://example.com(venue hi hi hi)",
     "<a href=\"https://example.com\">https://example.com</a>(venue hi hi hi)"
    },
    {
     "Open some example website (eg: https://www.example.com)",
     "Open some example website (eg: <a href=\"https://www.example.com\">https://www.example.com</a>)"
    },
    {
     "https://en.wikipedia.org/wiki/Sphere_(venue) You should check this out",
     "<a href=\"https://en.wikipedia.org/wiki/Sphere_(venue)\">https://en.wikipedia.org/wiki/Sphere_(venue)</a> You should check this out"
    },
    {
     "https://www.google.com/maps/place/Carnegie+Mellon+University/@40.4432027,-79.9454248,17z/data=!3m1!4b1!4m6!3m5!1s0x8834f21f58679a9f:0x88716b461fc4daf4!8m2!3d40.4432027!4d-79.9428499!16zL20vMGN3eF8?entry=ttu",
     "<a href=\"https://www.google.com/maps/place/Carnegie+Mellon+University/@40.4432027,-79.9454248,17z/data=!3m1!4b1!4m6!3m5!1s0x8834f21f58679a9f:0x88716b461fc4daf4!8m2!3d40.4432027!4d-79.9428499!16zL20vMGN3eF8?entry=ttu\">https://www.google.com/maps/place/Carnegie+Mellon+University/@40.4432027,-79.9454248,17z/data=!3m1!4b1!4m6!3m5!1s0x8834f21f58679a9f:0x88716b461fc4daf4!8m2!3d40.4432027!4d-79.9428499!16zL20vMGN3eF8?entry=ttu</a>"
    },
    {
     "https://www.google.com/maps/place/Carnegie+Mellon+University/@40.4432027,-79.9454248,17z/data=!3m1!4b1!4m6!3m5!1s0x8834f21f58679a9f:0x88716b461fc4daf4!8m2!3d40.4432027!4d-79.9428499!16zL20vMGN3eF8?entry=ttu This is the spot",
     "<a href=\"https://www.google.com/maps/place/Carnegie+Mellon+University/@40.4432027,-79.9454248,17z/data=!3m1!4b1!4m6!3m5!1s0x8834f21f58679a9f:0x88716b461fc4daf4!8m2!3d40.4432027!4d-79.9428499!16zL20vMGN3eF8?entry=ttu\">https://www.google.com/maps/place/Carnegie+Mellon+University/@40.4432027,-79.9454248,17z/data=!3m1!4b1!4m6!3m5!1s0x8834f21f58679a9f:0x88716b461fc4daf4!8m2!3d40.4432027!4d-79.9428499!16zL20vMGN3eF8?entry=ttu</a> This is the spot"
    },
    {//https://00xbyte.github.io/posts/Don%27t-Believe-Your-Eyes-A-WhatsApp-Clickjacking-Vulnerability/ Make sure that Chatty isn't vulnerable to this
     "‮https://moc.margatsni.nl//:sptth",
     "‮https://moc.margatsni.nl//:sptth"},
  };
#pragma GCC diagnostic pop
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

static void
test_message_strip_utm (void)
{
  typedef struct data {
    char *text;
    char *check;
  } data;
  data array[] = {
    {
     "http://www.example.com/user's-image.png?blah=1234",
     "http://www.example.com/user's-image.png?blah=1234"},
    {
     "http://www.example.com/user's-image.png?blah",
     "http://www.example.com/user's-image.png?blah"},
    {
     "http://www.example.com/user's-image.png?utm_source=1234qwer&fbclid=1234564&",
     "http://www.example.com/user's-image.png?"},
    {
     "https://www.example.com/?t=ftsa&q=hello&ia=definition",
     "https://www.example.com/?t=ftsa&q=hello&ia=definition"},
    {
     "http://example.com/utm_source/something?v=_utm_source",
     "http://example.com/utm_source/something?v=_utm_source"},
    {
     "http://www.example.com/user's-image.png?_utm_sourcecode=1234qwer&fbclid=1234564&",
     "http://www.example.com/user's-image.png?_utm_sourcecode=1234qwer"},
    {
     "http://utm_source.example.com/something?wowbraid=123",
     "http://utm_source.example.com/something?wowbraid=123"},
  };

  for (guint i = 0; i < G_N_ELEMENTS (array); i++) {
    g_autofree char *content = NULL;

    content = strip_utm (array[i].text);
    g_assert_cmpstr (content, ==, array[i].check);
  }
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/message-text/markup", test_message_text_markup);
  g_test_add_func ("/message-text/strip_utm", test_message_strip_utm);

  return g_test_run ();
}
