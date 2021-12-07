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

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include "settings.hxx"
#include "utils.hxx"

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

#ifdef HAVE_NONLATIN
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
#endif
