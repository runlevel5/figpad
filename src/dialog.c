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

/* GTK_MESSAGE_INFO, GTK_MESSAGE_WARNING, GTK_MESSAGE_ERROR */
void run_dialog_message(GtkWidget *window,
	GtkMessageType type,
	gchar *message, ...)
{
	va_list ap;
	gchar *str;
	GtkAlertDialog *alert;

	(void)type; /* GtkAlertDialog has no message-type icon */

	va_start(ap, message);
		str = g_strdup_vprintf(message, ap);
	va_end(ap);

	alert = gtk_alert_dialog_new("%s", str);
	gtk_alert_dialog_set_buttons(alert, (const char *[]){ _("_OK"), NULL });
	gtk_alert_dialog_set_default_button(alert, 0);
	gtk_alert_dialog_set_cancel_button(alert, 0);
	g_free(str);

	gtk_alert_dialog_show(alert, GTK_WINDOW(window));
	g_object_unref(alert);
}

typedef struct {
	GMainLoop *loop;
	gint chosen;
} QuestionSyncData;

static void on_question_sync_response(GObject *source, GAsyncResult *result,
	gpointer user_data)
{
	QuestionSyncData *data = user_data;
	data->chosen = gtk_alert_dialog_choose_finish(
		GTK_ALERT_DIALOG(source), result, NULL);
	g_main_loop_quit(data->loop);
}

/*
 * QUESTION_RESPONSE_NO      = 0
 * QUESTION_RESPONSE_CANCEL  = 1
 * QUESTION_RESPONSE_YES     = 2
 * Returns -1 on error / dialog dismissed.
 */
gint run_dialog_question_sync(GtkWidget *window, gchar *message, ...)
{
	va_list ap;
	gchar *str;
	GtkAlertDialog *alert;
	QuestionSyncData data;

	va_start(ap, message);
		str = g_strdup_vprintf(message, ap);
	va_end(ap);

	alert = gtk_alert_dialog_new("%s", str);
	gtk_alert_dialog_set_buttons(alert,
		(const char *[]){ _("_No"), _("_Cancel"), _("_Yes"), NULL });
	gtk_alert_dialog_set_default_button(alert, 2);
	gtk_alert_dialog_set_cancel_button(alert, 1);
	g_free(str);

	data.loop = g_main_loop_new(NULL, FALSE);
	data.chosen = -1;
	gtk_alert_dialog_choose(alert, GTK_WINDOW(window), NULL,
		on_question_sync_response, &data);
	g_main_loop_run(data.loop);
	g_main_loop_unref(data.loop);
	g_object_unref(alert);

	return data.chosen;
}

/* ---- Sync-loop helper for modal-style custom dialogs ---- */

void sync_dialog_init(SyncDialogData *sd)
{
	sd->loop = g_main_loop_new(NULL, FALSE);
	sd->accepted = FALSE;
}

void sync_dialog_accept(GtkWidget *widget, gpointer user_data)
{
	(void)widget;
	SyncDialogData *sd = user_data;
	sd->accepted = TRUE;
	g_main_loop_quit(sd->loop);
}

void sync_dialog_connect(SyncDialogData *sd,
	GtkWidget *dialog, GtkWidget *cancel_btn, GtkWidget *ok_btn)
{
	g_signal_connect_swapped(cancel_btn, "clicked",
		G_CALLBACK(g_main_loop_quit), sd->loop);
	g_signal_connect_swapped(dialog, "close-request",
		G_CALLBACK(g_main_loop_quit), sd->loop);
	g_signal_connect(ok_btn, "clicked",
		G_CALLBACK(sync_dialog_accept), sd);
}

gboolean sync_dialog_run(SyncDialogData *sd)
{
	g_main_loop_run(sd->loop);
	g_main_loop_unref(sd->loop);
	return sd->accepted;
}
