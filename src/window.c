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

static gboolean on_close_request(GtkWindow *window, gpointer data)
{
	(void)window; (void)data;
	on_file_quit();
	/* always return TRUE to prevent default destroy — on_file_quit()
	 * calls g_application_quit() if the user confirms */
	return TRUE;
}

MainWin *create_main_window(GtkApplication *app)
{
	GtkWidget *vbox;
	GtkWidget *sw;

	MainWin *mw = g_malloc(sizeof(MainWin));

	mw->window = gtk_application_window_new(app);
	gtk_widget_set_name(mw->window, PACKAGE_NAME);

	/* GTK4 removed gtk_window_set_icon_from_file(); use themed icon via
	 * gtk_window_set_default_icon_name() set in main.c instead */

	g_signal_connect(mw->window, "close-request",
		G_CALLBACK(on_close_request), NULL);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_window_set_child(GTK_WINDOW(mw->window), vbox);

	mw->menubar = create_menu_bar(GTK_WINDOW(mw->window), app);
	gtk_box_append(GTK_BOX(vbox), mw->menubar);

	sw = gtk_scrolled_window_new();
	gtk_widget_set_hexpand(sw, TRUE);
	gtk_widget_set_vexpand(sw, TRUE);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	/* GTK4 removed gtk_scrolled_window_set_shadow_type() */
	gtk_box_append(GTK_BOX(vbox), sw);

	mw->view = create_text_view();
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), mw->view);
	mw->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(mw->view));

	return mw;
}

void set_main_window_title(void)
{
	gchar *title;

	title = get_file_basename(pub->fi->filename, TRUE);
	gtk_window_set_title(GTK_WINDOW(pub->mw->window), title);
	g_free(title);
}
