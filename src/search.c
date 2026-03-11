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
#include "gtksourceiter.h"

#if SEARCH_HISTORY
static GList *find_history;
static GList *replace_history;
#endif
static gchar *string_find    = NULL;
static gchar *string_replace = NULL;
static gboolean match_case, replace_all;//, replace_mode = FALSE;

static gboolean hlight_searched_strings(GtkTextBuffer *buffer, gchar *str)
{
	GtkTextIter iter, start, end;
	gboolean res, retval = FALSE;
	GtkSourceSearchFlags search_flags =
		GTK_SOURCE_SEARCH_VISIBLE_ONLY | GTK_SOURCE_SEARCH_TEXT_ONLY;

	if (!string_find)
		return FALSE;

	if (!match_case)
		search_flags = search_flags | GTK_SOURCE_SEARCH_CASE_INSENSITIVE;

	gtk_text_buffer_get_bounds(buffer, &start, &end);
/*	gtk_text_buffer_remove_tag_by_name(buffer,
		"searched", &start, &end);
	gtk_text_buffer_remove_tag_by_name(buffer,
		"replaced", &start, &end);	*/
	gtk_text_buffer_remove_all_tags(buffer, &start, &end);
	iter = start;
	do {
		res = gtk_source_iter_forward_search(
			&iter, str, search_flags, &start, &end, NULL);
		if (res) {
			retval = TRUE;
			gtk_text_buffer_apply_tag_by_name(buffer,
				"searched", &start, &end);
//				replace_mode ? "replaced" : "searched", &start, &end);
			iter = end;
		}
	} while (res);
/*	if (replace_mode)
		replace_mode = FALSE;
	else	*/
	hlight_toggle_searched(buffer);

	return retval;
}

gboolean document_search_real(GtkWidget *textview, gint direction)
{
	GtkTextIter iter, match_start, match_end;
	gboolean res;
	GtkSourceSearchFlags search_flags = GTK_SOURCE_SEARCH_VISIBLE_ONLY | GTK_SOURCE_SEARCH_TEXT_ONLY;
	GtkTextBuffer *textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));

	if (!string_find)
		return FALSE;

	if (!match_case)
		search_flags = search_flags | GTK_SOURCE_SEARCH_CASE_INSENSITIVE;

//	if (direction == 0 || !hlight_check_searched())
	if (direction == 0 || (direction != 2 && !hlight_check_searched()))
		hlight_searched_strings(gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview)), string_find);

	gtk_text_mark_set_visible(
		gtk_text_buffer_get_selection_bound(
			gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview))), FALSE);

	gtk_text_buffer_get_iter_at_mark(textbuffer, &iter, gtk_text_buffer_get_insert(textbuffer));
	if (direction < 0) {
		res = gtk_source_iter_backward_search(
			&iter, string_find, search_flags, &match_start, &match_end, NULL);
		if (gtk_text_iter_equal(&iter, &match_end)) {
			res = gtk_source_iter_backward_search(
				&match_start, string_find, search_flags, &match_start, &match_end, NULL);
		}
	} else {
		res = gtk_source_iter_forward_search(
			&iter, string_find, search_flags, &match_start, &match_end, NULL);
	}
	/* TODO: both gtk_(text/source)_iter_backward_search works not fine for multi-byte */

	/* wrap */
	/* TODO: define limit NULL -> proper value */
	if (!res) {
		if (direction < 0) {
			gtk_text_buffer_get_end_iter(textbuffer, &iter);
			res = gtk_source_iter_backward_search(
				&iter, string_find, search_flags, &match_start, &match_end, NULL);
		} else {
			gtk_text_buffer_get_start_iter(textbuffer, &iter);
			res = gtk_source_iter_forward_search(
				&iter, string_find, search_flags, &match_start, &match_end, NULL);
		}
	}

	if (res) {
		gtk_text_buffer_place_cursor(textbuffer, &match_start);
		gtk_text_buffer_move_mark_by_name(textbuffer, "insert", &match_end);
//		gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(textview), &match_start, 0.1, FALSE, 0.5, 0.5);
		scroll_to_cursor(textbuffer, 0.05);
	}
	else if (direction == 0)
		run_dialog_message(GTK_WIDGET(gtk_widget_get_root(textview)), GTK_MESSAGE_WARNING,
			_("Search string not found"));

	return res;
}

static gint document_replace_real(GtkWidget *textview)
{
	GtkTextIter iter, match_start, match_end, rep_start;
	GtkTextMark *mark_init = NULL;
	gboolean res;
	gint num = 0, offset;
	GtkWidget *q_dialog = NULL;
	GtkSourceSearchFlags search_flags = GTK_SOURCE_SEARCH_VISIBLE_ONLY | GTK_SOURCE_SEARCH_TEXT_ONLY;
	GtkTextBuffer *textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));

	if (!match_case)
		search_flags = search_flags | GTK_SOURCE_SEARCH_CASE_INSENSITIVE;

	if (replace_all) {
		gtk_text_buffer_get_iter_at_mark(textbuffer,
			&iter, gtk_text_buffer_get_insert(textbuffer));
		mark_init = gtk_text_buffer_create_mark(textbuffer, NULL, &iter, FALSE);
		gtk_text_buffer_get_start_iter(textbuffer, &iter);

		gtk_text_buffer_get_end_iter(textbuffer, &match_end);
//		gtk_text_buffer_remove_tag_by_name(textbuffer,
//			"replaced", &iter, &match_end);
		gtk_text_buffer_remove_all_tags(textbuffer,
			&iter, &match_end);
	} else {
		hlight_searched_strings(textbuffer, string_find);
		hlight_toggle_searched(textbuffer);
	}

	do {
		if (replace_all) {
			res = gtk_source_iter_forward_search(
				&iter, string_find, search_flags, &match_start, &match_end, NULL);
			if (res) {
				gtk_text_buffer_place_cursor(textbuffer, &match_start);
				gtk_text_buffer_move_mark_by_name(textbuffer, "insert", &match_end);
				gtk_text_buffer_get_iter_at_mark(
					textbuffer, &iter, gtk_text_buffer_get_insert(textbuffer));
			}
		}
		else
//			res = document_search_real(textview, 0);
			res = document_search_real(textview, 2);

		if (res) {
			if (!replace_all) {
				if (num == 0 && q_dialog == NULL)
					/* TODO: convert to async dialog API in a future cleanup pass */
					q_dialog = create_dialog_message_question(
						GTK_WIDGET(gtk_widget_get_root(textview)), _("Replace?"));
					GtkTextIter ins,bou;
					gtk_text_buffer_get_selection_bounds(textbuffer, &ins, &bou);
				switch (run_dialog_sync(GTK_DIALOG(q_dialog))) {
				case GTK_RESPONSE_YES:
					gtk_text_buffer_select_range(textbuffer, &ins, &bou);
					break;
				case GTK_RESPONSE_NO:
					continue;
//				case GTK_RESPONSE_CANCEL:
				default:
					res = 0;
					if (num == 0)
						num = -1;
					continue;
				}
			}
			gtk_text_buffer_delete_selection(textbuffer, TRUE, TRUE);
			if (strlen(string_replace)) {
				gtk_text_buffer_get_iter_at_mark(
					textbuffer, &rep_start,
					gtk_text_buffer_get_insert(textbuffer));
				offset = gtk_text_iter_get_offset(&rep_start);
				undo_set_sequency(TRUE);
				g_signal_emit_by_name(G_OBJECT(textbuffer),
					"begin-user-action");
				gtk_text_buffer_insert_at_cursor(textbuffer,
					string_replace, strlen(string_replace));
				g_signal_emit_by_name(G_OBJECT(textbuffer),
					"end-user-action");
				gtk_text_buffer_get_iter_at_mark(
					textbuffer, &iter,
					gtk_text_buffer_get_insert(textbuffer));
				gtk_text_buffer_get_iter_at_offset(textbuffer,
					&rep_start, offset);
				gtk_text_buffer_apply_tag_by_name(textbuffer,
					"replaced", &rep_start, &iter);
			} else
				gtk_text_buffer_get_iter_at_mark(
					textbuffer, &iter,
					gtk_text_buffer_get_insert(textbuffer));

			num++;
/*			if (replace_all)
				undo_set_sequency(TRUE);
			else
				undo_set_sequency(FALSE);*/
			undo_set_sequency(replace_all);
		}
	} while (res);
	if (!hlight_check_searched())
		hlight_toggle_searched(textbuffer);

	if (q_dialog)
		gtk_window_destroy(GTK_WINDOW(q_dialog));
/*	if (strlen(string_replace)) {
		replace_mode = TRUE;
		hlight_searched_strings(textbuffer, string_replace);
	}	*/
	if (replace_all) {
		gtk_text_buffer_get_iter_at_mark(textbuffer, &iter, mark_init);
		gtk_text_buffer_place_cursor(textbuffer, &iter);
		run_dialog_message(GTK_WIDGET(gtk_widget_get_root(textview)), GTK_MESSAGE_INFO,
			_("%d strings replaced"), num);
		undo_set_sequency(FALSE);
	}

	return num;
}


static GtkWidget *search_ok_button;

#if !SEARCH_HISTORY
static gint entry_len;

static void toggle_sensitivity(GtkWidget *w, gint pos1, gint pos2, gint *pos3)
{
	(void)w;
	if (pos3) {
		if (!entry_len)
			gtk_widget_set_sensitive(search_ok_button, TRUE);
		entry_len += pos2;
	} else {
		entry_len = entry_len + pos1 - pos2;
		if (!entry_len)
			gtk_widget_set_sensitive(search_ok_button, FALSE);
	}
}
#endif /* !SEARCH_HISTORY */

#if SEARCH_HISTORY
static void toggle_sensitivity(GtkWidget *entry)
{
	gboolean has_text = *(gtk_editable_get_text(GTK_EDITABLE(entry))) != '\0';
	gtk_widget_set_sensitive(search_ok_button, has_text);
}
#endif

static void toggle_check_case(GtkWidget *widget)
{
	match_case = gtk_check_button_get_active(GTK_CHECK_BUTTON(widget));
}

static void toggle_check_all(GtkWidget *widget)
{
	replace_all = gtk_check_button_get_active(GTK_CHECK_BUTTON(widget));
}

typedef struct {
	GMainLoop *loop;
	gboolean accepted;
} SearchData;

static void on_search_accept(GtkWidget *widget, gpointer user_data)
{
	(void)widget;
	SearchData *data = user_data;
	data->accepted = TRUE;
	g_main_loop_quit(data->loop);
}

gboolean run_dialog_search(GtkWidget *textview, gint mode)
{
	GtkWidget *dialog;
	GtkWidget *content_box;
	GtkWidget *grid;
	GtkWidget *button_box;
	GtkWidget *cancel_button;
	GtkWidget *ok_button;
	GtkWidget *label_find, *label_replace;
#if SEARCH_HISTORY
	GtkWidget *combo_find, *combo_replace = NULL;
	GtkTextBuffer *buffer;
	GtkTextIter start_iter, end_iter;
#endif
	GtkWidget *entry_find, *entry_replace = NULL;
	GtkWidget *check_case, *check_all;
	SearchData data;

	/* Plain GtkWindow instead of deprecated GtkDialog */
	dialog = gtk_window_new();
	gtk_window_set_title(GTK_WINDOW(dialog),
		mode ? _("Replace") : _("Find"));
	gtk_window_set_transient_for(GTK_WINDOW(dialog),
		GTK_WINDOW(gtk_widget_get_root(textview)));
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
	gtk_widget_set_size_request(dialog, 400, -1);

	content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_margin_start(content_box, 12);
	gtk_widget_set_margin_end(content_box, 12);
	gtk_widget_set_margin_top(content_box, 12);
	gtk_widget_set_margin_bottom(content_box, 12);
	gtk_window_set_child(GTK_WINDOW(dialog), content_box);

	grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_box_append(GTK_BOX(content_box), grid);

	label_find = gtk_label_new_with_mnemonic(_("Fi_nd what:"));
	gtk_widget_set_halign(label_find, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(grid), label_find, 0, 0, 1, 1);
#if SEARCH_HISTORY
	combo_find = create_combo_with_history(&find_history);
	gtk_widget_set_hexpand(combo_find, TRUE);
	gtk_grid_attach(GTK_GRID(grid), combo_find, 1, 0, 1, 1);
	entry_find = gtk_combo_box_get_child(GTK_COMBO_BOX(combo_find));
#else
	entry_find = gtk_entry_new();
	gtk_widget_set_hexpand(entry_find, TRUE);
	gtk_grid_attach(GTK_GRID(grid), entry_find, 1, 0, 1, 1);
#endif
	gtk_label_set_mnemonic_widget(GTK_LABEL(label_find), entry_find);

	if (mode) {
		label_replace = gtk_label_new_with_mnemonic(_("Re_place with:"));
		gtk_widget_set_halign(label_replace, GTK_ALIGN_START);
		gtk_grid_attach(GTK_GRID(grid), label_replace, 0, 1, 1, 1);
#if SEARCH_HISTORY
		combo_replace = create_combo_with_history(&replace_history);
		gtk_widget_set_hexpand(combo_replace, TRUE);
		gtk_grid_attach(GTK_GRID(grid), combo_replace, 1, 1, 1, 1);
		entry_replace = gtk_combo_box_get_child(GTK_COMBO_BOX(combo_replace));
		gtk_label_set_mnemonic_widget(GTK_LABEL(label_replace), entry_replace);
		gtk_editable_set_text(GTK_EDITABLE(entry_replace), "");
#else
		entry_replace = gtk_entry_new();
		gtk_widget_set_hexpand(entry_replace, TRUE);
		gtk_grid_attach(GTK_GRID(grid), entry_replace, 1, 1, 1, 1);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label_replace), entry_replace);
		if (string_replace)
			gtk_editable_set_text(GTK_EDITABLE(entry_replace), string_replace);
#endif
	}

	check_case = gtk_check_button_new_with_mnemonic(_("_Match case"));
	gtk_check_button_set_active(GTK_CHECK_BUTTON(check_case), match_case);
	g_signal_connect(check_case, "toggled", G_CALLBACK(toggle_check_case), NULL);
	gtk_grid_attach(GTK_GRID(grid), check_case, 0, 1 + mode, 2, 1);

	if (mode) {
		check_all = gtk_check_button_new_with_mnemonic(_("Replace _all at once"));
		gtk_check_button_set_active(GTK_CHECK_BUTTON(check_all), replace_all);
		g_signal_connect(check_all, "toggled", G_CALLBACK(toggle_check_all), NULL);
		gtk_grid_attach(GTK_GRID(grid), check_all, 0, 2 + mode, 2, 1);
	}

	/* Action buttons */
	button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_set_halign(button_box, GTK_ALIGN_END);
	gtk_box_append(GTK_BOX(content_box), button_box);

	cancel_button = gtk_button_new_with_mnemonic(_("_Cancel"));
	gtk_box_append(GTK_BOX(button_box), cancel_button);

	if (mode)
		ok_button = gtk_button_new_with_mnemonic(_("Find and _Replace"));
	else
		ok_button = gtk_button_new_with_mnemonic(_("_Find"));
	gtk_widget_add_css_class(ok_button, "suggested-action");
	gtk_box_append(GTK_BOX(button_box), ok_button);
	search_ok_button = ok_button;

	/* Sensitivity tracking for the OK button */
#if !SEARCH_HISTORY
	gtk_widget_set_sensitive(ok_button, FALSE);
	entry_len = 0;
	g_signal_connect(G_OBJECT(entry_find), "insert-text",
		G_CALLBACK(toggle_sensitivity), NULL);
	g_signal_connect(G_OBJECT(entry_find), "delete-text",
		G_CALLBACK(toggle_sensitivity), NULL);
	if (string_find) {
		gtk_editable_set_text(GTK_EDITABLE(entry_find), string_find);
		gtk_widget_set_sensitive(ok_button, TRUE);
	}
#endif
#if SEARCH_HISTORY
	g_signal_connect_after(G_OBJECT(entry_find), "insert-text",
		G_CALLBACK(toggle_sensitivity), NULL);
	g_signal_connect_after(G_OBJECT(entry_find), "delete-text",
		G_CALLBACK(toggle_sensitivity), NULL);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	if (gtk_text_buffer_get_selection_bounds(buffer, &start_iter, &end_iter)) {
		if (string_find != NULL)
			g_free(string_find);
		string_find = gtk_text_buffer_get_text(buffer, &start_iter, &end_iter,
			FALSE);
		gtk_editable_set_text(GTK_EDITABLE(entry_find), string_find);
		gtk_widget_set_sensitive(ok_button, TRUE);
	} else
		gtk_editable_set_text(GTK_EDITABLE(entry_find), "");
	toggle_sensitivity(entry_find);
#endif

#if !SEARCH_HISTORY
	gtk_entry_set_activates_default(GTK_ENTRY(entry_find), TRUE);
#else
	gtk_entry_set_activates_default(GTK_ENTRY(entry_find), TRUE);
#endif
	if (mode && entry_replace)
		gtk_entry_set_activates_default(GTK_ENTRY(entry_replace), TRUE);

	/* Sync-loop plumbing */
	data.loop = g_main_loop_new(NULL, FALSE);
	data.accepted = FALSE;

	g_signal_connect_swapped(cancel_button, "clicked",
		G_CALLBACK(g_main_loop_quit), data.loop);
	g_signal_connect_swapped(dialog, "close-request",
		G_CALLBACK(g_main_loop_quit), data.loop);
	g_signal_connect(ok_button, "clicked",
		G_CALLBACK(on_search_accept), &data);

	gtk_window_set_default_widget(GTK_WINDOW(dialog), ok_button);
	gtk_window_present(GTK_WINDOW(dialog));
	g_main_loop_run(data.loop);
	g_main_loop_unref(data.loop);

	if (data.accepted) {
#if SEARCH_HISTORY
		update_combo_data(entry_find, &find_history);
		if (string_find != NULL)
#endif
		g_free(string_find);
		string_find = g_strdup(gtk_editable_get_text(GTK_EDITABLE(entry_find)));
		if (mode) {
#if SEARCH_HISTORY
			update_combo_data(entry_replace, &replace_history);
			if (string_replace != NULL)
#endif
			g_free(string_replace);
			string_replace = g_strdup(gtk_editable_get_text(GTK_EDITABLE(entry_replace)));
		}
	}

	gtk_window_destroy(GTK_WINDOW(dialog));
	search_ok_button = NULL;

	if (data.accepted) {
		if (strlen(string_find)) {
			if (mode)
				document_replace_real(textview);
			else
				document_search_real(textview, 0);
		}
	}

	return data.accepted;
}

typedef struct {
	GMainLoop *loop;
	gboolean accepted;
} JumpToData;

static void on_jump_to_accept(GtkWidget *widget, gpointer user_data)
{
	(void)widget;
	JumpToData *data = user_data;
	data->accepted = TRUE;
	g_main_loop_quit(data->loop);
}

void run_dialog_jump_to(GtkWidget *textview)
{
	GtkWidget *dialog;
	GtkWidget *button;
	GtkWidget *grid;
	GtkWidget *label;
	GtkWidget *spinner;
	GtkWidget *content_box;
	GtkWidget *button_box;
	GtkWidget *cancel_button;
	GtkAdjustment *spinner_adj;
	GtkTextIter iter;
	gint num, max_num;
	JumpToData data;

	GtkTextBuffer *textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));

	gtk_text_buffer_get_iter_at_mark(textbuffer, &iter,
		gtk_text_buffer_get_insert(textbuffer));
	num = gtk_text_iter_get_line(&iter) + 1;
	gtk_text_buffer_get_end_iter(textbuffer, &iter);
	max_num = gtk_text_iter_get_line(&iter) + 1;

	/* Plain GtkWindow instead of deprecated GtkDialog */
	dialog = gtk_window_new();
	gtk_window_set_title(GTK_WINDOW(dialog), _("Jump To"));
	gtk_window_set_transient_for(GTK_WINDOW(dialog),
		GTK_WINDOW(gtk_widget_get_root(textview)));
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);

	content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_margin_start(content_box, 12);
	gtk_widget_set_margin_end(content_box, 12);
	gtk_widget_set_margin_top(content_box, 12);
	gtk_widget_set_margin_bottom(content_box, 12);
	gtk_window_set_child(GTK_WINDOW(dialog), content_box);

	grid = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_box_append(GTK_BOX(content_box), grid);

	label = gtk_label_new_with_mnemonic(_("_Line number:"));
	spinner_adj = gtk_adjustment_new(num, 1, max_num, 1, 1, 0);
	spinner = gtk_spin_button_new(spinner_adj, 1, 0);
	gtk_editable_set_width_chars(GTK_EDITABLE(spinner), 8);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), spinner);
	gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), spinner, 1, 0, 1, 1);

	/* Action buttons */
	button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_set_halign(button_box, GTK_ALIGN_END);
	gtk_box_append(GTK_BOX(content_box), button_box);

	cancel_button = gtk_button_new_with_mnemonic(_("_Cancel"));
	gtk_box_append(GTK_BOX(button_box), cancel_button);

	button = create_button_with_stock_image(_("_Jump"), "go-jump");
	gtk_widget_add_css_class(button, "suggested-action");
	gtk_box_append(GTK_BOX(button_box), button);

	/* Sync-loop plumbing — will be removed in full async conversion */
	data.loop = g_main_loop_new(NULL, FALSE);
	data.accepted = FALSE;

	g_signal_connect_swapped(cancel_button, "clicked",
		G_CALLBACK(g_main_loop_quit), data.loop);
	g_signal_connect_swapped(dialog, "close-request",
		G_CALLBACK(g_main_loop_quit), data.loop);
	g_signal_connect(button, "clicked",
		G_CALLBACK(on_jump_to_accept), &data);
	g_signal_connect(spinner, "activate",
		G_CALLBACK(on_jump_to_accept), &data);

	gtk_window_present(GTK_WINDOW(dialog));
	g_main_loop_run(data.loop);
	g_main_loop_unref(data.loop);

	if (data.accepted) {
		gtk_text_buffer_get_iter_at_line(textbuffer, &iter,
			gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner)) - 1);
		gtk_text_buffer_place_cursor(textbuffer, &iter);
		scroll_to_cursor(textbuffer, 0.25);
	}

	gtk_window_destroy(GTK_WINDOW(dialog));
}
