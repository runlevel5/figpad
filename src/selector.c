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

#include "figpad.h"
#include <string.h>

#define DEFAULT_ITEM_NUM 2

static gint mode;
static gchar *other_codeset_title = NULL;
static gchar *lineend_str[] = {
	"LF",
	"CR+LF",
	"CR"
};

static void cb_select_lineend(GtkDropDown *dropdown, GParamSpec *pspec, FileInfo *selected_fi)
{
	(void)pspec;
	switch (gtk_drop_down_get_selected(dropdown)) {
	case 1:
		selected_fi->lineend = CR+LF;
		break;
	case 2:
		selected_fi->lineend = CR;
		break;
	default:
		selected_fi->lineend = LF;
	}
}

static GtkWidget *create_lineend_menu(FileInfo *selected_fi)
{
	const char *items[] = { lineend_str[0], lineend_str[1], lineend_str[2], NULL };
	GtkWidget *dropdown = gtk_drop_down_new_from_strings(items);
	guint idx = 0;

	switch (selected_fi->lineend) {
	case CR+LF:
		idx = 1;
		break;
	case CR:
		idx = 2;
	}
	gtk_drop_down_set_selected(GTK_DROP_DOWN(dropdown), idx);

	g_signal_connect(dropdown, "notify::selected",
		G_CALLBACK(cb_select_lineend), selected_fi);

	return dropdown;
}

typedef struct {
	const gchar *charset[ENCODING_MAX_ITEM_NUM + DEFAULT_ITEM_NUM];
	const gchar *str[ENCODING_MAX_ITEM_NUM + DEFAULT_ITEM_NUM];
	guint num;
} CharsetTable;

static CharsetTable *get_charset_table(void)
{
	static CharsetTable *ctable = NULL;
	EncArray *encarray;
	gint i;

	if (!ctable) {
		ctable = g_malloc(sizeof(CharsetTable));
		ctable->num = 0;
		ctable->charset[ctable->num] = get_default_charset();
		ctable->str[ctable->num] = g_strdup_printf(_("Current Locale (%s)"), get_default_charset());
		ctable->num++;
		ctable->charset[ctable->num] = "UTF-8";
		ctable->str[ctable->num] = ctable->charset[ctable->num];
		ctable->num++;
		encarray = get_encoding_items(get_encoding_code());
		for (i = 0; i < ENCODING_MAX_ITEM_NUM; i++)
			if (encarray->item[i]) {
				ctable->charset[ctable->num] = encarray->item[i];
				ctable->str[ctable->num] = encarray->item[i];
				ctable->num++;
			}
	}

	return ctable;
}

static GtkWidget *charset_dialog_ok_button;

static void toggle_sensitivity(GtkWidget *entry)
{
	gtk_widget_set_sensitive(charset_dialog_ok_button,
		strlen(gtk_editable_get_text(GTK_EDITABLE(entry))) ? TRUE : FALSE);
}

/* Build the label for the "Other Codeset" combo row.
 * Replaces the old GtkMenuItem-based approach (removed in GTK4). */
static gchar *manual_charset_label = NULL;
static gchar *get_manual_charset_label(gchar *manual_charset)
{
	if (other_codeset_title == NULL)
		other_codeset_title = _("Other Codeset");

	g_free(manual_charset_label);
	manual_charset_label = manual_charset
		? g_strdup_printf("%s (%s)", other_codeset_title, manual_charset)
		: g_strdup_printf("%s...", other_codeset_title);

	return manual_charset_label;
}

/* Update the "Other Codeset" row label in the GtkStringList model */
static void update_manual_charset_row(GtkDropDown *dropdown, gchar *manual_charset)
{
	GtkStringList *slist = GTK_STRING_LIST(gtk_drop_down_get_model(dropdown));
	guint n = g_list_model_get_n_items(G_LIST_MODEL(slist));

	/* The last row is always the "Other Codeset" entry */
	if (n > 0) {
		const char *new_label = get_manual_charset_label(manual_charset);
		gtk_string_list_splice(slist, n - 1, 1, (const char * const[]){ new_label, NULL });
	}
}

typedef struct {
	GMainLoop *loop;
	gboolean accepted;
} ManualCharsetData;

static void on_manual_charset_accept(GtkWidget *widget, gpointer user_data)
{
	(void)widget;
	ManualCharsetData *data = user_data;
	data->accepted = TRUE;
	g_main_loop_quit(data->loop);
}

static gboolean get_manual_charset(GtkDropDown *dropdown, FileInfo *selected_fi)
{
	GtkWidget *dialog;
	GtkWidget *content_box;
	GtkWidget *vbox;
	GtkWidget *button_box;
	GtkWidget *cancel_button;
	GtkWidget *ok_button;
	GtkWidget *label;
	GtkWidget *entry;
	GError *err = NULL;
	gchar *str;
	ManualCharsetData data;

	/* Plain GtkWindow instead of deprecated GtkDialog */
	dialog = gtk_window_new();
	gtk_window_set_title(GTK_WINDOW(dialog), other_codeset_title);
	gtk_window_set_transient_for(GTK_WINDOW(dialog),
		GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(dropdown))));
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);

	content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_margin_start(content_box, 12);
	gtk_widget_set_margin_end(content_box, 12);
	gtk_widget_set_margin_top(content_box, 12);
	gtk_widget_set_margin_bottom(content_box, 12);
	gtk_window_set_child(GTK_WINDOW(dialog), content_box);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_append(GTK_BOX(content_box), vbox);

	label = gtk_label_new_with_mnemonic(_("Code_set:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_widget_set_valign(label, GTK_ALIGN_START);
	gtk_widget_set_margin_top(label, 5);
	gtk_widget_set_margin_bottom(label, 5);
	gtk_box_append(GTK_BOX(vbox), label);

	entry = gtk_entry_new();
	gtk_widget_set_hexpand(entry, TRUE);
	gtk_widget_set_vexpand(entry, TRUE);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_widget_set_margin_top(entry, 5);
	gtk_widget_set_margin_bottom(entry, 5);
	gtk_box_append(GTK_BOX(vbox), entry);

	/* Action buttons */
	button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_set_halign(button_box, GTK_ALIGN_END);
	gtk_box_append(GTK_BOX(content_box), button_box);

	cancel_button = gtk_button_new_with_mnemonic(_("_Cancel"));
	gtk_box_append(GTK_BOX(button_box), cancel_button);

	ok_button = gtk_button_new_with_mnemonic(_("_OK"));
	gtk_widget_add_css_class(ok_button, "suggested-action");
	gtk_box_append(GTK_BOX(button_box), ok_button);
	charset_dialog_ok_button = ok_button;

	gtk_widget_set_sensitive(ok_button, FALSE);
	g_signal_connect_after(G_OBJECT(entry), "changed",
		G_CALLBACK(toggle_sensitivity), NULL);
	if (selected_fi->charset_flag)
		gtk_editable_set_text(GTK_EDITABLE(entry), selected_fi->charset);

	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);

	/* Sync-loop plumbing */
	data.loop = g_main_loop_new(NULL, FALSE);
	data.accepted = FALSE;

	g_signal_connect_swapped(cancel_button, "clicked",
		G_CALLBACK(g_main_loop_quit), data.loop);
	g_signal_connect_swapped(dialog, "close-request",
		G_CALLBACK(g_main_loop_quit), data.loop);
	g_signal_connect(ok_button, "clicked",
		G_CALLBACK(on_manual_charset_accept), &data);

	gtk_window_set_default_widget(GTK_WINDOW(dialog), ok_button);
	gtk_window_present(GTK_WINDOW(dialog));
	g_main_loop_run(data.loop);
	g_main_loop_unref(data.loop);
	charset_dialog_ok_button = NULL;

	if (data.accepted) {
		g_convert("TEST", -1, "UTF-8", gtk_editable_get_text(GTK_EDITABLE(entry)), NULL, NULL, &err);
		if (err) {
			g_error_free(err);
			gtk_widget_set_visible(dialog, FALSE);
			str = g_strdup_printf(_("'%s' is not supported"), gtk_editable_get_text(GTK_EDITABLE(entry)));
			run_dialog_message(GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(dropdown))),
				GTK_MESSAGE_ERROR, str);
			g_free(str);
		} else {
			g_free(selected_fi->charset);
			selected_fi->charset = g_strdup(gtk_editable_get_text(GTK_EDITABLE(entry)));
			selected_fi->charset_flag = TRUE;
			gtk_window_destroy(GTK_WINDOW(dialog));

			update_manual_charset_row(dropdown,
				selected_fi->charset_flag ? selected_fi->charset : NULL);

			return TRUE;
		}
	}
	gtk_window_destroy(GTK_WINDOW(dialog));

	return FALSE;
}

gboolean charset_menu_init_flag;

static void cb_select_charset(GtkDropDown *dropdown, GParamSpec *pspec, FileInfo *selected_fi)
{
	CharsetTable *ctable;
	static guint index_history = 0, prev_history;

	(void)pspec;
	prev_history = index_history;
	index_history = gtk_drop_down_get_selected(dropdown);
	if (!charset_menu_init_flag) {
		ctable = get_charset_table();
		if (index_history < ctable->num + mode) {
			if (selected_fi->charset)
				g_free(selected_fi->charset);
			if (index_history == 0 && mode == OPEN)
				selected_fi->charset = NULL;
			else {
				selected_fi->charset =
					g_strdup(ctable->charset[index_history - mode]);
			}
		} else
			if (!get_manual_charset(dropdown, selected_fi)) {
				index_history = prev_history;
				gtk_drop_down_set_selected(dropdown, index_history);
			}
	}
}

static GtkWidget *create_charset_menu(FileInfo *selected_fi)
{
	GtkStringList *slist = gtk_string_list_new(NULL);
	GtkWidget *dropdown;
	CharsetTable *ctable;
	guint i;

	if (mode == OPEN)
		gtk_string_list_append(slist, _("Auto-Detect"));
	ctable = get_charset_table();
	for (i = 0; i < ctable->num; i++)
		gtk_string_list_append(slist, ctable->str[i]);
	gtk_string_list_append(slist, get_manual_charset_label(
			selected_fi->charset_flag ? selected_fi->charset : NULL));

	dropdown = gtk_drop_down_new(G_LIST_MODEL(slist), NULL);

	charset_menu_init_flag = TRUE;
	g_signal_connect(dropdown, "notify::selected",
		G_CALLBACK(cb_select_charset), selected_fi);

	i = 0;
	if (selected_fi->charset) {
		do {
			if (g_ascii_strcasecmp(selected_fi->charset, ctable->charset[i]) == 0)
				break;
			i++;
		} while (i < ctable->num);
		if (mode == OPEN && selected_fi->charset_flag == FALSE) {
			g_free(selected_fi->charset);
			selected_fi->charset = NULL;
		} else if (i == ctable->num && selected_fi->charset_flag == FALSE) {
			update_manual_charset_row(GTK_DROP_DOWN(dropdown), selected_fi->charset);
		}
		i += mode;
	}
	if (mode == SAVE || selected_fi->charset_flag)
		gtk_drop_down_set_selected(GTK_DROP_DOWN(dropdown), i);
	charset_menu_init_flag = FALSE;

	return dropdown;
}

typedef struct {
	GMainLoop *loop;
	gboolean accepted;
	FileInfo *selected_fi;
	GtkWidget *dialog;
} FileSelectorData;

static void on_file_dialog_open_finish(GObject *source, GAsyncResult *result,
	gpointer user_data)
{
	FileSelectorData *data = user_data;
	GtkFileDialog *fd = GTK_FILE_DIALOG(source);
	GFile *gfile = gtk_file_dialog_open_finish(fd, result, NULL);

	if (gfile) {
		g_free(data->selected_fi->filename);
		data->selected_fi->filename = g_file_get_path(gfile);
		g_object_unref(gfile);
		data->accepted = TRUE;
		g_main_loop_quit(data->loop);
	}
	/* If cancelled, stay in the options dialog — user can retry or cancel */
}

static void on_file_dialog_save_finish(GObject *source, GAsyncResult *result,
	gpointer user_data)
{
	FileSelectorData *data = user_data;
	GtkFileDialog *fd = GTK_FILE_DIALOG(source);
	GFile *gfile = gtk_file_dialog_save_finish(fd, result, NULL);

	if (gfile) {
		g_free(data->selected_fi->filename);
		data->selected_fi->filename = g_file_get_path(gfile);
		g_object_unref(gfile);
		data->accepted = TRUE;
		g_main_loop_quit(data->loop);
	}
}

static void on_browse_clicked(GtkWidget *widget, gpointer user_data)
{
	(void)widget;
	FileSelectorData *data = user_data;
	GtkFileDialog *fd = gtk_file_dialog_new();

	gtk_file_dialog_set_title(fd,
		mode ? _("Open") : _("Save As"));
	gtk_file_dialog_set_modal(fd, TRUE);

	if (data->selected_fi->filename) {
		GFile *file = g_file_new_for_path(data->selected_fi->filename);
		if (mode == OPEN) {
			/* For open, set the initial folder to the file's parent */
			GFile *folder = g_file_get_parent(file);
			if (folder) {
				gtk_file_dialog_set_initial_folder(fd, folder);
				g_object_unref(folder);
			}
			gtk_file_dialog_set_initial_name(fd,
				g_file_get_basename(file));
		} else {
			/* For save, set initial file directly */
			GFile *folder = g_file_get_parent(file);
			if (folder) {
				gtk_file_dialog_set_initial_folder(fd, folder);
				g_object_unref(folder);
			}
			gtk_file_dialog_set_initial_name(fd,
				g_file_get_basename(file));
		}
		g_object_unref(file);
	}

	if (mode == OPEN) {
		gtk_file_dialog_open(fd,
			GTK_WINDOW(data->dialog), NULL,
			on_file_dialog_open_finish, data);
	} else {
		gtk_file_dialog_save(fd,
			GTK_WINDOW(data->dialog), NULL,
			on_file_dialog_save_finish, data);
	}
	g_object_unref(fd);
}

FileInfo *get_fileinfo_from_selector(FileInfo *fi, gint requested_mode)
{
	FileInfo *selected_fi;
	GtkWidget *dialog;
	GtkWidget *content_box;
	GtkWidget *grid;
	GtkWidget *label;
	GtkWidget *option_menu_charset;
	GtkWidget *option_menu_lineend;
	GtkWidget *button_box;
	GtkWidget *cancel_button;
	GtkWidget *browse_button;
	FileSelectorData data;

	/* init values */
	mode = requested_mode;
	selected_fi = g_malloc(sizeof(FileInfo));
	selected_fi->filename =
		fi->filename ? g_strdup(fi->filename) : NULL;
	selected_fi->charset =
		fi->charset ? g_strdup(fi->charset) : NULL;
	selected_fi->charset_flag = fi->charset_flag;
	selected_fi->lineend = fi->lineend;

	/* Plain GtkWindow with charset/lineend options + Browse button */
	dialog = gtk_window_new();
	gtk_window_set_title(GTK_WINDOW(dialog),
		mode ? _("Open") : _("Save As"));
	gtk_window_set_transient_for(GTK_WINDOW(dialog),
		GTK_WINDOW(pub->mw->window));
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
	gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
	gtk_box_append(GTK_BOX(content_box), grid);

	option_menu_charset = create_charset_menu(selected_fi);
	label = gtk_label_new_with_mnemonic(_("C_haracter Coding:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), option_menu_charset);
	gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), option_menu_charset, 1, 0, 1, 1);

	if (mode == SAVE) {
		option_menu_lineend = create_lineend_menu(selected_fi);
		label = gtk_label_new_with_mnemonic(_("Line _Ending:"));
		gtk_widget_set_halign(label, GTK_ALIGN_START);
		gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 1, 1);
		gtk_grid_attach(GTK_GRID(grid), option_menu_lineend, 1, 1, 1, 1);
	}

	/* Action buttons */
	button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_set_halign(button_box, GTK_ALIGN_END);
	gtk_box_append(GTK_BOX(content_box), button_box);

	cancel_button = gtk_button_new_with_mnemonic(_("_Cancel"));
	gtk_box_append(GTK_BOX(button_box), cancel_button);

	browse_button = gtk_button_new_with_mnemonic(
		mode ? _("_Open...") : _("_Save As..."));
	gtk_widget_add_css_class(browse_button, "suggested-action");
	gtk_box_append(GTK_BOX(button_box), browse_button);

	/* Sync-loop plumbing */
	data.loop = g_main_loop_new(NULL, FALSE);
	data.accepted = FALSE;
	data.selected_fi = selected_fi;
	data.dialog = dialog;

	g_signal_connect_swapped(cancel_button, "clicked",
		G_CALLBACK(g_main_loop_quit), data.loop);
	g_signal_connect_swapped(dialog, "close-request",
		G_CALLBACK(g_main_loop_quit), data.loop);
	g_signal_connect(browse_button, "clicked",
		G_CALLBACK(on_browse_clicked), &data);

	gtk_window_set_default_widget(GTK_WINDOW(dialog), browse_button);
	gtk_window_present(GTK_WINDOW(dialog));
	g_main_loop_run(data.loop);
	g_main_loop_unref(data.loop);

	if (!data.accepted) {
		if (selected_fi->charset)
			g_free(selected_fi->charset);
		g_free(selected_fi);
		selected_fi = NULL;
	}

	gtk_window_destroy(GTK_WINDOW(dialog));

	return selected_fi;
}
