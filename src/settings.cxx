/**
 * Copyright (C) 2015 IgnorantGuru <ignorantguru@gmx.com>
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

#include <string>
#include <string_view>

#include <format>

#include <filesystem>

#include <array>
#include <vector>

#include <map>

#include <optional>

#include <memory>

#include <chrono>

#include <cassert>

#include <gtkmm.h>
#include <glibmm.h>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "compat/gtk4-porting.hxx"

#include "types.hxx"

#include "settings.hxx"
#include "main-window.hxx"

#include "scripts.hxx"

#include "xset/xset.hxx"
#include "xset/xset-custom.hxx"
#include "xset/xset-defaults.hxx"
#include "xset/xset-design.hxx"
#include "xset/xset-design-clipboard.hxx"
#include "xset/xset-dialog.hxx"
#include "xset/xset-static-strings.hxx"

#include "settings/app.hxx"
#include "settings/config-load.hxx"
#include "settings/config-save.hxx"

#include "terminal-handlers.hxx"

#include "autosave.hxx"
#include "write.hxx"
#include "utils.hxx"

#include "vfs/vfs-app-desktop.hxx"
#include "vfs/vfs-file-task.hxx"
#include "vfs/vfs-mime-type.hxx"
#include "vfs/vfs-utils.hxx"
#include "vfs/vfs-user-dirs.hxx"

#include "ptk/ptk-dialog.hxx"
#include "ptk/ptk-keyboard.hxx"
#include "ptk/ptk-app-chooser.hxx"
#include "ptk/ptk-file-menu.hxx"
#include "ptk/ptk-file-task.hxx"
#include "ptk/ptk-location-view.hxx"

// MOD settings

static bool xset_design_cb(GtkWidget* item, GdkEventButton* event, xset_t set);
static void xset_builtin_tool_activate(xset::tool tool_type, xset_t set, GdkEventButton* event);

struct builtin_tool_data
{
    std::optional<std::string> name;
    std::optional<std::string> icon;
    std::optional<std::string> shared_key;
};

// must match xset::tool:: enum
const std::map<xset::tool, builtin_tool_data> builtin_tools{
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
         "panel1_show_devmon",
     }},
    {xset::tool::bookmarks,
     {
         "Show Bookmarks",
         "gtk-jump-to",
         "panel1_show_book",
     }},
    {xset::tool::tree,
     {
         "Show Tree",
         "gtk-directory",
         "panel1_show_dirtree",
     }},
    {xset::tool::home,
     {
         "Home",
         "gtk-home",
         "go_home",
     }},
    {xset::tool::DEFAULT,
     {
         "Default",
         "gtk-home",
         "go_default",
     }},
    {xset::tool::up,
     {
         "Up",
         "gtk-go-up",
         "go_up",
     }},
    {xset::tool::back,
     {
         "Back",
         "gtk-go-back",
         "go_back",
     }},
    {xset::tool::back_menu,
     {
         "Back History",
         "gtk-go-back",
         "go_back",
     }},
    {xset::tool::fwd,
     {
         "Forward",
         "gtk-go-forward",
         "go_forward",
     }},
    {xset::tool::fwd_menu,
     {
         "Forward History",
         "gtk-go-forward",
         "go_forward",
     }},
    {xset::tool::refresh,
     {
         "Refresh",
         "gtk-refresh",
         "view_refresh",
     }},
    {xset::tool::new_tab,
     {
         "New Tab",
         "gtk-add",
         "tab_new",
     }},
    {xset::tool::new_tab_here,
     {
         "New Tab Here",
         "gtk-add",
         "tab_new_here",
     }},
    {xset::tool::show_hidden,
     {
         "Show Hidden",
         "gtk-apply",
         "panel1_show_hidden",
     }},
    {xset::tool::show_thumb,
     {
         "Show Thumbnails",
         std::nullopt,
         "view_thumb",
     }},
    {xset::tool::large_icons,
     {
         "Large Icons",
         "zoom-in",
         "panel1_list_large",
     }},
    {xset::tool::invalid,
     {
         std::nullopt,
         std::nullopt,
         std::nullopt,
     }},
};

void
load_settings()
{
    const auto settings_config_dir = vfs::user_dirs->program_config_dir();

    app_settings.load_saved_tabs(true);

    // MOD extra settings
    xset_defaults();

    const auto session = std::filesystem::path() / settings_config_dir / CONFIG_FILE_FILENAME;

    if (!std::filesystem::exists(settings_config_dir))
    {
        std::filesystem::create_directories(settings_config_dir);
        std::filesystem::permissions(settings_config_dir, std::filesystem::perms::owner_all);
    }

    bool git_backed_settings = app_settings.git_backed_settings();
    if (git_backed_settings)
    {
        if (Glib::find_program_in_path("git").empty())
        {
            ztd::logger::error("git backed settings enabled but git is not installed");
            git_backed_settings = false;
        }
    }

    if (git_backed_settings)
    {
        const auto command_script = get_script_path(spacefm::script::config_update_git);

        if (script_exists(command_script))
        {
            const std::string command_args =
                std::format("{} --config-dir {} --config-file {} --config-version {}",
                            command_script.string(),
                            settings_config_dir.string(),
                            CONFIG_FILE_FILENAME,
                            CONFIG_FILE_VERSION);

            ztd::logger::info("SCRIPT={}", command_script.string());
            Glib::spawn_command_line_sync(command_args);
        }
    }
    else
    {
        const auto command_script = get_script_path(spacefm::script::config_update);

        if (script_exists(command_script))
        {
            const std::string command_args = std::format("{} --config-dir {} --config-file {}",
                                                         command_script.string(),
                                                         settings_config_dir.string(),
                                                         CONFIG_FILE_FILENAME);

            ztd::logger::info("SCRIPT={}", command_script.string());
            Glib::spawn_command_line_sync(command_args);
        }
    }

    if (std::filesystem::is_regular_file(session))
    {
        load_user_confing(session);
    }
    else
    {
        ztd::logger::info("No config file found, using defaults.");
    }

    // MOD turn off fullscreen
    xset_set_b(xset::name::main_full, false);

    const auto date_format = xset_get_s(xset::name::date_format);
    if (date_format)
    {
        app_settings.date_format(date_format.value());
    }
    else
    {
        xset_set(xset::name::date_format, xset::var::s, app_settings.date_format());
    }

    // MOD terminal discovery
    const auto main_terminal = xset_get_s(xset::name::main_terminal);
    if (!main_terminal)
    {
        const auto supported_terminals = terminal_handlers->get_supported_terminal_names();
        for (const std::string_view supported_terminal : supported_terminals)
        {
            const std::filesystem::path terminal =
                Glib::find_program_in_path(supported_terminal.data());
            if (terminal.empty())
            {
                continue;
            }

            xset_set(xset::name::main_terminal, xset::var::s, terminal.filename().string());
            xset_set_b(xset::name::main_terminal, true); // discovery
            break;
        }
    }

    // MOD editor discovery
    const auto main_editor = xset_get_s(xset::name::editor);
    if (!main_editor)
    {
        vfs::mime_type mime_type = vfs_mime_type_get_from_type("text/plain");
        if (mime_type)
        {
            const auto default_app = mime_type->default_action();
            if (default_app)
            {
                const vfs::desktop desktop = vfs_get_desktop(default_app.value());
                xset_set(xset::name::editor, xset::var::s, desktop->path().string());
            }
        }
    }

    // set default keys
    xset_default_keys();
}

void
save_settings()
{
    // ztd::logger::info("save_settings");

    MainWindow* main_window = main_window_get_last_active();

    // save tabs
    const bool save_tabs = xset_get_b(xset::name::main_save_tabs);

    if (GTK_IS_WIDGET(main_window))
    {
        if (save_tabs)
        {
            for (const panel_t p : PANELS)
            {
                xset_t set = xset_get_panel(p, xset::panel::show);
                if (GTK_IS_NOTEBOOK(main_window->get_panel_notebook(p)))
                {
                    const i32 pages = gtk_notebook_get_n_pages(main_window->get_panel_notebook(p));
                    if (pages) // panel was shown
                    {
                        std::string tabs;
                        for (const auto i : ztd::range(pages))
                        {
                            PtkFileBrowser* file_browser = PTK_FILE_BROWSER_REINTERPRET(
                                gtk_notebook_get_nth_page(main_window->get_panel_notebook(p), i));
                            tabs = std::format("{}{}{}",
                                               tabs,
                                               CONFIG_FILE_TABS_DELIM,
                                               // Need to use .string() as libfmt will by default
                                               // format paths with double quotes surrounding them
                                               // which will break config file parsing.
                                               file_browser->cwd().string());
                        }
                        set->s = tabs;

                        // save current tab
                        const i32 current_page =
                            gtk_notebook_get_current_page(main_window->get_panel_notebook(p));
                        set->x = std::to_string(current_page);
                    }
                }
            }
        }
        else
        {
            // clear saved tabs
            for (const panel_t p : PANELS)
            {
                xset_t set = xset_get_panel(p, xset::panel::show);
                set->s = std::nullopt;
                set->x = std::nullopt;
            }
        }
    }

    /* save settings */
    const auto settings_config_dir = vfs::user_dirs->program_config_dir();
    if (!std::filesystem::exists(settings_config_dir))
    {
        std::filesystem::create_directories(settings_config_dir);
        std::filesystem::permissions(settings_config_dir, std::filesystem::perms::owner_all);
    }

    save_user_confing();
}

void
free_settings()
{
    while (true)
    {
        if (xsets.empty())
        {
            break;
        }

        xset_t set = xsets.back();
        xsets.pop_back();

        if (set->ob2_data && ztd::startswith(set->name, "evt_"))
        {
            g_list_foreach((GList*)set->ob2_data, (GFunc)std::free, nullptr);
            g_list_free((GList*)set->ob2_data);
        }

        delete set;
    }
}

GtkWidget*
xset_get_image(const std::string_view icon, GtkIconSize icon_size)
{
    /*
        GtkIconSize::GTK_ICON_SIZE_MENU,
        GtkIconSize::GTK_ICON_SIZE_SMALL_TOOLBAR,
        GtkIconSize::GTK_ICON_SIZE_LARGE_TOOLBAR,
        GtkIconSize::GTK_ICON_SIZE_BUTTON,
        GtkIconSize::GTK_ICON_SIZE_DND,
        GtkIconSize::GTK_ICON_SIZE_DIALOG
    */

    if (icon.empty())
    {
        return nullptr;
    }

    if (!icon_size)
    {
        icon_size = GtkIconSize::GTK_ICON_SIZE_MENU;
    }

    return gtk_image_new_from_icon_name(icon.data(), icon_size);
}

#if (GTK_MAJOR_VERSION == 4)
void
xset_add_menu(PtkFileBrowser* file_browser, GtkWidget* menu, GtkEventController* accel_group,
              const std::vector<xset::name>& submenu_entries)
#elif (GTK_MAJOR_VERSION == 3)
void
xset_add_menu(PtkFileBrowser* file_browser, GtkWidget* menu, GtkAccelGroup* accel_group,
              const std::vector<xset::name>& submenu_entries)
#endif
{
    if (submenu_entries.empty())
    {
        return;
    }

    for (const auto submenu_entry : submenu_entries)
    {
        xset_t set = xset_get(submenu_entry);
        xset_add_menuitem(file_browser, menu, accel_group, set);
    }
}

static GtkWidget*
xset_new_menuitem(const std::string_view label, const std::string_view icon)
{
    (void)icon;

    GtkWidget* item;

    if (ztd::contains(label, "\\_"))
    {
        // allow escape of underscore
        const std::string str = clean_label(label, false, false);
        item = gtk_menu_item_new_with_label(str.data());
    }
    else
    {
        item = gtk_menu_item_new_with_mnemonic(label.data());
    }
    // if (!(icon && icon[0]))
    // {
    //     return item;
    // }
    // GtkWidget* image = xset_get_image(icon, GtkIconSize::GTK_ICON_SIZE_MENU);

    return item;
}

#if (GTK_MAJOR_VERSION == 4)
GtkWidget*
xset_add_menuitem(PtkFileBrowser* file_browser, GtkWidget* menu, GtkEventController* accel_group,
                  xset_t set)
#elif (GTK_MAJOR_VERSION == 3)
GtkWidget*
xset_add_menuitem(PtkFileBrowser* file_browser, GtkWidget* menu, GtkAccelGroup* accel_group,
                  xset_t set)
#endif
{
    GtkWidget* item = nullptr;
    GtkWidget* submenu;
    xset_t keyset;
    xset_t set_next;
    std::string icon_name;

    // ztd::logger::info("xset_add_menuitem {}", set->name);

    if (icon_name.empty() && set->icon)
    {
        icon_name = set->icon.value();
    }

    if (icon_name.empty())
    {
        const auto icon_file =
            vfs::user_dirs->program_config_dir() / "scripts" / set->name / "icon";
        if (std::filesystem::exists(icon_file))
        {
            icon_name = icon_file;
        }
    }

    if (set->tool != xset::tool::NOT && set->menu_style != xset::menu::submenu)
    {
        // item = xset_new_menuitem(set->menu_label, icon_name);
    }
    else if (set->menu_style != xset::menu::normal)
    {
        xset_t set_radio;
        switch (set->menu_style)
        {
            case xset::menu::check:
                if (!(!set->lock &&
                      (xset::cmd(xset_get_int(set, xset::var::x)) > xset::cmd::script)))
                {
                    item = gtk_check_menu_item_new_with_mnemonic(set->menu_label.value().data());
                    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),
                                                   set->b == xset::b::xtrue);
                }
                break;
            case xset::menu::radio:
                if (set->ob2_data)
                {
                    set_radio = xset_get(static_cast<const char*>(set->ob2_data));
                }
                else
                {
                    set_radio = set;
                }
                item = gtk_radio_menu_item_new_with_mnemonic((GSList*)set_radio->ob2_data,
                                                             set->menu_label.value().data());
                set_radio->ob2_data =
                    (void*)gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), set->b == xset::b::xtrue);
                break;
            case xset::menu::submenu:
                submenu = gtk_menu_new();
                item = xset_new_menuitem(set->menu_label.value_or(""), icon_name);
                gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
                if (set->lock)
                {
                    if (!set->context_menu_entries.empty())
                    {
                        xset_add_menu(file_browser,
                                      submenu,
                                      accel_group,
                                      set->context_menu_entries);
                    }
                }
                else if (set->child)
                {
                    set_next = xset_get(set->child.value());
                    xset_add_menuitem(file_browser, submenu, accel_group, set_next);
                    GList* l = gtk_container_get_children(GTK_CONTAINER(submenu));
                    if (l)
                    {
                        g_list_free(l);
                    }
                    else
                    {
                        // Nothing was added to the menu (all items likely have
                        // invisible context) so destroy (hide) - issue #215
                        gtk_widget_destroy(item);

                        // next item
                        if (set->next)
                        {
                            set_next = xset_get(set->next.value());
                            xset_add_menuitem(file_browser, menu, accel_group, set_next);
                        }
                        return item;
                    }
                }
                break;
            case xset::menu::sep:
                item = gtk_separator_menu_item_new();
                break;
            case xset::menu::normal:
            case xset::menu::string:
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
                break;
        }
    }
    if (!item)
    {
        // get menu icon size
        i32 icon_w;
        i32 icon_h;
        gtk_icon_size_lookup(GtkIconSize::GTK_ICON_SIZE_MENU, &icon_w, &icon_h);
        const i32 icon_size = icon_w > icon_h ? icon_w : icon_h;

        GdkPixbuf* app_icon = nullptr;
        const auto cmd_type = xset::cmd(xset_get_int(set, xset::var::x));
        if (!set->lock && cmd_type == xset::cmd::app)
        {
            // Application
            const std::string menu_label = xset_custom_get_app_name_icon(set, &app_icon, icon_size);
            item = xset_new_menuitem(menu_label, "");
        }
        else
        {
            item = xset_new_menuitem(set->menu_label.value_or(""), icon_name);
        }

        if (app_icon)
        {
            g_object_unref(app_icon);
        }
    }

    set->browser = file_browser;
    g_object_set_data(G_OBJECT(item), "menu", menu);
    g_object_set_data(G_OBJECT(item), "set", set->name.data());

    if (set->ob1)
    {
        g_object_set_data(G_OBJECT(item), set->ob1, set->ob1_data);
    }
    if (set->menu_style != xset::menu::radio && set->ob2)
    {
        g_object_set_data(G_OBJECT(item), set->ob2, set->ob2_data);
    }

    if (set->menu_style < xset::menu::submenu)
    {
        // activate callback
        if (!set->cb_func || set->menu_style != xset::menu::normal)
        {
            // use xset menu callback
            // if ( !design_mode )
            //{
            g_signal_connect(item, "activate", G_CALLBACK(xset_menu_cb), set);
            //}
        }
        else if (set->cb_func)
        {
            // use custom callback directly
            // if ( !design_mode )
            g_signal_connect(item, "activate", G_CALLBACK(set->cb_func), set->cb_data);
        }

        // key accel
        if (set->shared_key)
        {
            keyset = xset_get(set->shared_key.value());
        }
        else
        {
            keyset = set;
        }
        if (keyset->key > 0 && accel_group)
        {
#if (GTK_MAJOR_VERSION == 4)
            ztd::logger::debug("TODO - PORT - accel_group");
            // gtk_widget_add_controller(item, accel_group);
            // gtk_shortcut_controller_add_shortcut(
            //     GTK_SHORTCUT_CONTROLLER(accel_group),
            //     gtk_shortcut_new(gtk_keyval_trigger_new(keyset->key, (GdkModifierType)keyset->keymod),
            //                     gtk_callback_action_new(callback_func /* TODO */, nullptr, nullptr)))
#elif (GTK_MAJOR_VERSION == 3)
            gtk_widget_add_accelerator(item,
                                       "activate",
                                       accel_group,
                                       keyset->key,
                                       (GdkModifierType)keyset->keymod,
                                       GTK_ACCEL_VISIBLE);
#endif
        }
    }
    // design mode callback
    g_signal_connect(item, "button-press-event", G_CALLBACK(xset_design_cb), set);
    g_signal_connect(item, "button-release-event", G_CALLBACK(xset_design_cb), set);

    gtk_widget_set_sensitive(item, !set->disable);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    // next item
    if (set->next)
    {
        set_next = xset_get(set->next.value());
        xset_add_menuitem(file_browser, menu, accel_group, set_next);
    }
    return item;
}

void
xset_custom_insert_after(xset_t target, xset_t set)
{ // inserts single set 'set', no next

    assert(target != nullptr);
    assert(set != nullptr);
    assert(target != nullptr);

#if 0
    if (!set)
    {
        ztd::logger::warn("xset_custom_insert_after set == nullptr");
        return;
    }
    if (!target)
    {
        ztd::logger::warn("xset_custom_insert_after target == nullptr");
        return;
    }
#endif

    if (set->parent)
    {
        set->parent = std::nullopt;
    }

    set->prev = target->name;
    set->next = target->next;
    if (target->next)
    {
        xset_t target_next = xset_get(target->next.value());
        target_next->prev = set->name;
    }
    target->next = set->name;
    if (target->tool != xset::tool::NOT)
    {
        if (set->tool < xset::tool::custom)
        {
            set->tool = xset::tool::custom;
        }
    }
    else
    {
        if (set->tool > xset::tool::custom)
        {
            ztd::logger::warn("xset_custom_insert_after builtin tool inserted after non-tool");
        }
        set->tool = xset::tool::NOT;
    }
}

static bool
xset_clipboard_in_set(xset_t set)
{ // look upward to see if clipboard is in set's tree
    assert(set != nullptr);

    if (!xset_set_clipboard || set->lock)
    {
        return false;
    }
    if (set == xset_set_clipboard)
    {
        return true;
    }

    if (set->parent)
    {
        xset_t set_parent = xset_get(set->parent.value());
        if (xset_clipboard_in_set(set_parent))
        {
            return true;
        }
    }

    if (set->prev)
    {
        xset_t set_prev = xset_get(set->prev.value());
        while (set_prev)
        {
            if (set_prev->parent)
            {
                xset_t set_prev_parent = xset_get(set_prev->parent.value());
                if (xset_clipboard_in_set(set_prev_parent))
                {
                    return true;
                }
                set_prev = nullptr;
            }
            else if (set_prev->prev)
            {
                set_prev = xset_get(set_prev->prev.value());
            }
            else
            {
                set_prev = nullptr;
            }
        }
    }
    return false;
}

void
xset_edit(GtkWidget* parent, const std::filesystem::path& path)
{
    GtkWidget* dlgparent = nullptr;
    if (parent)
    {
        dlgparent = gtk_widget_get_toplevel(GTK_WIDGET(parent));
    }

    const auto check_editor = xset_get_s(xset::name::editor);
    if (!check_editor)
    {
        ptk_show_error(dlgparent ? GTK_WINDOW(dlgparent) : nullptr,
                       "Editor Not Set",
                       "Please set your editor in View|Preferences|Advanced");
        return;
    }
    const auto& editor = check_editor.value();

    vfs::desktop desktop;
    if (ztd::endswith(editor, ".desktop"))
    {
        desktop = vfs_get_desktop(editor);
    }
    else
    { // this might work
        ztd::logger::warn("Editor is not set to a .desktop file");
        desktop = vfs_get_desktop(fmt::format("{}.desktop", editor));
    }

    const std::vector<std::filesystem::path> open_files{path};
    try
    {
        desktop->open_files(path.parent_path(), open_files);
    }
    catch (const VFSAppDesktopException& e)
    {
        ptk_show_error(dlgparent ? GTK_WINDOW(dlgparent) : nullptr,
                       "Error",
                       std::format("Unable to open file:\n{}\n{}", path.string(), e.what()));
    }
}

static bool
xset_job_is_valid(xset_t set, xset::job job)
{
    assert(set != nullptr);

    bool no_remove = false;
    bool no_paste = false;

    switch (job)
    {
        case xset::job::key:
            return set->menu_style < xset::menu::submenu;
        case xset::job::cut:
            return !set->lock;
        case xset::job::copy:
            return !set->lock;
        case xset::job::paste:
            if (!xset_set_clipboard)
            {
                no_paste = true;
            }
            else if (set == xset_set_clipboard && xset_clipboard_is_cut)
            {
                // do not allow cut paste to self
                no_paste = true;
            }
            else if (xset_set_clipboard->tool > xset::tool::custom && set->tool == xset::tool::NOT)
            {
                // do not allow paste of builtin tool item to menu
                no_paste = true;
            }
            else if (xset_set_clipboard->menu_style == xset::menu::submenu)
            {
                // do not allow paste of submenu to self or below
                no_paste = xset_clipboard_in_set(set);
            }
            return !no_paste;
        case xset::job::remove:
            return (!set->lock && !no_remove);
        case xset::job::add_tool:
        case xset::job::remove_book:
        case xset::job::invalid:
            break;
    }
    return false;
}

static void
on_menu_hide(GtkWidget* widget, GtkWidget* design_menu)
{
    gtk_widget_set_sensitive(widget, true);
    gtk_menu_shell_deactivate(GTK_MENU_SHELL(design_menu));
}

static GtkWidget*
xset_design_additem(GtkWidget* menu, const std::string_view label, xset::job job, xset_t set)
{
    GtkWidget* item;
    item = gtk_menu_item_new_with_mnemonic(label.data());

    g_object_set_data(G_OBJECT(item), "job", GINT_TO_POINTER(magic_enum::enum_integer(job)));
    gtk_container_add(GTK_CONTAINER(menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(xset_design_job), set);
    return item;
}

GtkWidget*
xset_design_show_menu(GtkWidget* menu, xset_t set, xset_t book_insert, u32 button, std::time_t time)
{
    (void)button;
    (void)time;

    GtkWidget* newitem;
    GtkWidget* submenu;
    bool no_remove = false;
    bool no_paste = false;

    // xset_t mset;
    xset_t insert_set;

    // book_insert is a bookmark set to be used for Paste, etc
    insert_set = book_insert ? book_insert : set;
    // to signal this is a bookmark, pass book_insert = set
    const bool show_keys = set->tool == xset::tool::NOT;

    if (!xset_set_clipboard)
    {
        no_paste = true;
    }
    else if (insert_set == xset_set_clipboard && xset_clipboard_is_cut)
    {
        // do not allow cut paste to self
        no_paste = true;
    }
    else if (xset_set_clipboard->tool > xset::tool::custom && insert_set->tool == xset::tool::NOT)
    {
        // do not allow paste of builtin tool item to menu
        no_paste = true;
    }
    else if (xset_set_clipboard->menu_style == xset::menu::submenu)
    {
        // do not allow paste of submenu to self or below
        no_paste = xset_clipboard_in_set(insert_set);
    }

    GtkWidget* design_menu = gtk_menu_new();

#if (GTK_MAJOR_VERSION == 4)
    GtkEventController* accel_group = gtk_shortcut_controller_new();
#elif (GTK_MAJOR_VERSION == 3)
    GtkAccelGroup* accel_group = gtk_accel_group_new();
#endif

    // Cut
    newitem = xset_design_additem(design_menu, "Cu_t", xset::job::cut, set);
    gtk_widget_set_sensitive(newitem, !set->lock);
    if (show_keys)
    {
#if (GTK_MAJOR_VERSION == 4)
        ztd::logger::debug("TODO - PORT - accel_group");
#elif (GTK_MAJOR_VERSION == 3)
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_x,
                                   GdkModifierType::GDK_CONTROL_MASK,
                                   GTK_ACCEL_VISIBLE);
#endif
    }

    // Copy
    newitem = xset_design_additem(design_menu, "_Copy", xset::job::copy, set);
    gtk_widget_set_sensitive(newitem, !set->lock);
    if (show_keys)
    {
#if (GTK_MAJOR_VERSION == 4)
        ztd::logger::debug("TODO - PORT - accel_group");
#elif (GTK_MAJOR_VERSION == 3)
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_c,
                                   GdkModifierType::GDK_CONTROL_MASK,
                                   GTK_ACCEL_VISIBLE);
#endif
    }

    // Paste
    newitem = xset_design_additem(design_menu, "_Paste", xset::job::paste, insert_set);
    gtk_widget_set_sensitive(newitem, !no_paste);
    if (show_keys)
    {
#if (GTK_MAJOR_VERSION == 4)
        ztd::logger::debug("TODO - PORT - accel_group");
#elif (GTK_MAJOR_VERSION == 3)
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_v,
                                   GdkModifierType::GDK_CONTROL_MASK,
                                   GTK_ACCEL_VISIBLE);
#endif
    }

    // Remove
    newitem = xset_design_additem(design_menu, "_Remove", xset::job::remove, set);
    gtk_widget_set_sensitive(newitem, !set->lock && !no_remove);
    if (show_keys)
    {
#if (GTK_MAJOR_VERSION == 4)
        ztd::logger::debug("TODO - PORT - accel_group");
#elif (GTK_MAJOR_VERSION == 3)
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_Delete,
                                   (GdkModifierType)0,
                                   GTK_ACCEL_VISIBLE);
#endif
    }

    // Add >
    if (insert_set->tool != xset::tool::NOT)
    {
        // "Add" submenu for builtin tool items
        newitem = gtk_menu_item_new_with_mnemonic("_Add");
        submenu = gtk_menu_new();
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(newitem), submenu);
        gtk_container_add(GTK_CONTAINER(design_menu), newitem);

        for (const auto& it : builtin_tools)
        {
            const auto name = it.second.name;
            if (name)
            {
                newitem =
                    xset_design_additem(submenu, name.value(), xset::job::add_tool, insert_set);
                g_object_set_data(G_OBJECT(newitem),
                                  "tool_type",
                                  GINT_TO_POINTER(magic_enum::enum_integer(it.first)));
            }
        }
    }

    // Separator
    gtk_container_add(GTK_CONTAINER(design_menu), gtk_separator_menu_item_new());

    // Key Shorcut
    newitem = xset_design_additem(design_menu, "_Key Shortcut", xset::job::key, set);
    gtk_widget_set_sensitive(newitem, (set->menu_style < xset::menu::submenu));
    if (show_keys)
    {
#if (GTK_MAJOR_VERSION == 4)
        ztd::logger::debug("TODO - PORT - accel_group");
#elif (GTK_MAJOR_VERSION == 3)
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_k,
                                   GdkModifierType::GDK_CONTROL_MASK,
                                   GTK_ACCEL_VISIBLE);
#endif
    }

    // show menu
    gtk_widget_show_all(GTK_WIDGET(design_menu));
    /* sfm 1.0.6 passing button (3) here when menu == nullptr causes items in New
     * submenu to not activate with some trackpads (eg two-finger right-click)
     * to open original design menu.  Affected only bookmarks pane and toolbar
     * where menu == nullptr.  So pass 0 for button if !menu. */

    // Get the pointer location
    i32 x, y;
    GdkModifierType mods;
    gdk_window_get_device_position(gtk_widget_get_window(menu), nullptr, &x, &y, &mods);

    // Popup the menu at the pointer location
    gtk_menu_popup_at_pointer(GTK_MENU(design_menu), nullptr);

    if (menu)
    {
        gtk_widget_set_sensitive(GTK_WIDGET(menu), false);
        g_signal_connect(menu, "hide", G_CALLBACK(on_menu_hide), design_menu);
    }
    g_signal_connect(design_menu, "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);

    gtk_menu_shell_set_take_focus(GTK_MENU_SHELL(design_menu), true);
    // this is required when showing the menu via F2 or Menu key for focus
    gtk_menu_shell_select_first(GTK_MENU_SHELL(design_menu), true);

    return design_menu;
}

static bool
xset_design_cb(GtkWidget* item, GdkEventButton* event, xset_t set)
{
    xset::job job = xset::job::invalid;

    GtkWidget* menu = item ? GTK_WIDGET(g_object_get_data(G_OBJECT(item), "menu")) : nullptr;
    const u32 keymod = ptk_get_keymod(event->state);

    if (event->type == GdkEventType::GDK_BUTTON_RELEASE)
    {
        if (event->button == 1 && keymod == 0)
        {
            // user released left button - due to an apparent gtk bug, activate
            // does not always fire on this event so handle it ourselves
            // see also ptk-file-menu.c on_app_button_press()
            // test: gtk2 Crux theme with touchpad on Edit|Copy To|Location
            // https://github.com/IgnorantGuru/spacefm/issues/31
            // https://github.com/IgnorantGuru/spacefm/issues/228
            if (menu)
            {
                gtk_menu_shell_deactivate(GTK_MENU_SHELL(menu));
            }
            gtk_menu_item_activate(GTK_MENU_ITEM(item));
            return true;
        }
        // true for issue #521 where a right-click also left-clicks the first
        // menu item in some GTK2/3 themes.
        return true;
    }
    else if (event->type != GdkEventType::GDK_BUTTON_PRESS)
    {
        return false;
    }

    switch (event->button)
    {
        case 1:
        case 3:
            switch (keymod)
            {
                // left or right click
                case 0:
                    // no modifier
                    if (event->button == 3)
                    {
                        // right
                        xset_design_show_menu(menu, set, nullptr, event->button, event->time);
                        return true;
                    }
                    else if (event->button == 1 && set->tool != xset::tool::NOT && !set->lock)
                    {
                        // activate
                        if (set->tool == xset::tool::custom)
                        {
                            xset_menu_cb(nullptr, set);
                        }
                        else
                        {
                            xset_builtin_tool_activate(set->tool, set, event);
                        }
                        return true;
                    }
                    break;
                case GdkModifierType::GDK_CONTROL_MASK:
                    // ctrl
                    job = xset::job::copy;
                    break;
                case GdkModifierType::GDK_MOD1_MASK:
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
            switch (keymod)
            {
                // middle click
                case 0:
                    // no modifier
                    if (set->lock)
                    {
                        xset_design_show_menu(menu, set, nullptr, event->button, event->time);
                        return true;
                    }
                    break;
                case GdkModifierType::GDK_CONTROL_MASK:
                    // ctrl
                    job = xset::job::key;
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
            if (menu)
            {
                gtk_menu_shell_deactivate(GTK_MENU_SHELL(menu));
            }
            g_object_set_data(G_OBJECT(item), "job", GINT_TO_POINTER(job));
            xset_design_job(item, set);
        }
        else
        {
            xset_design_show_menu(menu, set, nullptr, event->button, event->time);
        }
        return true;
    }
    return false; // true will not stop activate on button-press (will on release)
}

void
xset_menu_cb(GtkWidget* item, xset_t set)
{
    GFunc cb_func = nullptr;
    void* cb_data = nullptr;

    if (item)
    {
        if (set->lock && set->menu_style == xset::menu::radio && GTK_IS_CHECK_MENU_ITEM(item) &&
            !gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)))
        {
            return;
        }

        cb_func = set->cb_func;
        cb_data = set->cb_data;
    }

    GtkWidget* parent = GTK_WIDGET(set->browser);

    switch (set->menu_style)
    {
        case xset::menu::normal:
            if (cb_func)
            {
                cb_func(item, cb_data);
            }
            break;
        case xset::menu::sep:
            break;
        case xset::menu::check:
            if (set->b == xset::b::xtrue)
            {
                set->b = xset::b::xfalse;
            }
            else
            {
                set->b = xset::b::xtrue;
            }

            if (cb_func)
            {
                cb_func(item, cb_data);
            }

            if (set->tool == xset::tool::custom)
            {
                set->browser->update_toolbar_widgets(set, xset::tool::invalid);
            }
            break;
        case xset::menu::string:
        {
            std::string title;
            std::string msg = set->desc.value();
            std::string default_str;
            if (set->title && set->lock)
            {
                title = set->title.value();
            }
            else
            {
                title = clean_label(set->menu_label.value(), false, false);
            }
            if (set->lock)
            {
                default_str = set->z.value();
            }
            else
            {
                msg = ztd::replace(msg, "\\n", "\n");
                msg = ztd::replace(msg, "\\t", "\t");
            }
            const auto [response, answer] =
                xset_text_dialog(parent, title, msg, "", set->s.value(), default_str, false);
            set->s = answer;
            if (response)
            {
                if (cb_func)
                {
                    cb_func(item, cb_data);
                }
            }
        }
        break;
        case xset::menu::radio:
            if (set->b != xset::b::xtrue)
            {
                set->b = xset::b::xtrue;
            }
            if (cb_func)
            {
                cb_func(item, cb_data);
            }
            break;
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
        case xset::menu::submenu:
            if (cb_func)
            {
                cb_func(item, cb_data);
            }
            break;
    }

    if (set->menu_style != xset::menu::normal)
    {
        autosave_request_add();
    }
}

void
multi_input_select_region(GtkWidget* input, i32 start, i32 end)
{
    GtkTextIter iter, siter;

    if (start < 0 || !GTK_IS_TEXT_VIEW(input))
    {
        return;
    }

    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(input));

    gtk_text_buffer_get_iter_at_offset(buf, &siter, start);

    if (end < 0)
    {
        gtk_text_buffer_get_end_iter(buf, &iter);
    }
    else
    {
        gtk_text_buffer_get_iter_at_offset(buf, &iter, end);
    }

    gtk_text_buffer_select_range(buf, &iter, &siter);
}

static void
xset_builtin_tool_activate(xset::tool tool_type, xset_t set, GdkEventButton* event)
{
    xset_t set2;
    panel_t p;
    xset::main_window_panel mode;
    PtkFileBrowser* file_browser = nullptr;
    MainWindow* main_window = main_window_get_last_active();

    // set may be a submenu that does not match tool_type
    if (!(set && !set->lock && tool_type > xset::tool::custom))
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
            if (!file_browser->is_view_mode(ptk::file_browser::view_mode::icon_view))
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

xset_t
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
on_tool_icon_button_press(GtkWidget* widget, GdkEventButton* event, xset_t set)
{
    xset::job job = xset::job::invalid;

    // ztd::logger::info("on_tool_icon_button_press  {}   button = {}", set->menu_label,
    // event->button);
    if (event->type != GdkEventType::GDK_BUTTON_PRESS)
    {
        return false;
    }
    const u32 keymod = ptk_get_keymod(event->state);

    // get and focus browser
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER(g_object_get_data(G_OBJECT(widget), "browser"));
    file_browser->focus_me();
    set->browser = file_browser;

    switch (event->button)
    {
        case 1:
        case 3:
            // left or right click
            switch (keymod)
            {
                case 0:
                    // no modifier
                    if (event->button == 1)
                    {
                        // left click
                        if (set->tool == xset::tool::custom &&
                            set->menu_style == xset::menu::submenu)
                        {
                            if (set->child)
                            {
                                xset_t set_child = xset_is(set->child.value());
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
                    else // if ( event->button == 3 )
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
                case GdkModifierType::GDK_MOD1_MASK:
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
                case GdkModifierType::GDK_MOD1_MASK:
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
on_tool_menu_button_press(GtkWidget* widget, GdkEventButton* event, xset_t set)
{
    // ztd::logger::info("on_tool_menu_button_press  {}   button = {}", set->menu_label,
    // event->button);
    if (event->type != GdkEventType::GDK_BUTTON_PRESS)
    {
        return false;
    }
    const u32 keymod = ptk_get_keymod(event->state);
    if (keymod != 0 || event->button != 1)
    {
        return on_tool_icon_button_press(widget, event, set);
    }

    // get and focus browser
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER(g_object_get_data(G_OBJECT(widget), "browser"));
    file_browser->focus_me();

    if (event->button == 1)
    {
        if (set->tool == xset::tool::custom)
        {
            // show custom submenu
            xset_t set_child;
            if (!(set && !set->lock && set->child && set->menu_style == xset::menu::submenu &&
                  (set_child = xset_is(set->child.value()))))
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
xset_add_toolitem(GtkWidget* parent, PtkFileBrowser* file_browser, GtkToolbar* toolbar,
                  i32 icon_size, xset_t set, bool show_tooltips)
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
    i32 icon_w, icon_h;
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
        set->shared_key = builtin_tools.at(set->tool).shared_key;
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
            vfs::user_dirs->program_config_dir() / "scripts" / set->name / "icon";
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

    GtkBox* hbox;
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
            gtk_widget_show(image);
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
            gtk_widget_set_margin_start(GTK_WIDGET(btn), 0);
            gtk_widget_set_margin_end(GTK_WIDGET(btn), 0);

            // create tool item containing an ebox to capture click on button
            item = GTK_WIDGET(gtk_tool_item_new());
            GtkEventBox* ebox = GTK_EVENT_BOX(gtk_event_box_new());
            gtk_container_add(GTK_CONTAINER(item), GTK_WIDGET(ebox));
            gtk_container_add(GTK_CONTAINER(ebox), GTK_WIDGET(btn));
            gtk_event_box_set_visible_window(ebox, false);
            gtk_event_box_set_above_child(ebox, true);
            g_signal_connect(ebox,
                             "button-press-event",
                             G_CALLBACK(on_tool_icon_button_press),
                             set);
            g_object_set_data(G_OBJECT(ebox), "browser", file_browser);
            ptk_file_browser_add_toolbar_widget(set, GTK_WIDGET(btn));

            // tooltip
            if (show_tooltips)
            {
                str = clean_label(new_menu_label.value(), false, false);
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
            gtk_widget_show(image);
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
            gtk_widget_set_margin_start(GTK_WIDGET(check_btn), 0);
            gtk_widget_set_margin_end(GTK_WIDGET(check_btn), 0);

            // create tool item containing an ebox to capture click on button
            item = GTK_WIDGET(gtk_tool_item_new());
            GtkEventBox* ebox = GTK_EVENT_BOX(gtk_event_box_new());
            gtk_container_add(GTK_CONTAINER(item), GTK_WIDGET(ebox));
            gtk_container_add(GTK_CONTAINER(ebox), GTK_WIDGET(check_btn));
            gtk_event_box_set_visible_window(ebox, false);
            gtk_event_box_set_above_child(ebox, true);
            g_signal_connect(ebox,
                             "button-press-event",
                             G_CALLBACK(on_tool_icon_button_press),
                             set);
            g_object_set_data(G_OBJECT(ebox), "browser", file_browser);
            ptk_file_browser_add_toolbar_widget(set, GTK_WIDGET(check_btn));

            // tooltip
            if (show_tooltips)
            {
                str = clean_label(menu_label.value(), false, false);
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
                image = xset_get_image(icon_name ? icon_name.value() : "gtk-directory",
                                       (GtkIconSize)icon_size);
            }

            // cannot use gtk_tool_button_new because icon does not obey size
            // btn = GTK_WIDGET( gtk_tool_button_new( image, menu_label ) );
            GtkButton* btn = GTK_BUTTON(gtk_button_new());
            gtk_widget_show(image);
            gtk_button_set_image(btn, image);
            gtk_button_set_relief(btn, GTK_RELIEF_NONE);
            gtk_widget_set_margin_start(GTK_WIDGET(btn), 0);
            gtk_widget_set_margin_end(GTK_WIDGET(btn), 0);
            gtk_widget_set_margin_top(GTK_WIDGET(btn), 0);
            gtk_widget_set_margin_bottom(GTK_WIDGET(btn), 0);
            gtk_widget_set_hexpand(GTK_WIDGET(btn), false);
            gtk_widget_set_vexpand(GTK_WIDGET(btn), false);
            gtk_button_set_always_show_image(btn, true);
            gtk_widget_set_margin_start(GTK_WIDGET(btn), 0);
            gtk_widget_set_margin_end(GTK_WIDGET(btn), 0);

            GtkEventBox* ebox;

            // create eventbox for btn
            ebox = GTK_EVENT_BOX(gtk_event_box_new());
            gtk_event_box_set_visible_window(ebox, false);
            gtk_event_box_set_above_child(ebox, true);
            gtk_container_add(GTK_CONTAINER(ebox), GTK_WIDGET(btn));
            g_signal_connect(G_OBJECT(ebox),
                             "button_press_event",
                             G_CALLBACK(on_tool_icon_button_press),
                             set);
            g_object_set_data(G_OBJECT(ebox), "browser", file_browser);
            ptk_file_browser_add_toolbar_widget(set, GTK_WIDGET(btn));

            // pack into hbox
            hbox = GTK_BOX(gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0));
            gtk_box_pack_start(hbox, GTK_WIDGET(ebox), false, false, 0);
            // tooltip
            if (show_tooltips)
            {
                str = clean_label(menu_label.value(), false, false);
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
            GtkWidget* menu_btn;
            GtkWidget* hbox_menu;
            GList* children;

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
            gtk_widget_set_margin_start(GTK_WIDGET(btn), 0);
            gtk_widget_set_margin_end(GTK_WIDGET(btn), 0);

            g_list_free(children);
            gtk_widget_destroy(menu_btn);

            gtk_box_pack_start(hbox, GTK_WIDGET(ebox), false, false, 0);
            g_signal_connect(G_OBJECT(ebox),
                             "button_press_event",
                             G_CALLBACK(on_tool_menu_button_press),
                             set);
            g_object_set_data(G_OBJECT(ebox), "browser", file_browser);
            ptk_file_browser_add_toolbar_widget(set, GTK_WIDGET(btn));

            item = GTK_WIDGET(gtk_tool_item_new());
            gtk_container_add(GTK_CONTAINER(item), GTK_WIDGET(hbox));
            gtk_widget_show_all(item);

            // tooltip
            if (show_tooltips)
            {
                str = clean_label(menu_label.value(), false, false);
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
            g_signal_connect(ebox,
                             "button-press-event",
                             G_CALLBACK(on_tool_icon_button_press),
                             set);
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
xset_fill_toolbar(GtkWidget* parent, PtkFileBrowser* file_browser, GtkToolbar* toolbar,
                  xset_t set_parent, bool show_tooltips)
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
    i32 stop_b4;
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

    xset_t set_child = nullptr;
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
            for (const auto i : ztd::range(stop_b4))
            {
                set = xset_new_builtin_toolitem(default_tools.at(i));
                xset_custom_insert_after(set_target, set);
                set_target = set;
            }
        }
    }

    xset_add_toolitem(parent, file_browser, toolbar, icon_size, set_child, show_tooltips);

    // These do not seem to do anything
    gtk_container_set_border_width(GTK_CONTAINER(toolbar), 0);
    gtk_widget_set_margin_start(GTK_WIDGET(toolbar), 0);
    gtk_widget_set_margin_end(GTK_WIDGET(toolbar), 0);
    gtk_widget_set_margin_top(GTK_WIDGET(toolbar), 0);
    gtk_widget_set_margin_bottom(GTK_WIDGET(toolbar), 0);
    gtk_widget_set_margin_start(GTK_WIDGET(toolbar), 0);
    gtk_widget_set_margin_end(GTK_WIDGET(toolbar), 0);

    gtk_widget_show_all(GTK_WIDGET(toolbar));
}
