/*
 *  Figpad - GTK+ based simple text editor
 *  Copyright (C) 2004-2005 Tarot Osuji
 *  Copyright (C)      2011 Wen-Yen Chuang <caleb AT calno DOT com>
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

/*
 * File selector using GtkFileDialog with text-file encoding support
 * (requires GTK >= 4.18).
 *
 * open_text_file  — lets the user pick a file + optional encoding
 * save_text_file  — lets the user pick a file + encoding + line ending
 *
 * The public entry point, get_fileinfo_from_selector(), blocks the
 * caller with a GMainLoop so that the existing synchronous call sites
 * in callback.c do not need to be restructured.
 */

#include "figpad.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Async-callback context shared between open and save paths         */
/* ------------------------------------------------------------------ */

typedef struct {
	GMainLoop *loop;
	FileInfo  *selected_fi;   /* result — NULL on cancel */
	gboolean   done;          /* TRUE once the callback has fired */
	gint       mode;          /* OPEN or SAVE */
} TextFileDialogData;

/* ------------------------------------------------------------------ */
/*  Map GTK line-ending string to our LF / CR / CR+LF values         */
/* ------------------------------------------------------------------ */

static gchar line_ending_from_gtk(const char *le)
{
	if (le == NULL || le[0] == '\0')
		return LF;                  /* preserve / default */
	if (strcmp(le, "\r\n") == 0)
		return CR + LF;
	if (strcmp(le, "\r") == 0)
		return CR;
	return LF;                          /* "\n" or anything else */
}

/* ------------------------------------------------------------------ */
/*  Async callbacks                                                   */
/* ------------------------------------------------------------------ */

static void on_open_text_file_finish(GObject *source, GAsyncResult *result,
	gpointer user_data)
{
	TextFileDialogData *data = user_data;
	GtkFileDialog *fd = GTK_FILE_DIALOG(source);
	const char *encoding = NULL;
	GError *err = NULL;

	GFile *gfile = gtk_file_dialog_open_text_file_finish(fd, result,
		&encoding, &err);

	if (gfile) {
		data->selected_fi = g_malloc0(sizeof(FileInfo));
		data->selected_fi->filename = g_file_get_path(gfile);
		if (encoding) {
			data->selected_fi->charset = g_strdup(encoding);
			data->selected_fi->charset_flag = TRUE;
		}
		data->selected_fi->lineend = LF;   /* irrelevant for open */
		g_object_unref(gfile);
	} else {
		/* User cancelled or error — leave selected_fi NULL */
		if (err)
			g_error_free(err);
	}

	data->done = TRUE;
	g_main_loop_quit(data->loop);
}

static void on_save_text_file_finish(GObject *source, GAsyncResult *result,
	gpointer user_data)
{
	TextFileDialogData *data = user_data;
	GtkFileDialog *fd = GTK_FILE_DIALOG(source);
	const char *encoding = NULL;
	const char *line_ending = NULL;
	GError *err = NULL;

	GFile *gfile = gtk_file_dialog_save_text_file_finish(fd, result,
		&encoding, &line_ending, &err);

	if (gfile) {
		data->selected_fi = g_malloc0(sizeof(FileInfo));
		data->selected_fi->filename = g_file_get_path(gfile);
		if (encoding) {
			data->selected_fi->charset = g_strdup(encoding);
			data->selected_fi->charset_flag = TRUE;
		}
		data->selected_fi->lineend = line_ending_from_gtk(line_ending);
		g_object_unref(gfile);
	} else {
		if (err)
			g_error_free(err);
	}

	data->done = TRUE;
	g_main_loop_quit(data->loop);
}

/* ------------------------------------------------------------------ */
/*  Public entry point                                                */
/* ------------------------------------------------------------------ */

FileInfo *get_fileinfo_from_selector(FileInfo *fi, gint requested_mode)
{
	GtkFileDialog *fd;
	TextFileDialogData data = { 0 };

	data.mode = requested_mode;
	data.loop = g_main_loop_new(NULL, FALSE);

	fd = gtk_file_dialog_new();
	gtk_file_dialog_set_title(fd,
		requested_mode == OPEN ? _("Open") : _("Save As"));
	gtk_file_dialog_set_modal(fd, TRUE);

	/* Pre-select the current file / folder when available */
	if (fi->filename) {
		GFile *file = g_file_new_for_path(fi->filename);
		if (requested_mode == OPEN) {
			GFile *folder = g_file_get_parent(file);
			if (folder) {
				gtk_file_dialog_set_initial_folder(fd, folder);
				g_object_unref(folder);
			}
		} else {
			/* For Save As, set both folder and suggested name */
			GFile *folder = g_file_get_parent(file);
			if (folder) {
				gtk_file_dialog_set_initial_folder(fd, folder);
				g_object_unref(folder);
			}
			gchar *basename = g_file_get_basename(file);
			gtk_file_dialog_set_initial_name(fd, basename);
			g_free(basename);
		}
		g_object_unref(file);
	}

	if (requested_mode == OPEN) {
		gtk_file_dialog_open_text_file(fd,
			GTK_WINDOW(pub->mw->window), NULL,
			on_open_text_file_finish, &data);
	} else {
		gtk_file_dialog_save_text_file(fd,
			GTK_WINDOW(pub->mw->window), NULL,
			on_save_text_file_finish, &data);
	}

	g_object_unref(fd);

	/* Block until the async callback fires */
	g_main_loop_run(data.loop);
	g_main_loop_unref(data.loop);

	return data.selected_fi;
}
