/**
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <string>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib.h>

// The string 'message' can contain pango markups.
// If special characters like < and > are used in the string,
// they should be escaped with g_markup_escape_text().
void ptk_show_error(GtkWindow* parent, const std::string& title, const std::string& message);

unsigned int ptk_get_keymod(unsigned int event);

GtkBuilder* _gtk_builder_new_from_file(const char* file);

#ifdef HAVE_NONLATIN
void transpose_nonlatin_keypress(GdkEventKey* event);
#endif
