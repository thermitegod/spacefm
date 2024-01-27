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

#include <filesystem>

#include <array>

#include <unordered_map>

#include <optional>

#include <memory>

#include <ranges>

#include <cassert>

#include <gtkmm.h>
#include <glibmm.h>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "compat/gtk4-porting.hxx"

#include "types.hxx"

#include "main-window.hxx"

#include "xset/xset.hxx"
#include "xset/xset-context-menu.hxx"
#include "xset/xset-custom.hxx"
#include "xset/xset-design.hxx"
#include "xset/xset-misc.hxx"
#include "xset/xset-toolbar.hxx"
#include "xset/utils/xset-utils.hxx"

#include "vfs/vfs-user-dirs.hxx"

#include "ptk/utils/ptk-utils.hxx"

#include "ptk/ptk-file-menu.hxx"

// must match xset::tool:: enum
const std::unordered_map<xset::tool, builtin_tool_data> builtin_tools{
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
         "New Tab Here",
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
};

const std::unordered_map<xset::tool, builtin_tool_data>&
xset_toolbar_get_builtin_tools()
{
    return builtin_tools;
}

void
xset_builtin_tool_activate(xset::tool tool_type, const xset_t& set, GdkEvent* event)
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

const std::string
xset_get_builtin_toolitem_label(xset::tool tool_type)
{
    assert(tool_type != xset::tool::NOT);
    assert(tool_type != xset::tool::custom);
    // assert(tool_type != xset::tool::devices);

    const auto tool_name = builtin_tools.at(tool_type).name;
    if (tool_name)
    {
        return tool_name.value();
    }
    return "";
}

const xset_t
xset_new_builtin_toolitem(xset::tool tool_type)
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
on_tool_icon_button_press(GtkWidget* widget, GdkEvent* event, const xset_t& set)
{
    xset::job job = xset::job::invalid;

    const auto button = gdk_button_event_get_button(event);
    // ztd::logger::info("on_tool_icon_button_press  {}   button = {}", set->menu_label, button);

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

    switch (button)
    {
        case 1:
        case 3:
            // left or right click
            switch (keymod)
            {
                case 0:
                    // no modifier
                    if (button == 1)
                    {
                        // left click
                        if (set->tool == xset::tool::custom &&
                            set->menu_style == xset::menu::submenu)
                        {
                            if (set->child)
                            {
                                const xset_t set_child = xset_is(set->child.value());
                                // activate first item in custom submenu
                                xset_menu_cb(nullptr, set_child);
                            }
                        }
                        else if (set->tool == xset::tool::custom)
                        {
                            // activate
                            xset_menu_cb(nullptr, set);
                        }
                        else if (set->tool == xset::tool::back_menu)
                        {
                            xset_builtin_tool_activate(xset::tool::back, set, event);
                        }
                        else if (set->tool == xset::tool::fwd_menu)
                        {
                            xset_builtin_tool_activate(xset::tool::fwd, set, event);
                        }
                        else if (set->tool != xset::tool::NOT)
                        {
                            xset_builtin_tool_activate(set->tool, set, event);
                        }
                        return true;
                    }
                    else // if ( button == 3 )
                    {
                        // right-click show design menu for submenu set
                        xset_design_cb(nullptr, event, set);
                        return true;
                    }
                    break;
                case GdkModifierType::GDK_CONTROL_MASK:
                    // ctrl
                    job = xset::job::copy;
                    break;
#if (GTK_MAJOR_VERSION == 4)
                case GdkModifierType::GDK_ALT_MASK:
#elif (GTK_MAJOR_VERSION == 3)
                case GdkModifierType::GDK_MOD1_MASK:
#endif
                    // alt
                    job = xset::job::cut;
                    break;
                case GdkModifierType::GDK_SHIFT_MASK:
                    // shift
                    job = xset::job::paste;
                    break;
                default:
                    break;
            }
            break;
        case 2:
            // middle click
            switch (keymod)
            {
                case GdkModifierType::GDK_CONTROL_MASK:
                    // ctrl
                    job = xset::job::key;
                    break;
#if (GTK_MAJOR_VERSION == 4)
                case GdkModifierType::GDK_ALT_MASK:
#elif (GTK_MAJOR_VERSION == 3)
                case GdkModifierType::GDK_MOD1_MASK:
#endif
                    // alt
                    break;
                case (GdkModifierType::GDK_CONTROL_MASK | GdkModifierType::GDK_SHIFT_MASK):
                    // ctrl + shift
                    job = xset::job::remove;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }

    if (job != xset::job::invalid)
    {
        if (xset_job_is_valid(set, job))
        {
            g_object_set_data(G_OBJECT(widget), "job", GINT_TO_POINTER(job));
            xset_design_job(widget, set);
        }
        else
        {
            // right-click show design menu for submenu set
            xset_design_cb(nullptr, event, set);
        }
        return true;
    }
    return true;
}

static bool
on_tool_menu_button_press(GtkWidget* widget, GdkEvent* event, const xset_t& set)
{
    const auto button = gdk_button_event_get_button(event);
    // ztd::logger::info("on_tool_menu_button_press  {}   button = {}", set->menu_label,  button);

    const auto type = gdk_event_get_event_type(event);
    if (type != GdkEventType::GDK_BUTTON_PRESS)
    {
        return false;
    }
    const auto keymod = ptk::utils::get_keymod(gdk_event_get_modifier_state(event));

    if (keymod != 0 || button != 1)
    {
        return on_tool_icon_button_press(widget, event, set);
    }

    // get and focus browser
    ptk::browser* file_browser = PTK_FILE_BROWSER(g_object_get_data(G_OBJECT(widget), "browser"));
    file_browser->focus_me();

    if (button == 1)
    {
        if (set->tool == xset::tool::custom)
        {
            // show custom submenu
            xset_t set_child;
            if (!set || set->lock || !set->child || set->menu_style != xset::menu::submenu ||
                !(set_child = xset_is(set->child.value())))
            {
                return true;
            }
            GtkWidget* menu = gtk_menu_new();

#if (GTK_MAJOR_VERSION == 4)
            GtkEventController* accel_group = gtk_shortcut_controller_new();
#elif (GTK_MAJOR_VERSION == 3)
            GtkAccelGroup* accel_group = gtk_accel_group_new();
#endif

            xset_add_menuitem(file_browser, menu, accel_group, set_child);
            gtk_widget_show_all(GTK_WIDGET(menu));
            gtk_menu_popup_at_pointer(GTK_MENU(menu), nullptr);
        }
        else
        {
            xset_builtin_tool_activate(set->tool, set, event);
        }
        return true;
    }
    return true;
}

static GtkWidget*
xset_add_toolitem(GtkWidget* parent, ptk::browser* file_browser, GtkToolbar* toolbar, i32 icon_size,
                  const xset_t& set, bool show_tooltips)
{
    if (!set)
    {
        return nullptr;
    }

    if (set->lock)
    {
        return nullptr;
    }

    if (set->tool == xset::tool::NOT)
    {
        ztd::logger::warn("xset_add_toolitem set->tool == xset::tool::NOT");
        set->tool = xset::tool::custom;
    }

    GtkWidget* image = nullptr;
    GtkWidget* item = nullptr;
    xset_t set_next;
    GdkPixbuf* pixbuf = nullptr;
    std::string str;

    // get real icon size from gtk icon size
    i32 icon_w = 0;
    i32 icon_h = 0;
    gtk_icon_size_lookup((GtkIconSize)icon_size, &icon_w, &icon_h);
    const i32 real_icon_size = icon_w > icon_h ? icon_w : icon_h;

    set->browser = file_browser;

    // builtin toolitems set shared_key on build
    if (set->tool >= xset::tool::invalid)
    {
        // looks like an unknown built-in toolitem from a future version - skip
        if (set->next)
        {
            set_next = xset_is(set->next.value());
            // ztd::logger::info("    NEXT {}", set_next->name);
            xset_add_toolitem(parent, file_browser, toolbar, icon_size, set_next, show_tooltips);
        }
        return item;
    }
    if (set->tool > xset::tool::custom && set->tool < xset::tool::invalid && !set->shared_key)
    {
        const auto shared_key = builtin_tools.at(set->tool).shared_key;
        if (shared_key)
        {
            set->shared_key = xset_get(shared_key.value());
        }
    }

    // builtin toolitems do not have menu_style set
    xset::menu menu_style;
    switch (set->tool)
    {
        case xset::tool::devices:
        case xset::tool::bookmarks:
        case xset::tool::tree:
        case xset::tool::show_hidden:
        case xset::tool::show_thumb:
        case xset::tool::large_icons:
            menu_style = xset::menu::check;
            break;
        case xset::tool::back_menu:
        case xset::tool::fwd_menu:
            menu_style = xset::menu::submenu;
            break;
        case xset::tool::NOT:
        case xset::tool::custom:
        case xset::tool::home:
        case xset::tool::DEFAULT:
        case xset::tool::up:
        case xset::tool::back:
        case xset::tool::fwd:
        case xset::tool::refresh:
        case xset::tool::new_tab:
        case xset::tool::new_tab_here:
        case xset::tool::invalid:
            menu_style = set->menu_style;
    }

    std::optional<std::string> icon_name = set->icon;
    if (!icon_name && set->tool == xset::tool::custom)
    {
        // custom 'icon' file?
        const std::filesystem::path icon_file =
            vfs::program::config() / "scripts" / set->name / "icon";
        if (std::filesystem::exists(icon_file))
        {
            icon_name = icon_file;
        }
    }

    std::optional<std::string> new_menu_label = std::nullopt;
    auto menu_label = set->menu_label;
    if (!menu_label && set->tool > xset::tool::custom)
    {
        menu_label = xset_get_builtin_toolitem_label(set->tool);
    }

    if (menu_style == xset::menu::normal)
    {
        menu_style = xset::menu::string;
    }

    GtkBox* hbox = nullptr;
    xset_t set_child;
    xset::cmd cmd_type;

    switch (menu_style)
    {
        case xset::menu::string:
        {
            // normal item
            cmd_type = xset::cmd(xset_get_int(set, xset::var::x));
            if (set->tool > xset::tool::custom)
            {
                // builtin tool item
                if (icon_name)
                {
                    image = xset_get_image(icon_name.value(), (GtkIconSize)icon_size);
                }
                else if (set->tool > xset::tool::custom && set->tool < xset::tool::invalid)
                {
                    image = xset_get_image(builtin_tools.at(set->tool).icon.value(),
                                           (GtkIconSize)icon_size);
                }
            }
            else if (!set->lock && cmd_type == xset::cmd::app)
            {
                // Application
                new_menu_label = xset_custom_get_app_name_icon(set, &pixbuf, real_icon_size);
            }

            if (pixbuf)
            {
                image = gtk_image_new_from_pixbuf(pixbuf);
                g_object_unref(pixbuf);
            }
            if (!image)
            {
                image = xset_get_image(icon_name ? icon_name.value() : "gtk-execute",
                                       (GtkIconSize)icon_size);
            }
            if (!new_menu_label)
            {
                new_menu_label = menu_label;
            }

            // cannot use gtk_tool_button_new because icon does not obey size
            // btn = GTK_WIDGET( gtk_tool_button_new( image, new_menu_label ) );
            GtkButton* btn = GTK_BUTTON(gtk_button_new());
            gtk_widget_show(GTK_WIDGET(image));
            gtk_button_set_image(btn, image);
            gtk_button_set_relief(btn, GTK_RELIEF_NONE);
            // These do not seem to do anything
            gtk_widget_set_margin_start(GTK_WIDGET(btn), 0);
            gtk_widget_set_margin_end(GTK_WIDGET(btn), 0);
            gtk_widget_set_margin_top(GTK_WIDGET(btn), 0);
            gtk_widget_set_margin_bottom(GTK_WIDGET(btn), 0);
            gtk_widget_set_hexpand(GTK_WIDGET(btn), false);
            gtk_widget_set_vexpand(GTK_WIDGET(btn), false);
            gtk_button_set_always_show_image(btn, true);

            // create tool item containing an ebox to capture click on button
            item = GTK_WIDGET(gtk_tool_item_new());
            GtkEventBox* ebox = GTK_EVENT_BOX(gtk_event_box_new());
            gtk_container_add(GTK_CONTAINER(item), GTK_WIDGET(ebox));
            gtk_container_add(GTK_CONTAINER(ebox), GTK_WIDGET(btn));
            gtk_event_box_set_visible_window(ebox, false);
            gtk_event_box_set_above_child(ebox, true);
            // clang-format off
            g_signal_connect(G_OBJECT(ebox), "button-press-event", G_CALLBACK(on_tool_icon_button_press), set.get());
            // clang-format on
            g_object_set_data(G_OBJECT(ebox), "browser", file_browser);
            ptk_file_browser_add_toolbar_widget(set, GTK_WIDGET(btn));

            // tooltip
            if (show_tooltips)
            {
                str = xset::utils::clean_label(new_menu_label.value(), false, false);
                gtk_widget_set_tooltip_text(GTK_WIDGET(ebox), str.data());
            }
            break;
        }
        case xset::menu::check:
        {
            if (!icon_name && set->tool > xset::tool::custom && set->tool < xset::tool::invalid)
            {
                // builtin tool item
                image = xset_get_image(builtin_tools.at(set->tool).icon.value(),
                                       (GtkIconSize)icon_size);
            }
            else
            {
                image = xset_get_image(icon_name ? icon_name.value() : "gtk-execute",
                                       (GtkIconSize)icon_size);
            }

            // cannot use gtk_tool_button_new because icon does not obey size
            // btn = GTK_WIDGET( gtk_toggle_tool_button_new() );
            // gtk_tool_button_set_icon_widget( GTK_TOOL_BUTTON( btn ), image );
            // gtk_tool_button_set_label( GTK_TOOL_BUTTON( btn ), set->menu_label );
            GtkToggleButton* check_btn = GTK_TOGGLE_BUTTON(gtk_toggle_button_new());
            gtk_widget_show(GTK_WIDGET(image));
            gtk_button_set_image(GTK_BUTTON(check_btn), image);
            gtk_button_set_relief(GTK_BUTTON(check_btn), GTK_RELIEF_NONE);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_btn), xset_get_b(set));
            gtk_widget_set_margin_start(GTK_WIDGET(check_btn), 0);
            gtk_widget_set_margin_end(GTK_WIDGET(check_btn), 0);
            gtk_widget_set_margin_top(GTK_WIDGET(check_btn), 0);
            gtk_widget_set_margin_bottom(GTK_WIDGET(check_btn), 0);
            gtk_widget_set_hexpand(GTK_WIDGET(check_btn), false);
            gtk_widget_set_vexpand(GTK_WIDGET(check_btn), false);
            gtk_button_set_always_show_image(GTK_BUTTON(check_btn), true);

            // create tool item containing an ebox to capture click on button
            item = GTK_WIDGET(gtk_tool_item_new());
            GtkEventBox* ebox = GTK_EVENT_BOX(gtk_event_box_new());
            gtk_container_add(GTK_CONTAINER(item), GTK_WIDGET(ebox));
            gtk_container_add(GTK_CONTAINER(ebox), GTK_WIDGET(check_btn));
            gtk_event_box_set_visible_window(ebox, false);
            gtk_event_box_set_above_child(ebox, true);
            // clang-format off
            g_signal_connect(G_OBJECT(ebox), "button-press-event", G_CALLBACK(on_tool_icon_button_press), set.get());
            // clang-format on
            g_object_set_data(G_OBJECT(ebox), "browser", file_browser);
            ptk_file_browser_add_toolbar_widget(set, GTK_WIDGET(check_btn));

            // tooltip
            if (show_tooltips)
            {
                str = xset::utils::clean_label(menu_label.value(), false, false);
                gtk_widget_set_tooltip_text(GTK_WIDGET(ebox), str.data());
            }
            break;
        }
        case xset::menu::submenu:
        {
            menu_label = std::nullopt;
            // create a tool button
            set_child = nullptr;
            if (set->child && set->tool == xset::tool::custom)
            {
                set_child = xset_is(set->child.value());
            }

            if (!icon_name && set_child && set_child->icon)
            {
                // take the user icon from the first item in the submenu
                icon_name = set_child->icon;
            }
            else if (!icon_name && set->tool > xset::tool::custom &&
                     set->tool < xset::tool::invalid)
            {
                icon_name = builtin_tools.at(set->tool).icon;
            }
            else if (!icon_name && set_child && set->tool == xset::tool::custom)
            {
                // take the auto icon from the first item in the submenu
                cmd_type = xset::cmd(xset_get_int(set_child, xset::var::x));
                switch (cmd_type)
                {
                    case xset::cmd::app:
                        // Application
                        new_menu_label = menu_label =
                            xset_custom_get_app_name_icon(set_child, &pixbuf, real_icon_size);
                        break;
                    case xset::cmd::bookmark:
                    case xset::cmd::script:
                    case xset::cmd::line:
                    case xset::cmd::invalid:
                        icon_name = "gtk-execute";
                        break;
                }

                if (pixbuf)
                {
                    image = gtk_image_new_from_pixbuf(pixbuf);
                    g_object_unref(pixbuf);
                }
            }

            if (!menu_label)
            {
                switch (set->tool)
                {
                    case xset::tool::back_menu:
                        menu_label = builtin_tools.at(xset::tool::back).name;
                        break;
                    case xset::tool::fwd_menu:
                        menu_label = builtin_tools.at(xset::tool::fwd).name;
                        break;
                    case xset::tool::custom:
                        if (set_child)
                        {
                            menu_label = set_child->menu_label;
                        }
                        break;
                    case xset::tool::NOT:
                    case xset::tool::devices:
                    case xset::tool::bookmarks:
                    case xset::tool::tree:
                    case xset::tool::home:
                    case xset::tool::DEFAULT:
                    case xset::tool::up:
                    case xset::tool::back:
                    case xset::tool::fwd:
                    case xset::tool::refresh:
                    case xset::tool::new_tab:
                    case xset::tool::new_tab_here:
                    case xset::tool::show_hidden:
                    case xset::tool::show_thumb:
                    case xset::tool::large_icons:
                    case xset::tool::invalid:
                        if (!set->menu_label)
                        {
                            menu_label = xset_get_builtin_toolitem_label(set->tool);
                        }
                        else
                        {
                            menu_label = set->menu_label;
                        }
                        break;
                }
            }

            if (!image)
            {
                image = xset_get_image(icon_name ? icon_name.value() : "folder",
                                       (GtkIconSize)icon_size);
            }

            // cannot use gtk_tool_button_new because icon does not obey size
            // btn = GTK_WIDGET( gtk_tool_button_new( image, menu_label ) );
            GtkButton* btn = GTK_BUTTON(gtk_button_new());
            gtk_widget_show(GTK_WIDGET(image));
            gtk_button_set_image(btn, image);
            gtk_button_set_relief(btn, GTK_RELIEF_NONE);
            gtk_widget_set_margin_start(GTK_WIDGET(btn), 0);
            gtk_widget_set_margin_end(GTK_WIDGET(btn), 0);
            gtk_widget_set_margin_top(GTK_WIDGET(btn), 0);
            gtk_widget_set_margin_bottom(GTK_WIDGET(btn), 0);
            gtk_widget_set_hexpand(GTK_WIDGET(btn), false);
            gtk_widget_set_vexpand(GTK_WIDGET(btn), false);
            gtk_button_set_always_show_image(btn, true);

            GtkEventBox* ebox = nullptr;

            // create eventbox for btn
            ebox = GTK_EVENT_BOX(gtk_event_box_new());
            gtk_event_box_set_visible_window(ebox, false);
            gtk_event_box_set_above_child(ebox, true);
            gtk_container_add(GTK_CONTAINER(ebox), GTK_WIDGET(btn));
            // clang-format off
            g_signal_connect(G_OBJECT(ebox), "button_press_event", G_CALLBACK(on_tool_icon_button_press), set.get());
            // clang-format on
            g_object_set_data(G_OBJECT(ebox), "browser", file_browser);
            ptk_file_browser_add_toolbar_widget(set, GTK_WIDGET(btn));

            // pack into hbox
            hbox = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0));
            gtk_box_pack_start(hbox, GTK_WIDGET(ebox), false, false, 0);
            // tooltip
            if (show_tooltips)
            {
                str = xset::utils::clean_label(menu_label.value(), false, false);
                gtk_widget_set_tooltip_text(GTK_WIDGET(ebox), str.data());
            }

            // reset menu_label for below
            menu_label = set->menu_label;
            if (!menu_label && set->tool > xset::tool::custom)
            {
                menu_label = xset_get_builtin_toolitem_label(set->tool);
            }

            ///////// create a menu_tool_button to steal the button from
            ebox = GTK_EVENT_BOX(gtk_event_box_new());
            gtk_event_box_set_visible_window(ebox, false);
            gtk_event_box_set_above_child(ebox, true);
            GtkWidget* menu_btn = nullptr;
            GtkWidget* hbox_menu = nullptr;
            GList* children = nullptr;

            menu_btn = GTK_WIDGET(gtk_menu_tool_button_new(nullptr, nullptr));
            hbox_menu = gtk_bin_get_child(GTK_BIN(menu_btn));
            children = gtk_container_get_children(GTK_CONTAINER(hbox_menu));
            btn = GTK_BUTTON(children->next->data);
            if (!btn)
            {
                // failed so just create a button
                btn = GTK_BUTTON(gtk_button_new());
                gtk_button_set_label(btn, ".");
                gtk_button_set_relief(btn, GTK_RELIEF_NONE);
                gtk_container_add(GTK_CONTAINER(ebox), GTK_WIDGET(btn));
            }
            else
            {
                // steal the drop-down button
                gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(GTK_WIDGET(btn))),
                                     GTK_WIDGET(btn));
                gtk_container_add(GTK_CONTAINER(ebox), GTK_WIDGET(btn));

                gtk_button_set_relief(btn, GTK_RELIEF_NONE);
            }
            gtk_widget_set_margin_start(GTK_WIDGET(btn), 0);
            gtk_widget_set_margin_end(GTK_WIDGET(btn), 0);
            gtk_widget_set_margin_top(GTK_WIDGET(btn), 0);
            gtk_widget_set_margin_bottom(GTK_WIDGET(btn), 0);
            gtk_widget_set_hexpand(GTK_WIDGET(btn), false);
            gtk_widget_set_vexpand(GTK_WIDGET(btn), false);
            gtk_button_set_always_show_image(btn, true);

            g_list_free(children);
            gtk_widget_destroy(menu_btn);

            gtk_box_pack_start(hbox, GTK_WIDGET(ebox), false, false, 0);
            // clang-format off
            g_signal_connect(G_OBJECT(ebox), "button_press_event", G_CALLBACK(on_tool_menu_button_press), set.get());
            // clang-format on
            g_object_set_data(G_OBJECT(ebox), "browser", file_browser);
            ptk_file_browser_add_toolbar_widget(set, GTK_WIDGET(btn));

            item = GTK_WIDGET(gtk_tool_item_new());
            gtk_container_add(GTK_CONTAINER(item), GTK_WIDGET(hbox));
            gtk_widget_show_all(GTK_WIDGET(item));

            // tooltip
            if (show_tooltips)
            {
                str = xset::utils::clean_label(menu_label.value(), false, false);
                gtk_widget_set_tooltip_text(GTK_WIDGET(ebox), str.data());
            }
            break;
        }
        case xset::menu::sep:
        {
            // create tool item containing an ebox to capture click on sep
            GtkWidget* sep = GTK_WIDGET(gtk_separator_tool_item_new());
            gtk_separator_tool_item_set_draw(GTK_SEPARATOR_TOOL_ITEM(sep), true);
            item = GTK_WIDGET(gtk_tool_item_new());
            GtkEventBox* ebox = GTK_EVENT_BOX(gtk_event_box_new());
            gtk_container_add(GTK_CONTAINER(item), GTK_WIDGET(ebox));
            gtk_container_add(GTK_CONTAINER(ebox), GTK_WIDGET(sep));
            gtk_event_box_set_visible_window(ebox, false);
            gtk_event_box_set_above_child(ebox, true);
            // clang-format off
            g_signal_connect(G_OBJECT(ebox), "button-press-event", G_CALLBACK(on_tool_icon_button_press), set.get());
            // clang-format on
            g_object_set_data(G_OBJECT(ebox), "browser", file_browser);
            break;
        }
        case xset::menu::normal:
        case xset::menu::radio:
        case xset::menu::reserved_00:
        case xset::menu::reserved_01:
        case xset::menu::reserved_02:
        case xset::menu::reserved_03:
        case xset::menu::reserved_04:
        case xset::menu::reserved_05:
        case xset::menu::reserved_06:
        case xset::menu::reserved_07:
        case xset::menu::reserved_08:
        case xset::menu::reserved_09:
        case xset::menu::reserved_10:
        case xset::menu::reserved_11:
        case xset::menu::reserved_12:
            return nullptr;
    }

    gtk_toolbar_insert(toolbar, GTK_TOOL_ITEM(item), -1);

    // ztd::logger::info("    set={}   set->next={}", set->name, set->next);
    // next toolitem
    if (set->next)
    {
        set_next = xset_is(set->next.value());
        // ztd::logger::info("    NEXT {}", set_next->name);
        xset_add_toolitem(parent, file_browser, toolbar, icon_size, set_next, show_tooltips);
    }

    return item;
}

void
xset_fill_toolbar(GtkWidget* parent, ptk::browser* file_browser, GtkToolbar* toolbar,
                  const xset_t& set_parent, bool show_tooltips)
{
    static constexpr std::array<xset::tool, 7> default_tools{
        xset::tool::bookmarks,
        xset::tool::tree,
        xset::tool::new_tab_here,
        xset::tool::back_menu,
        xset::tool::fwd_menu,
        xset::tool::up,
        xset::tool::DEFAULT,
    };
    i32 stop_b4 = 0;
    xset_t set;
    xset_t set_target;

    // ztd::logger::info("xset_fill_toolbar {}", set_parent->name);
    if (!(file_browser && toolbar && set_parent))
    {
        return;
    }

    set_parent->lock = true;
    set_parent->menu_style = xset::menu::submenu;

    const GtkIconSize icon_size = gtk_toolbar_get_icon_size(toolbar);

    xset_t set_child;
    if (set_parent->child)
    {
        set_child = xset_is(set_parent->child.value());
    }
    if (!set_child)
    {
        // toolbar is empty - add default items
        set_child = xset_new_builtin_toolitem((set_parent->xset_name == xset::name::tool_r)
                                                  ? xset::tool::refresh
                                                  : xset::tool::devices);
        set_parent->child = set_child->name;
        set_child->parent = set_parent->name;
        if (set_parent->xset_name != xset::name::tool_r)
        {
            if (set_parent->xset_name == xset::name::tool_s)
            {
                stop_b4 = 2;
            }
            else
            {
                stop_b4 = default_tools.size();
            }
            set_target = set_child;
            for (const auto i : std::views::iota(0z, stop_b4))
            {
                set = xset_new_builtin_toolitem(default_tools.at(i));
                xset_custom_insert_after(set_target, set);
                set_target = set;
            }
        }
    }

    xset_add_toolitem(parent, file_browser, toolbar, icon_size, set_child, show_tooltips);

    // These do not seem to do anything
    gtk_widget_set_margin_start(GTK_WIDGET(toolbar), 0);
    gtk_widget_set_margin_end(GTK_WIDGET(toolbar), 0);
    gtk_widget_set_margin_top(GTK_WIDGET(toolbar), 0);
    gtk_widget_set_margin_bottom(GTK_WIDGET(toolbar), 0);

    gtk_widget_show_all(GTK_WIDGET(toolbar));
}
