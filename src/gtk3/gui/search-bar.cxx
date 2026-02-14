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

#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "compat/gtk4-porting.hxx"

#include "xset/xset-context-menu.hxx"
#include "xset/xset.hxx"

#include "gui/search-bar.hxx"

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

    const std::string text = gtk_entry_get_text(GTK_ENTRY(entry));

    const auto keyval = gdk_key_event_get_keyval(event);
    if (keyval == GDK_KEY_Return)
    {
        auto* const browser =
            static_cast<gui::browser*>(g_object_get_data(G_OBJECT(entry), "browser"));

        if (xset_get_b(xset::name::search_select))
        {
            if (!text.empty())
            {
                browser->select_pattern(text);
            }
        }
        else
        {
            browser->update_model(text);
        }

        gtk_entry_set_text(GTK_ENTRY(entry), "");

        browser->focus(gui::browser::focus_widget::filelist);
    }

    return false;
}

static void
on_populate_popup(GtkEntry* entry, GtkMenu* menu, gui::browser* browser) noexcept
{
    (void)entry;

    if (!browser)
    {
        return;
    }

    GtkAccelGroup* accel_group = gtk_accel_group_new();

    {
        const auto set = xset::set::get(xset::name::separator);
        xset_add_menuitem(browser, GTK_WIDGET(menu), accel_group, set);
    }

    {
        const auto set = xset::set::get(xset::name::search_select);
        xset_add_menuitem(browser, GTK_WIDGET(menu), accel_group, set);
    }

    gtk_widget_show_all(GTK_WIDGET(menu));
}

GtkEntry*
gui::search_bar_new(gui::browser* browser) noexcept
{
    GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(entry, "Search");
    gtk_entry_set_has_frame(entry, true);
    gtk_widget_set_size_request(GTK_WIDGET(entry), 50, -1);

    // clang-format off
    g_signal_connect(G_OBJECT(entry), "focus-in-event", G_CALLBACK(on_focus_in), nullptr);
    g_signal_connect(G_OBJECT(entry), "focus-out-event", G_CALLBACK(on_focus_out), nullptr);
    g_signal_connect(G_OBJECT(entry), "key-press-event", G_CALLBACK(on_key_press), nullptr);
    g_signal_connect(G_OBJECT(entry), "populate-popup", G_CALLBACK(on_populate_popup), browser);
    // clang-format on

    g_object_set_data(G_OBJECT(entry), "browser", browser);

    return entry;
}
