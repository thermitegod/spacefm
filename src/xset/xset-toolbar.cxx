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

#include <span>

#include <optional>

#include <memory>

#include <gtkmm.h>
#include <glibmm.h>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "compat/gtk4-porting.hxx"

#include "settings/settings.hxx"

#include "xset/xset.hxx"
#include "xset/xset-misc.hxx"
#include "xset/xset-toolbar.hxx"

#include "ptk/ptk-file-browser.hxx"
#include "ptk/utils/ptk-utils.hxx"

static bool
on_tool_icon_button_press(GtkWidget* widget, GdkEvent* event, const xset_t& set) noexcept
{
    const auto button = gdk_button_event_get_button(event);
    // ztd::logger::info("on_tool_icon_button_press  {}   button = {}", set->menu_label.value_or(""), button);

    const auto type = gdk_event_get_event_type(event);
    if (type != GdkEventType::GDK_BUTTON_PRESS)
    {
        return false;
    }
    const auto keymod = ptk::utils::get_keymod(gdk_event_get_modifier_state(event));

    // get and focus browser
    ptk::browser* file_browser = PTK_FILE_BROWSER(g_object_get_data(G_OBJECT(widget), "browser"));
    file_browser->focus_me();
    set->browser = file_browser;

    if (button == 1 && keymod == 0)
    { // left click and no modifier
        // ztd::logger::debug("set={}  menu={}", set->name, magic_enum::enum_name(set->menu_style));
        set->browser->on_action(set->xset_name);
        return true;
    }
    return true;
}

static void
xset_add_toolitem(ptk::browser* file_browser, GtkBox* toolbar, const i32 icon_size,
                  const xset_t& set) noexcept
{
    set->browser = file_browser;

    // get real icon size from gtk icon size
    i32 icon_w = 0;
    i32 icon_h = 0;
    gtk_icon_size_lookup((GtkIconSize)icon_size, &icon_w, &icon_h);

    GtkWidget* image = nullptr;

    // builtin tool item
    if (set->icon)
    {
        image = xset_get_image(set->icon.value(), (GtkIconSize)icon_size);
    }
    else
    {
        ztd::logger::warn("set missing icon {}", set->name);
        image = xset_get_image("gtk-execute", (GtkIconSize)icon_size);
    }

    GtkButton* btn = GTK_BUTTON(gtk_button_new());
    gtk_widget_show(GTK_WIDGET(image));
    gtk_button_set_image(btn, image);
    gtk_button_set_always_show_image(btn, true);
    gtk_button_set_relief(btn, GTK_RELIEF_NONE);

    // clang-format off
    g_signal_connect(G_OBJECT(btn), "button-press-event", G_CALLBACK(on_tool_icon_button_press), set.get());
    // clang-format on

    g_object_set_data(G_OBJECT(btn), "browser", file_browser);

    gtk_box_pack_start(toolbar, GTK_WIDGET(btn), false, false, 0);
}

void
xset_fill_toolbar(ptk::browser* file_browser, GtkBox* toolbar,
                  const std::span<const xset::name> toolbar_items) noexcept
{
    for (const auto item : toolbar_items)
    {
        xset_t set = xset_get(item);
        xset_add_toolitem(file_browser, toolbar, config::settings->icon_size_tool(), set);
    }
}
