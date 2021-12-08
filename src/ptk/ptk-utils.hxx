/*
 *  C Interface: ptkutils
 *
 * Description: Some GUI utilities
 *
 *
 * Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 */

#pragma once

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib.h>

/* The string 'message' can contain pango markups.
 * If special characters like < and > are used in the string,
 * they should be escaped with g_markup_escape_text().
 */
void ptk_show_error(GtkWindow* parent, const char* title, const char* message);

GtkBuilder* _gtk_builder_new_from_file(const char* file);

#ifdef HAVE_NONLATIN
void transpose_nonlatin_keypress(GdkEventKey* event);
#endif
