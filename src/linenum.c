/*
 *  Figpad - GTK+ based simple text editor
 *  Copyright (C) 2004-2005 Tarot Osuji
 *  Copyright (C) 2012      Yoo, Taik-Yon <jaagar AT gmail DOT com>
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

#include <gtk/gtk.h>
#include "linenum.h"

#define	DV(x)

static gint min_number_window_width;
static gboolean line_number_visible = FALSE;
static GtkWidget *gutter_widget = NULL;
static GtkTextView *gutter_text_view = NULL;
#define	margin 5
#define	submargin 2

static gint calculate_min_number_window_width(GtkWidget *widget)
{
	PangoLayout *layout;
	gchar *str;
	gint width, col = 2;

	str = g_strnfill(col, 0x20);
	layout = gtk_widget_create_pango_layout(widget, str);
	g_free (str);

	pango_layout_get_pixel_size(layout, &width, NULL);
	g_object_unref(G_OBJECT(layout));

	return width;
}

/* taken from gedit and gtksourceview */
/* originated from gtk+/tests/testtext.c */

static void
get_lines (GtkTextView  *text_view,
           gint          y1,
           gint          y2,
           GArray       *buffer_coords,
           GArray       *numbers,
           gint         *countp)
{
	GtkTextIter iter;
	gint count;
		gint last_line_num;

	g_array_set_size (buffer_coords, 0);
	g_array_set_size (numbers, 0);

	/* Get iter at first y */
	gtk_text_view_get_line_at_y (text_view, &iter, y1, NULL);

	/* For each iter, get its location and add it to the arrays.
	 * Stop when we pass y2
	 */
	count = 0;

	while (!gtk_text_iter_is_end (&iter))
	{
		gint y, height;

		gtk_text_view_get_line_yrange (text_view, &iter, &y, &height);

		g_array_append_val (buffer_coords, y);
		last_line_num = gtk_text_iter_get_line (&iter);
		g_array_append_val (numbers, last_line_num);

		++count;

		if ((y + height) >= y2)
		break;

		gtk_text_iter_forward_line (&iter);
	}

	if (gtk_text_iter_is_end (&iter))
	{
		gint y, height;
		gint line_num;

		gtk_text_view_get_line_yrange (text_view, &iter, &y, &height);

		line_num = gtk_text_iter_get_line (&iter);

		if (line_num != last_line_num) {
			g_array_append_val (buffer_coords, y);
			g_array_append_val (numbers, line_num);
			++count;
		}
	}

	*countp = count;
}

static inline PangoAttribute *
line_numbers_foreground_attr_new(GtkWidget *widget)
{
	GdkRGBA          rgb;

	gtk_widget_get_color(widget, &rgb);

	return pango_attr_foreground_new((guint16)(rgb.red   * 65535),
									 (guint16)(rgb.green * 65535),
									 (guint16)(rgb.blue  * 65535));
}

/*
 * GTK4 gutter draw function.
 *
 * In GTK3, line numbers were drawn via the text view's "draw" signal
 * into the left border window.  In GTK4, border windows are gone;
 * instead we install a GtkDrawingArea as the left gutter widget via
 * gtk_text_view_set_gutter().  The gutter widget's draw function
 * receives a cairo context already clipped and translated to the
 * gutter area, so coordinates start at (0,0) in the gutter.
 *
 * The gutter widget is automatically scrolled in sync with the text
 * view by GTK, so y coordinates in the gutter correspond directly to
 * the text view's window coordinates for GTK_TEXT_WINDOW_LEFT.
 */
static void
line_numbers_draw (GtkDrawingArea *area,
                   cairo_t        *cr,
                   int             width,
                   int             height,
                   gpointer        user_data)
{
	GtkTextView *text_view = gutter_text_view;
	GtkWidget *tv_widget;
	PangoLayout *layout;
	PangoAttrList *alist;
	PangoAttribute *attr;
	GArray *numbers;
	GArray *pixels;
	gint y1, y2;
	gint count;
	gint layout_width;
	gint justify_width = 0;
	gint i;
	gchar str [8];  /* we don't expect more than ten million lines */
	gint content_width;

	(void)user_data;

	if (!line_number_visible || text_view == NULL)
		return;

	tv_widget = GTK_WIDGET(text_view);

	/* The gutter is scrolled in sync with the text view by GTK4.
	 * y=0 in the gutter corresponds to the top of the visible area.
	 * Convert to buffer coordinates for get_lines(). */
	gtk_text_view_window_to_buffer_coords (text_view,
	                                       GTK_TEXT_WINDOW_LEFT,
	                                       0, 0,
	                                       NULL, &y1);
	gtk_text_view_window_to_buffer_coords (text_view,
	                                       GTK_TEXT_WINDOW_LEFT,
	                                       0, height,
	                                       NULL, &y2);

	numbers = g_array_new (FALSE, FALSE, sizeof (gint));
	pixels = g_array_new (FALSE, FALSE, sizeof (gint));

	get_lines (text_view,
	           y1,
	           y2,
	           pixels,
	           numbers,
	           &count);

	/* a zero-lined document should display a "1"; we don't need to worry about
	scrolling effects of the text widget in this special case */

	if (count == 0) {
		gint y = 0;
		gint n = 0;
		count = 1;
		g_array_append_val (pixels, y);
		g_array_append_val (numbers, n);
	}

DV({g_print("Painting line numbers %d - %d\n",
			g_array_index(numbers, gint, 0),
			g_array_index(numbers, gint, count - 1));	});

	layout = gtk_widget_create_pango_layout (tv_widget, "");

	g_snprintf (str, sizeof (str),
			"%d", MAX (99, gtk_text_buffer_get_line_count(gtk_text_view_get_buffer(text_view))));
	pango_layout_set_text (layout, str, -1);

	pango_layout_get_pixel_size (layout, &layout_width, NULL);

	min_number_window_width = calculate_min_number_window_width(tv_widget);
	if (layout_width > min_number_window_width) {
		content_width = layout_width + margin + submargin;
	} else {
		content_width = min_number_window_width + margin + submargin;
		justify_width = min_number_window_width - layout_width;
	}

	/* Update the gutter width if needed */
	gtk_drawing_area_set_content_width(area, content_width);

	pango_layout_set_width (layout, layout_width);
	pango_layout_set_alignment (layout, PANGO_ALIGN_RIGHT);

	alist = pango_attr_list_new();
/* TODO: should change line number color by conffile */
	attr = line_numbers_foreground_attr_new(tv_widget);
	attr->start_index = 0;
	attr->end_index = G_MAXUINT;
	pango_attr_list_insert(alist, attr);
	pango_layout_set_attributes(layout, alist);
	pango_attr_list_unref(alist);

	/* Draw fully internationalized numbers! */

	for (i = 0; i < count; i++) {
		gint pos;

		gtk_text_view_buffer_to_window_coords (text_view,
		                                       GTK_TEXT_WINDOW_LEFT,
		                                       0,
		                                       g_array_index (pixels, gint, i),
		                                       NULL,
		                                       &pos);
		g_snprintf (str, sizeof (str),
				"%d", g_array_index (numbers, gint, i) + 1);

		pango_layout_set_text (layout, str, -1);

		cairo_move_to(cr,
		              layout_width + justify_width + margin / 2 + 1,
		              pos);
		pango_cairo_show_layout(cr, layout);
	}

	g_array_free (pixels, TRUE);
	g_array_free (numbers, TRUE);
	g_object_unref (G_OBJECT (layout));
}

static void
on_text_changed_or_scrolled(gpointer data)
{
	if (gutter_widget != NULL && line_number_visible)
		gtk_widget_queue_draw(gutter_widget);
}

static void
on_buffer_changed(GtkTextBuffer *buffer, gpointer data)
{
	(void)buffer;
	on_text_changed_or_scrolled(data);
}

static void
on_vadjustment_changed(GtkAdjustment *adj, gpointer data)
{
	(void)adj;
	on_text_changed_or_scrolled(data);
}

void show_line_numbers(GtkWidget *text_view, gboolean visible)
{
	line_number_visible = visible;
	if (gutter_widget == NULL)
		return;

	if (visible) {
		gtk_widget_set_visible(gutter_widget, TRUE);
		gtk_widget_queue_draw(gutter_widget);
	} else {
		gtk_widget_set_visible(gutter_widget, FALSE);
	}
}

static gulong vadj_signal_id = 0;
static GtkAdjustment *connected_vadj = NULL;

static void
on_vadj_weak_notify(gpointer data, GObject *where_the_object_was)
{
	(void)data;
	(void)where_the_object_was;
	connected_vadj = NULL;
	vadj_signal_id = 0;
}

static void
connect_vadjustment(GtkTextView *text_view)
{
	GtkAdjustment *vadj;

	/* Disconnect from old adjustment if any */
	if (connected_vadj != NULL && vadj_signal_id != 0) {
		g_object_weak_unref(G_OBJECT(connected_vadj),
			on_vadj_weak_notify, NULL);
		g_signal_handler_disconnect(connected_vadj, vadj_signal_id);
		vadj_signal_id = 0;
		connected_vadj = NULL;
	}

	vadj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(text_view));
	if (vadj != NULL) {
		vadj_signal_id = g_signal_connect(vadj, "value-changed",
			G_CALLBACK(on_vadjustment_changed), NULL);
		connected_vadj = vadj;
		g_object_weak_ref(G_OBJECT(connected_vadj),
			on_vadj_weak_notify, NULL);
	}
}

static void
on_notify_vadjustment(GObject *object, GParamSpec *pspec, gpointer data)
{
	(void)pspec;
	(void)data;
	connect_vadjustment(GTK_TEXT_VIEW(object));
}

void linenum_init(GtkWidget *text_view)
{
	gutter_text_view = GTK_TEXT_VIEW(text_view);
	min_number_window_width = calculate_min_number_window_width(text_view);

	/* Create a GtkDrawingArea to serve as the left gutter */
	gutter_widget = gtk_drawing_area_new();
	gtk_drawing_area_set_content_width(
		GTK_DRAWING_AREA(gutter_widget),
		min_number_window_width + margin + submargin);
	gtk_drawing_area_set_draw_func(
		GTK_DRAWING_AREA(gutter_widget),
		line_numbers_draw,
		NULL, NULL);

	gtk_text_view_set_gutter(
		GTK_TEXT_VIEW(text_view),
		GTK_TEXT_WINDOW_LEFT,
		gutter_widget);

	/* Redraw line numbers when the buffer content changes */
	g_signal_connect(
		gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view)),
		"changed",
		G_CALLBACK(on_buffer_changed),
		NULL);

	/* Redraw line numbers when the view scrolls.
	 * The vadjustment may be replaced when the text view is later
	 * added to a GtkScrolledWindow, so we listen for changes to it
	 * and reconnect our signal handler accordingly. */
	connect_vadjustment(GTK_TEXT_VIEW(text_view));
	g_signal_connect(text_view, "notify::vadjustment",
		G_CALLBACK(on_notify_vadjustment), NULL);

	show_line_numbers(text_view, FALSE);
}
