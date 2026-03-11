/*
 *  Figpad - GTK+ based simple text editor
 *  Copyright (C) 2004-2005 Tarot Osuji
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

#include <string.h>
#include "figpad.h"

static gboolean searched_flag = FALSE;

static void cb_changed(GtkTextBuffer *buffer)
{
	GtkTextIter start, end;

	gtk_text_buffer_get_bounds(buffer, &start, &end);
	gtk_text_buffer_remove_all_tags(buffer, &start, &end);
	g_signal_handlers_block_by_func(G_OBJECT(buffer),
		G_CALLBACK(cb_changed), NULL);
	searched_flag = FALSE;
}

/*
 * cb_paste_clipboard_finish: async callback that receives the clipboard text
 * and re-sets it as plain text (strips any rich formatting).
 */
static void cb_paste_clipboard_finish(GObject *source, GAsyncResult *res, gpointer user_data)
{
	(void)user_data;
	GdkClipboard *clipboard = GDK_CLIPBOARD(source);
	char *text = gdk_clipboard_read_text_finish(clipboard, res, NULL);

	if (text) {
		gdk_clipboard_set_text(clipboard, text);
		g_free(text);
	}
}

static void cb_paste_clipboard(void)
{
	GdkClipboard *clipboard = gdk_display_get_clipboard(gdk_display_get_default());

	gdk_clipboard_read_text_async(clipboard, NULL, cb_paste_clipboard_finish, NULL);
}

gboolean hlight_check_searched(void)
{
	return searched_flag;
}

gboolean hlight_toggle_searched(GtkTextBuffer *buffer)
{
	if (!searched_flag) {
		g_signal_handlers_unblock_by_func(G_OBJECT(buffer),
			G_CALLBACK(cb_changed), NULL);
		searched_flag = TRUE;
	} else {
		g_signal_handlers_block_by_func(G_OBJECT(buffer),
			G_CALLBACK(cb_changed), NULL);
		searched_flag = FALSE;
	}
	return searched_flag;
}

void hlight_init(GtkTextBuffer *buffer)
{
	gtk_text_buffer_create_tag(buffer, "searched",
		"background", "yellow",
		"foreground", "black",
		NULL);
	gtk_text_buffer_create_tag(buffer, "replaced",
		"background", "cyan",
		"foreground", "black",
		NULL);
	g_signal_connect(G_OBJECT(buffer), "changed",
		G_CALLBACK(cb_changed), NULL);
	g_signal_handlers_block_by_func(G_OBJECT(buffer),
		G_CALLBACK(cb_changed), NULL);

	g_signal_connect(G_OBJECT(pub->mw->view), "paste-clipboard",
		G_CALLBACK(cb_paste_clipboard), NULL);
}
