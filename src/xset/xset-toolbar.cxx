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

#include "types.hxx"

#include "settings/settings.hxx"

#include "main-window.hxx"

#include "xset/xset.hxx"
#include "xset/xset-context-menu.hxx"
#include "xset/xset-custom.hxx"
#include "xset/xset-design.hxx"
#include "xset/xset-misc.hxx"
#include "xset/xset-toolbar.hxx"

#include "ptk/utils/ptk-utils.hxx"

#include "ptk/ptk-file-menu.hxx"

// must match xset::tool:: enum
static constexpr ztd::map<xset::tool, builtin_tool_data, 19> builtin_tools{{{
    {xset::tool::NOT,
     {
         std::nullopt,
         std::nullopt,
         std::nullopt,
     }},
    {xset::tool::custom,
     {
         std::nullopt,
         std::nullopt,
         std::nullopt,
     }},
    {xset::tool::devices,
     {
         "Show Devices",
         "gtk-harddisk",
         xset::name::panel1_show_devmon,
     }},
    {xset::tool::bookmarks,
     {
         "Show Bookmarks",
         "gtk-jump-to",
         std::nullopt,
     }},
    {xset::tool::tree,
     {
         "Show Tree",
         "folder",
         xset::name::panel1_show_dirtree,
     }},
    {xset::tool::home,
     {
         "Home",
         "gtk-home",
         xset::name::go_home,
     }},
    {xset::tool::DEFAULT,
     {
         "Default",
         "gtk-home",
         xset::name::go_default,
     }},
    {xset::tool::up,
     {
         "Up",
         "gtk-go-up",
         xset::name::go_up,
     }},
    {xset::tool::back,
     {
         "Back",
         "gtk-go-back",
         xset::name::go_back,
     }},
    {xset::tool::back_menu,
     {
         "Back History",
         "gtk-go-back",
         xset::name::go_back,
     }},
    {xset::tool::fwd,
     {
         "Forward",
         "gtk-go-forward",
         xset::name::go_forward,
     }},
    {xset::tool::fwd_menu,
     {
         "Forward History",
         "gtk-go-forward",
         xset::name::go_forward,
     }},
    {xset::tool::refresh,
     {
         "Refresh",
         "gtk-refresh",
         xset::name::view_refresh,
     }},
    {xset::tool::new_tab,
     {
         "New Tab",
         "gtk-add",
         xset::name::tab_new,
     }},
    {xset::tool::new_tab_here,
     {
         "New Tab",
         "gtk-add",
         xset::name::tab_new_here,
     }},
    {xset::tool::show_hidden,
     {
         "Show Hidden",
         "gtk-apply",
         xset::name::panel1_show_hidden,
     }},
    {xset::tool::show_thumb,
     {
         "Show Thumbnails",
         std::nullopt,
         xset::name::view_thumb,
     }},
    {xset::tool::large_icons,
     {
         "Large Icons",
         "zoom-in",
         xset::name::panel1_list_large,
     }},
    {xset::tool::invalid,
     {
         std::nullopt,
         std::nullopt,
         std::nullopt,
     }},
}}};

const ztd::map<xset::tool, builtin_tool_data, 19>&
xset_toolbar_builtin_tools() noexcept
{
    return builtin_tools;
}

void
xset_builtin_tool_activate(xset::tool tool_type, const xset_t& set, GdkEvent* event) noexcept
{
    xset_t set2;
    panel_t p = 0;
    xset::main_window_panel mode;
    ptk::browser* file_browser = nullptr;
    MainWindow* main_window = main_window_get_last_active();

    // set may be a submenu that does not match tool_type
    if (!set || set->lock || tool_type <= xset::tool::custom)
    {
        ztd::logger::warn("xset_builtin_tool_activate invalid");
        return;
    }
    // ztd::logger::info("xset_builtin_tool_activate  {}", set->menu_label);

    // get current browser, panel, and mode
    if (main_window)
    {
        file_browser = main_window->current_file_browser();
        p = file_browser->panel();
        mode = main_window->panel_context.at(p);
    }

    switch (tool_type)
    {
        case xset::tool::devices:
            set2 = xset_get_panel_mode(p, xset::panel::show_devmon, mode);
            set2->b = set2->b == xset::b::xtrue ? xset::b::unset : xset::b::xtrue;
            update_views_all_windows(nullptr, file_browser);
            break;
        case xset::tool::bookmarks:
            update_views_all_windows(nullptr, file_browser);
            break;
        case xset::tool::tree:
            set2 = xset_get_panel_mode(p, xset::panel::show_dirtree, mode);
            set2->b = set2->b == xset::b::xtrue ? xset::b::unset : xset::b::xtrue;
            update_views_all_windows(nullptr, file_browser);
            break;
        case xset::tool::home:
            file_browser->go_home();
            break;
        case xset::tool::DEFAULT:
            file_browser->go_default();
            break;
        case xset::tool::up:
            file_browser->go_up();
            break;
        case xset::tool::back:
            file_browser->go_back();
            break;
        case xset::tool::back_menu:
            file_browser->show_history_menu(true, event);
            break;
        case xset::tool::fwd:
            file_browser->go_forward();
            break;
        case xset::tool::fwd_menu:
            file_browser->show_history_menu(false, event);
            break;
        case xset::tool::refresh:
            file_browser->refresh();
            break;
        case xset::tool::new_tab:
            file_browser->new_tab();
            break;
        case xset::tool::new_tab_here:
            file_browser->new_tab_here();
            break;
        case xset::tool::show_hidden:
            set2 = xset_get_panel(p, xset::panel::show_hidden);
            set2->b = set2->b == xset::b::xtrue ? xset::b::unset : xset::b::xtrue;
            file_browser->show_hidden_files(set2->b);
            break;
        case xset::tool::show_thumb:
            main_window_toggle_thumbnails_all_windows();
            break;
        case xset::tool::large_icons:
            if (!file_browser->is_view_mode(ptk::browser::view_mode::icon_view))
            {
                xset_set_b_panel(p, xset::panel::list_large, !file_browser->using_large_icons());
                on_popup_list_large(nullptr, file_browser);
            }
            break;
        case xset::tool::NOT:
        case xset::tool::custom:
        case xset::tool::invalid:
            ztd::logger::warn("xset_builtin_tool_activate invalid tool_type");
    }
}

const xset_t
xset_new_builtin_toolitem(xset::tool tool_type) noexcept
{
    if (tool_type < xset::tool::devices || tool_type >= xset::tool::invalid)
    {
        return nullptr;
    }

    xset_t set = xset_custom_new();
    set->tool = tool_type;
    set->task = false;
    set->task_err = false;
    set->task_out = false;
    set->keep_terminal = false;

    return set;
}

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

    if ((button == 1 || button == 3) && keymod == 0)
    { // left or right click and no modifier
        if (button == 1)
        {
            // left click
            // ztd::logger::debug("set={}  menu={}", set->name, magic_enum::enum_name(set->menu_style));
            set->browser->on_action(set->xset_name);
            return true;
        }
        else // if ( button == 3 )
        {
            // right-click show design menu for submenu set
            xset_design_cb(nullptr, event, set);
            return true;
        }
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
