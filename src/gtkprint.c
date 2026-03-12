/*
 *  Figpad - GTK+ based simple text editor
 *  Copyright (C) 2004-2007 Tarot Osuji
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

/* Per-print-job state, threaded through callback user_data. */
typedef struct {
	PangoLayout          *layout;
	PangoFontDescription *font_desc;
	gint                  line_count;
	gint                  lines_per_page;
	gint                  text_height;
	gint                  n_pages;
	gdouble               page_width;
	gdouble               page_height;
	const gchar          *page_title;
	GtkTextView          *text_view;   /* kept for begin-print */
} PrintJobState;

static void get_tab_array(PangoTabArray **tabs,
	GtkPrintContext *ctx, GtkTextView *text_view)
{
	gint xft_dpi, loc;
	GtkSettings *settings = gtk_settings_get_default();

	g_object_get(settings, "gtk-xft-dpi", &xft_dpi, NULL);
	if ((*tabs = gtk_text_view_get_tabs(text_view))) {
		pango_tab_array_get_tab(*tabs, 0, NULL, &loc);
		pango_tab_array_set_tab(*tabs, 0, PANGO_TAB_LEFT,
			loc * gtk_print_context_get_dpi_x(ctx) / (xft_dpi / PANGO_SCALE));
	}
}

static void cb_begin_print(GtkPrintOperation *op,
		GtkPrintContext *ctx, gpointer data)
{
	PrintJobState *ps = data;
	gint layout_height;
	gchar *text;
	GtkTextIter start, end;
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(ps->text_view);
	PangoTabArray *tabs;

	gtk_text_buffer_get_bounds(buffer, &start, &end);
	text = g_strchomp(gtk_text_buffer_get_text(buffer, &start, &end, FALSE));

	ps->page_width = gtk_print_context_get_width(ctx);
	ps->page_height = gtk_print_context_get_height(ctx);

	gchar *font_name = get_current_font_name();
	ps->font_desc = pango_font_description_from_string(font_name);
	g_free(font_name);
	ps->layout = gtk_print_context_create_pango_layout(ctx);
	pango_layout_set_width(ps->layout, ps->page_width * PANGO_SCALE);
	pango_layout_set_font_description(ps->layout, ps->font_desc);
	pango_layout_set_text(ps->layout, text, -1);

	get_tab_array(&tabs, ctx, ps->text_view);
	if (tabs) {
		pango_layout_set_tabs(ps->layout, tabs);
		pango_tab_array_free(tabs);
	}
	pango_layout_get_size(ps->layout, NULL, &layout_height);

	ps->line_count = pango_layout_get_line_count(ps->layout);
	ps->text_height = pango_font_description_get_size(ps->font_desc) / PANGO_SCALE;
	ps->lines_per_page = ps->page_height / ps->text_height;

	ps->n_pages = (ps->line_count - 1) / ps->lines_per_page + 1;
	gtk_print_operation_set_n_pages(op, ps->n_pages);

	g_free(text);
}

static void cb_draw_page(GtkPrintOperation *op,
		GtkPrintContext *ctx, gint page_nr, gpointer data)
{
	PrintJobState *ps = data;
	cairo_t *cr;
	PangoLayoutLine *line;
	gint n_line, i, j = 0;

	PangoLayout *layout_lh, *layout_rh;
	gchar *page_text;
	gint layout_width;

	cr = gtk_print_context_get_cairo_context(ctx);

	layout_lh = gtk_print_context_create_pango_layout(ctx);
	pango_layout_set_font_description(layout_lh, ps->font_desc);
	pango_layout_set_text(layout_lh, ps->page_title, -1);
	cairo_move_to(cr, 0, -PRINT_HEADER_OFFSET_PT);
	pango_cairo_show_layout(cr, layout_lh);

	page_text = g_strdup_printf("%d / %d", page_nr + 1, ps->n_pages);
	layout_rh = gtk_print_context_create_pango_layout(ctx);
	pango_layout_set_font_description(layout_rh, ps->font_desc);
	pango_layout_set_text(layout_rh, page_text, -1);
	pango_layout_get_size(layout_rh, &layout_width, NULL);
	cairo_move_to(cr,
		ps->page_width - layout_width / PANGO_SCALE, -PRINT_HEADER_OFFSET_PT);
	pango_cairo_show_layout(cr, layout_rh);
	g_free(page_text);

	if (ps->line_count > ps->lines_per_page * (page_nr + 1))
		n_line = ps->lines_per_page * (page_nr + 1);
	else
		n_line = ps->line_count;

	for (i = ps->lines_per_page * page_nr; i < n_line; i++) {
		line = pango_layout_get_line(ps->layout, i);
		cairo_move_to(cr, 0, ps->text_height * (j + 1));
		pango_cairo_show_layout_line(cr, line);
		j++;
	}
}

static void cb_end_print(GtkPrintOperation *op,
		GtkPrintContext *ctx, gpointer data)
{
	PrintJobState *ps = data;
	g_object_unref(ps->layout);
	if (ps->font_desc) {
		pango_font_description_free(ps->font_desc);
		ps->font_desc = NULL;
	}
}

static GtkPrintSettings *settings = NULL;

static GtkPrintOperation *create_print_operation(PrintJobState *ps)
{
	GtkPrintOperation *op;
	static GtkPageSetup *page_setup = NULL;

	op = gtk_print_operation_new();

	if (settings)
		gtk_print_operation_set_print_settings(op, settings);

	if (!page_setup) {
		page_setup = gtk_page_setup_new();
		gtk_page_setup_set_top_margin(page_setup, PRINT_MARGIN_TOP_MM, GTK_UNIT_MM);
		gtk_page_setup_set_bottom_margin(page_setup, PRINT_MARGIN_BOTTOM_MM, GTK_UNIT_MM);
		gtk_page_setup_set_left_margin(page_setup, PRINT_MARGIN_LEFT_MM, GTK_UNIT_MM);
		gtk_page_setup_set_right_margin(page_setup, PRINT_MARGIN_RIGHT_MM, GTK_UNIT_MM);
	}
	gtk_print_operation_set_default_page_setup(op, page_setup);

	g_signal_connect(op, "begin-print", G_CALLBACK(cb_begin_print), ps);
	g_signal_connect(op, "draw-page", G_CALLBACK(cb_draw_page), ps);
	g_signal_connect(op, "end-print", G_CALLBACK(cb_end_print), ps);

	return op;
}

static void create_error_dialog(GtkTextView *text_view, gchar *message)
{
	GtkAlertDialog *alert;

	alert = gtk_alert_dialog_new("%s", message);
	gtk_alert_dialog_set_buttons(alert, (const char *[]){ _("_OK"), NULL });
	gtk_alert_dialog_set_default_button(alert, 0);
	gtk_alert_dialog_set_cancel_button(alert, 0);
	gtk_alert_dialog_show(alert,
		GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(text_view))));
	g_object_unref(alert);
}

void create_gtkprint_session(GtkTextView *text_view, const gchar *title)
{
	GtkPrintOperation *op;
	GtkPrintOperationResult res;
	GError *err = NULL;

	PrintJobState *ps = g_new0(PrintJobState, 1);
	ps->page_title = title;
	ps->text_view = text_view;
	op = create_print_operation(ps);

	res = gtk_print_operation_run(op, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
		GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(text_view))), &err);
	switch (res) {
	case GTK_PRINT_OPERATION_RESULT_ERROR:
		create_error_dialog(text_view, err->message);
		g_error_free(err);
		break;
	case GTK_PRINT_OPERATION_RESULT_APPLY:
		if (settings)
			g_object_unref(settings);
		settings = g_object_ref(gtk_print_operation_get_print_settings(op));
		break;
	default:
		break;
	}

	g_object_unref(op);
	g_free(ps);
}