#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib-unix.h>
#include "libglnx.h"
#include "rpmostree-json-parsing.h"
#include "rpmostree-util.h"

static const gchar *test_data =
"{ \"text\" : \"hello, world!\", \"foo\" : null, \"blah\" : 47, \"double\" : 42.47 }";

static JsonObject *
get_test_data (void)
{
  GError *error = NULL;
  glnx_unref_object JsonParser *parser = json_parser_new ();
  (void)json_parser_load_from_data (parser, test_data, -1, &error);
  g_assert_no_error (error);
  return json_object_ref (json_node_get_object (json_parser_get_root (parser)));
}

static void
test_get_optional_string_member (void)
{
  GError *error = NULL;
  JsonObject *obj = get_test_data ();
  const char *str;

  (void) _rpmostree_jsonutil_object_get_optional_string_member (obj, "nomember", &str, &error);
  g_assert_no_error (error);
  g_assert (str == NULL);

  (void) _rpmostree_jsonutil_object_get_optional_string_member (obj, "text", &str, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (str, ==, "hello, world!");

  str = _rpmostree_jsonutil_object_require_string_member (obj, "nomember", &error);
  g_assert (error != NULL);
  g_clear_error (&error);
  g_assert (str == NULL);

  str = _rpmostree_jsonutil_object_require_string_member (obj, "text", &error);
  g_assert_no_error (error);
  g_assert_cmpstr (str, ==, "hello, world!");

  json_object_unref (obj);
}

static void
test_auto_version (void)
{
  char *version = NULL;
  char *final_version = NULL;
  GTimeVal current_datetime;
  GDate current_date;
  char date_buffer[16] = {'\0'};
  g_get_current_time (&current_datetime);
  g_date_set_time_val (&current_date, &current_datetime);

#define _VER_TST(x, y, z)                              \
  version = _rpmostree_util_next_version ((x), (y));   \
  g_assert_cmpstr (version, ==, z);                    \
  g_free (version);                                    \
  version = NULL

  _VER_TST("10",  "",      "10");
  _VER_TST("10",  "xyz",   "10");
  _VER_TST("10",  "9",     "10");
  _VER_TST("10", "11",     "10");

  _VER_TST("10", "10",     "10.1");
  _VER_TST("10.1", "10.1",     "10.1.1");

  _VER_TST("10", "10.0",   "10.1");
  _VER_TST("10", "10.1",   "10.2");
  _VER_TST("10", "10.2",   "10.3");
  _VER_TST("10", "10.3",   "10.4");
  _VER_TST("10", "10.1.5", "10.2");
  _VER_TST("10.1", "10.1.5",   "10.1.6");
  _VER_TST("10.1", "10.1.1.5", "10.1.2");

  _VER_TST("10", "10001",  "10");
  _VER_TST("10", "101.1",  "10");
  _VER_TST("10", "10x.1",  "10");
  _VER_TST("10.1", "10",    "10.1");
  _VER_TST("10.1", "10.",   "10.1");
  _VER_TST("10.1", "10.0",  "10.1");
  _VER_TST("10.1", "10.2",  "10.1");
  _VER_TST("10.1", "10.12", "10.1");
  _VER_TST("10.1", "10.1x", "10.1");
  _VER_TST("10.1", "10.1.x", "10.1.1");
  _VER_TST("10.1", "10.1.2x", "10.1.3");

  _VER_TST("10.<increment: XXX>", "", "10.001");
  _VER_TST("10.<increment: XXX>", "10", "10.001");
  _VER_TST("10.<increment: XXX>", "10.001", "10.002");

#define _VER_CMPLX_TST(pre, last, finalfmt, datefmt)        \
  g_date_strftime (date_buffer, sizeof(date_buffer),        \
                   datefmt, &current_date);                 \
  final_version = g_strdup_printf(finalfmt, date_buffer);   \
  version = _rpmostree_util_next_version ((pre), (last));   \
  g_assert_cmpstr (version, ==, final_version);             \
  g_free(final_version);                                    \
  g_free(version);                                          \
  version = NULL;                                           \
  final_version = NULL;

  /* Test tends towards the given format. */
  _VER_CMPLX_TST("10.<date: %Y%m%d>.<increment: XXX>", "", "10.%s.001", "%Y%m%d");
  _VER_CMPLX_TST("10.<date: %Y%m%d>.<increment: XXX>", "10", "10.%s.001", "%Y%m%d");
  _VER_CMPLX_TST("10.<date: %Y%m%d>.<increment: XXX>", "10.1", "10.%s.001", "%Y%m%d");
  _VER_CMPLX_TST("10.<date: %Y%m%d>.<increment: XXX>", "10.1.1", "10.%s.001", "%Y%m%d");
  _VER_CMPLX_TST("10.<date: %Y%m%d>.<increment: XXX>", "10.1.1.1", "10.%s.001", "%Y%m%d");
  _VER_CMPLX_TST("10.<date: %Y%m%d>.<increment: XXX>", "10abcd", "10.%s.001", "%Y%m%d");
  _VER_CMPLX_TST("10.<date: %Y%m%d>.<increment: XXX>", "abcd", "10.%s.001", "%Y%m%d");
  _VER_CMPLX_TST("10.<increment: XXX>.<date: %Y%m%d>", "10.20001010.001", "10.001.%s", "%Y%m%d");

  /* Test tends towards the given format with no punctuation. */
  _VER_CMPLX_TST("10<date: %Y%m%d><increment: XXX>", "", "10%s001", "%Y%m%d");
  _VER_CMPLX_TST("10<date: %Y%m%d><increment: XXX>", "10abcd", "10%s001", "%Y%m%d");
  _VER_CMPLX_TST("10<date: %Y%m%d><increment: XXX>", "10200010109", "10%s001", "%Y%m%d");

  /* Test increment. */
  const char *prev_version = g_strdup_printf("10.001.%s", date_buffer);
  _VER_CMPLX_TST("10.<increment: XXX>.<date: %Y%m%d>", "10.001.20001010", "10.001.%s", "%Y%m%d");
  _VER_CMPLX_TST("10.<increment: XXX>.<date: %Y%m%d>", prev_version, "10.002.%s", "%Y%m%d");

}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/jsonparsing/get-optional-member", test_get_optional_string_member);
  g_test_add_func ("/versioning/automatic", test_auto_version);

  return g_test_run ();
}
