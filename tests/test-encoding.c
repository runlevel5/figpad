/*
 *  Figpad - GTK+ based simple text editor
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <glib.h>
#include "encoding.h"

static void test_detect_line_ending(void)
{
	g_assert_cmpint(detect_line_ending(""), ==, LF);
	g_assert_cmpint(detect_line_ending("one line"), ==, LF);
	g_assert_cmpint(detect_line_ending("one\ntwo\n"), ==, LF);
	g_assert_cmpint(detect_line_ending("one\rtwo\r"), ==, CR);
	g_assert_cmpint(detect_line_ending("one\r\ntwo\r\n"), ==, CR+LF);
}

static void test_convert_line_ending_to_lf(void)
{
	g_autofree gchar *text = g_strdup("one\r\ntwo\rthree\n");

	convert_line_ending_to_lf(text);
	g_assert_cmpstr(text, ==, "one\ntwo\nthree\n");
}

static void test_convert_line_ending(void)
{
	g_autofree gchar *cr = g_strdup("one\ntwo\n");
	g_autofree gchar *crlf = g_strdup("one\ntwo\n");

	convert_line_ending(&cr, CR);
	g_assert_cmpstr(cr, ==, "one\rtwo\r");

	convert_line_ending(&crlf, CR+LF);
	g_assert_cmpstr(crlf, ==, "one\r\ntwo\r\n");
}

static void test_detect_charset(void)
{
	g_assert_cmpstr(detect_charset("plain ascii"), ==, get_default_charset());
	g_assert_cmpstr(detect_charset("caf\xc3\xa9"), ==, "UTF-8");
	g_assert_cmpstr(detect_charset("\x1b$B"), ==, "ISO-2022-JP");
}

gint main(gint argc, gchar **argv)
{
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/encoding/detect-line-ending", test_detect_line_ending);
	g_test_add_func("/encoding/convert-line-ending-to-lf", test_convert_line_ending_to_lf);
	g_test_add_func("/encoding/convert-line-ending", test_convert_line_ending);
	g_test_add_func("/encoding/detect-charset", test_detect_charset);

	return g_test_run();
}
