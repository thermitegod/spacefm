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

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "ptk/ptk-utils.hxx"

#include "settings.hxx"

void
ptk_show_error(GtkWindow* parent, const std::string& title, const std::string& message)
{
    std::string msg = ztd::replace(message.c_str(), "%", "%%");
    GtkWidget* dlg = gtk_message_dialog_new(parent,
                                            GTK_DIALOG_MODAL,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_OK,
                                            msg.c_str(),
                                            nullptr);
    if (!title.empty())
        gtk_window_set_title(GTK_WINDOW(dlg), title.c_str());
    xset_set_window_icon(GTK_WINDOW(dlg));
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

unsigned int
ptk_get_keymod(unsigned int event)
{
    return (event & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SUPER_MASK |
                     GDK_HYPER_MASK | GDK_META_MASK));
}

GtkBuilder*
_gtk_builder_new_from_file(const char* file)
{
    char* filename = g_build_filename(PACKAGE_UI_DIR, file, nullptr);
    GtkBuilder* builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, filename, nullptr);

    return builder;
}

#ifdef HAVE_NONLATIN
void
transpose_nonlatin_keypress(GdkEventKey* event)
{
    if (!(event && event->keyval != 0))
        return;

    // is already a latin key?
    if ((GDK_KEY_0 <= event->keyval && event->keyval <= GDK_KEY_9) ||
        (GDK_KEY_A <= event->keyval && event->keyval <= GDK_KEY_Z) ||
        (GDK_KEY_a <= event->keyval && event->keyval <= GDK_KEY_z))
        return;

    // We have a non-latin char, try other keyboard groups
    GdkKeymapKey* keys = nullptr;
    unsigned int* keyvals;
    int n_entries;
    int level;
    int n;

    GdkDisplay* display = gdk_display_get_default();

    if (gdk_keymap_translate_keyboard_state(gdk_keymap_get_for_display(display),
                                            event->hardware_keycode,
                                            (GdkModifierType)event->state,
                                            event->group,
                                            nullptr,
                                            nullptr,
                                            &level,
                                            nullptr) &&
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
