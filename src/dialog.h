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

#ifndef _DIALOG_H
#define _DIALOG_H

void run_dialog_message(GtkWidget *window, GtkMessageType type, gchar *message, ...);
gint run_dialog_question_sync(GtkWidget *window, gchar *message, ...);

/* Button indices returned by run_dialog_question_sync() */
#define QUESTION_RESPONSE_NO     0
#define QUESTION_RESPONSE_CANCEL 1
#define QUESTION_RESPONSE_YES    2

/*
 * Sync-loop helper for modal-style dialogs.
 *
 * Usage:
 *   SyncDialogData sd;
 *   sync_dialog_init(&sd);
 *   // connect cancel_button "clicked" and dialog "close-request" to
 *   // sync_dialog_quit(), and ok_button "clicked" to sync_dialog_accept():
 *   sync_dialog_connect(&sd, dialog, cancel_button, ok_button);
 *   // optionally connect additional accept triggers:
 *   //   g_signal_connect(widget, "activate",
 *   //       G_CALLBACK(sync_dialog_accept), &sd);
 *   gboolean accepted = sync_dialog_run(&sd);
 */
typedef struct {
	GMainLoop *loop;
	gboolean   accepted;
} SyncDialogData;

void     sync_dialog_init(SyncDialogData *sd);
void     sync_dialog_accept(GtkWidget *widget, gpointer user_data);
void     sync_dialog_connect(SyncDialogData *sd,
             GtkWidget *dialog, GtkWidget *cancel_btn, GtkWidget *ok_btn);
gboolean sync_dialog_run(SyncDialogData *sd);

#endif /* _DIALOG_H */
