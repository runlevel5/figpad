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

/* action names used to look up GSimpleAction for sensitivity changes */
static GSimpleAction *action_save;
static GSimpleAction *action_cut;
static GSimpleAction *action_copy;
static GSimpleAction *action_paste;
static GSimpleAction *action_delete;
static GSimpleAction *action_find_next;
static GSimpleAction *action_find_prev;

/* window-level action map, saved so other modules can look up actions */
static GActionMap *win_action_map;

/*
 * After a menu action fires the GtkPopoverMenuBar popover keeps focus until
 * its close animation finishes.  A plain gtk_widget_grab_focus() on the text
 * view may be a no-op if GTK still considers the view "focused" (same
 * toplevel), so the IM context never gets its focus_in call and typing
 * appears dead.
 *
 * Work-around: first move focus away to the menu bar, then immediately move
 * it to the text view.  This forces a real focus-leave / focus-enter cycle
 * on the GtkTextView, which makes GTK call gtk_im_context_focus_in()
 * internally so that keyboard input works again.
 */
static gboolean refocus_textview_timeout(gpointer data)
{
	(void)data;
	/* Force a focus transition: menubar -> textview */
	gtk_widget_grab_focus(pub->mw->menubar);
	gtk_widget_grab_focus(pub->mw->view);
	return G_SOURCE_REMOVE;
}

static void schedule_refocus(void)
{
	g_timeout_add(50, refocus_textview_timeout, NULL);
}

/*
 * When the user clicks a menu title and then dismisses the popover without
 * choosing an action, focus stays somewhere in the menu bar widget tree and
 * the text view's IM context is not active — typing appears dead.
 *
 * Work-around: install a key-press controller on the window in the CAPTURE
 * phase.  If a non-modifier key arrives while the text view does not have
 * focus, force a focus transition to the text view and let the event
 * propagate normally.  The text view's IM context will be re-activated
 * by the focus-enter and will process the key.
 */
static gboolean
on_window_key_capture(GtkEventControllerKey *ctl,
                      guint keyval, guint keycode,
                      GdkModifierType state, gpointer data)
{
	(void)ctl; (void)keycode; (void)state; (void)data;

	/* Only intervene if the text view exists but doesn't have focus */
	if (pub->mw->view == NULL)
		return FALSE;
	if (gtk_widget_has_focus(pub->mw->view))
		return FALSE;

	/*
	 * Ignore bare modifier keys (Shift, Ctrl, Alt, Super) — they are
	 * not text input and should not steal focus back.
	 */
	if (keyval == GDK_KEY_Shift_L   || keyval == GDK_KEY_Shift_R   ||
	    keyval == GDK_KEY_Control_L  || keyval == GDK_KEY_Control_R  ||
	    keyval == GDK_KEY_Alt_L      || keyval == GDK_KEY_Alt_R      ||
	    keyval == GDK_KEY_Super_L    || keyval == GDK_KEY_Super_R    ||
	    keyval == GDK_KEY_Meta_L     || keyval == GDK_KEY_Meta_R     ||
	    keyval == GDK_KEY_Caps_Lock  || keyval == GDK_KEY_Num_Lock)
		return FALSE;

	/*
	 * Force a focus transition so the text view's IM context gets its
	 * focus_in call.  Move focus to the menubar first, then to the text
	 * view, so GTK sees a genuine leave/enter cycle.
	 */
	gtk_widget_grab_focus(pub->mw->menubar);
	gtk_widget_grab_focus(pub->mw->view);

	/* Don't consume the event — let it propagate to the text view */
	return FALSE;
}

/*
 * Wrappers that adapt void(void) callbacks to the GAction activate signature.
 * GSimpleAction "activate" passes (GSimpleAction *action, GVariant *param,
 * gpointer user_data), but our old callbacks take no arguments.
 */
#define ACTION_CB(name, func) \
static void action_##name(GSimpleAction *action, GVariant *param, gpointer data) \
{ (void)action; (void)param; (void)data; func(); \
  schedule_refocus(); }

ACTION_CB(file_new, on_file_new)
ACTION_CB(file_open, on_file_open)
ACTION_CB(file_save, on_file_save)
ACTION_CB(file_save_as, on_file_save_as)
#if ENABLE_STATISTICS
ACTION_CB(file_stats, on_file_stats)
#endif
#if ENABLE_PRINT
ACTION_CB(file_print, on_file_print)
#endif
ACTION_CB(file_close, on_file_close)
ACTION_CB(file_quit, on_file_quit)
ACTION_CB(edit_undo, on_edit_undo)
ACTION_CB(edit_redo, on_edit_redo)
ACTION_CB(edit_cut, on_edit_cut)
ACTION_CB(edit_copy, on_edit_copy)
ACTION_CB(edit_paste, on_edit_paste)
ACTION_CB(edit_delete, on_edit_delete)
ACTION_CB(edit_select_all, on_edit_select_all)
ACTION_CB(search_find, on_search_find)
ACTION_CB(search_find_next, on_search_find_next)
ACTION_CB(search_find_prev, on_search_find_previous)
ACTION_CB(search_replace, on_search_replace)
ACTION_CB(search_jump_to, on_search_jump_to)
ACTION_CB(option_font, on_option_font)
ACTION_CB(option_always_on_top, on_option_always_on_top)
ACTION_CB(help_about, on_help_about)

/* Toggle action callbacks — read new state from the GVariant */
static void action_toggle_word_wrap(GSimpleAction *action, GVariant *param, gpointer data)
{
	(void)param; (void)data;
	GVariant *state = g_action_get_state(G_ACTION(action));
	gboolean active = !g_variant_get_boolean(state);
	g_variant_unref(state);
	g_simple_action_set_state(action, g_variant_new_boolean(active));
	on_option_word_wrap();
	schedule_refocus();
}

static void action_toggle_line_numbers(GSimpleAction *action, GVariant *param, gpointer data)
{
	(void)param; (void)data;
	GVariant *state = g_action_get_state(G_ACTION(action));
	gboolean active = !g_variant_get_boolean(state);
	g_variant_unref(state);
	g_simple_action_set_state(action, g_variant_new_boolean(active));
	on_option_line_numbers();
	schedule_refocus();
}

static void action_toggle_auto_indent(GSimpleAction *action, GVariant *param, gpointer data)
{
	(void)param; (void)data;
	GVariant *state = g_action_get_state(G_ACTION(action));
	gboolean active = !g_variant_get_boolean(state);
	g_variant_unref(state);
	g_simple_action_set_state(action, g_variant_new_boolean(active));
	on_option_auto_indent();
	schedule_refocus();
}

/* plain actions (no state) */
static const GActionEntry win_actions[] = {
	{ "new",            action_file_new,          NULL, NULL, NULL },
	{ "open",           action_file_open,         NULL, NULL, NULL },
	{ "save",           action_file_save,         NULL, NULL, NULL },
	{ "save-as",        action_file_save_as,      NULL, NULL, NULL },
#if ENABLE_STATISTICS
	{ "statistics",     action_file_stats,        NULL, NULL, NULL },
#endif
#if ENABLE_PRINT
	{ "print",          action_file_print,        NULL, NULL, NULL },
#endif
	{ "close",          action_file_close,        NULL, NULL, NULL },
	{ "quit",           action_file_quit,         NULL, NULL, NULL },
	{ "undo",           action_edit_undo,         NULL, NULL, NULL },
	{ "redo",           action_edit_redo,         NULL, NULL, NULL },
	{ "cut",            action_edit_cut,          NULL, NULL, NULL },
	{ "copy",           action_edit_copy,         NULL, NULL, NULL },
	{ "paste",          action_edit_paste,        NULL, NULL, NULL },
	{ "delete",         action_edit_delete,       NULL, NULL, NULL },
	{ "select-all",     action_edit_select_all,   NULL, NULL, NULL },
	{ "find",           action_search_find,       NULL, NULL, NULL },
	{ "find-next",      action_search_find_next,  NULL, NULL, NULL },
	{ "find-previous",  action_search_find_prev,  NULL, NULL, NULL },
	{ "replace",        action_search_replace,    NULL, NULL, NULL },
	{ "jump-to",        action_search_jump_to,    NULL, NULL, NULL },
	{ "font",           action_option_font,       NULL, NULL, NULL },
	{ "always-on-top",  action_option_always_on_top, NULL, NULL, NULL },
	{ "about",          action_help_about,        NULL, NULL, NULL },
};

/* stateful toggle actions — initial state FALSE, toggled on activate */
static const GActionEntry toggle_actions[] = {
	{ "word-wrap",     action_toggle_word_wrap,     NULL, "false", NULL },
	{ "line-numbers",  action_toggle_line_numbers,  NULL, "false", NULL },
	{ "auto-indent",   action_toggle_auto_indent,   NULL, "false", NULL },
};

static GMenuModel *build_menu_model(void)
{
	GMenu *menubar = g_menu_new();

	/* File menu */
	GMenu *file_menu = g_menu_new();
	g_menu_append(file_menu, _("_New"),            "win.new");
	g_menu_append(file_menu, _("_Open..."),        "win.open");
	g_menu_append(file_menu, _("_Save"),           "win.save");
	g_menu_append(file_menu, _("Save _As..."),     "win.save-as");

	GMenu *file_section2 = g_menu_new();
#if ENABLE_STATISTICS
	g_menu_append(file_section2, _("Sta_tistics..."), "win.statistics");
#endif
#if ENABLE_PRINT
	g_menu_append(file_section2, _("_Print..."),      "win.print");
#endif

	GMenu *file_section3 = g_menu_new();
	g_menu_append(file_section3, _("_Quit"), "win.quit");

	g_menu_append_section(file_menu, NULL, G_MENU_MODEL(file_section2));
	g_menu_append_section(file_menu, NULL, G_MENU_MODEL(file_section3));
	g_menu_append_submenu(menubar, _("_File"), G_MENU_MODEL(file_menu));
	g_object_unref(file_section2);
	g_object_unref(file_section3);
	g_object_unref(file_menu);

	/* Edit menu */
	GMenu *edit_menu = g_menu_new();
	GMenu *edit_undo_section = g_menu_new();
	g_menu_append(edit_undo_section, _("_Undo"), "win.undo");
	g_menu_append(edit_undo_section, _("_Redo"), "win.redo");

	GMenu *edit_clip_section = g_menu_new();
	g_menu_append(edit_clip_section, _("Cu_t"),    "win.cut");
	g_menu_append(edit_clip_section, _("_Copy"),   "win.copy");
	g_menu_append(edit_clip_section, _("_Paste"),  "win.paste");
	g_menu_append(edit_clip_section, _("_Delete"), "win.delete");

	GMenu *edit_sel_section = g_menu_new();
	g_menu_append(edit_sel_section, _("Select _All"), "win.select-all");

	g_menu_append_section(edit_menu, NULL, G_MENU_MODEL(edit_undo_section));
	g_menu_append_section(edit_menu, NULL, G_MENU_MODEL(edit_clip_section));
	g_menu_append_section(edit_menu, NULL, G_MENU_MODEL(edit_sel_section));
	g_menu_append_submenu(menubar, _("_Edit"), G_MENU_MODEL(edit_menu));
	g_object_unref(edit_undo_section);
	g_object_unref(edit_clip_section);
	g_object_unref(edit_sel_section);
	g_object_unref(edit_menu);

	/* Search menu */
	GMenu *search_menu = g_menu_new();
	GMenu *search_find_section = g_menu_new();
	g_menu_append(search_find_section, _("_Find..."),        "win.find");
	g_menu_append(search_find_section, _("Find _Next"),      "win.find-next");
	g_menu_append(search_find_section, _("Find _Previous"),  "win.find-previous");
	g_menu_append(search_find_section, _("_Replace..."),     "win.replace");

	GMenu *search_jump_section = g_menu_new();
	g_menu_append(search_jump_section, _("_Jump To..."), "win.jump-to");

	g_menu_append_section(search_menu, NULL, G_MENU_MODEL(search_find_section));
	g_menu_append_section(search_menu, NULL, G_MENU_MODEL(search_jump_section));
	g_menu_append_submenu(menubar, _("_Search"), G_MENU_MODEL(search_menu));
	g_object_unref(search_find_section);
	g_object_unref(search_jump_section);
	g_object_unref(search_menu);

	/* Options menu */
	GMenu *options_menu = g_menu_new();
	GMenu *options_section1 = g_menu_new();
	g_menu_append(options_section1, _("_Font..."),       "win.font");
	g_menu_append(options_section1, _("_Word Wrap"),     "win.word-wrap");
	g_menu_append(options_section1, _("_Line Numbers"),  "win.line-numbers");

	GMenu *options_section2 = g_menu_new();
	g_menu_append(options_section2, _("_Auto Indent"), "win.auto-indent");

	g_menu_append_section(options_menu, NULL, G_MENU_MODEL(options_section1));
	g_menu_append_section(options_menu, NULL, G_MENU_MODEL(options_section2));
	g_menu_append_submenu(menubar, _("_Options"), G_MENU_MODEL(options_menu));
	g_object_unref(options_section1);
	g_object_unref(options_section2);
	g_object_unref(options_menu);

	/* Help menu */
	GMenu *help_menu = g_menu_new();
	g_menu_append(help_menu, _("_About"), "win.about");
	g_menu_append_submenu(menubar, _("_Help"), G_MENU_MODEL(help_menu));
	g_object_unref(help_menu);

	return G_MENU_MODEL(menubar);
}

void menu_sensitivity_from_modified_flag(gboolean is_text_modified)
{
	g_simple_action_set_enabled(action_save, is_text_modified);
}

void menu_sensitivity_from_selection_bound(gboolean is_bound_exist)
{
	g_simple_action_set_enabled(action_cut,    is_bound_exist);
	g_simple_action_set_enabled(action_copy,   is_bound_exist);
	g_simple_action_set_enabled(action_delete, is_bound_exist);
}

void menu_sensitivity_from_clipboard(void)
{
	/* In GTK4 there is no synchronous clipboard check.
	 * For now, always enable paste — will be refined during view.c port
	 * when we switch to the GdkClipboard async API. */
	g_simple_action_set_enabled(action_paste, TRUE);
}

void menu_sensitivity_from_find(gboolean sensitive)
{
	g_simple_action_set_enabled(action_find_next, sensitive);
	g_simple_action_set_enabled(action_find_prev, sensitive);
}

gboolean menu_toggle_get_active(const gchar *action_name)
{
	GAction *action = g_action_map_lookup_action(win_action_map, action_name);
	if (!action) return FALSE;
	GVariant *state = g_action_get_state(action);
	gboolean active = g_variant_get_boolean(state);
	g_variant_unref(state);
	return active;
}

void menu_toggle_set_active(const gchar *action_name, gboolean is_active)
{
	GAction *action = g_action_map_lookup_action(win_action_map, action_name);
	if (!action) return;
	/* Activate the action to toggle it — only if current state differs */
	GVariant *state = g_action_get_state(action);
	if (g_variant_get_boolean(state) != is_active)
		g_action_activate(action, NULL);
	g_variant_unref(state);
}

GtkWidget *create_menu_bar(GtkWindow *window, GtkApplication *app)
{
	win_action_map = G_ACTION_MAP(window);

	/* register actions on the window */
	g_action_map_add_action_entries(G_ACTION_MAP(window),
		win_actions, G_N_ELEMENTS(win_actions), NULL);
	g_action_map_add_action_entries(G_ACTION_MAP(window),
		toggle_actions, G_N_ELEMENTS(toggle_actions), NULL);

	/* keyboard accelerators */
	const gchar *new_accels[]           = { "<Control>n", NULL };
	const gchar *open_accels[]          = { "<Control>o", NULL };
	const gchar *save_accels[]          = { "<Control>s", NULL };
	const gchar *save_as_accels[]       = { "<Shift><Control>s", NULL };
	const gchar *quit_accels[]          = { "<Control>q", NULL };
	/* Ctrl+W closes the document (clears buffer), not the app */
	const gchar *close_accels[]         = { "<Control>w", NULL };
	const gchar *undo_accels[]          = { "<Control>z", NULL };
	const gchar *redo_accels[]          = { "<Shift><Control>z", "<Control>y", NULL };
	const gchar *cut_accels[]           = { "<Control>x", NULL };
	const gchar *copy_accels[]          = { "<Control>c", NULL };
	const gchar *paste_accels[]         = { "<Control>v", NULL };
	const gchar *select_all_accels[]    = { "<Control>a", NULL };
	const gchar *find_accels[]          = { "<Control>f", NULL };
	const gchar *find_next_accels[]     = { "<Control>g", "F3", NULL };
	const gchar *find_prev_accels[]     = { "<Shift><Control>g", "<Shift>F3", NULL };
	const gchar *replace_accels[]       = { "<Control>h", "<Control>r", NULL };
	const gchar *jump_to_accels[]       = { "<Control>j", NULL };
	const gchar *always_on_top_accels[] = { "<Control>t", NULL };
#if ENABLE_PRINT
	const gchar *print_accels[]         = { "<Control>p", NULL };
#endif

	gtk_application_set_accels_for_action(app, "win.new",           new_accels);
	gtk_application_set_accels_for_action(app, "win.open",          open_accels);
	gtk_application_set_accels_for_action(app, "win.save",          save_accels);
	gtk_application_set_accels_for_action(app, "win.save-as",       save_as_accels);
	gtk_application_set_accels_for_action(app, "win.quit",          quit_accels);
	gtk_application_set_accels_for_action(app, "win.close",         close_accels);
	gtk_application_set_accels_for_action(app, "win.undo",          undo_accels);
	gtk_application_set_accels_for_action(app, "win.redo",          redo_accels);
	gtk_application_set_accels_for_action(app, "win.cut",           cut_accels);
	gtk_application_set_accels_for_action(app, "win.copy",          copy_accels);
	gtk_application_set_accels_for_action(app, "win.paste",         paste_accels);
	gtk_application_set_accels_for_action(app, "win.select-all",    select_all_accels);
	gtk_application_set_accels_for_action(app, "win.find",          find_accels);
	gtk_application_set_accels_for_action(app, "win.find-next",     find_next_accels);
	gtk_application_set_accels_for_action(app, "win.find-previous", find_prev_accels);
	gtk_application_set_accels_for_action(app, "win.replace",       replace_accels);
	gtk_application_set_accels_for_action(app, "win.jump-to",       jump_to_accels);
	gtk_application_set_accels_for_action(app, "win.always-on-top", always_on_top_accels);
#if ENABLE_PRINT
	gtk_application_set_accels_for_action(app, "win.print",         print_accels);
#endif

	/* cache action pointers for sensitivity functions */
	action_save      = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(window), "save"));
	action_cut       = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(window), "cut"));
	action_copy      = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(window), "copy"));
	action_paste     = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(window), "paste"));
	action_delete    = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(window), "delete"));
	action_find_next = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(window), "find-next"));
	action_find_prev = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(window), "find-previous"));

	/* initial sensitivities */
	menu_sensitivity_from_selection_bound(FALSE);
	menu_sensitivity_from_find(FALSE);

	/* build the GMenuModel and create a popover menu bar */
	GMenuModel *model = build_menu_model();
	GtkWidget *bar = gtk_popover_menu_bar_new_from_model(model);
	g_object_unref(model);

	/*
	 * Install a key-press controller on the window (CAPTURE phase) to
	 * reclaim focus for the text view whenever a non-modifier key is
	 * pressed while the menu bar (or its popover) holds focus.
	 */
	GtkEventController *key_ctl = gtk_event_controller_key_new();
	gtk_event_controller_set_propagation_phase(key_ctl, GTK_PHASE_CAPTURE);
	g_signal_connect(key_ctl, "key-pressed",
		G_CALLBACK(on_window_key_capture), NULL);
	gtk_widget_add_controller(GTK_WIDGET(window), key_ctl);

	return bar;
}
