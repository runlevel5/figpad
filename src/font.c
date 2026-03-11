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
	const gchar *family;
	gdouble size_pt;
	PangoWeight weight;
	PangoStyle style;

	if (!current_font_desc)
		return;

	if (!font_css_provider) {
		font_css_provider = gtk_css_provider_new();
		gtk_style_context_add_provider_for_display(
			gdk_display_get_default(),
			GTK_STYLE_PROVIDER(font_css_provider),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}

	family = pango_font_description_get_family(current_font_desc);
	if (!family)
		family = "Monospace";

	/* Pango sizes are in Pango units (1/1024 of a point) when
	 * not absolute.  Convert to CSS pt. */
	size_pt = pango_font_description_get_size(current_font_desc)
		/ (gdouble)PANGO_SCALE;

	if (size_pt <= 0)
		size_pt = 12.0;

	weight = pango_font_description_get_weight(current_font_desc);
	style = pango_font_description_get_style(current_font_desc);

	css_str = g_strdup_printf(
		"textview { font-family: \"%s\"; font-size: %.1fpt;"
		" font-weight: %d; font-style: %s; }",
		family, size_pt, (int)weight,
		style == PANGO_STYLE_ITALIC ? "italic" :
		style == PANGO_STYLE_OBLIQUE ? "oblique" : "normal");
	gtk_css_provider_load_from_string(font_css_provider, css_str);
	g_free(css_str);
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

static void on_font_chosen(GObject *source, GAsyncResult *result, gpointer user_data)
{
	GtkFontDialog *fd = GTK_FONT_DIALOG(source);
	GtkWidget *widget = user_data;
	GError *error = NULL;
	PangoFontDescription *desc;

	desc = gtk_font_dialog_choose_font_finish(fd, result, &error);
	if (desc) {
		gchar *fontname = pango_font_description_to_string(desc);
		set_text_font_by_name(widget, fontname);
		indent_refresh_tab_width(widget);
		g_free(fontname);
		pango_font_description_free(desc);
	}
	if (error)
		g_error_free(error);
}

void change_text_font_by_selector(GtkWidget *widget)
{
	GtkFontDialog *fd;
	PangoFontDescription *initial_desc;

	fd = gtk_font_dialog_new();
	gtk_font_dialog_set_title(fd, _("Font"));

	initial_desc = current_font_desc
		? pango_font_description_copy(current_font_desc)
		: pango_font_description_from_string("Monospace 12");

	gtk_font_dialog_choose_font(fd,
		GTK_WINDOW(gtk_widget_get_root(widget)),
		initial_desc, NULL, on_font_chosen, widget);

	pango_font_description_free(initial_desc);
	g_object_unref(fd);
}
