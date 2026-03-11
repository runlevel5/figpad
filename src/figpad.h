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

#ifndef _FIGPAD_H
#define _FIGPAD_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>

#ifndef ENABLE_PRINT
#define ENABLE_PRINT 1
#endif

#ifndef SEARCH_HISTORY
#define SEARCH_HISTORY 0
#endif

#ifndef ENABLE_STATISTICS
#define ENABLE_STATISTICS 1
#endif

/* ---- application-wide constants ---- */

/* Default window geometry */
#define DEFAULT_WINDOW_WIDTH   600
#define DEFAULT_WINDOW_HEIGHT  400

/* Scroll-to-cursor margins (fraction of visible area) */
#define SCROLL_MARGIN_NARROW   0.05
#define SCROLL_MARGIN_WIDE     0.25

/* Print header offset: 10 mm expressed in points (72pt/in, 25.4mm/in) */
#define PRINT_HEADER_OFFSET_PT (72.0 / 25.4 * 10.0)

/* Print page margins (millimetres) */
#define PRINT_MARGIN_TOP_MM    25.0
#define PRINT_MARGIN_BOTTOM_MM 20.0
#define PRINT_MARGIN_LEFT_MM   20.0
#define PRINT_MARGIN_RIGHT_MM  20.0

/* Keyval threshold — values below this are printable characters */
#define KEYVAL_NON_CHAR_THRESHOLD 0xF000

/* Delay (ms) before forcing focus back to the text view after a menu action */
#define MENU_REFOCUS_DELAY_MS  50

/* Minimum width for the search/replace dialog */
#define SEARCH_DIALOG_MIN_WIDTH 400

#include "window.h"
#include "menu.h"
#include "callback.h"
#include "view.h"
#include "undo.h"
#include "font.h"
#include "linenum.h"
#include "indent.h"
#include "hlight.h"
#include "selector.h"
#include "file.h"
#include "encoding.h"
#include "search.h"
#include "dialog.h"
#include "utils.h"
#include "gtkprint.h"
typedef struct {
	FileInfo *fi;
	MainWin *mw;
	GtkApplication *app;
} PublicData;

#ifndef _FIGPAD_MAIN
extern PublicData *pub;
#endif

void save_config_file(void);

#endif /* _FIGPAD_H */
