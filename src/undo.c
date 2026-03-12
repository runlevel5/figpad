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

#define DV(x)

typedef struct {
	gchar command;
	gint start;
	gint end;
	gboolean seq; // sequency flag
	gchar *str;
} UndoInfo;

enum {
	INS = 0,
	BS,
	DEL
};

/* All undo/redo related state, grouped into a file-scope singleton. */
typedef struct {
	GSimpleAction *undo_action;
	GSimpleAction *redo_action;
	GList         *undo_list;
	GList         *redo_list;
	GString       *undo_gstr;
	UndoInfo      *ui_tmp;
	guint          modified_step;
	guint          prev_keyval;
	gboolean       seq_reserve;
} UndoState;

static UndoState us;

static void undo_flush_temporal_buffer(GtkTextBuffer *buffer);

/* helpers to enable/disable undo/redo, safe with NULL action pointers */
static inline void set_undo_sensitive(gboolean sensitive)
{
	if (us.undo_action)
		g_simple_action_set_enabled(us.undo_action, sensitive);
}
static inline void set_redo_sensitive(gboolean sensitive)
{
	if (us.redo_action)
		g_simple_action_set_enabled(us.redo_action, sensitive);
}

static GList *undo_clear_info_list(GList *info_list)
{
	while (g_list_length(info_list)) {
		g_free(((UndoInfo *)info_list->data)->str);
		g_free(info_list->data);
		info_list = g_list_delete_link(info_list, info_list);
	}
	return info_list;
}

static void undo_append_undo_info(GtkTextBuffer *buffer, gchar command, gint start, gint end, gchar *str)
{
	UndoInfo *ui = g_malloc(sizeof(UndoInfo));

	ui->command = command;
	ui->start = start;
	ui->end = end;
	ui->seq = us.seq_reserve;
	ui->str = str;

	us.seq_reserve = FALSE;

	us.undo_list = g_list_append(us.undo_list, ui);
DV(g_print("undo_cb: %d %s (%d-%d)\n", command, str, start, end));
}

static void undo_create_undo_info(GtkTextBuffer *buffer, gchar command, gint start, gint end)
{
	GtkTextIter start_iter, end_iter;
	gboolean seq_flag = FALSE;
	gchar *str;
	gint keyval = get_current_keyval();

	gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, start);
	gtk_text_buffer_get_iter_at_offset(buffer, &end_iter, end);
	str = gtk_text_buffer_get_text(buffer, &start_iter, &end_iter, FALSE);

	if (us.undo_gstr->len) {
		if ((end - start == 1) && (command == us.ui_tmp->command)) {
			switch (keyval) {
			case GDK_KEY_BackSpace:
				if (end == us.ui_tmp->start)
					seq_flag = TRUE;
				break;
			case GDK_KEY_Delete:
				if (start == us.ui_tmp->start)
					seq_flag = TRUE;
				break;
			case GDK_KEY_Tab:
			case GDK_KEY_space:
				if (start == us.ui_tmp->end)
					seq_flag = TRUE;
				break;
			default:
				if (start == us.ui_tmp->end)
					if (keyval && keyval < KEYVAL_NON_CHAR_THRESHOLD)
						switch (us.prev_keyval) {
						case GDK_KEY_Return:
						case GDK_KEY_Tab:
						case GDK_KEY_space:
							break;
						default:
							seq_flag = TRUE;
						}
			}
		}
		if (seq_flag) {
			switch (command) {
			case BS:
				us.undo_gstr = g_string_prepend(us.undo_gstr, str);
				us.ui_tmp->start--;
				break;
			default:
				us.undo_gstr = g_string_append(us.undo_gstr, str);
				us.ui_tmp->end++;
			}
			us.redo_list = undo_clear_info_list(us.redo_list);
			us.prev_keyval = keyval;
			set_undo_sensitive(TRUE);
			set_redo_sensitive(FALSE);
			return;
		}
		undo_append_undo_info(buffer, us.ui_tmp->command, us.ui_tmp->start, us.ui_tmp->end, g_strdup(us.undo_gstr->str));
		us.undo_gstr = g_string_erase(us.undo_gstr, 0, -1);
	}

	if (!keyval && us.prev_keyval)
		undo_set_sequency(TRUE);

	if (end - start == 1 &&
		((keyval && keyval < KEYVAL_NON_CHAR_THRESHOLD) ||
		  keyval == GDK_KEY_BackSpace || keyval == GDK_KEY_Delete || keyval == GDK_KEY_Tab)) {
		us.ui_tmp->command = command;
		us.ui_tmp->start = start;
		us.ui_tmp->end = end;
		us.undo_gstr = g_string_erase(us.undo_gstr, 0, -1);
		g_string_append(us.undo_gstr, str);
	} else
		undo_append_undo_info(buffer, command, start, end, g_strdup(str));

	us.redo_list = undo_clear_info_list(us.redo_list);
	us.prev_keyval = keyval;
	clear_current_keyval();
	set_undo_sensitive(TRUE);
	set_redo_sensitive(FALSE);
}

static void cb_insert_text(GtkTextBuffer *buffer, GtkTextIter *iter, gchar *str)
{
	gint start, end;

DV(	g_print("insert-text\n"));
	end = gtk_text_iter_get_offset(iter);
	start = end - g_utf8_strlen(str, -1);

	undo_create_undo_info(buffer, INS, start, end);
}

static void cb_delete_range(GtkTextBuffer *buffer, GtkTextIter *start_iter, GtkTextIter *end_iter)
{
	gint start, end;
	gchar command;

DV(	g_print("delete-range\n"));
	start = gtk_text_iter_get_offset(start_iter);
	end = gtk_text_iter_get_offset(end_iter);

	if (get_current_keyval() == GDK_KEY_BackSpace)
		command = BS;
	else
		command = DEL;
	undo_create_undo_info(buffer, command, start, end);
}

void undo_reset_modified_step(GtkTextBuffer *buffer)
{
	undo_flush_temporal_buffer(buffer);
	us.modified_step = g_list_length(us.undo_list);
DV(g_print("undo_reset_modified_step: Reseted modified_step by %d\n", us.modified_step));
}

static void undo_check_modified_step(GtkTextBuffer *buffer)
{
	gboolean flag;

	flag = (us.modified_step == g_list_length(us.undo_list));
	if (gtk_text_buffer_get_modified(buffer) == flag)
		gtk_text_buffer_set_modified(buffer, !flag);
}

static void cb_begin_user_action(GtkTextBuffer *buffer)
{
	g_signal_handlers_unblock_by_func(G_OBJECT(buffer),
		G_CALLBACK(cb_insert_text), NULL);
	g_signal_handlers_unblock_by_func(G_OBJECT(buffer),
		G_CALLBACK(cb_delete_range), NULL);
DV(g_print("begin-user-action(unblock_func)"));
DV(g_print(": keyval = 0x%X\n", get_current_keyval()));
}

static void cb_end_user_action(GtkTextBuffer *buffer)
{
	g_signal_handlers_block_by_func(G_OBJECT(buffer),
		G_CALLBACK(cb_insert_text), NULL);
	g_signal_handlers_block_by_func(G_OBJECT(buffer),
		G_CALLBACK(cb_delete_range), NULL);
DV(g_print("end-user-action(block_func)\n"));
}

void undo_clear_all(GtkTextBuffer *buffer)
{
	us.undo_list = undo_clear_info_list(us.undo_list);
	us.redo_list = undo_clear_info_list(us.redo_list);
	undo_reset_modified_step(buffer);
	set_undo_sensitive(FALSE);
	set_redo_sensitive(FALSE);

	us.ui_tmp->command = INS;
	us.undo_gstr = g_string_erase(us.undo_gstr, 0, -1);
	us.prev_keyval = 0;
}

void undo_init(GtkWidget *view, GtkWidget *undo_w, GtkWidget *redo_w)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));

	(void)undo_w; (void)redo_w;

	/* look up GSimpleActions from the window's action map */
	GtkWidget *toplevel = GTK_WIDGET(gtk_widget_get_root(view));
	if (toplevel) {
		GAction *a;
		a = g_action_map_lookup_action(G_ACTION_MAP(toplevel), "undo");
		if (a) us.undo_action = G_SIMPLE_ACTION(a);
		a = g_action_map_lookup_action(G_ACTION_MAP(toplevel), "redo");
		if (a) us.redo_action = G_SIMPLE_ACTION(a);
	}

	g_signal_connect_after(G_OBJECT(buffer), "insert-text",
		G_CALLBACK(cb_insert_text), NULL);
	g_signal_connect(G_OBJECT(buffer), "delete-range",
		G_CALLBACK(cb_delete_range), NULL);
	g_signal_connect_after(G_OBJECT(buffer), "begin-user-action",
		G_CALLBACK(cb_begin_user_action), NULL);
	g_signal_connect(G_OBJECT(buffer), "end-user-action",
		G_CALLBACK(cb_end_user_action), NULL);
	cb_end_user_action(buffer);

	us.ui_tmp = g_malloc(sizeof(UndoInfo));
	us.undo_gstr = g_string_new("");

	undo_clear_all(buffer);
}

void undo_set_sequency(gboolean seq)
{
	if (g_list_length(us.undo_list))
		((UndoInfo *)g_list_last(us.undo_list)->data)->seq = seq;
DV(g_print("<undo_set_sequency: %d>\n", seq));
}

void undo_set_sequency_reserve(void)
{
	us.seq_reserve = TRUE;
}

static void undo_flush_temporal_buffer(GtkTextBuffer *buffer)
{
	if (us.undo_gstr->len) {
		undo_append_undo_info(buffer, us.ui_tmp->command,
			us.ui_tmp->start, us.ui_tmp->end, g_strdup(us.undo_gstr->str));
		us.undo_gstr = g_string_erase(us.undo_gstr, 0, -1);
	}
}

gboolean undo_undo_real(GtkTextBuffer *buffer)
{
	GtkTextIter start_iter, end_iter;
	UndoInfo *ui;

	undo_flush_temporal_buffer(buffer);
	if (g_list_length(us.undo_list)) {
		ui = g_list_last(us.undo_list)->data;
		gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, ui->start);
		switch (ui->command) {
		case INS:
			gtk_text_buffer_get_iter_at_offset(buffer, &end_iter, ui->end);
			gtk_text_buffer_delete(buffer, &start_iter, &end_iter);
			break;
		default:
			gtk_text_buffer_insert(buffer, &start_iter, ui->str, -1);
		}
		us.redo_list = g_list_append(us.redo_list, ui);
		us.undo_list = g_list_delete_link(us.undo_list, g_list_last(us.undo_list));
DV(g_print("cb_edit_undo: undo left = %d, redo left = %d\n",
g_list_length(us.undo_list), g_list_length(us.redo_list)));
		if (g_list_length(us.undo_list)) {
			if (((UndoInfo *)g_list_last(us.undo_list)->data)->seq)
				return TRUE;
		} else
			set_undo_sensitive(FALSE);
		set_redo_sensitive(TRUE);
		if (ui->command == DEL)
			gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, ui->start);
		gtk_text_buffer_place_cursor(buffer, &start_iter);
		scroll_to_cursor(buffer, SCROLL_MARGIN_NARROW);
	}
	undo_check_modified_step(buffer);
	return FALSE;
}

gboolean undo_redo_real(GtkTextBuffer *buffer)
{
	GtkTextIter start_iter, end_iter;
	UndoInfo *ri;

	if (g_list_length(us.redo_list)) {
		ri = g_list_last(us.redo_list)->data;
		gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, ri->start);
		switch (ri->command) {
		case INS:
			gtk_text_buffer_insert(buffer, &start_iter, ri->str, -1);
			break;
		default:
			gtk_text_buffer_get_iter_at_offset(buffer, &end_iter, ri->end);
			gtk_text_buffer_delete(buffer, &start_iter, &end_iter);
		}
		us.undo_list = g_list_append(us.undo_list, ri);
		us.redo_list = g_list_delete_link(us.redo_list, g_list_last(us.redo_list));
DV(g_print("cb_edit_redo: undo left = %d, redo left = %d\n",
g_list_length(us.undo_list), g_list_length(us.redo_list)));
		if (ri->seq) {
			undo_set_sequency(TRUE);
			return TRUE;
		}
		if (!g_list_length(us.redo_list))
			set_redo_sensitive(FALSE);
		set_undo_sensitive(TRUE);
		gtk_text_buffer_place_cursor(buffer, &start_iter);
		scroll_to_cursor(buffer, SCROLL_MARGIN_NARROW);
	}
	undo_check_modified_step(buffer);
	return FALSE;
}

void undo_undo(GtkTextBuffer *buffer)
{
	while (undo_undo_real(buffer)) {};
}

void undo_redo(GtkTextBuffer *buffer)
{
	while (undo_redo_real(buffer)) {};
}
