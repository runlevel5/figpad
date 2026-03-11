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


#if !SEARCH_HISTORY
static gint entry_len;

static void toggle_sensitivity(GtkWidget *w, gint pos1, gint pos2, gint *pos3)
{
	if (pos3) {
		if (!entry_len)
			gtk_dialog_set_response_sensitive(GTK_DIALOG(gtk_widget_get_root(w)),
				GTK_RESPONSE_OK, TRUE);
		entry_len += pos2;
//		entry_len = entry_len + pos2;
	} else {
		entry_len = entry_len + pos1 - pos2;
		if (!entry_len)
			gtk_dialog_set_response_sensitive(GTK_DIALOG(gtk_widget_get_root(w)),
				GTK_RESPONSE_OK, FALSE);
	}
}
#endif /* !SEARCH_HISTORY */

#if SEARCH_HISTORY
static void toggle_sensitivity(GtkWidget *entry)
{
	gboolean has_text = *(gtk_editable_get_text(GTK_EDITABLE(entry))) != '\0';
	gtk_dialog_set_response_sensitive(
		GTK_DIALOG(gtk_widget_get_root(entry)), GTK_RESPONSE_OK,
		(has_text) ? TRUE : FALSE);
}
#endif

static void toggle_check_case(GtkWidget *widget)
{
	match_case = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void toggle_check_all(GtkWidget *widget)
{
	replace_all = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

gint run_dialog_search(GtkWidget *textview, gint mode)
{
	GtkWidget *dialog;
	GtkWidget *grid;
	GtkWidget *label_find, *label_replace;
#if SEARCH_HISTORY
	GtkWidget *combo_find, *combo_replace = NULL;
	GtkTextBuffer *buffer;
	GtkTextIter start_iter, end_iter;
#endif
	GtkWidget *entry_find, *entry_replace = NULL;
	GtkWidget *check_case, *check_all;
	gint res;

	if (mode) {
		dialog = gtk_dialog_new_with_buttons(_("Replace"),
			GTK_WINDOW(gtk_widget_get_root(textview)),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			_("_Cancel"), GTK_RESPONSE_CANCEL,
			_("Find and _Replace"), GTK_RESPONSE_OK,
			NULL);
		gtk_widget_set_margin_start(dialog, 4);
		gtk_widget_set_margin_end(dialog, 4);
		gtk_widget_set_margin_top(dialog, 4);
		gtk_widget_set_margin_bottom(dialog, 4);
		gtk_widget_set_size_request(dialog, 400, -1);
	} else {
		dialog = gtk_dialog_new_with_buttons(_("Find"),
			GTK_WINDOW(gtk_widget_get_root(textview)),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			_("_Cancel"), GTK_RESPONSE_CANCEL,
			_("_Find"), GTK_RESPONSE_OK,
			NULL);
		gtk_widget_set_margin_start(dialog, 4);
		gtk_widget_set_margin_end(dialog, 4);
		gtk_widget_set_margin_top(dialog, 4);
		gtk_widget_set_margin_bottom(dialog, 4);
		gtk_widget_set_size_request(dialog, 400, -1);
	}

	grid = gtk_grid_new();
	 gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
	 gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	 gtk_widget_set_margin_start(grid, 8);
	 gtk_widget_set_margin_end(grid, 8);
	 gtk_widget_set_margin_top(grid, 8);
	 gtk_widget_set_margin_bottom(grid, 8);
	 gtk_box_append(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), grid);
	label_find = gtk_label_new_with_mnemonic(_("Fi_nd what:"));
	 gtk_widget_set_halign(label_find, GTK_ALIGN_START);
	 gtk_grid_attach(GTK_GRID(grid), label_find, 0, 0, 1, 1);
#if SEARCH_HISTORY
	combo_find = create_combo_with_history (&find_history);
	 gtk_widget_set_hexpand(combo_find, TRUE);
	 gtk_grid_attach(GTK_GRID(grid), combo_find, 1, 0, 1, 1);
	 entry_find = gtk_combo_box_get_child(GTK_COMBO_BOX(combo_find));
#else
	entry_find = gtk_entry_new();
	 gtk_widget_set_hexpand(entry_find, TRUE);
	 gtk_grid_attach(GTK_GRID(grid), entry_find, 1, 0, 1, 1);
#endif
	 gtk_label_set_mnemonic_widget(GTK_LABEL(label_find), entry_find);
#if !SEARCH_HISTORY
	 gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
		GTK_RESPONSE_OK, FALSE);
	 entry_len = 0;
	 g_signal_connect(G_OBJECT(entry_find), "insert-text",
		G_CALLBACK(toggle_sensitivity), NULL);
	 g_signal_connect(G_OBJECT(entry_find), "delete-text",
		G_CALLBACK(toggle_sensitivity), NULL);
	 if (string_find) {
		 gtk_editable_set_text(GTK_EDITABLE(entry_find), string_find);
		 gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
			GTK_RESPONSE_OK, TRUE);
	 }
#endif
#if SEARCH_HISTORY
	 gtk_entry_set_activates_default(GTK_ENTRY(entry_find), TRUE);
	 g_signal_connect_after(G_OBJECT(entry_find), "insert-text",
		G_CALLBACK(toggle_sensitivity), NULL);
	 g_signal_connect_after(G_OBJECT(entry_find), "delete-text",
		G_CALLBACK(toggle_sensitivity), NULL);
	 buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
	 if (gtk_text_buffer_get_selection_bounds (buffer, &start_iter, &end_iter)) {
		if (string_find != NULL)
		 g_free(string_find);
		string_find = gtk_text_buffer_get_text (buffer, &start_iter, &end_iter,
			FALSE);
		gtk_editable_set_text(GTK_EDITABLE(entry_find), string_find);
		gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
			GTK_RESPONSE_OK, TRUE);
	 }
	 else
		gtk_editable_set_text(GTK_EDITABLE(entry_find), "");
#endif
	if (mode) {
		label_replace = gtk_label_new_with_mnemonic(_("Re_place with:"));
		 gtk_widget_set_halign(label_replace, GTK_ALIGN_START);
		 gtk_grid_attach(GTK_GRID(grid), label_replace, 0, 1, 1, 1);
#if SEARCH_HISTORY
		combo_replace = create_combo_with_history (&replace_history);
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
	}
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
	gtk_entry_set_activates_default(GTK_ENTRY(entry_find), TRUE);
	if (mode)
#endif
		gtk_entry_set_activates_default(GTK_ENTRY(entry_replace), TRUE);

#if !SEARCH_HISTORY
	check_case = gtk_check_button_new_with_mnemonic(_("_Match case"));
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_case), match_case);
	 g_signal_connect(check_case, "toggled", G_CALLBACK(toggle_check_case), NULL);
	 gtk_grid_attach(GTK_GRID(grid), check_case, 0, 1 + mode, 2, 1);
	if (mode) {
#endif
	check_all = gtk_check_button_new_with_mnemonic(_("Replace _all at once"));
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_all), replace_all);
	 g_signal_connect(check_all, "toggled", G_CALLBACK(toggle_check_all), NULL);
	 gtk_grid_attach(GTK_GRID(grid), check_all, 0, 2 + mode, 2, 1);
	}
#if SEARCH_HISTORY
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
	toggle_sensitivity (entry_find);
	check_case = gtk_check_button_new_with_mnemonic(_("_Match case"));
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_case), match_case);
	 g_signal_connect(check_case, "toggled", G_CALLBACK(toggle_check_case), NULL);
	 gtk_grid_attach(GTK_GRID(grid), check_case, 0, 1 + mode, 2, 1);
#endif
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);

	/* TODO: convert to async dialog API in a future cleanup pass */
	res = run_dialog_sync(GTK_DIALOG(dialog));
	if (res == GTK_RESPONSE_OK) {
#if SEARCH_HISTORY
		update_combo_data (entry_find, &find_history);
		if (string_find != NULL)
#endif
		g_free(string_find);
		string_find = g_strdup(gtk_editable_get_text(GTK_EDITABLE(entry_find)));
		if (mode) {
#if SEARCH_HISTORY
			update_combo_data (entry_replace, &replace_history);
			if (string_replace != NULL)
#endif
			g_free(string_replace);
			string_replace = g_strdup(gtk_editable_get_text(GTK_EDITABLE(entry_replace)));
		}
	}

	gtk_window_destroy(GTK_WINDOW(dialog));

	if (res == GTK_RESPONSE_OK) {
		if (strlen(string_find)) {
			if (mode)
				document_replace_real(textview);
			else
				document_search_real(textview, 0);
		}
	}

	return res;
}

void run_dialog_jump_to(GtkWidget *textview)
{
	GtkWidget *dialog;
	GtkWidget *button;
	GtkWidget *grid;
	GtkWidget *label;
	GtkWidget *spinner;
	GtkAdjustment *spinner_adj;
	GtkTextIter iter;
	gint num, max_num;

	GtkTextBuffer *textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));

	gtk_text_buffer_get_iter_at_mark(textbuffer, &iter,
		gtk_text_buffer_get_insert(textbuffer));
	num = gtk_text_iter_get_line(&iter) + 1;
	gtk_text_buffer_get_end_iter(textbuffer, &iter);
	max_num = gtk_text_iter_get_line(&iter) + 1;

	dialog = gtk_dialog_new_with_buttons(_("Jump To"),
		GTK_WINDOW(gtk_widget_get_root(textview)),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		NULL);
	gtk_widget_set_margin_start(dialog, 4);
	gtk_widget_set_margin_end(dialog, 4);
	gtk_widget_set_margin_top(dialog, 4);
	gtk_widget_set_margin_bottom(dialog, 4);
	button = create_button_with_stock_image(_("_Jump"), "go-jump");
	gtk_dialog_add_action_widget(GTK_DIALOG(dialog), button, GTK_RESPONSE_OK);
	grid = gtk_grid_new();
	 gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	 gtk_widget_set_margin_start(grid, 8);
	 gtk_widget_set_margin_end(grid, 8);
	 gtk_widget_set_margin_top(grid, 8);
	 gtk_widget_set_margin_bottom(grid, 8);
	 gtk_box_append(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), grid);
	label = gtk_label_new_with_mnemonic(_("_Line number:"));
	spinner_adj = gtk_adjustment_new(num, 1, max_num, 1, 1, 0);
	spinner = gtk_spin_button_new(spinner_adj, 1, 0);
	 gtk_editable_set_width_chars(GTK_EDITABLE(spinner), 8);
	 gtk_label_set_mnemonic_widget(GTK_LABEL(label), spinner);
	 gtk_entry_set_activates_default(GTK_ENTRY(spinner), TRUE);
	gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), spinner, 1, 0, 1, 1);

	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);

	/* TODO: convert to async dialog API in a future cleanup pass */
	if (run_dialog_sync(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
		gtk_text_buffer_get_iter_at_line(textbuffer, &iter,
			gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner)) - 1);
		gtk_text_buffer_place_cursor(textbuffer, &iter);
//		gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(textview), &iter, 0.1, FALSE, 0.5, 0.5);
		scroll_to_cursor(textbuffer, 0.25);
	}

	gtk_window_destroy(GTK_WINDOW(dialog));
}
