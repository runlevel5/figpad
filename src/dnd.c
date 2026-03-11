/*
 *  Figpad - GTK+ based simple text editor
 *  Copyright (C) 2004-2006 Tarot Osuji
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
#include "string.h"

/*
 * In GTK3, dnd_init() registered drag-dest targets via gtk_drag_dest_set().
 * The data-received and motion handlers were disabled (#if 0).
 *
 * In GTK4, GtkTextView already handles text drops natively, so we only need
 * to register a GtkDropTarget if we want custom URI-list handling in the
 * future.  For now, the function is a no-op since the old handlers were
 * disabled.
 */
void dnd_init(GtkWidget *widget)
{
	(void)widget;
	/* GtkTextView in GTK4 handles text drag-and-drop natively.
	 * Re-enable with GtkDropTarget when URI-list file opening is
	 * implemented. */
}
