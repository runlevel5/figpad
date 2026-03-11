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

/* Manually tracked font description — GTK4 removed
 * gtk_widget_override_font() and gtk_style_context_get_font(). */
static PangoFontDescription *current_font_desc = NULL;
static GtkCssProvider *font_css_provider = NULL;

static void apply_font_css(GtkWidget *widget)
{
	gchar *css_str;
	gchar *font_str;

	if (!current_font_desc)
		return;

	if (!font_css_provider) {
		font_css_provider = gtk_css_provider_new();
		gtk_style_context_add_provider_for_display(
			gdk_display_get_default(),
			GTK_STYLE_PROVIDER(font_css_provider),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}

	font_str = pango_font_description_to_string(current_font_desc);
	css_str = g_strdup_printf("textview { font: %s; }", font_str);
	gtk_css_provider_load_from_string(font_css_provider, css_str);
	g_free(css_str);
	g_free(font_str);
}

void set_text_font_by_name(GtkWidget *widget, gchar *fontname)
{
	if (current_font_desc)
		pango_font_description_free(current_font_desc);
	current_font_desc = pango_font_description_from_string(fontname);
	apply_font_css(widget);
}

gchar *get_current_font_name(void)
{
	if (current_font_desc)
		return pango_font_description_to_string(current_font_desc);
	return g_strdup("Monospace 12");
}

static gchar *get_font_name_by_selector(GtkWidget *window, gchar *current_fontname)
{
	GtkWidget *dialog;
	gchar *fontname;

	dialog = gtk_font_chooser_dialog_new(_("Font"), NULL);
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
	gtk_font_chooser_set_font(GTK_FONT_CHOOSER(dialog), current_fontname);

	/* TODO: convert to async dialog API in a future cleanup pass */
	gint response = run_dialog_sync(GTK_DIALOG(dialog));

	if (response == GTK_RESPONSE_OK)
		fontname = gtk_font_chooser_get_font(GTK_FONT_CHOOSER(dialog));
	else
		fontname = NULL;

	gtk_window_destroy(GTK_WINDOW(dialog));
	return fontname;
}

void change_text_font_by_selector(GtkWidget *widget)
{
	gchar *current_fontname, *fontname;

	current_fontname = get_current_font_name();
	fontname = get_font_name_by_selector(
		GTK_WIDGET(gtk_widget_get_root(widget)), current_fontname);
	if (fontname) {
		set_text_font_by_name(widget, fontname);
		indent_refresh_tab_width(widget);
	}

	g_free(fontname);
	g_free(current_fontname);
}
