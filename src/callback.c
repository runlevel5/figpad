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

#include "figpad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

/* Resolve the path of the running binary.
 * Uses /proc/self/exe on Linux, falls back to PACKAGE for PATH lookup. */
static gchar *get_self_exe_path(void)
{
	char buf[PATH_MAX];
	ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
	if (len > 0) {
		buf[len] = '\0';
		return g_strdup(buf);
	}
	return g_strdup(PACKAGE);
}

static void set_selection_bound(GtkTextBuffer *buffer, gint start, gint end)
{
	GtkTextIter start_iter, end_iter;

	gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, start);
	if (end < 0)
		gtk_text_buffer_get_end_iter(buffer, &end_iter);
	else
		gtk_text_buffer_get_iter_at_offset(buffer, &end_iter, end);
	gtk_text_buffer_place_cursor(buffer, &end_iter);
	gtk_text_buffer_move_mark_by_name(buffer, "selection_bound", &start_iter);
}

void on_file_new(void)
{
	gchar *comline;
	gchar *option;
	gchar *exe;

	save_config_file();
	exe = get_self_exe_path();
	option = pub->fi->charset_flag ?
		g_strdup_printf("%s%s", " --codeset=", pub->fi->charset) : "";
	comline = g_strdup_printf("%s%s", exe, option);
	if (pub->fi->charset_flag)
		g_free(option);
	g_spawn_command_line_async(comline, NULL);
	g_free(comline);
	g_free(exe);
}

void on_file_open(void)
#ifdef ENABLE_CSDI
{ // too slow...
	FileInfo *fi;
	gchar *comline;
	gchar *option = NULL;

	fi = get_fileinfo_from_selector(pub->fi, OPEN);
	if (fi) {
		save_config_file();
		option = g_strdup_printf("--codeset=%s ", fi->charset);
		comline = g_strdup_printf("%s %s%s", PACKAGE,
			fi->charset ? option : "",
			fi->filename);
		g_spawn_command_line_async(comline, NULL);
		g_free(option);
		g_free(comline);
		g_free(fi);
	}
}
#else
{
	FileInfo *fi;

	if (check_text_modification())
		return;
	fi = get_fileinfo_from_selector(pub->fi, OPEN);
	if (fi) {
		if (file_open_real(pub->mw->view, fi))
			g_free(fi);
		else {
			g_free(pub->fi);
			pub->fi = fi;
			undo_clear_all(pub->mw->buffer);
			force_call_cb_modified_changed(pub->mw->view);
		}
	}
}
#endif

gint on_file_save(void)
{
	if (pub->fi->filename == NULL)
		return on_file_save_as();
	if (check_file_writable(pub->fi->filename) == FALSE)
		return on_file_save_as();
	if (file_save_real(pub->mw->view, pub->fi))
		return -1;
	force_call_cb_modified_changed(pub->mw->view);
	return 0;
}

gint on_file_save_as(void)
{
	FileInfo *fi;

	fi = get_fileinfo_from_selector(pub->fi, SAVE);
	if (fi == NULL)
		return -1;
	if (file_save_real(pub->mw->view, fi)) {
		g_free(fi);
		return -1;
	}
	g_free(pub->fi);
	pub->fi = fi;
	undo_clear_all(pub->mw->buffer);
	force_call_cb_modified_changed(pub->mw->view);
	return 0;
}

#if ENABLE_STATISTICS
void on_file_stats(void)
{
	gchar *stats = file_stats(pub->mw->view, pub->fi);
	GtkAlertDialog *alert;

	alert = gtk_alert_dialog_new("%s", _("Statistics"));
	gtk_alert_dialog_set_detail(alert, stats);
	gtk_alert_dialog_set_buttons(alert, (const char *[]){ _("_OK"), NULL });
	gtk_alert_dialog_set_default_button(alert, 0);
	gtk_alert_dialog_set_cancel_button(alert, 0);

	gtk_alert_dialog_show(alert, GTK_WINDOW(pub->mw->window));
	g_object_unref(alert);

	g_free(stats);
}
#endif

#if ENABLE_PRINT
void on_file_print_preview(void)
{
	create_gtkprint_preview_session(GTK_TEXT_VIEW(pub->mw->view),
		get_file_basename(pub->fi->filename, FALSE));
}

void on_file_print(void)
{
	create_gtkprint_session(GTK_TEXT_VIEW(pub->mw->view),
		get_file_basename(pub->fi->filename, FALSE));
}
#endif

void on_file_close(void)
{
	if (!check_text_modification()) {
		force_block_cb_modified_changed(pub->mw->view);
		gtk_text_buffer_set_text(pub->mw->buffer, "", 0);
		gtk_text_buffer_set_modified(pub->mw->buffer, FALSE);
		if (pub->fi->filename)
			g_free(pub->fi->filename);
		pub->fi->filename = NULL;
		if (pub->fi->charset)
			g_free(pub->fi->charset);
		pub->fi->charset = NULL;
		pub->fi->charset_flag = FALSE;
		pub->fi->lineend = LF;
		undo_clear_all(pub->mw->buffer);
		force_call_cb_modified_changed(pub->mw->view);
		force_unblock_cb_modified_changed(pub->mw->view);
	}
}

void on_file_quit(void)
{
	if (!check_text_modification()) {
		save_config_file();
		g_application_quit(G_APPLICATION(pub->app));
	}
}

void on_edit_undo(void)
{
	undo_undo(pub->mw->buffer);
}

void on_edit_redo(void)
{
	undo_redo(pub->mw->buffer);
}

void on_edit_cut(void)
{
	g_signal_emit_by_name(G_OBJECT(pub->mw->view), "cut-clipboard");
}

void on_edit_copy(void)
{
	g_signal_emit_by_name(G_OBJECT(pub->mw->view), "copy-clipboard");
}

void on_edit_paste(void)
{
	g_signal_emit_by_name(G_OBJECT(pub->mw->view), "paste-clipboard");
}

void on_edit_delete(void)
{
	gtk_text_buffer_delete_selection(pub->mw->buffer, TRUE, TRUE);
}

void on_edit_select_all(void)
{
	set_selection_bound(pub->mw->buffer, 0, -1);
}

static void activate_quick_find(void)
{
	static gboolean flag = FALSE;

	if (!flag) {
		menu_sensitivity_from_find(TRUE);
		flag = TRUE;
	}
}

void on_search_find(void)
{
	if (run_dialog_search(pub->mw->view, 0))
		activate_quick_find();
}

void on_search_find_next(void)
{
	document_search_real(pub->mw->view, 1);
}

void on_search_find_previous(void)
{
	document_search_real(pub->mw->view, -1);
}

void on_search_replace(void)
{
	if (run_dialog_search(pub->mw->view, 1))
		activate_quick_find();
}

void on_search_jump_to(void)
{
	run_dialog_jump_to(pub->mw->view);
}

void on_option_font(void)
{
	change_text_font_by_selector(pub->mw->view);
}

void on_option_word_wrap(void)
{
	gboolean state = menu_toggle_get_active("word-wrap");
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(pub->mw->view),
		state ? GTK_WRAP_WORD : GTK_WRAP_NONE);
}

void on_option_line_numbers(void)
{
	gboolean state = menu_toggle_get_active("line-numbers");
	show_line_numbers(pub->mw->view, state);
}

void on_option_always_on_top(void)
{
	/* gtk_window_set_keep_above() was removed in GTK4 with no replacement.
	 * Always-on-top is now a compositor/window-manager policy and cannot be
	 * requested by the application.  Keep the callback wired up so the menu
	 * entry doesn't crash, but the action is a no-op for now. */
	(void)0;
}

void on_option_auto_indent(void)
{
	gboolean state = menu_toggle_get_active("auto-indent");
	indent_set_state(state);
}

void on_help_about(void)
{
	const gchar *copyright = "Copyright \xc2\xa9 2026 Trung L\xc3\xaa\n\nBased on l3afpad by:\nCopyright \xc2\xa9 2014 Steven Honeyman\nCopyright \xc2\xa9 2012 Yoo, Taik-Yon\n\nBased on leafpad by:\nCopyright \xc2\xa9 2004-2010 Tarot Osuji\nCopyright \xc2\xa9 2011 Wen-Yen Chuang\nCopyright \xc2\xa9 2011 Jack Gandy";
	const gchar *comments = _("GTK4 based simple text editor");
	const gchar *authors[] = {
		"Trung L\xc3\xaa <8@tle.id.au>",
		"",
		"l3afpad authors:",
		"Steven Honeyman <stevenhoneyman@gmail.com>",
		"Yoo, Taik-Yon <jaagar@gmail.com>",
		"",
		"leafpad authors:",
		"Tarot Osuji <tarot@sdf.lonestar.org>",
		"Wen-Yen Chuang <caleb@calno.com>",
		NULL
	};
	const gchar *translator_credits = _("translator-credits");

	translator_credits = strcmp(translator_credits, "translator-credits")
		? translator_credits : NULL;

	const gchar *artists[] = {
		"Lapo Calamandrei <calamandrei@gmail.com>",
		"Jack Gandy <scionicspectre@gmail.com>",
		NULL
	};
	gtk_show_about_dialog(GTK_WINDOW(pub->mw->window),
		"program-name", _("figpad"),
		"version", PACKAGE_VERSION,
		"copyright", copyright,
		"comments", comments,
		"authors", authors,
		"artists", artists,
		"translator-credits", translator_credits,
		"logo-icon-name", PACKAGE,
		NULL);
}
