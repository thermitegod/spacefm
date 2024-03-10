/**
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

#include <string>

#include <gtkmm.h>
#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "compat/gtk4-porting.hxx"

#include "xset/xset.hxx"
#include "xset/xset-context-menu.hxx"

#include "ptk/ptk-search-bar.hxx"

static bool
on_focus_in(GtkWidget* entry, GdkEventFocus* evt, void* user_data) noexcept
{
    (void)entry;
    (void)evt;
    (void)user_data;
    return false;
}

static bool
on_focus_out(GtkWidget* entry, GdkEventFocus* evt, void* user_data) noexcept
{
    (void)entry;
    (void)evt;
    (void)user_data;
    return false;
}

static bool
on_key_press(GtkWidget* entry, GdkEvent* event, void* user_data) noexcept
{
    (void)event;
    (void)user_data;

#if (GTK_MAJOR_VERSION == 4)
    const std::string text = gtk_editable_get_text(GTK_EDITABLE(entry));
#elif (GTK_MAJOR_VERSION == 3)
    const std::string text = gtk_entry_get_text(GTK_ENTRY(entry));
#endif

    const auto keyval = gdk_key_event_get_keyval(event);
    if (keyval == GDK_KEY_Return)
    {
        auto* const file_browser =
            static_cast<ptk::browser*>(g_object_get_data(G_OBJECT(entry), "browser"));

        if (xset_get_b(xset::name::search_select))
        {
            if (!text.empty())
            {
                file_browser->select_pattern(text);
            }
        }
        else
        {
            file_browser->update_model(text);
        }

#if (GTK_MAJOR_VERSION == 4)
        gtk_editable_set_text(GTK_EDITABLE(entry), "");
#elif (GTK_MAJOR_VERSION == 3)
        gtk_entry_set_text(GTK_ENTRY(entry), "");
#endif
    }

    return false;
}

static void
on_populate_popup(GtkEntry* entry, GtkMenu* menu, ptk::browser* file_browser) noexcept
{
    (void)entry;

    if (!file_browser)
    {
        return;
    }

#if (GTK_MAJOR_VERSION == 4)
    GtkEventController* accel_group = gtk_shortcut_controller_new();
#elif (GTK_MAJOR_VERSION == 3)
    GtkAccelGroup* accel_group = gtk_accel_group_new();
#endif

    xset_t set;

    set = xset_get(xset::name::separator);
    xset_add_menuitem(file_browser, GTK_WIDGET(menu), accel_group, set);

    set = xset_get(xset::name::search_select);
    xset_add_menuitem(file_browser, GTK_WIDGET(menu), accel_group, set);

    gtk_widget_show_all(GTK_WIDGET(menu));
}

GtkEntry*
ptk::search_bar_new(ptk::browser* file_browser) noexcept
{
    GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(entry, "Search");
    gtk_entry_set_has_frame(entry, true);
    gtk_widget_set_size_request(GTK_WIDGET(entry), 50, -1);

    // clang-format off
    g_signal_connect(G_OBJECT(entry), "focus-in-event", G_CALLBACK(on_focus_in), nullptr);
    g_signal_connect(G_OBJECT(entry), "focus-out-event", G_CALLBACK(on_focus_out), nullptr);
    g_signal_connect(G_OBJECT(entry), "key-press-event", G_CALLBACK(on_key_press), nullptr);
    g_signal_connect(G_OBJECT(entry), "populate-popup", G_CALLBACK(on_populate_popup), file_browser);
    // clang-format on

    g_object_set_data(G_OBJECT(entry), "browser", file_browser);

    return entry;
}
