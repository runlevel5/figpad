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
#include <gdk/gdkkeysyms.h>
#include <string.h>

static gint keyval;
static gboolean view_scroll_flag = FALSE;

gint get_current_keyval(void)
{
	return keyval;
}

void clear_current_keyval(void)
{
	keyval = 0;
}
/*
gboolean scroll_to_cursor(GtkTextBuffer *buffer, gdouble within_margin)
{
	GtkTextIter iter;

//	gtk_text_buffer_get_start_iter(buffer, &iter);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter,
		gtk_text_buffer_get_insert(buffer));
	return gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(pub->mw->view),
		&iter, within_margin, FALSE, 0.5, 0.5);
}
*/
void scroll_to_cursor(GtkTextBuffer *buffer, gdouble within_margin)
{
	gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(pub->mw->view),
		gtk_text_buffer_get_insert(buffer),
		within_margin, FALSE, 0, 0);
}

gint check_text_modification(void)
{
	gchar *basename, *str;
	gint res;

	if (gtk_text_buffer_get_modified(pub->mw->buffer)) {
		basename = get_file_basename(pub->fi->filename, FALSE);
		str = g_strdup_printf(_("Save changes to '%s'?"), basename);
		g_free(basename);
		res = run_dialog_question_sync(pub->mw->window, str);
		g_free(str);
		switch (res) {
		case QUESTION_RESPONSE_NO:
			return 0;
		case QUESTION_RESPONSE_YES:
			if (!on_file_save())
				return 0;
		}
		return -1;
	}

	return 0;
}

#if 0
static gint check_preedit(GtkWidget *view)
{
	gint cursor_pos;

	gtk_im_context_get_preedit_string(
		GTK_TEXT_VIEW(view)->im_context, NULL, NULL, &cursor_pos);

	return cursor_pos;
}
#endif

static gboolean check_selection_bound(GtkTextBuffer *buffer)
{
	GtkTextIter start, end;
	gchar *str, *p;

	if (gtk_text_buffer_get_selection_bounds(buffer, &start, &end)) {
		str = gtk_text_iter_get_text(&start, &end);
		p = strchr(str, '\n');
		g_free(str);
		if (p)
			return TRUE;
	}

	return FALSE;
}

/*
 * GTK4: key-press-event is replaced by GtkEventControllerKey.
 * The callback signature changes from (GtkWidget*, GdkEventKey*) to
 * (GtkEventControllerKey*, guint keyval, guint keycode, GdkModifierType state).
 */
static gboolean cb_key_pressed(GtkEventControllerKey *controller,
                               guint kv, guint keycode,
                               GdkModifierType state,
                               gpointer user_data)
{
	GtkWidget *view = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller));
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter iter;
	GdkRectangle prev_rect;

	(void)keycode;
	(void)user_data;

#if 0
	if (check_preedit(view))
		return FALSE;
#endif

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
	gtk_text_view_get_iter_location(GTK_TEXT_VIEW(view), &iter, &prev_rect);

	keyval = 0;
//g_print("key-pressed: 0x%X\n", kv);
	switch (kv) {
	case GDK_KEY_Up:		// Try [Shift]+[Down]. it works bad.
	case GDK_KEY_Down:
		if (gtk_text_view_move_mark_onscreen(GTK_TEXT_VIEW(view), mark)) {
			GdkRectangle iter_rect;
			gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
			gtk_text_view_get_iter_location(GTK_TEXT_VIEW(view), &iter, &iter_rect);
			if (iter_rect.y < prev_rect.y) {
				gtk_text_view_get_line_at_y(GTK_TEXT_VIEW(view), &iter,
					iter_rect.y - iter_rect.height, NULL);
				gtk_text_buffer_move_mark(buffer, mark, &iter);
			}
			if (!(state & GDK_SHIFT_MASK)) {
				gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
				gtk_text_buffer_place_cursor(buffer, &iter);
			}
			return TRUE;
		}
		break;
	case GDK_KEY_Page_Up:
	case GDK_KEY_Page_Down:
		if (gtk_text_view_move_mark_onscreen(GTK_TEXT_VIEW(view), mark)) {
			GdkRectangle visible_rect, iter_rect;
			gint pos = 0;
			gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(view), &visible_rect);
			gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
			gtk_text_view_get_iter_location(GTK_TEXT_VIEW(view), &iter, &iter_rect);
			if (iter_rect.y < prev_rect.y)
				pos = 1;
			if (kv == GDK_KEY_Page_Up)
				gtk_text_view_get_line_at_y(GTK_TEXT_VIEW(view), &iter,
					iter_rect.y - visible_rect.height + iter_rect.height, NULL);
			else
				gtk_text_view_get_line_at_y(GTK_TEXT_VIEW(view), &iter,
					iter_rect.y + visible_rect.height - iter_rect.height, NULL);
			gtk_text_buffer_move_mark(buffer, mark, &iter);
			gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(view),
				mark, 0, TRUE, 0, pos);
			if (!(state & GDK_SHIFT_MASK)) {
				gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
				gtk_text_buffer_place_cursor(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)), &iter);
			}
			return TRUE;
		}
		break;
	case GDK_KEY_Return:
		if (indent_get_state()) {
			indent_real(view);
			return TRUE;
		}
		break;
	case GDK_KEY_Tab:
		if (state & GDK_CONTROL_MASK) {
			indent_toggle_tab_width(view);
			return TRUE;
		}
		/* fall through */
	case GDK_KEY_ISO_Left_Tab:
		if (state & GDK_SHIFT_MASK)
			indent_multi_line_unindent(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)));
		else if (!check_selection_bound(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view))))
			break;
		else
			indent_multi_line_indent(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)));
		return TRUE;
	}
	keyval = kv;
	if ((state & GDK_CONTROL_MASK)
		|| (kv == GDK_KEY_Control_L)
		|| (kv == GDK_KEY_Control_R)) {
		keyval = keyval + 0x10000;
//g_print("=================================================\n");
	}

	return FALSE;
}

/*
 * GTK4: button-press-event is replaced by GtkGestureClick.
 * The right-click cursor placement logic is preserved.
 * Primary clipboard backup/restore is dropped — GTK4's GtkTextView
 * handles primary selection natively.
 */
static void cb_button_pressed(GtkGestureClick *gesture,
                              gint n_press, gdouble x, gdouble y,
                              gpointer user_data)
{
	GtkWidget *view = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
	GtkTextIter iter, start, end;
	gint bx, by;
	guint button;

	(void)n_press;
	(void)user_data;

	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));

	button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
	if (button == 3) {
		gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(view),
			GTK_TEXT_WINDOW_WIDGET,
			(gint)x, (gint)y, &bx, &by);
		gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(view), &iter, bx, by);
		gtk_text_buffer_get_selection_bounds(buffer, &start, &end);
		if (!gtk_text_iter_in_range(&iter, &start, &end))
			gtk_text_buffer_place_cursor(buffer, &iter);
	}
}

static void cb_modified_changed(GtkTextBuffer *buffer, GtkWidget *view)
{
	gboolean modified_flag, exist_flag = FALSE;
	gchar *filename, *title;

	modified_flag = gtk_text_buffer_get_modified(buffer);
	filename = get_file_basename(pub->fi->filename, TRUE);
	if (modified_flag)
		title = g_strconcat("*", filename, NULL);
	else {
		title = g_strdup(filename);
		undo_reset_modified_step(buffer);
	}
	g_free(filename);
	gtk_window_set_title(GTK_WINDOW(gtk_widget_get_root(view)), title);
	g_free(title);
	if (pub->fi->filename) {
		gchar *utf8 = g_filename_to_utf8(pub->fi->filename, -1, NULL, NULL, NULL);
		exist_flag = g_file_test(utf8, G_FILE_TEST_EXISTS);
		g_free(utf8);
	}
	menu_sensitivity_from_modified_flag(modified_flag || !exist_flag);
}

void force_call_cb_modified_changed(GtkWidget *view)
{
	cb_modified_changed(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)), view);
}

void force_block_cb_modified_changed(GtkWidget *view)
{
	g_signal_handlers_block_by_func(G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view))),
		G_CALLBACK(cb_modified_changed), view);
}

void force_unblock_cb_modified_changed(GtkWidget *view)
{
	g_signal_handlers_unblock_by_func(G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view))),
		G_CALLBACK(cb_modified_changed), view);
}
/*
static void cb_mark_set(GtkTextBuffer *buffer, GtkTextIter *iter, GtkTextMark *mark)
{
	if (gtk_text_mark_get_name(mark))
{g_print(gtk_text_mark_get_name(mark));
}else g_print("|");
		menu_sensitivity_from_selection_bound(
			gtk_text_buffer_get_selection_bounds(buffer, NULL, NULL));
}
*/
static void cb_mark_changed(GtkTextBuffer *buffer)
{
	menu_sensitivity_from_selection_bound(
		gtk_text_buffer_get_selection_bounds(buffer, NULL, NULL));
}

/*
 * GTK4: focus-in-event / focus-out-event are replaced by
 * GtkEventControllerFocus with "enter" and "leave" signals.
 */
static void cb_focus_enter(GtkEventControllerFocus *controller, gpointer user_data)
{
	GtkWidget *view = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller));

	(void)user_data;

	if (!gtk_text_buffer_get_selection_bounds(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)), NULL, NULL))
		gtk_text_mark_set_visible(
			gtk_text_buffer_get_selection_bound(
				gtk_text_view_get_buffer(GTK_TEXT_VIEW(view))), FALSE);
	menu_sensitivity_from_clipboard();
}

static void cb_focus_leave(GtkEventControllerFocus *controller, gpointer user_data)
{
	GtkWidget *view = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller));

	(void)user_data;

	if (!gtk_text_buffer_get_selection_bounds(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)), NULL, NULL))
		gtk_text_mark_set_visible(
			gtk_text_buffer_get_selection_bound(
				gtk_text_view_get_buffer(GTK_TEXT_VIEW(view))), TRUE);
}
/*
static void cb_begin_user_action(GtkTextBuffer *buffer, GtkWidget *view)
{
	g_signal_handlers_unblock_by_func(G_OBJECT(buffer),
		G_CALLBACK(cb_modified_changed), view);
//	g_print("begin-user-action\n");
}

static void cb_end_user_action(GtkTextBuffer *buffer, GtkWidget *view)
{
	g_signal_handlers_block_by_func(G_OBJECT(buffer),
		G_CALLBACK(cb_modified_changed), view);
	gtk_text_view_scroll_mark_onscreen(		// TODO: require?
		GTK_TEXT_VIEW(view),
		gtk_text_buffer_get_insert(buffer));
//	g_print("end-user-action\n");
}
*//*
static void cb_something(GtkTextBuffer *buffer, gchar *data)
{
	g_print("%s\n", data);
}
*/
void set_view_scroll(void)
{
	view_scroll_flag = TRUE;
}

static void cb_end_user_action(GtkTextBuffer *buffer, GtkWidget *view)
{
	if (view_scroll_flag) {
		gtk_text_view_scroll_mark_onscreen(		// TODO: require?
			GTK_TEXT_VIEW(view),
			gtk_text_buffer_get_insert(buffer));
		view_scroll_flag = FALSE;
	}
}

GtkWidget *create_text_view(void)
{
	GtkWidget *view;
	GtkTextBuffer *buffer;
	GtkEventController *key_controller;
	GtkGesture *click_gesture;
	GtkEventController *focus_controller;

	view = gtk_text_view_new();
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));

	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 8);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(view), 8);
	gtk_text_view_set_top_margin(GTK_TEXT_VIEW(view), 4);
	gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(view), 4);

	/* GTK4: use GtkEventControllerKey instead of key-press-event signal */
	key_controller = gtk_event_controller_key_new();
	g_signal_connect(key_controller, "key-pressed",
		G_CALLBACK(cb_key_pressed), NULL);
	gtk_widget_add_controller(view, key_controller);

	/* GTK4: use GtkGestureClick instead of button-press-event signal */
	click_gesture = gtk_gesture_click_new();
	gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click_gesture), 0);
	g_signal_connect(click_gesture, "pressed",
		G_CALLBACK(cb_button_pressed), NULL);
	gtk_widget_add_controller(view, GTK_EVENT_CONTROLLER(click_gesture));

	g_signal_connect_after(G_OBJECT(view), "cut-clipboard",
		G_CALLBACK(menu_sensitivity_from_clipboard), NULL);
	g_signal_connect_after(G_OBJECT(view), "copy-clipboard",
		G_CALLBACK(menu_sensitivity_from_clipboard), NULL);
	g_signal_connect_after(G_OBJECT(view), "paste-clipboard",
		G_CALLBACK(set_view_scroll),
		gtk_text_buffer_get_insert(buffer));
/*	g_signal_connect_after(G_OBJECT(view), "paste-clipboard",
		G_CALLBACK(gtk_text_view_scroll_mark_onscreen),
		gtk_text_buffer_get_insert(buffer));*/

	/* GTK4: use GtkEventControllerFocus instead of focus-in/out-event */
	focus_controller = gtk_event_controller_focus_new();
	g_signal_connect(focus_controller, "enter",
		G_CALLBACK(cb_focus_enter), NULL);
	g_signal_connect(focus_controller, "leave",
		G_CALLBACK(cb_focus_leave), NULL);
	gtk_widget_add_controller(view, focus_controller);

	g_signal_connect(G_OBJECT(buffer), "mark-set",
		G_CALLBACK(cb_mark_changed), NULL);
	g_signal_connect(G_OBJECT(buffer), "mark-deleted",
		G_CALLBACK(cb_mark_changed), NULL);
	g_signal_connect(G_OBJECT(buffer), "modified-changed",
		G_CALLBACK(cb_modified_changed), view);
	g_signal_connect_after(G_OBJECT(buffer), "end-user-action",
		G_CALLBACK(cb_end_user_action), view);
/*	g_signal_connect(G_OBJECT(buffer), "begin-user-action",
		G_CALLBACK(cb_begin_user_action), view);
	g_signal_connect_after(G_OBJECT(buffer), "end-user-action",
		G_CALLBACK(cb_end_user_action), view);
	cb_end_user_action(buffer, view);
*/
	linenum_init(view);

	return view;
}
