/*
 *  C Implementation: ptkutils
 *
 * Description:
 *
 *
 * Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 */

#include "ptk-utils.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include "settings.h"
#include "utils.h"

GtkWidget* ptk_menu_new_from_data(PtkMenuItemEntry* entries, void* cb_data,
                                  GtkAccelGroup* accel_group)
{
    GtkWidget* menu;
    menu = gtk_menu_new();
    ptk_menu_add_items_from_data(menu, entries, cb_data, accel_group);
    return menu;
}

void ptk_menu_add_items_from_data(GtkWidget* menu, PtkMenuItemEntry* entries, void* cb_data,
                                  GtkAccelGroup* accel_group)
{
    PtkMenuItemEntry* ent;
    GtkWidget* menu_item = NULL;

    for (ent = entries;; ++ent)
    {
        if (G_LIKELY(ent->label))
        {
            /* Stock item */
            const char* signal = "activate";
            if (G_UNLIKELY(PTK_IS_STOCK_ITEM(ent)))
            {
                menu_item = gtk_menu_item_new_with_mnemonic(ent->label);
            }
            else if (G_LIKELY(ent->stock_icon))
            {
                if (G_LIKELY(ent->stock_icon > (char*)2))
                {
                    menu_item = gtk_menu_item_new_with_mnemonic(_(ent->label));
                }
                else if (G_UNLIKELY(PTK_IS_CHECK_MENU_ITEM(ent)))
                {
                    menu_item = gtk_check_menu_item_new_with_mnemonic(_(ent->label));
                    signal = "toggled";
                }
                else if (G_UNLIKELY(PTK_IS_RADIO_MENU_ITEM(ent)))
                {
                    GSList* radio_group = NULL;
                    menu_item = gtk_radio_menu_item_new_with_mnemonic(radio_group, _(ent->label));
                    if (G_LIKELY(PTK_IS_RADIO_MENU_ITEM((ent + 1))))
                        radio_group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menu_item));
                    else
                        radio_group = NULL;
                    signal = "toggled";
                }
            }
            else
            {
                menu_item = gtk_menu_item_new_with_mnemonic(_(ent->label));
            }

            if (G_LIKELY(accel_group) && ent->key)
            {
                gtk_widget_add_accelerator(menu_item,
                                           "activate",
                                           accel_group,
                                           ent->key,
                                           ent->mod,
                                           GTK_ACCEL_VISIBLE);
            }

            if (G_LIKELY(ent->callback))
            { /* Callback */
                g_signal_connect(menu_item, signal, ent->callback, cb_data);
            }

            if (G_UNLIKELY(ent->sub_menu))
            { /* Sub menu */
                GtkWidget* sub_menu = ptk_menu_new_from_data(ent->sub_menu, cb_data, accel_group);
                gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), sub_menu);
                ent->menu = sub_menu; // MOD
            }
        }
        else
        {
            if (!ent->stock_icon) /* End of menu */
                break;
            menu_item = gtk_separator_menu_item_new();
        }

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
        if (G_UNLIKELY(ent->ret))
        { // Return
            *ent->ret = menu_item;
            ent->ret = NULL;
        }
    }
}

void ptk_show_error(GtkWindow* parent, const char* title, const char* message)
{
    char* msg = replace_string(message, "%", "%%", FALSE);
    GtkWidget* dlg = gtk_message_dialog_new(parent,
                                            GTK_DIALOG_MODAL,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_OK,
                                            msg,
                                            NULL);
    g_free(msg);
    if (title)
        gtk_window_set_title((GtkWindow*)dlg, title);
    xset_set_window_icon(GTK_WINDOW(dlg));
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

GtkBuilder* _gtk_builder_new_from_file(const char* file)
{
    char* filename = g_build_filename(PACKAGE_UI_DIR, file, NULL);
    GtkBuilder* builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, filename, NULL);

    return builder;
}

void transpose_nonlatin_keypress(GdkEventKey* event)
{
    if (!(event && event->keyval != 0))
        return;

    // is already a latin key?
    if ((GDK_KEY_0 <= event->keyval && event->keyval <= GDK_KEY_9) ||
        (GDK_KEY_A <= event->keyval && event->keyval <= GDK_KEY_Z) ||
        (GDK_KEY_a <= event->keyval && event->keyval <= GDK_KEY_z))
        return;

    // We have a non-latin char, try other keyboard groups
    GdkKeymapKey* keys = NULL;
    unsigned int* keyvals;
    int n_entries;
    int level;
    int n;

    GdkDisplay* display = gdk_display_get_default();

    if (gdk_keymap_translate_keyboard_state(gdk_keymap_get_for_display(display),
                                            event->hardware_keycode,
                                            (GdkModifierType)event->state,
                                            event->group,
                                            NULL,
                                            NULL,
                                            &level,
                                            NULL) &&
        gdk_keymap_get_entries_for_keycode(gdk_keymap_get_for_display(display),
                                           event->hardware_keycode,
                                           &keys,
                                           &keyvals,
                                           &n_entries))
    {
        for (n = 0; n < n_entries; n++)
        {
            if (keys[n].group == event->group)
                // Skip keys from the same group
                continue;
            if (keys[n].level != level)
                // Allow only same level keys
                continue;
            if ((GDK_KEY_0 <= keyvals[n] && keyvals[n] <= GDK_KEY_9) ||
                (GDK_KEY_A <= keyvals[n] && keyvals[n] <= GDK_KEY_Z) ||
                (GDK_KEY_a <= keyvals[n] && keyvals[n] <= GDK_KEY_z))
            {
                // Latin character found
                event->keyval = keyvals[n];
                break;
            }
        }
        g_free(keys);
        g_free(keyvals);
    }
}
