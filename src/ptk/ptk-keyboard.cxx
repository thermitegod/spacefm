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

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

u32
ptk_get_keymod(u32 event)
{
    return (event & (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK |
                     GdkModifierType::GDK_MOD1_MASK | GdkModifierType::GDK_SUPER_MASK |
                     GdkModifierType::GDK_HYPER_MASK | GdkModifierType::GDK_META_MASK));
}

#if defined(HAVE_NONLATIN_KEYBOARD_SUPPORT)
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
    u32* keyvals;
    i32 n_entries;
    i32 level;

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
        for (const auto i : ztd::range(n_entries))
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
        std::free(keys);
        std::free(keyvals);
    }
}
#endif
