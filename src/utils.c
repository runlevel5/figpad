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
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#define STDIN_DELAY_MICROSECONDS 100000
#define GEDIT_STDIN_BUFSIZE 1024

// imported from gedit
#ifdef G_OS_UNIX
gchar *gedit_utils_get_stdin (void)
{
	GString * file_contents;
	gchar *tmp_buf = NULL;
	guint buffer_length;
	fd_set rfds;
	struct timeval tv;

	FD_ZERO (&rfds);
	FD_SET (0, &rfds);

	// wait for 1/4 of a second
	tv.tv_sec = 0;
	tv.tv_usec = STDIN_DELAY_MICROSECONDS;

	if (select (1, &rfds, NULL, NULL, &tv) != 1)
		return NULL;

	tmp_buf = g_new0 (gchar, GEDIT_STDIN_BUFSIZE + 1);
	g_return_val_if_fail (tmp_buf != NULL, NULL);

	file_contents = g_string_new (NULL);

	while (feof (stdin) == 0)
	{
		buffer_length = fread (tmp_buf, 1, GEDIT_STDIN_BUFSIZE, stdin);
		tmp_buf [buffer_length] = '\0';
		g_string_append (file_contents, tmp_buf);

		if (ferror (stdin) != 0)
		{
			g_free (tmp_buf);
			g_string_free (file_contents, TRUE);
			return NULL;
		}
	}

	fclose (stdin);

	return g_string_free (file_contents, FALSE);
}
#endif

GtkWidget *create_button_with_stock_image(const gchar *text, const gchar *stock_id)
{
	GtkWidget *button;
	GtkWidget *hbox;
	GtkWidget *image;
	GtkWidget *label;

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_widget_set_halign(hbox, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(hbox, GTK_ALIGN_CENTER);

	image = gtk_image_new_from_icon_name(stock_id);
	gtk_box_append(GTK_BOX(hbox), image);

	label = gtk_label_new_with_mnemonic(text);
	gtk_box_append(GTK_BOX(hbox), label);

	button = gtk_button_new();
	gtk_button_set_child(GTK_BUTTON(button), hbox);

	return button;
}

#if SEARCH_HISTORY
void update_combo_data (GtkWidget *entry, GList **history)
{
	const gchar *text;
	GList *node;

	text = gtk_editable_get_text(GTK_EDITABLE(entry));
	if (*text == '\0')
		return;
	for (node = *history; node != NULL; node = g_list_next (node))
	{
		if (g_str_equal ((gchar *)node->data, text))
		{
			g_free (node->data);
			*history = g_list_delete_link (*history, node);
			break;
		}
	}
	*history = g_list_prepend (*history, g_strdup (text));
}

static void on_history_selected(GObject *dropdown, GParamSpec *pspec, gpointer user_data)
{
	(void)pspec;
	GtkDropDown *dd = GTK_DROP_DOWN(dropdown);
	GtkWidget *entry = GTK_WIDGET(user_data);
	guint pos = gtk_drop_down_get_selected(dd);

	if (pos == GTK_INVALID_LIST_POSITION)
		return;

	GtkStringObject *obj = GTK_STRING_OBJECT(
		g_list_model_get_item(gtk_drop_down_get_model(dd), pos));
	if (obj) {
		gtk_editable_set_text(GTK_EDITABLE(entry),
			gtk_string_object_get_string(obj));
		g_object_unref(obj);
	}
}

GtkWidget *create_combo_with_history (GList **history)
{
	GtkWidget *box;
	GtkWidget *entry;
	GtkWidget *dropdown;
	GtkStringList *slist;
	GList *node;

	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

	entry = gtk_entry_new();
	gtk_widget_set_hexpand(entry, TRUE);
	gtk_box_append(GTK_BOX(box), entry);

	slist = gtk_string_list_new(NULL);
	for (node = *history; node != NULL; node = g_list_next(node))
		gtk_string_list_append(slist, (const gchar *)node->data);

	dropdown = gtk_drop_down_new(G_LIST_MODEL(slist), NULL);
	gtk_drop_down_set_selected(GTK_DROP_DOWN(dropdown),
		GTK_INVALID_LIST_POSITION);
	gtk_box_append(GTK_BOX(box), dropdown);

	g_signal_connect(dropdown, "notify::selected",
		G_CALLBACK(on_history_selected), entry);

	/* Store the entry as widget data so callers can retrieve it */
	g_object_set_data(G_OBJECT(box), "entry", entry);

	return box;
}

GtkWidget *get_combo_entry(GtkWidget *combo_box)
{
	return GTK_WIDGET(g_object_get_data(G_OBJECT(combo_box), "entry"));
}

#endif
