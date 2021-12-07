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

/*
  I don't like GtkUIManager provided by gtk+, so I implement my own. ;-)
*/

#pragma once

#include <stdint.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

#define PTK_IS_STOCK_ITEM(ent)      (ent->label && (*(uint32_t*)ent->label) == *(uint32_t*)"gtk-")
#define PTK_IS_CHECK_MENU_ITEM(ent) (ent->stock_icon == (char*)1)
#define PTK_IS_RADIO_MENU_ITEM(ent) (ent->stock_icon == (char*)2)

typedef struct PtkMenuItemEntry
{
    const char* label;      /* or stock id */
    const char* stock_icon; /* or menu type  1: check, 2: radio */
    GCallback callback;
    unsigned int key;
    GdkModifierType mod;
    struct PtkMenuItemEntry* sub_menu;
    GtkWidget** ret;
    GtkWidget* menu; // MOD
} PtkMenuItemEntry;

GtkWidget* ptk_menu_new_from_data(PtkMenuItemEntry* entries, void* cb_data,
                                  GtkAccelGroup* accel_group);

void ptk_menu_add_items_from_data(GtkWidget* menu, PtkMenuItemEntry* entries, void* cb_data,
                                  GtkAccelGroup* accel_group);

/* The string 'message' can contain pango markups.
 * If special characters like < and > are used in the string,
 * they should be escaped with g_markup_escape_text().
 */
void ptk_show_error(GtkWindow* parent, const char* title, const char* message);

GtkBuilder* _gtk_builder_new_from_file(const char* file);

#ifdef HAVE_NONLATIN
void transpose_nonlatin_keypress(GdkEventKey* event);
#endif

G_END_DECLS
