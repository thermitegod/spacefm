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

#include <filesystem>

#include <array>
#include <map>
#include <vector>

#include <iostream>
#include <fstream>

#include <algorithm>
#include <ranges>

#include <cassert>

#include <unistd.h>

#include <fmt/format.h>

#include <glibmm.h>
#include <glibmm/convert.h>

#include <gtk/gtk.h>

#include <gdk/gdkkeysyms.h>
#include <fcntl.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "types.hxx"

#include "settings.hxx"
#include "main-window.hxx"
#include "item-prop.hxx"

#include "scripts.hxx"

#include "xset/xset.hxx"
#include "xset/xset-context.hxx"
#include "xset/xset-custom.hxx"
#include "xset/xset-defaults.hxx"
#include "xset/xset-design.hxx"
#include "xset/xset-design-clipboard.hxx"
#include "xset/xset-dialog.hxx"
#include "xset/xset-event-handler.hxx"
#include "xset/xset-plugins.hxx"
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

#include "ptk/ptk-error.hxx"
#include "ptk/ptk-keyboard.hxx"
#include "ptk/ptk-app-chooser.hxx"
#include "ptk/ptk-handler.hxx"
#include "ptk/ptk-file-menu.hxx"
#include "ptk/ptk-file-task.hxx"
#include "ptk/ptk-location-view.hxx"

// MOD settings

static bool xset_design_cb(GtkWidget* item, GdkEventButton* event, xset_t set);
static void xset_builtin_tool_activate(XSetTool tool_type, xset_t set, GdkEventButton* event);

// must match XSetTool:: enum
inline constexpr std::array<const char*, 18> builtin_tool_name{
    nullptr,
    nullptr,
    "Show Devices",
    "Show Bookmarks",
    "Show Tree",
    "Home",
    "Default",
    "Up",
    "Back",
    "Back History",
    "Forward",
    "Forward History",
    "Refresh",
    "New Tab",
    "New Tab Here",
    "Show Hidden",
    "Show Thumbnails",
    "Large Icons",
};

// must match XSetTool:: enum
inline constexpr std::array<const char*, 18> builtin_tool_icon{
    nullptr,
    nullptr,
    "gtk-harddisk",
    "gtk-jump-to",
    "gtk-directory",
    "gtk-home",
    "gtk-home",
    "gtk-go-up",
    "gtk-go-back",
    "gtk-go-back",
    "gtk-go-forward",
    "gtk-go-forward",
    "gtk-refresh",
    "gtk-add",
    "gtk-add",
    "gtk-apply",
    nullptr,
    "zoom-in",
};

// must match XSetTool:: enum
inline constexpr std::array<const char*, 18> builtin_tool_shared_key{
    nullptr,
    nullptr,
    "panel1_show_devmon",
    "panel1_show_book",
    "panel1_show_dirtree",
    "go_home",
    "go_default",
    "go_up",
    "go_back",
    "go_back",
    "go_forward",
    "go_forward",
    "view_refresh",
    "tab_new",
    "tab_new_here",
    "panel1_show_hidden",
    "view_thumb",
    "panel1_list_large",
};

void
load_settings()
{
    const std::string& settings_config_dir = vfs::user_dirs->program_config_dir();

    app_settings.set_load_saved_tabs(true);

    // MOD extra settings
    xset_defaults();

#if defined(HAVE_DEPRECATED_INI_CONFIG_LOADING)
    // choose which config file to load
    std::string conf_ini = Glib::build_filename(settings_config_dir, CONFIG_FILE_INI_FILENAME);
    std::string conf_toml = Glib::build_filename(settings_config_dir, CONFIG_FILE_FILENAME);
    std::string& session = conf_toml;
    bool load_deprecated_ini_config = false;
    if (std::filesystem::exists(conf_ini) && !std::filesystem::exists(conf_toml))
    {
        ztd::logger::warn("INI config files are deprecated, loading support will be removed");
        load_deprecated_ini_config = true;
        session = conf_ini;
    }
#else
    const std::string session = Glib::build_filename(settings_config_dir, CONFIG_FILE_FILENAME);
#endif

    if (!std::filesystem::exists(settings_config_dir))
    {
        std::filesystem::create_directories(settings_config_dir);
        std::filesystem::permissions(settings_config_dir, std::filesystem::perms::owner_all);
    }

    bool git_backed_settings = app_settings.get_git_backed_settings();
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
        const std::string command_script = get_script_path(Scripts::CONFIG_UPDATE_GIT);

        if (script_exists(command_script))
        {
            const std::string command_args =
                fmt::format("{} --config-dir {} --config-file {} --config-version {}",
                            command_script,
                            settings_config_dir,
                            CONFIG_FILE_FILENAME,
                            CONFIG_FILE_VERSION);

            ztd::logger::info("SCRIPT={}", command_script);
            Glib::spawn_command_line_sync(command_args);
        }
    }
    else
    {
        const std::string command_script = get_script_path(Scripts::CONFIG_UPDATE);

        if (script_exists(command_script))
        {
            const std::string command_args = fmt::format("{} --config-dir {} --config-file {}",
                                                         command_script,
                                                         settings_config_dir,
                                                         CONFIG_FILE_FILENAME);

            ztd::logger::info("SCRIPT={}", command_script);
            Glib::spawn_command_line_sync(command_args);
        }
    }

    if (std::filesystem::is_regular_file(session))
    {
#if defined(HAVE_DEPRECATED_INI_CONFIG_LOADING)
        load_user_confing(session, load_deprecated_ini_config);
#else
        load_user_confing(session);
#endif
    }
    else
    {
        ztd::logger::info("No config file found, using defaults.");
    }

    // MOD turn off fullscreen
    xset_set_b(XSetName::MAIN_FULL, false);

    char* date_format = xset_get_s(XSetName::DATE_FORMAT);
    if (!date_format || date_format[0] == '\0')
    {
        xset_set(XSetName::DATE_FORMAT, XSetVar::S, app_settings.get_date_format());
    }
    else
    {
        app_settings.set_date_format(date_format);
    }

    // MOD su command discovery (sets default)
    get_valid_su();

    // MOD terminal discovery
    const char* main_terminal = xset_get_s(XSetName::MAIN_TERMINAL);
    if (!main_terminal)
    {
        const auto supported_terminals = terminal_handlers->get_supported_terminal_names();
        for (const std::string_view supported_terminal : supported_terminals)
        {
            const std::string terminal = Glib::find_program_in_path(supported_terminal.data());
            if (terminal.empty())
            {
                continue;
            }

            xset_set(XSetName::MAIN_TERMINAL, XSetVar::S, Glib::path_get_basename(terminal));
            xset_set_b(XSetName::MAIN_TERMINAL, true); // discovery
            break;
        }
    }

    // MOD editor discovery
    std::string app_name = xset_get_s(XSetName::EDITOR);
    if (app_name.empty())
    {
        vfs::mime_type mime_type = vfs_mime_type_get_from_type("text/plain");
        if (mime_type)
        {
            app_name = mime_type->get_default_action();
            if (app_name.empty())
            {
                const vfs::desktop desktop = vfs_get_desktop(app_name);
                xset_set(XSetName::EDITOR, XSetVar::S, desktop->get_exec());
            }
        }
    }

    // add default handlers
    ptk_handler_add_defaults(PtkHandlerMode::HANDLER_MODE_ARC, false, false);
    ptk_handler_add_defaults(PtkHandlerMode::HANDLER_MODE_FS, false, false);
    ptk_handler_add_defaults(PtkHandlerMode::HANDLER_MODE_NET, false, false);
    ptk_handler_add_defaults(PtkHandlerMode::HANDLER_MODE_FILE, false, false);

    // set default keys
    xset_default_keys();

    // cache event handlers
    event_handler = std::make_unique<XSetEventHandler>();
}

void
autosave_settings()
{
    save_settings(nullptr);
}

void
save_settings(void* main_window_ptr)
{
    MainWindow* main_window;
    // ztd::logger::info("save_settings");

    // save tabs
    const bool save_tabs = xset_get_b(XSetName::MAIN_SAVE_TABS);
    if (main_window_ptr)
    {
        main_window = MAIN_WINDOW(main_window_ptr);
    }
    else
    {
        main_window = main_window_get_last_active();
    }

    if (GTK_IS_WIDGET(main_window))
    {
        if (save_tabs)
        {
            for (const panel_t p : PANELS)
            {
                xset_t set = xset_get_panel(p, XSetPanel::SHOW);
                if (GTK_IS_NOTEBOOK(main_window->panel[p - 1]))
                {
                    const i32 pages =
                        gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_window->panel[p - 1]));
                    if (pages) // panel was shown
                    {
                        if (set->s)
                        {
                            std::free(set->s);
                            set->s = nullptr;
                        }
                        std::string tabs;
                        for (const auto i : ztd::range(pages))
                        {
                            PtkFileBrowser* file_browser = PTK_FILE_BROWSER_REINTERPRET(
                                gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_window->panel[p - 1]),
                                                          i));
                            tabs = fmt::format("{}{}{}",
                                               tabs,
                                               CONFIG_FILE_TABS_DELIM,
                                               ptk_file_browser_get_cwd(file_browser));
                        }
                        set->s = ztd::strdup(tabs);

                        // save current tab
                        if (set->x)
                        {
                            std::free(set->x);
                        }

                        const i32 current_page =
                            gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->panel[p - 1]));
                        set->x = ztd::strdup(current_page);
                    }
                }
            }
        }
        else
        {
            // clear saved tabs
            for (const panel_t p : PANELS)
            {
                xset_t set = xset_get_panel(p, XSetPanel::SHOW);
                if (set->s)
                {
                    std::free(set->s);
                    set->s = nullptr;
                }
                if (set->x)
                {
                    std::free(set->x);
                    set->x = nullptr;
                }
            }
        }
    }

    /* save settings */
    const std::string& settings_config_dir = vfs::user_dirs->program_config_dir();
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

bool
xset_opener(PtkFileBrowser* file_browser, const char job)
{ // find an opener for job
    xset_t set;
    xset_t mset;
    xset_t open_all_set;
    xset_t tset;
    xset_t open_all_tset;
    xset_context_t context;
    bool found = false;
    char pinned;

    for (xset_t set2 : xsets)
    {
        assert(set2 != nullptr);

        if (!set2->lock && set2->opener == job && set2->tool == XSetTool::NOT &&
            set2->menu_style != XSetMenu::SUBMENU && set2->menu_style != XSetMenu::SEP)
        {
            if (set2->desc && ztd::same(set2->desc, "@plugin@mirror@"))
            {
                // is a plugin mirror
                mset = set2;
                if (!mset->shared_key)
                {
                    continue;
                }
                set2 = xset_is(mset->shared_key);
            }
            else if (set2->plugin && set2->shared_key)
            {
                // plugin with mirror - ignore to use mirror's context only
                continue;
            }
            else
            {
                set = mset = set2;
            }

            if (!context)
            {
                if (!(context = xset_context_new()))
                {
                    return false;
                }
                if (file_browser)
                {
                    main_context_fill(file_browser, context);
                }
                else
                {
                    return false;
                }

                if (!context->valid)
                {
                    return false;
                }

                // get mime type open_all_type set
                std::string str = context->var[ItemPropContext::CONTEXT_MIME];
                str = ztd::replace(str, "-", "_");
                str = ztd::replace(str, " ", "");
                open_all_set = xset_is(fmt::format("open_all_type_{}", str));
            }

            // test context
            if (mset->context)
            {
                const i32 context_action = xset_context_test(context, mset->context, false);
                if (context_action == ItemPropContextState::CONTEXT_HIDE ||
                    context_action == ItemPropContextState::CONTEXT_DISABLE)
                {
                    continue;
                }
            }

            // valid custom type?
            const auto cmd_type = XSetCMD(xset_get_int(set, XSetVar::X));
            if (cmd_type != XSetCMD::APP && cmd_type != XSetCMD::LINE &&
                cmd_type != XSetCMD::SCRIPT)
            {
                continue;
            }

            // is set pinned to open_all_type for pre-context?
            pinned = 0;
            for (xset_t set3 : xsets)
            {
                assert(set3 != nullptr);

                if (set3->next && ztd::startswith(set3->name, "open_all_type_"))
                {
                    tset = open_all_tset = set3;
                    while (tset->next)
                    {
                        if (ztd::same(set->name, tset->next))
                        {
                            // found pinned to open_all_type
                            if (open_all_tset == open_all_set)
                            {
                                // correct mime type
                                pinned = 2;
                            }
                            else
                            {
                                // wrong mime type
                                pinned = 1;
                            }
                            break;
                        }
                        if (tset->next)
                        {
                            tset = xset_is(tset->next);
                        }
                    }
                }
            }
            if (pinned == 1)
            {
                continue;
            }

            // valid
            found = true;
            set->browser = file_browser;
            const std::string clean = clean_label(set->menu_label, false, false);
            ztd::logger::info("Selected Menu Item '{}' As Handler", clean);
            xset_menu_cb(nullptr, set); // also does custom activate
        }
    }
    return found;
}

GtkWidget*
xset_get_image(const char* icon, GtkIconSize icon_size)
{
    /*
        GtkIconSize::GTK_ICON_SIZE_MENU,
        GtkIconSize::GTK_ICON_SIZE_SMALL_TOOLBAR,
        GtkIconSize::GTK_ICON_SIZE_LARGE_TOOLBAR,
        GtkIconSize::GTK_ICON_SIZE_BUTTON,
        GtkIconSize::GTK_ICON_SIZE_DND,
        GtkIconSize::GTK_ICON_SIZE_DIALOG
    */

    if (!(icon && icon[0]))
    {
        return nullptr;
    }

    if (!icon_size)
    {
        icon_size = GtkIconSize::GTK_ICON_SIZE_MENU;
    }

    return gtk_image_new_from_icon_name(icon, icon_size);
}

void
xset_add_menu(PtkFileBrowser* file_browser, GtkWidget* menu, GtkAccelGroup* accel_group,
              const char* elements)
{
    if (!elements)
    {
        return;
    }

    const std::vector<std::string> split_elements = ztd::split(elements, " ");

    for (const std::string_view element : split_elements)
    {
        xset_t set = xset_get(element);
        xset_add_menuitem(file_browser, menu, accel_group, set);
    }
}

static GtkWidget*
xset_new_menuitem(const char* label, const char* icon)
{
    GtkWidget* item;

    if (label && ztd::contains(label, "\\_"))
    {
        // allow escape of underscore
        const std::string str = clean_label(label, false, false);
        item = gtk_menu_item_new_with_label(str.data());
    }
    else
    {
        item = gtk_menu_item_new_with_mnemonic(label);
    }
    if (!(icon && icon[0]))
    {
        return item;
    }
    // GtkWidget* image = xset_get_image(icon, GtkIconSize::GTK_ICON_SIZE_MENU);

    return item;
}

GtkWidget*
xset_add_menuitem(PtkFileBrowser* file_browser, GtkWidget* menu, GtkAccelGroup* accel_group,
                  xset_t set)
{
    GtkWidget* item = nullptr;
    GtkWidget* submenu;
    xset_t keyset;
    xset_t set_next;
    char* icon_name = nullptr;
    char* context = nullptr;
    i32 context_action = ItemPropContextState::CONTEXT_SHOW;
    xset_t mset;
    std::string icon_file;
    // ztd::logger::info("xset_add_menuitem {}", set->name);

    // plugin?
    mset = xset_get_plugin_mirror(set);
    if (set->plugin && set->shared_key)
    {
        icon_name = mset->icon;
        context = mset->context;
    }
    if (!icon_name)
    {
        icon_name = set->icon;
    }
    if (!icon_name)
    {
        if (set->plugin)
        {
            icon_file = Glib::build_filename(set->plug_dir, set->plug_name, "icon");
        }
        else
        {
            icon_file = Glib::build_filename(vfs::user_dirs->program_config_dir(),
                                             "scripts",
                                             set->name,
                                             "icon");
        }

        if (std::filesystem::exists(icon_file))
        {
            icon_name = ztd::strdup(icon_file);
        }
    }
    if (!context)
    {
        context = set->context;
    }

    // context?
    if (context && set->tool == XSetTool::NOT && xset_context && xset_context->valid &&
        !xset_get_b(XSetName::CONTEXT_DLG))
    {
        context_action = xset_context_test(xset_context, context, set->disable);
    }

    if (context_action != ItemPropContextState::CONTEXT_HIDE)
    {
        if (set->tool != XSetTool::NOT && set->menu_style != XSetMenu::SUBMENU)
        {
            // item = xset_new_menuitem( set->menu_label, icon_name );
        }
        else if (set->menu_style != XSetMenu::NORMAL)
        {
            xset_t set_radio;
            switch (set->menu_style)
            {
                case XSetMenu::CHECK:
                    if (!(!set->lock && (XSetCMD(xset_get_int(set, XSetVar::X)) > XSetCMD::SCRIPT)))
                    {
                        item = gtk_check_menu_item_new_with_mnemonic(set->menu_label);
                        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),
                                                       mset->b == XSetB::XSET_B_TRUE);
                    }
                    break;
                case XSetMenu::RADIO:
                    if (set->ob2_data)
                    {
                        set_radio = XSET(set->ob2_data);
                    }
                    else
                    {
                        set_radio = set;
                    }
                    item = gtk_radio_menu_item_new_with_mnemonic((GSList*)set_radio->ob2_data,
                                                                 set->menu_label);
                    set_radio->ob2_data =
                        (void*)gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
                    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),
                                                   mset->b == XSetB::XSET_B_TRUE);
                    break;
                case XSetMenu::SUBMENU:
                    submenu = gtk_menu_new();
                    item = xset_new_menuitem(set->menu_label, icon_name);
                    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
                    g_signal_connect(submenu,
                                     "key-press-event",
                                     G_CALLBACK(xset_menu_keypress),
                                     nullptr);
                    if (set->lock)
                    {
                        xset_add_menu(file_browser, submenu, accel_group, set->desc);
                    }
                    else if (set->child)
                    {
                        set_next = xset_get(set->child);
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
                                set_next = xset_get(set->next);
                                xset_add_menuitem(file_browser, menu, accel_group, set_next);
                            }
                            return item;
                        }
                    }
                    break;
                case XSetMenu::SEP:
                    item = gtk_separator_menu_item_new();
                    break;
                case XSetMenu::NORMAL:
                case XSetMenu::STRING:
                case XSetMenu::FILEDLG:
                case XSetMenu::FONTDLG:
                case XSetMenu::ICON:
                case XSetMenu::COLORDLG: // deprecated
                case XSetMenu::CONFIRM:
                case XSetMenu::RESERVED_03:
                case XSetMenu::RESERVED_04:
                case XSetMenu::RESERVED_05:
                case XSetMenu::RESERVED_06:
                case XSetMenu::RESERVED_07:
                case XSetMenu::RESERVED_08:
                case XSetMenu::RESERVED_09:
                case XSetMenu::RESERVED_10:
                default:
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
            const auto cmd_type = XSetCMD(xset_get_int(set, XSetVar::X));
            if (!set->lock && cmd_type == XSetCMD::APP)
            {
                // Application
                char* menu_label = xset_custom_get_app_name_icon(set, &app_icon, icon_size);
                item = xset_new_menuitem(menu_label, nullptr);
                std::free(menu_label);
            }
            else
            {
                item = xset_new_menuitem(set->menu_label, icon_name);
            }

            if (app_icon)
            {
                g_object_unref(app_icon);
            }
        }

        set->browser = file_browser;
        g_object_set_data(G_OBJECT(item), "menu", menu);
        g_object_set_data(G_OBJECT(item), "set", set);

        if (set->ob1)
        {
            g_object_set_data(G_OBJECT(item), set->ob1, set->ob1_data);
        }
        if (set->menu_style != XSetMenu::RADIO && set->ob2)
        {
            g_object_set_data(G_OBJECT(item), set->ob2, set->ob2_data);
        }

        if (set->menu_style < XSetMenu::SUBMENU)
        {
            // activate callback
            if (!set->cb_func || set->menu_style != XSetMenu::NORMAL)
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
                keyset = xset_get(set->shared_key);
            }
            else
            {
                keyset = set;
            }
            if (keyset->key > 0 && accel_group)
            {
                gtk_widget_add_accelerator(item,
                                           "activate",
                                           accel_group,
                                           keyset->key,
                                           (GdkModifierType)keyset->keymod,
                                           GTK_ACCEL_VISIBLE);
            }
        }
        // design mode callback
        g_signal_connect(item, "button-press-event", G_CALLBACK(xset_design_cb), set);
        g_signal_connect(item, "button-release-event", G_CALLBACK(xset_design_cb), set);

        gtk_widget_set_sensitive(item,
                                 context_action != ItemPropContextState::CONTEXT_DISABLE &&
                                     !set->disable);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }

    // next item
    if (set->next)
    {
        set_next = xset_get(set->next);
        xset_add_menuitem(file_browser, menu, accel_group, set_next);
    }
    return item;
}

static void
xset_custom_activate(GtkWidget* item, xset_t set)
{
    (void)item;
    GtkWidget* parent;
    GtkWidget* task_view = nullptr;
    std::string cwd;
    std::string value;
    xset_t mset;

    // builtin toolitem?
    if (set->tool > XSetTool::CUSTOM)
    {
        xset_builtin_tool_activate(set->tool, set, nullptr);
        return;
    }

    // plugin?
    mset = xset_get_plugin_mirror(set);

    if (set->browser)
    {
        parent = GTK_WIDGET(set->browser);
        task_view = set->browser->task_view;
        cwd = ptk_file_browser_get_cwd(set->browser);
    }
    else
    {
        ztd::logger::warn("xset_custom_activate !browser !desktop");
        return;
    }

    // name
    if (!set->plugin && !(!set->lock && XSetCMD(xset_get_int(set, XSetVar::X)) >
                                            XSetCMD::SCRIPT /*app or bookmark*/))
    {
        if (!(set->menu_label && set->menu_label[0]) ||
            (set->menu_label && ztd::same(set->menu_label, "New _Command")))
        {
            const bool response = xset_text_dialog(parent,
                                                   "Change Item Name",
                                                   enter_menu_name_new,
                                                   "",
                                                   set->menu_label,
                                                   &set->menu_label,
                                                   "",
                                                   false);
            if (!response)
            {
                return;
            }
        }
    }

    // variable value
    switch (set->menu_style)
    {
        case XSetMenu::CHECK:
            value = fmt::format("{:d}", mset->b == XSetB::XSET_B_TRUE ? 1 : 0);
            break;
        case XSetMenu::STRING:
            value = mset->s;
            break;
        case XSetMenu::NORMAL:
        case XSetMenu::RADIO:
        case XSetMenu::FILEDLG:
        case XSetMenu::FONTDLG:
        case XSetMenu::ICON:
        case XSetMenu::COLORDLG: // deprecated
        case XSetMenu::CONFIRM:
        case XSetMenu::RESERVED_03:
        case XSetMenu::RESERVED_04:
        case XSetMenu::RESERVED_05:
        case XSetMenu::RESERVED_06:
        case XSetMenu::RESERVED_07:
        case XSetMenu::RESERVED_08:
        case XSetMenu::RESERVED_09:
        case XSetMenu::RESERVED_10:
        case XSetMenu::SUBMENU:
        case XSetMenu::SEP:
        default:
            value = set->menu_label;
            break;
    }

    // is not activatable command?
    if (!(!set->lock && set->menu_style < XSetMenu::SUBMENU))
    {
        xset_item_prop_dlg(xset_context, set, 0);
        return;
    }

    // command
    std::string command;
    bool app_no_sync = false;
    const auto cmd_type = XSetCMD(xset_get_int(set, XSetVar::X));
    switch (cmd_type)
    {
        case XSetCMD::LINE:
            // line
            if (!set->line || set->line[0] == '\0')
            {
                xset_item_prop_dlg(xset_context, set, 2);
                return;
            }
            command = replace_line_subs(set->line);
            command = ztd::replace(command, "\\n", "\n");
            command = ztd::replace(command, "\\t", "\t");
            break;
        case XSetCMD::SCRIPT:
            // script
            command = xset_custom_get_script(set, false);
            if (command.empty())
            {
                return;
            }
            break;
        case XSetCMD::APP:
            // app or executable
            if (!(set->z && set->z[0]))
            {
                xset_item_prop_dlg(xset_context, set, 0);
                return;
            }
            else if (ztd::endswith(set->z, ".desktop"))
            {
                const vfs::desktop desktop = vfs_get_desktop(set->z);
                if (!desktop->get_exec().empty())
                {
                    // get file list
                    std::vector<vfs::file_info> sel_files;
                    if (set->browser)
                    {
                        sel_files = ptk_file_browser_get_selected_files(set->browser);
                    }
                    else
                    {
                        cwd = "/";
                    }

                    std::vector<std::string> open_files;
                    open_files.reserve(sel_files.size());
                    for (vfs::file_info file : sel_files)
                    {
                        const std::string open_file = Glib::build_filename(cwd, file->get_name());

                        open_files.emplace_back(open_file);
                    }

                    // open in app
                    try
                    {
                        desktop->open_files(cwd, open_files);
                    }
                    catch (const VFSAppDesktopException& e)
                    {
                        ptk_show_error(parent ? GTK_WINDOW(parent) : nullptr, "Error", e.what());
                    }

                    vfs_file_info_list_free(sel_files);
                }
                return;
            }
            else
            {
                command = ztd::shell::quote(set->z);
                app_no_sync = true;
            }
            break;
        case XSetCMD::BOOKMARK:
        case XSetCMD::INVALID:
        default:
            return;
    }

    // task
    const std::string task_name = clean_label(set->menu_label, false, false);
    PtkFileTask* ptask = ptk_file_exec_new(task_name, cwd, parent, task_view);
    // do not free cwd!
    ptask->task->exec_browser = set->browser;
    ptask->task->exec_command = command;
    ptask->task->exec_set = set;

    if (set->y && set->y[0] != '\0')
    {
        ptask->task->exec_as_user = set->y;
    }

    if (set->plugin && set->shared_key && mset->icon)
    {
        ptask->task->exec_icon = mset->icon;
    }
    else if (set->icon)
    {
        ptask->task->exec_icon = set->icon;
    }

    ptask->task->current_dest = value; // temp storage
    ptask->task->exec_terminal = mset->in_terminal;
    ptask->task->exec_keep_terminal = mset->keep_terminal;
    ptask->task->exec_sync = !app_no_sync && mset->task;
    ptask->task->exec_popup = mset->task_pop;
    ptask->task->exec_show_output = mset->task_out;
    ptask->task->exec_show_error = mset->task_err;
    ptask->task->exec_scroll_lock = mset->scroll_lock;
    ptask->task->exec_checksum = set->plugin;
    ptask->task->exec_export = true;
    // ptask->task->exec_keep_tmp = true;

    ptk_file_task_run(ptask);
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
        std::free(set->parent);
        set->parent = nullptr;
    }

    std::free(set->prev);
    std::free(set->next);
    set->prev = ztd::strdup(target->name);
    set->next = target->next; // steal string
    if (target->next)
    {
        xset_t target_next = xset_get(target->next);
        if (target_next->prev)
        {
            std::free(target_next->prev);
        }
        target_next->prev = ztd::strdup(set->name);
    }
    target->next = ztd::strdup(set->name);
    if (target->tool != XSetTool::NOT)
    {
        if (set->tool < XSetTool::CUSTOM)
        {
            set->tool = XSetTool::CUSTOM;
        }
    }
    else
    {
        if (set->tool > XSetTool::CUSTOM)
        {
            ztd::logger::warn("xset_custom_insert_after builtin tool inserted after non-tool");
        }
        set->tool = XSetTool::NOT;
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
        xset_t set_parent = xset_get(set->parent);
        if (xset_clipboard_in_set(set_parent))
        {
            return true;
        }
    }

    if (set->prev)
    {
        xset_t set_prev = xset_get(set->prev);
        while (set_prev)
        {
            if (set_prev->parent)
            {
                xset_t set_prev_parent = xset_get(set_prev->parent);
                if (xset_clipboard_in_set(set_prev_parent))
                {
                    return true;
                }
                set_prev = nullptr;
            }
            else if (set_prev->prev)
            {
                set_prev = xset_get(set_prev->prev);
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
xset_edit(GtkWidget* parent, const char* path, bool force_root, bool no_root)
{
    bool as_root = false;
    bool terminal;
    GtkWidget* dlgparent = nullptr;
    if (!path)
    {
        return;
    }
    if (force_root && no_root)
    {
        return;
    }

    if (parent)
    {
        dlgparent = gtk_widget_get_toplevel(GTK_WIDGET(parent));
    }

    std::string editor;
    if (geteuid() != 0 && !force_root && (no_root || have_rw_access(path)))
    {
        editor = xset_get_s(XSetName::EDITOR);
        if (editor.empty() || editor.at(0) == '\0')
        {
            ptk_show_error(dlgparent ? GTK_WINDOW(dlgparent) : nullptr,
                           "Editor Not Set",
                           "Please set your editor in View|Preferences|Advanced");
            return;
        }
        terminal = xset_get_b(XSetName::EDITOR);
    }
    else
    {
        editor = xset_get_s(XSetName::ROOT_EDITOR);
        if (editor.empty() || editor.at(0) == '\0')
        {
            ptk_show_error(dlgparent ? GTK_WINDOW(dlgparent) : nullptr,
                           "Root Editor Not Set",
                           "Please set root's editor in View|Preferences|Advanced");
            return;
        }
        as_root = true;
        terminal = xset_get_b(XSetName::ROOT_EDITOR);
    }
    // replacements
    const std::string quoted_path = ztd::shell::quote(path);
    if (ztd::contains(editor, "%f"))
    {
        editor = ztd::replace(editor, "%f", quoted_path);
    }
    else if (ztd::contains(editor, "%F"))
    {
        editor = ztd::replace(editor, "%F", quoted_path);
    }
    else if (ztd::contains(editor, "%u"))
    {
        editor = ztd::replace(editor, "%u", quoted_path);
    }
    else if (ztd::contains(editor, "%U"))
    {
        editor = ztd::replace(editor, "%U", quoted_path);
    }
    else
    {
        editor = fmt::format("{} {}", editor, quoted_path);
    }
    editor = fmt::format("{} {}", editor, quoted_path);

    // task
    const std::string task_name = fmt::format("Edit {}", path);
    const std::string cwd = Glib::path_get_dirname(path);
    PtkFileTask* ptask = ptk_file_exec_new(task_name, cwd, dlgparent, nullptr);
    ptask->task->exec_command = editor;
    ptask->task->exec_sync = false;
    ptask->task->exec_terminal = terminal;
    if (as_root)
    {
        ptask->task->exec_as_user = "root";
    }
    ptk_file_task_run(ptask);
}

const std::string
xset_get_keyname(xset_t set, i32 key_val, i32 key_mod)
{
    assert(set != nullptr);

    i32 keyval;
    i32 keymod;
    if (set)
    {
        keyval = set->key;
        keymod = set->keymod;
    }
    else
    {
        keyval = key_val;
        keymod = key_mod;
    }
    if (keyval <= 0)
    {
        return "( none )";
    }

    std::string mod = gdk_keyval_name(keyval);

#if 0
    if (mod && mod[0] && !mod[1] && g_ascii_isalpha(mod[0]))
        mod[0] = g_ascii_toupper(mod[0]);
    else if (!mod)
        mod = ztd::strdup("NA");
#endif

    if (keymod)
    {
        if (keymod & GdkModifierType::GDK_SUPER_MASK)
        {
            mod = fmt::format("Super+{}", mod);
        }
        if (keymod & GdkModifierType::GDK_HYPER_MASK)
        {
            mod = fmt::format("Hyper+{}", mod);
        }
        if (keymod & GdkModifierType::GDK_META_MASK)
        {
            mod = fmt::format("Meta+{}", mod);
        }
        if (keymod & GdkModifierType::GDK_MOD1_MASK)
        {
            mod = fmt::format("Alt+{}", mod);
        }
        if (keymod & GdkModifierType::GDK_CONTROL_MASK)
        {
            mod = fmt::format("Ctrl+{}", mod);
        }
        if (keymod & GdkModifierType::GDK_SHIFT_MASK)
        {
            mod = fmt::format("Shift+{}", mod);
        }
    }
    return mod;
}

static bool
on_set_key_keypress(GtkWidget* widget, GdkEventKey* event, GtkWidget* dlg)
{
    (void)widget;
    i32* newkey = (i32*)g_object_get_data(G_OBJECT(dlg), "newkey");
    i32* newkeymod = (i32*)g_object_get_data(G_OBJECT(dlg), "newkeymod");
    GtkWidget* btn = GTK_WIDGET(g_object_get_data(G_OBJECT(dlg), "btn"));
    xset_t set = XSET(g_object_get_data(G_OBJECT(dlg), "set"));
    xset_t keyset = nullptr;
    std::string keyname;

    const u32 keymod = ptk_get_keymod(event->state);

    if (!event->keyval) // || ( event->keyval < 1000 && !keymod ) )
    {
        *newkey = 0;
        *newkeymod = 0;
        gtk_widget_set_sensitive(btn, false);
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), nullptr);
        return true;
    }

    gtk_widget_set_sensitive(btn, true);

    if (*newkey != 0 && keymod == 0)
    {
        if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter)
        {
            // user pressed Enter after selecting a key, so click Set
            gtk_button_clicked(GTK_BUTTON(btn));
            return true;
        }
        else if (event->keyval == GDK_KEY_Escape && *newkey == GDK_KEY_Escape)
        {
            // user pressed Escape twice so click Unset
            GtkWidget* btn_unset = GTK_WIDGET(g_object_get_data(G_OBJECT(dlg), "btn_unset"));
            gtk_button_clicked(GTK_BUTTON(btn_unset));
            return true;
        }
    }

#if defined(HAVE_NONLATIN_KEYBOARD_SUPPORT)
    u32 nonlatin_key = 0;
    // need to transpose nonlatin keyboard layout ?
    if (!((GDK_KEY_0 <= event->keyval && event->keyval <= GDK_KEY_9) ||
          (GDK_KEY_A <= event->keyval && event->keyval <= GDK_KEY_Z) ||
          (GDK_KEY_a <= event->keyval && event->keyval <= GDK_KEY_z)))
    {
        nonlatin_key = event->keyval;
        transpose_nonlatin_keypress(event);
    }
#endif

    *newkey = 0;
    *newkeymod = 0;
    if (set->shared_key)
    {
        keyset = xset_get(set->shared_key);
    }

    for (xset_t set2 : xsets)
    {
        assert(set2 != nullptr);

        if (set2 && set2 != set && set2->key > 0 && set2->key == event->keyval &&
            set2->keymod == keymod && set2 != keyset)
        {
            std::string name;
            if (set2->desc && ztd::same(set2->desc, "@plugin@mirror@") && set2->shared_key)
            {
                // set2 is plugin mirror
                xset_t rset = xset_get(set2->shared_key);
                if (rset->menu_label)
                {
                    name = clean_label(rset->menu_label, false, false);
                }
                else
                {
                    name = "( no name )";
                }
            }
            else if (set2->menu_label)
            {
                name = clean_label(set2->menu_label, false, false);
            }
            else
            {
                name = "( no name )";
            }

            keyname = xset_get_keyname(nullptr, event->keyval, keymod);
#if defined(HAVE_NONLATIN_KEYBOARD_SUPPORT)
            if (nonlatin_key == 0)
#endif
                gtk_message_dialog_format_secondary_text(
                    GTK_MESSAGE_DIALOG(dlg),
                    "\t%s\n\tKeycode: %#4x  Modifier: %#x\n\n%s is already assigned to "
                    "'%s'.\n\nPress a different key or click Set to replace the current key "
                    "assignment.",
                    keyname.data(),
                    event->keyval,
                    keymod,
                    keyname.data(),
                    name.data());
#if defined(HAVE_NONLATIN_KEYBOARD_SUPPORT)
            else
                gtk_message_dialog_format_secondary_text(
                    GTK_MESSAGE_DIALOG(dlg),
                    "\t%s\n\tKeycode: %#4x [%#4x]  Modifier: %#x\n\n%s is already assigned to "
                    "'%s'.\n\nPress a different key or click Set to replace the current key "
                    "assignment.",
                    keyname.data(),
                    event->keyval,
                    nonlatin_key,
                    keymod,
                    keyname.data(),
                    name.data());
#endif
            *newkey = event->keyval;
            *newkeymod = keymod;
            return true;
        }
    }
    keyname = xset_get_keyname(nullptr, event->keyval, keymod);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg),
                                             "\t%s\n\tKeycode: %#4x  Modifier: %#x",
                                             keyname.data(),
                                             event->keyval,
                                             keymod);
    *newkey = event->keyval;
    *newkeymod = keymod;
    return true;
}

void
xset_set_key(GtkWidget* parent, xset_t set)
{
    std::string name;
    xset_t keyset;
    u32 newkey = 0;
    u32 newkeymod = 0;
    GtkWidget* dlgparent = nullptr;

    if (set->menu_label)
    {
        name = clean_label(set->menu_label, false, true);
    }
    else if (set->tool > XSetTool::CUSTOM)
    {
        name = xset_get_builtin_toolitem_label(set->tool);
    }
    else if (ztd::startswith(set->name, "open_all_type_"))
    {
        keyset = xset_get(XSetName::OPEN_ALL);
        name = clean_label(keyset->menu_label, false, true);
        if (set->shared_key)
        {
            std::free(set->shared_key);
        }
        set->shared_key = ztd::strdup(xset_get_name_from_xsetname(XSetName::OPEN_ALL));
    }
    else
    {
        name = "( no name )";
    }

    const std::string keymsg =
        fmt::format("Press your key combination for item '{}' then click Set.  To "
                    "remove the current key assignment, click Unset.",
                    name);
    if (parent)
    {
        dlgparent = gtk_widget_get_toplevel(parent);
    }

    GtkWidget* dlg = gtk_message_dialog_new_with_markup(GTK_WINDOW(dlgparent),
                                                        GtkDialogFlags::GTK_DIALOG_MODAL,
                                                        GtkMessageType::GTK_MESSAGE_QUESTION,
                                                        GtkButtonsType::GTK_BUTTONS_NONE,
                                                        keymsg.data(),
                                                        nullptr);
    xset_set_window_icon(GTK_WINDOW(dlg));

    GtkWidget* btn_cancel = gtk_button_new_with_label("Cancel");
    gtk_button_set_label(GTK_BUTTON(btn_cancel), "Cancel");
    gtk_dialog_add_action_widget(GTK_DIALOG(dlg), btn_cancel, GtkResponseType::GTK_RESPONSE_CANCEL);

    GtkWidget* btn_unset = gtk_button_new_with_label("NO");
    gtk_button_set_label(GTK_BUTTON(btn_unset), "Unset");
    gtk_dialog_add_action_widget(GTK_DIALOG(dlg), btn_unset, GtkResponseType::GTK_RESPONSE_NO);

    if (set->shared_key)
    {
        keyset = xset_get(set->shared_key);
    }
    else
    {
        keyset = set;
    }
    if (keyset->key <= 0)
    {
        gtk_widget_set_sensitive(btn_unset, false);
    }

    GtkWidget* btn = gtk_button_new_with_label("Apply");
    gtk_button_set_label(GTK_BUTTON(btn), "Set");
    gtk_dialog_add_action_widget(GTK_DIALOG(dlg), btn, GtkResponseType::GTK_RESPONSE_OK);
    gtk_widget_set_sensitive(btn, false);

    g_object_set_data(G_OBJECT(dlg), "set", set);
    g_object_set_data(G_OBJECT(dlg), "newkey", &newkey);
    g_object_set_data(G_OBJECT(dlg), "newkeymod", &newkeymod);
    g_object_set_data(G_OBJECT(dlg), "btn", btn);
    g_object_set_data(G_OBJECT(dlg), "btn_unset", btn_unset);
    g_signal_connect(dlg, "key_press_event", G_CALLBACK(on_set_key_keypress), dlg);
    gtk_widget_show_all(dlg);
    gtk_window_set_title(GTK_WINDOW(dlg), "Set Key");

    const i32 response = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
    if (response == GtkResponseType::GTK_RESPONSE_OK ||
        response == GtkResponseType::GTK_RESPONSE_NO)
    {
        if (response == GtkResponseType::GTK_RESPONSE_OK && (newkey || newkeymod))
        {
            // clear duplicate key assignments
            for (xset_t set2 : xsets)
            {
                assert(set2 != nullptr);

                if (set2 && set2->key > 0 && set2->key == newkey && set2->keymod == newkeymod)
                {
                    set2->key = 0;
                    set2->keymod = 0;
                }
            }
        }
        else if (response == GtkResponseType::GTK_RESPONSE_NO)
        {
            newkey = 0; // unset
            newkeymod = 0;
        }
        // plugin? set shared_key to mirror if not
        if (set->plugin && !set->shared_key)
        {
            xset_get_plugin_mirror(set);
        }
        // set new key
        if (set->shared_key)
        {
            keyset = xset_get(set->shared_key);
        }
        else
        {
            keyset = set;
        }
        keyset->key = newkey;
        keyset->keymod = newkeymod;
    }
}

static bool
xset_job_is_valid(xset_t set, XSetJob job)
{
    assert(set != nullptr);

    bool no_remove = false;
    bool no_paste = false;
    bool open_all = false;

    if (!set)
    {
        return false;
    }

    if (set->plugin)
    {
        if (!set->plug_dir)
        {
            return false;
        }
        if (!set->plugin_top)
        {
            no_remove = true;
        }
    }

    // control open_all item
    if (ztd::startswith(set->name, "open_all_type_"))
    {
        open_all = true;
    }

    switch (job)
    {
        case XSetJob::KEY:
            return set->menu_style < XSetMenu::SUBMENU;
        case XSetJob::ICON:
            return ((set->menu_style == XSetMenu::NORMAL || set->menu_style == XSetMenu::STRING ||
                     set->menu_style == XSetMenu::FONTDLG || set->menu_style == XSetMenu::SUBMENU ||
                     set->tool != XSetTool::NOT) &&
                    !open_all);
        case XSetJob::EDIT:
            return !set->lock && set->menu_style < XSetMenu::SUBMENU;
        case XSetJob::COMMAND:
            return !set->plugin;
        case XSetJob::CUT:
            return (!set->lock && !set->plugin);
        case XSetJob::COPY:
            return !set->lock;
        case XSetJob::PASTE:
            if (!xset_set_clipboard)
            {
                no_paste = true;
            }
            else if (set->plugin)
            {
                no_paste = true;
            }
            else if (set == xset_set_clipboard && xset_clipboard_is_cut)
            {
                // do not allow cut paste to self
                no_paste = true;
            }
            else if (xset_set_clipboard->tool > XSetTool::CUSTOM && set->tool == XSetTool::NOT)
            {
                // do not allow paste of builtin tool item to menu
                no_paste = true;
            }
            else if (xset_set_clipboard->menu_style == XSetMenu::SUBMENU)
            {
                // do not allow paste of submenu to self or below
                no_paste = xset_clipboard_in_set(set);
            }
            return !no_paste;
        case XSetJob::REMOVE:
            return (!set->lock && !no_remove);
        // case XSetJob::CONTEXT:
        //    return ( xset_context && xset_context->valid && !open_all );
        case XSetJob::PROP:
        case XSetJob::PROP_CMD:
            return true;
        case XSetJob::LABEL:
        case XSetJob::EDIT_ROOT:
        case XSetJob::LINE:
        case XSetJob::SCRIPT:
        case XSetJob::CUSTOM:
        case XSetJob::TERM:
        case XSetJob::KEEP:
        case XSetJob::USER:
        case XSetJob::TASK:
        case XSetJob::POP:
        case XSetJob::ERR:
        case XSetJob::OUT:
        case XSetJob::BOOKMARK:
        case XSetJob::APP:
        case XSetJob::SUBMENU:
        case XSetJob::SUBMENU_BOOK:
        case XSetJob::SEP:
        case XSetJob::ADD_TOOL:
        case XSetJob::IMPORT_FILE:
        case XSetJob::REMOVE_BOOK:
        case XSetJob::NORMAL:
        case XSetJob::CHECK:
        case XSetJob::CONFIRM:
        case XSetJob::DIALOG:
        case XSetJob::MESSAGE:
        case XSetJob::COPYNAME:
        case XSetJob::IGNORE_CONTEXT:
        case XSetJob::SCROLL:
        case XSetJob::EXPORT:
        case XSetJob::BROWSE_FILES:
        case XSetJob::BROWSE_DATA:
        case XSetJob::BROWSE_PLUGIN:
        case XSetJob::HELP:
        case XSetJob::HELP_NEW:
        case XSetJob::HELP_ADD:
        case XSetJob::HELP_BROWSE:
        case XSetJob::HELP_STYLE:
        case XSetJob::HELP_BOOK:
        case XSetJob::TOOLTIPS:
        case XSetJob::INVALID:
        default:
            break;
    }
    return false;
}

static bool
xset_design_menu_keypress(GtkWidget* widget, GdkEventKey* event, xset_t set)
{
    XSetJob job = XSetJob::INVALID;

    GtkWidget* item = gtk_menu_shell_get_selected_item(GTK_MENU_SHELL(widget));
    if (!item)
    {
        return false;
    }

    const u32 keymod = ptk_get_keymod(event->state);

#if defined(HAVE_NONLATIN_KEYBOARD_SUPPORT)
    transpose_nonlatin_keypress(event);
#endif

    switch (keymod)
    {
        case 0:
            switch (event->keyval)
            {
                case GDK_KEY_F1:
                    return true;
                case GDK_KEY_F3:
                    job = XSetJob::PROP;
                    break;
                case GDK_KEY_F4:
                    if (XSetCMD(xset_get_int(set, XSetVar::X)) == XSetCMD::SCRIPT)
                    {
                        job = XSetJob::EDIT;
                    }
                    else
                    {
                        job = XSetJob::PROP_CMD;
                    }
                    break;
                case GDK_KEY_Delete:
                    job = XSetJob::REMOVE;
                    break;
                case GDK_KEY_Insert:
                    job = XSetJob::COMMAND;
                    break;
                default:
                    break;
            }
            break;
        case GdkModifierType::GDK_CONTROL_MASK:
            switch (event->keyval)
            {
                case GDK_KEY_c:
                    job = XSetJob::COPY;
                    break;
                case GDK_KEY_x:
                    job = XSetJob::CUT;
                    break;
                case GDK_KEY_v:
                    job = XSetJob::PASTE;
                    break;
                case GDK_KEY_e:
                    if (set->lock)
                    {
                        return false;
                    }
                    else
                    {
                        job = XSetJob::EDIT;
                    }
                    break;
                case GDK_KEY_k:
                    job = XSetJob::KEY;
                    break;
                case GDK_KEY_i:
                    job = XSetJob::ICON;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    if (job != XSetJob::INVALID)
    {
        if (xset_job_is_valid(set, job))
        {
            gtk_menu_shell_deactivate(GTK_MENU_SHELL(widget));
            g_object_set_data(G_OBJECT(item), "job", GINT_TO_POINTER(job));
            xset_design_job(item, set);
            return true;
        }
    }
    return false;
}

static void
on_menu_hide(GtkWidget* widget, GtkWidget* design_menu)
{
    gtk_widget_set_sensitive(widget, true);
    gtk_menu_shell_deactivate(GTK_MENU_SHELL(design_menu));
}

static void
set_check_menu_item_block(GtkWidget* item)
{
    g_signal_handlers_block_matched(item,
                                    GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                    0,
                                    0,
                                    nullptr,
                                    (void*)xset_design_job,
                                    nullptr);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), true);
    g_signal_handlers_unblock_matched(item,
                                      GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                      0,
                                      0,
                                      nullptr,
                                      (void*)xset_design_job,
                                      nullptr);
}

static GtkWidget*
xset_design_additem(GtkWidget* menu, const char* label, XSetJob job, xset_t set)
{
    GtkWidget* item;
    item = gtk_menu_item_new_with_mnemonic(label);

    g_object_set_data(G_OBJECT(item), "job", GINT_TO_POINTER(INT(job)));
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
    GtkWidget* submenu2;
    bool no_remove = false;
    bool no_paste = false;
    // bool open_all = false;

    // xset_t mset;
    xset_t insert_set;

    // book_insert is a bookmark set to be used for Paste, etc
    insert_set = book_insert ? book_insert : set;
    // to signal this is a bookmark, pass book_insert = set
    const bool show_keys = set->tool == XSetTool::NOT;

    if (set->plugin)
    {
        if (set->plug_dir)
        {
            if (!set->plugin_top)
            {
                no_remove = true;
            }
        }
        else
        {
            no_remove = true;
        }
    }

    if (!xset_set_clipboard)
    {
        no_paste = true;
    }
    else if (insert_set->plugin)
    {
        no_paste = true;
    }
    else if (insert_set == xset_set_clipboard && xset_clipboard_is_cut)
    {
        // do not allow cut paste to self
        no_paste = true;
    }
    else if (xset_set_clipboard->tool > XSetTool::CUSTOM && insert_set->tool == XSetTool::NOT)
    {
        // do not allow paste of builtin tool item to menu
        no_paste = true;
    }
    else if (xset_set_clipboard->menu_style == XSetMenu::SUBMENU)
    {
        // do not allow paste of submenu to self or below
        no_paste = xset_clipboard_in_set(insert_set);
    }

    // control open_all item
    // if (ztd::startswith(set->name, "open_all_type_"))
    //    open_all = true;

    GtkWidget* design_menu = gtk_menu_new();
    GtkAccelGroup* accel_group = gtk_accel_group_new();

    // Cut
    newitem = xset_design_additem(design_menu, "Cu_t", XSetJob::CUT, set);
    gtk_widget_set_sensitive(newitem, !set->lock && !set->plugin);
    if (show_keys)
    {
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_x,
                                   GdkModifierType::GDK_CONTROL_MASK,
                                   GTK_ACCEL_VISIBLE);
    }

    // Copy
    newitem = xset_design_additem(design_menu, "_Copy", XSetJob::COPY, set);
    gtk_widget_set_sensitive(newitem, !set->lock);
    if (show_keys)
    {
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_c,
                                   GdkModifierType::GDK_CONTROL_MASK,
                                   GTK_ACCEL_VISIBLE);
    }

    // Paste
    newitem = xset_design_additem(design_menu, "_Paste", XSetJob::PASTE, insert_set);
    gtk_widget_set_sensitive(newitem, !no_paste);
    if (show_keys)
    {
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_v,
                                   GdkModifierType::GDK_CONTROL_MASK,
                                   GTK_ACCEL_VISIBLE);
    }

    // Remove
    newitem = xset_design_additem(design_menu, "_Remove", XSetJob::REMOVE, set);
    gtk_widget_set_sensitive(newitem, !set->lock && !no_remove);
    if (show_keys)
    {
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_Delete,
                                   (GdkModifierType)0,
                                   GTK_ACCEL_VISIBLE);
    }

    // Export
    newitem = xset_design_additem(design_menu, "E_xport", XSetJob::EXPORT, set);
    gtk_widget_set_sensitive(
        newitem,
        (!set->lock && set->menu_style < XSetMenu::SEP && set->tool <= XSetTool::CUSTOM) ||
            set->xset_name == XSetName::MAIN_BOOK);

    //// New submenu
    newitem = gtk_menu_item_new_with_mnemonic("_New");
    submenu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(newitem), submenu);
    gtk_container_add(GTK_CONTAINER(design_menu), newitem);
    gtk_widget_set_sensitive(newitem, !set->plugin);
    g_object_set_data(G_OBJECT(newitem), "job", GINT_TO_POINTER(XSetJob::HELP_NEW));
    g_signal_connect(submenu, "key_press_event", G_CALLBACK(xset_design_menu_keypress), set);

    // New > Bookmark
    newitem = xset_design_additem(submenu, "_Bookmark", XSetJob::BOOKMARK, insert_set);

    // New > Application
    newitem = xset_design_additem(submenu, "_Application", XSetJob::APP, insert_set);

    // New > Command
    newitem = xset_design_additem(submenu, "_Command", XSetJob::COMMAND, insert_set);
    if (show_keys)
    {
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_Insert,
                                   (GdkModifierType)0,
                                   GTK_ACCEL_VISIBLE);
    }

    // New > Submenu
    newitem = xset_design_additem(submenu, "Sub_menu", XSetJob::SUBMENU, insert_set);

    // New > Separator
    newitem = xset_design_additem(submenu, "S_eparator", XSetJob::SEP, insert_set);

    // New > Import >
    newitem = gtk_menu_item_new_with_mnemonic("_Import");
    submenu2 = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(newitem), submenu2);
    gtk_container_add(GTK_CONTAINER(submenu), newitem);
    gtk_widget_set_sensitive(newitem, !insert_set->plugin);
    g_object_set_data(G_OBJECT(newitem), "job", GINT_TO_POINTER(XSetJob::IMPORT_FILE));
    g_signal_connect(submenu2,
                     "key_press_event",
                     G_CALLBACK(xset_design_menu_keypress),
                     insert_set);

    newitem = xset_design_additem(submenu2, "_File", XSetJob::IMPORT_FILE, insert_set);

    if (insert_set->tool != XSetTool::NOT)
    {
        // "Add" submenu for builtin tool items
        newitem = gtk_menu_item_new_with_mnemonic("_Add");
        submenu = gtk_menu_new();
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(newitem), submenu);
        gtk_container_add(GTK_CONTAINER(design_menu), newitem);
        g_object_set_data(G_OBJECT(newitem), "job", GINT_TO_POINTER(XSetJob::HELP_ADD));
        g_signal_connect(submenu, "key_press_event", G_CALLBACK(xset_design_menu_keypress), set);

        for (usize i = INT(XSetTool::DEVICES); i < builtin_tool_name.size(); ++i)
        {
            newitem =
                xset_design_additem(submenu, builtin_tool_name[i], XSetJob::ADD_TOOL, insert_set);
            g_object_set_data(G_OBJECT(newitem), "tool_type", GINT_TO_POINTER(i));
        }
    }

    // Separator
    gtk_container_add(GTK_CONTAINER(design_menu), gtk_separator_menu_item_new());

    // Help
    newitem = xset_design_additem(design_menu, "_Help", XSetJob::HELP, set);
    gtk_widget_set_sensitive(newitem, !set->lock || set->line);
    if (show_keys)
    {
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_F1,
                                   (GdkModifierType)0,
                                   GTK_ACCEL_VISIBLE);
    }

    // Tooltips (toolbar)
    if (set->tool != XSetTool::NOT)
    {
        newitem = xset_design_additem(design_menu, "T_ooltips", XSetJob::TOOLTIPS, set);
        if (!xset_get_b_panel(1, XSetPanel::TOOL_L))
        {
            set_check_menu_item_block(newitem);
        }
    }

    // Key
    newitem = xset_design_additem(design_menu, "_Key Shortcut", XSetJob::KEY, set);
    gtk_widget_set_sensitive(newitem, (set->menu_style < XSetMenu::SUBMENU));
    if (show_keys)
    {
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_k,
                                   GdkModifierType::GDK_CONTROL_MASK,
                                   GTK_ACCEL_VISIBLE);
    }

    // Edit (script)
    if (!set->lock && set->menu_style < XSetMenu::SUBMENU && set->tool <= XSetTool::CUSTOM)
    {
        if (XSetCMD(xset_get_int(set, XSetVar::X)) == XSetCMD::SCRIPT)
        {
            char* script = xset_custom_get_script(set, false);
            if (script)
            {
                if (geteuid() != 0 && have_rw_access(script))
                {
                    // edit as user
                    newitem = xset_design_additem(design_menu, "_Edit Script", XSetJob::EDIT, set);
                    if (show_keys)
                    {
                        gtk_widget_add_accelerator(newitem,
                                                   "activate",
                                                   accel_group,
                                                   GDK_KEY_F4,
                                                   (GdkModifierType)0,
                                                   GTK_ACCEL_VISIBLE);
                    }
                }
                else
                {
                    // edit as root
                    newitem =
                        xset_design_additem(design_menu, "E_dit As Root", XSetJob::EDIT_ROOT, set);
                    if (geteuid() == 0 && show_keys)
                    {
                        gtk_widget_add_accelerator(newitem,
                                                   "activate",
                                                   accel_group,
                                                   GDK_KEY_F4,
                                                   (GdkModifierType)0,
                                                   GTK_ACCEL_VISIBLE);
                    }
                }
                std::free(script);
            }
        }
        else if (XSetCMD(xset_get_int(set, XSetVar::X)) == XSetCMD::LINE)
        {
            // edit command line
            newitem = xset_design_additem(design_menu, "_Edit Command", XSetJob::PROP_CMD, set);
            if (show_keys)
            {
                gtk_widget_add_accelerator(newitem,
                                           "activate",
                                           accel_group,
                                           GDK_KEY_F4,
                                           (GdkModifierType)0,
                                           GTK_ACCEL_VISIBLE);
            }
        }
    }

    // Properties
    newitem = xset_design_additem(design_menu, "_Properties", XSetJob::PROP, set);
    if (show_keys)
    {
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_F3,
                                   (GdkModifierType)0,
                                   GTK_ACCEL_VISIBLE);
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
    g_signal_connect(design_menu, "key_press_event", G_CALLBACK(xset_design_menu_keypress), set);

    gtk_menu_shell_set_take_focus(GTK_MENU_SHELL(design_menu), true);
    // this is required when showing the menu via F2 or Menu key for focus
    gtk_menu_shell_select_first(GTK_MENU_SHELL(design_menu), true);

    return design_menu;
}

static bool
xset_design_cb(GtkWidget* item, GdkEventButton* event, xset_t set)
{
    XSetJob job = XSetJob::INVALID;

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
                    else if (event->button == 1 && set->tool != XSetTool::NOT && !set->lock)
                    {
                        // activate
                        if (set->tool == XSetTool::CUSTOM)
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
                    job = XSetJob::COPY;
                    break;
                case GdkModifierType::GDK_MOD1_MASK:
                    // alt
                    job = XSetJob::CUT;
                    break;
                case GdkModifierType::GDK_SHIFT_MASK:
                    // shift
                    job = XSetJob::PASTE;
                    break;
                case (GdkModifierType::GDK_CONTROL_MASK | GdkModifierType::GDK_SHIFT_MASK):
                    // ctrl + shift
                    job = XSetJob::COMMAND;
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
                    else
                    {
                        if (XSetCMD(xset_get_int(set, XSetVar::X)) == XSetCMD::SCRIPT)
                        {
                            job = XSetJob::EDIT;
                        }
                        else
                        {
                            job = XSetJob::PROP_CMD;
                        }
                    }
                    break;
                case GdkModifierType::GDK_CONTROL_MASK:
                    // ctrl
                    job = XSetJob::KEY;
                    break;
                case GdkModifierType::GDK_MOD1_MASK:
                    // alt
                    job = XSetJob::HELP;
                    break;
                case GdkModifierType::GDK_SHIFT_MASK:
                    // shift
                    job = XSetJob::ICON;
                    break;
                case (GdkModifierType::GDK_CONTROL_MASK | GdkModifierType::GDK_SHIFT_MASK):
                    // ctrl + shift
                    job = XSetJob::REMOVE;
                    break;
                case (GdkModifierType::GDK_CONTROL_MASK | GdkModifierType::GDK_MOD1_MASK):
                    // ctrl + alt
                    job = XSetJob::PROP;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    if (job != XSetJob::INVALID)
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

bool
xset_menu_keypress(GtkWidget* widget, GdkEventKey* event, void* user_data)
{
    (void)user_data;
    XSetJob job = XSetJob::INVALID;
    xset_t set;

    GtkWidget* item = gtk_menu_shell_get_selected_item(GTK_MENU_SHELL(widget));
    if (item)
    {
        set = XSET(g_object_get_data(G_OBJECT(item), "set"));
        if (!set)
        {
            return false;
        }
    }
    else
    {
        return false;
    }

    const u32 keymod = ptk_get_keymod(event->state);

#if defined(HAVE_NONLATIN_KEYBOARD_SUPPORT)
    transpose_nonlatin_keypress(event);
#endif

    switch (keymod)
    {
        case 0:
            switch (event->keyval)
            {
                case GDK_KEY_F2:
                case GDK_KEY_Menu:
                    xset_design_show_menu(widget, set, nullptr, 0, event->time);
                    return true;
                case GDK_KEY_F3:
                    job = XSetJob::PROP;
                    break;
                case GDK_KEY_F4:
                    if (XSetCMD(xset_get_int(set, XSetVar::X)) == XSetCMD::SCRIPT)
                    {
                        job = XSetJob::EDIT;
                    }
                    else
                    {
                        job = XSetJob::PROP_CMD;
                    }
                    break;
                case GDK_KEY_Delete:
                    job = XSetJob::REMOVE;
                    break;
                case GDK_KEY_Insert:
                    job = XSetJob::COMMAND;
                    break;
                default:
                    break;
            }
            break;
        case GdkModifierType::GDK_CONTROL_MASK:
            switch (event->keyval)
            {
                case GDK_KEY_c:
                    job = XSetJob::COPY;
                    break;
                case GDK_KEY_x:
                    job = XSetJob::CUT;
                    break;
                case GDK_KEY_v:
                    job = XSetJob::PASTE;
                    break;
                case GDK_KEY_e:
                    if (set->lock)
                    {
                        xset_design_show_menu(widget, set, nullptr, 0, event->time);
                        return true;
                    }
                    else
                    {
                        if (XSetCMD(xset_get_int(set, XSetVar::X)) == XSetCMD::SCRIPT)
                        {
                            job = XSetJob::EDIT;
                        }
                        else
                        {
                            job = XSetJob::PROP_CMD;
                        }
                    }
                    break;
                case GDK_KEY_k:
                    job = XSetJob::KEY;
                    break;
                case GDK_KEY_i:
                    job = XSetJob::ICON;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }

    if (job != XSetJob::INVALID)
    {
        if (xset_job_is_valid(set, job))
        {
            gtk_menu_shell_deactivate(GTK_MENU_SHELL(widget));
            g_object_set_data(G_OBJECT(item), "job", GINT_TO_POINTER(job));
            xset_design_job(item, set);
        }
        else
        {
            xset_design_show_menu(widget, set, nullptr, 0, event->time);
        }
        return true;
    }
    return false;
}

void
xset_menu_cb(GtkWidget* item, xset_t set)
{
    GtkWidget* parent;
    GFunc cb_func = nullptr;
    void* cb_data = nullptr;
    std::string title;
    xset_t mset; // mirror set or set
    xset_t rset; // real set

    if (item)
    {
        if (set->lock && set->menu_style == XSetMenu::RADIO && GTK_IS_CHECK_MENU_ITEM(item) &&
            !gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)))
        {
            return;
        }

        cb_func = set->cb_func;
        cb_data = set->cb_data;
    }

    parent = GTK_WIDGET(set->browser);

    if (set->plugin)
    {
        // set is plugin
        mset = xset_get_plugin_mirror(set);
        rset = set;
    }
    else if (!set->lock && set->desc && ztd::same(set->desc, "@plugin@mirror@") && set->shared_key)
    {
        // set is plugin mirror
        mset = set;
        rset = xset_get(set->shared_key);
        rset->browser = set->browser;
    }
    else
    {
        mset = set;
        rset = set;
    }

    bool response2;

    switch (rset->menu_style)
    {
        case XSetMenu::NORMAL:
            if (cb_func)
            {
                cb_func(item, cb_data);
            }
            else if (!rset->lock)
            {
                xset_custom_activate(item, rset);
            }
            break;
        case XSetMenu::SEP:
            break;
        case XSetMenu::CHECK:
            if (mset->b == XSetB::XSET_B_TRUE)
            {
                mset->b = XSetB::XSET_B_FALSE;
            }
            else
            {
                mset->b = XSetB::XSET_B_TRUE;
            }
            if (cb_func)
            {
                cb_func(item, cb_data);
            }
            else if (!rset->lock)
            {
                xset_custom_activate(item, rset);
            }
            if (set->tool == XSetTool::CUSTOM)
            {
                ptk_file_browser_update_toolbar_widgets(set->browser, set, XSetTool::INVALID);
            }
            break;
        case XSetMenu::STRING:
        case XSetMenu::CONFIRM:
        {
            std::string msg = rset->desc;
            char* default_str = nullptr;
            if (rset->title && rset->lock)
            {
                title = ztd::strdup(rset->title);
            }
            else
            {
                title = clean_label(rset->menu_label, false, false);
            }
            if (rset->lock)
            {
                default_str = rset->z;
            }
            else
            {
                msg = ztd::replace(msg, "\\n", "\n");
                msg = ztd::replace(msg, "\\t", "\t");
            }
            if (rset->menu_style == XSetMenu::CONFIRM)
            {
                const i32 response = xset_msg_dialog(parent,
                                                     GtkMessageType::GTK_MESSAGE_QUESTION,
                                                     title,
                                                     GtkButtonsType::GTK_BUTTONS_OK_CANCEL,
                                                     msg);

                if (response == GtkResponseType::GTK_RESPONSE_OK)
                {
                    if (cb_func)
                    {
                        cb_func(item, cb_data);
                    }
                    else if (!set->lock)
                    {
                        xset_custom_activate(item, rset);
                    }
                }
            }
            else
            {
                response2 =
                    xset_text_dialog(parent, title, msg, "", mset->s, &mset->s, default_str, false);
                if (response2)
                {
                    if (cb_func)
                    {
                        cb_func(item, cb_data);
                    }
                    else if (!set->lock)
                    {
                        xset_custom_activate(item, rset);
                    }
                }
            }
        }
        break;
        case XSetMenu::RADIO:
            if (mset->b != XSetB::XSET_B_TRUE)
            {
                mset->b = XSetB::XSET_B_TRUE;
            }
            if (cb_func)
            {
                cb_func(item, cb_data);
            }
            else if (!rset->lock)
            {
                xset_custom_activate(item, rset);
            }
            break;
        case XSetMenu::FONTDLG:
            break;
        case XSetMenu::FILEDLG:
            // test purpose only
            {
                char* file;
                file = xset_file_dialog(parent,
                                        GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SAVE,
                                        rset->title,
                                        rset->s,
                                        "foobar.xyz");
                // ztd::logger::info("file={}", file);
                std::free(file);
            }
            break;
        case XSetMenu::ICON:
            // Note: xset_text_dialog uses the title passed to know this is an
            // icon chooser, so it adds a Choose button.  If you change the title,
            // change xset_text_dialog.
            response2 = xset_text_dialog(parent,
                                         rset->title ? rset->title : "Set Icon",
                                         rset->desc ? rset->desc : icon_desc,
                                         "",
                                         rset->icon,
                                         &rset->icon,
                                         "",
                                         false);
            if (response2)
            {
                if (rset->lock)
                {
                    rset->keep_terminal = true; // trigger save of changed icon
                }
                if (cb_func)
                {
                    cb_func(item, cb_data);
                }
            }
            break;
        case XSetMenu::COLORDLG: // deprecated
        case XSetMenu::RESERVED_03:
        case XSetMenu::RESERVED_04:
        case XSetMenu::RESERVED_05:
        case XSetMenu::RESERVED_06:
        case XSetMenu::RESERVED_07:
        case XSetMenu::RESERVED_08:
        case XSetMenu::RESERVED_09:
        case XSetMenu::RESERVED_10:
        case XSetMenu::SUBMENU:
        default:
            if (cb_func)
            {
                cb_func(item, cb_data);
            }
            else if (!set->lock)
            {
                xset_custom_activate(item, rset);
            }
            break;
    }

    if (rset->menu_style != XSetMenu::NORMAL)
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
xset_builtin_tool_activate(XSetTool tool_type, xset_t set, GdkEventButton* event)
{
    xset_t set2;
    panel_t p;
    MainWindowPanel mode;
    PtkFileBrowser* file_browser = nullptr;
    MainWindow* main_window = main_window_get_last_active();

    // set may be a submenu that does not match tool_type
    if (!(set && !set->lock && tool_type > XSetTool::CUSTOM))
    {
        ztd::logger::warn("xset_builtin_tool_activate invalid");
        return;
    }
    // ztd::logger::info("xset_builtin_tool_activate  {}", set->menu_label);

    // get current browser, panel, and mode
    if (main_window)
    {
        file_browser =
            PTK_FILE_BROWSER_REINTERPRET(main_window_get_current_file_browser(main_window));
        p = file_browser->mypanel;
        mode = main_window->panel_context.at(p);
    }
    if (!PTK_IS_FILE_BROWSER(file_browser))
    {
        return;
    }

    switch (tool_type)
    {
        case XSetTool::DEVICES:
            set2 = xset_get_panel_mode(p, XSetPanel::SHOW_DEVMON, mode);
            set2->b = set2->b == XSetB::XSET_B_TRUE ? XSetB::XSET_B_UNSET : XSetB::XSET_B_TRUE;
            update_views_all_windows(nullptr, file_browser);
            break;
        case XSetTool::BOOKMARKS:
            update_views_all_windows(nullptr, file_browser);
            break;
        case XSetTool::TREE:
            set2 = xset_get_panel_mode(p, XSetPanel::SHOW_DIRTREE, mode);
            set2->b = set2->b == XSetB::XSET_B_TRUE ? XSetB::XSET_B_UNSET : XSetB::XSET_B_TRUE;
            update_views_all_windows(nullptr, file_browser);
            break;
        case XSetTool::HOME:
            ptk_file_browser_go_home(nullptr, file_browser);
            break;
        case XSetTool::DEFAULT:
            ptk_file_browser_go_default(nullptr, file_browser);
            break;
        case XSetTool::UP:
            ptk_file_browser_go_up(nullptr, file_browser);
            break;
        case XSetTool::BACK:
            ptk_file_browser_go_back(nullptr, file_browser);
            break;
        case XSetTool::BACK_MENU:
            ptk_file_browser_show_history_menu(file_browser, true, event);
            break;
        case XSetTool::FWD:
            ptk_file_browser_go_forward(nullptr, file_browser);
            break;
        case XSetTool::FWD_MENU:
            ptk_file_browser_show_history_menu(file_browser, false, event);
            break;
        case XSetTool::REFRESH:
            ptk_file_browser_refresh(nullptr, file_browser);
            break;
        case XSetTool::NEW_TAB:
            ptk_file_browser_new_tab(nullptr, file_browser);
            break;
        case XSetTool::NEW_TAB_HERE:
            ptk_file_browser_new_tab_here(nullptr, file_browser);
            break;
        case XSetTool::SHOW_HIDDEN:
            set2 = xset_get_panel(p, XSetPanel::SHOW_HIDDEN);
            set2->b = set2->b == XSetB::XSET_B_TRUE ? XSetB::XSET_B_UNSET : XSetB::XSET_B_TRUE;
            ptk_file_browser_show_hidden_files(file_browser, set2->b);
            break;
        case XSetTool::SHOW_THUMB:
            main_window_toggle_thumbnails_all_windows();
            break;
        case XSetTool::LARGE_ICONS:
            if (file_browser->view_mode != PtkFBViewMode::PTK_FB_ICON_VIEW)
            {
                xset_set_b_panel(p, XSetPanel::LIST_LARGE, !file_browser->large_icons);
                on_popup_list_large(nullptr, file_browser);
            }
            break;
        case XSetTool::NOT:
        case XSetTool::CUSTOM:
        case XSetTool::INVALID:
        default:
            ztd::logger::warn("xset_builtin_tool_activate invalid tool_type");
    }
}

const char*
xset_get_builtin_toolitem_label(XSetTool tool_type)
{
    if (tool_type < XSetTool::DEVICES || tool_type >= XSetTool::INVALID)
    {
        return nullptr;
    }
    return builtin_tool_name[INT(tool_type)];
}

xset_t
xset_new_builtin_toolitem(XSetTool tool_type)
{
    if (tool_type < XSetTool::DEVICES || tool_type >= XSetTool::INVALID)
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
    XSetJob job = XSetJob::INVALID;

    // ztd::logger::info("on_tool_icon_button_press  {}   button = {}", set->menu_label,
    // event->button);
    if (event->type != GdkEventType::GDK_BUTTON_PRESS)
    {
        return false;
    }
    const u32 keymod = ptk_get_keymod(event->state);

    // get and focus browser
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER(g_object_get_data(G_OBJECT(widget), "browser"));
    if (!PTK_IS_FILE_BROWSER(file_browser))
    {
        return true;
    }
    ptk_file_browser_focus_me(file_browser);
    set->browser = file_browser;

    // get context
    const xset_context_t context = xset_context_new();
    main_context_fill(file_browser, context);
    if (!context->valid)
    {
        return true;
    }

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
                        if (set->tool == XSetTool::CUSTOM && set->menu_style == XSetMenu::SUBMENU)
                        {
                            if (set->child)
                            {
                                xset_t set_child = xset_is(set->child);
                                // activate first item in custom submenu
                                xset_menu_cb(nullptr, set_child);
                            }
                        }
                        else if (set->tool == XSetTool::CUSTOM)
                        {
                            // activate
                            xset_menu_cb(nullptr, set);
                        }
                        else if (set->tool == XSetTool::BACK_MENU)
                        {
                            xset_builtin_tool_activate(XSetTool::BACK, set, event);
                        }
                        else if (set->tool == XSetTool::FWD_MENU)
                        {
                            xset_builtin_tool_activate(XSetTool::FWD, set, event);
                        }
                        else if (set->tool != XSetTool::NOT)
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
                    job = XSetJob::COPY;
                    break;
                case GdkModifierType::GDK_MOD1_MASK:
                    // alt
                    job = XSetJob::CUT;
                    break;
                case GdkModifierType::GDK_SHIFT_MASK:
                    // shift
                    job = XSetJob::PASTE;
                    break;
                case (GdkModifierType::GDK_CONTROL_MASK | GdkModifierType::GDK_SHIFT_MASK):
                    // ctrl + shift
                    job = XSetJob::COMMAND;
                    break;
                default:
                    break;
            }
            break;
        case 2:
            // middle click
            switch (keymod)
            {
                case 0:
                    // no modifier
                    if (set->tool == XSetTool::CUSTOM &&
                        XSetCMD(xset_get_int(set, XSetVar::X)) == XSetCMD::SCRIPT)
                    {
                        job = XSetJob::EDIT;
                    }
                    else
                    {
                        job = XSetJob::PROP_CMD;
                    }
                    break;
                case GdkModifierType::GDK_CONTROL_MASK:
                    // ctrl
                    job = XSetJob::KEY;
                    break;
                case GdkModifierType::GDK_MOD1_MASK:
                    // alt
                    break;
                case GdkModifierType::GDK_SHIFT_MASK:
                    // shift
                    job = XSetJob::ICON;
                    break;
                case (GdkModifierType::GDK_CONTROL_MASK | GdkModifierType::GDK_SHIFT_MASK):
                    // ctrl + shift
                    job = XSetJob::REMOVE;
                    break;
                case (GdkModifierType::GDK_CONTROL_MASK | GdkModifierType::GDK_MOD1_MASK):
                    // ctrl + alt
                    job = XSetJob::PROP;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }

    if (job != XSetJob::INVALID)
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
    if (!PTK_IS_FILE_BROWSER(file_browser))
    {
        return true;
    }
    ptk_file_browser_focus_me(file_browser);

    // get context
    const xset_context_t context = xset_context_new();
    main_context_fill(file_browser, context);
    if (!context->valid)
    {
        return true;
    }

    if (event->button == 1)
    {
        if (set->tool == XSetTool::CUSTOM)
        {
            // show custom submenu
            xset_t set_child;
            if (!(set && !set->lock && set->child && set->menu_style == XSetMenu::SUBMENU &&
                  (set_child = xset_is(set->child))))
            {
                return true;
            }
            GtkWidget* menu = gtk_menu_new();
            GtkAccelGroup* accel_group = gtk_accel_group_new();
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

static void
set_gtk3_widget_padding(GtkWidget* widget, i32 left_right, i32 top_bottom)
{
    const std::string str = fmt::format("GtkWidget {{ padding-left: {}px; padding-right: {}px; "
                                        "padding-top: {}px; padding-bottom: {}px; }}",
                                        left_right,
                                        left_right,
                                        top_bottom,
                                        top_bottom);

    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(provider), str.data(), -1, nullptr);
    GtkStyleContext* context = gtk_widget_get_style_context(widget);
    gtk_style_context_add_provider(context,
                                   GTK_STYLE_PROVIDER(provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static GtkWidget*
xset_add_toolitem(GtkWidget* parent, PtkFileBrowser* file_browser, GtkWidget* toolbar,
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

    if (set->tool == XSetTool::NOT)
    {
        ztd::logger::warn("xset_add_toolitem set->tool == XSetTool::NOT");
        set->tool = XSetTool::CUSTOM;
    }

    GtkWidget* image = nullptr;
    GtkWidget* item = nullptr;
    GtkWidget* btn;
    xset_t set_next;
    char* new_menu_label = nullptr;
    GdkPixbuf* pixbuf = nullptr;
    std::string str;

    // get real icon size from gtk icon size
    i32 icon_w, icon_h;
    gtk_icon_size_lookup((GtkIconSize)icon_size, &icon_w, &icon_h);
    const i32 real_icon_size = icon_w > icon_h ? icon_w : icon_h;

    set->browser = file_browser;

    // builtin toolitems set shared_key on build
    if (set->tool >= XSetTool::INVALID)
    {
        // looks like an unknown built-in toolitem from a future version - skip
        if (set->next)
        {
            set_next = xset_is(set->next);
            // ztd::logger::info("    NEXT {}", set_next->name);
            xset_add_toolitem(parent, file_browser, toolbar, icon_size, set_next, show_tooltips);
        }
        return item;
    }
    if (set->tool > XSetTool::CUSTOM && set->tool < XSetTool::INVALID && !set->shared_key)
    {
        set->shared_key = ztd::strdup(builtin_tool_shared_key[INT(set->tool)]);
    }

    // builtin toolitems do not have menu_style set
    XSetMenu menu_style;
    switch (set->tool)
    {
        case XSetTool::DEVICES:
        case XSetTool::BOOKMARKS:
        case XSetTool::TREE:
        case XSetTool::SHOW_HIDDEN:
        case XSetTool::SHOW_THUMB:
        case XSetTool::LARGE_ICONS:
            menu_style = XSetMenu::CHECK;
            break;
        case XSetTool::BACK_MENU:
        case XSetTool::FWD_MENU:
            menu_style = XSetMenu::SUBMENU;
            break;
        case XSetTool::NOT:
        case XSetTool::CUSTOM:
        case XSetTool::HOME:
        case XSetTool::DEFAULT:
        case XSetTool::UP:
        case XSetTool::BACK:
        case XSetTool::FWD:
        case XSetTool::REFRESH:
        case XSetTool::NEW_TAB:
        case XSetTool::NEW_TAB_HERE:
        case XSetTool::INVALID:
        default:
            menu_style = set->menu_style;
    }

    const char* icon_name;
    icon_name = set->icon;
    if (!icon_name && set->tool == XSetTool::CUSTOM)
    {
        // custom 'icon' file?
        const std::string icon_file = Glib::build_filename(vfs::user_dirs->program_config_dir(),
                                                           "scripts",
                                                           set->name,
                                                           "icon");
        if (std::filesystem::exists(icon_file))
        {
            icon_name = ztd::strdup(icon_file);
        }
    }

    char* menu_label;
    menu_label = set->menu_label;
    if (!menu_label && set->tool > XSetTool::CUSTOM)
    {
        menu_label = (char*)xset_get_builtin_toolitem_label(set->tool);
    }

    if (menu_style == XSetMenu::NORMAL)
    {
        menu_style = XSetMenu::STRING;
    }

    GtkWidget* ebox;
    GtkWidget* hbox;
    xset_t set_child;
    XSetCMD cmd_type;

    switch (menu_style)
    {
        case XSetMenu::STRING:
            // normal item
            cmd_type = XSetCMD(xset_get_int(set, XSetVar::X));
            if (set->tool > XSetTool::CUSTOM)
            {
                // builtin tool item
                if (icon_name)
                {
                    image = xset_get_image(icon_name, (GtkIconSize)icon_size);
                }
                else if (set->tool > XSetTool::CUSTOM && set->tool < XSetTool::INVALID)
                {
                    image =
                        xset_get_image(builtin_tool_icon[INT(set->tool)], (GtkIconSize)icon_size);
                }
            }
            else if (!set->lock && cmd_type == XSetCMD::APP)
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
                image =
                    xset_get_image(icon_name ? icon_name : "gtk-execute", (GtkIconSize)icon_size);
            }
            if (!new_menu_label)
            {
                new_menu_label = ztd::strdup(menu_label);
            }

            // cannot use gtk_tool_button_new because icon does not obey size
            // btn = GTK_WIDGET( gtk_tool_button_new( image, new_menu_label ) );
            btn = GTK_WIDGET(gtk_button_new());
            gtk_widget_show(image);
            gtk_button_set_image(GTK_BUTTON(btn), image);
            gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
            // These do not seem to do anything
            gtk_widget_set_margin_start(btn, 0);
            gtk_widget_set_margin_end(btn, 0);
            gtk_widget_set_margin_top(btn, 0);
            gtk_widget_set_margin_bottom(btn, 0);
            gtk_widget_set_hexpand(btn, false);
            gtk_widget_set_vexpand(btn, false);
            set_gtk3_widget_padding(btn, 0, 0);
            gtk_button_set_always_show_image(GTK_BUTTON(btn), true);
            gtk_widget_set_margin_start(btn, 0);
            gtk_widget_set_margin_end(btn, 0);

            // create tool item containing an ebox to capture click on button
            item = GTK_WIDGET(gtk_tool_item_new());
            ebox = gtk_event_box_new();
            gtk_container_add(GTK_CONTAINER(item), ebox);
            gtk_container_add(GTK_CONTAINER(ebox), btn);
            gtk_event_box_set_visible_window(GTK_EVENT_BOX(ebox), false);
            gtk_event_box_set_above_child(GTK_EVENT_BOX(ebox), true);
            g_signal_connect(ebox,
                             "button-press-event",
                             G_CALLBACK(on_tool_icon_button_press),
                             set);
            g_object_set_data(G_OBJECT(ebox), "browser", file_browser);
            ptk_file_browser_add_toolbar_widget(set, btn);

            // tooltip
            if (show_tooltips)
            {
                str = clean_label(new_menu_label, false, false);
                gtk_widget_set_tooltip_text(ebox, str.data());
            }
            std::free(new_menu_label);
            break;
        case XSetMenu::CHECK:
            if (!icon_name && set->tool > XSetTool::CUSTOM && set->tool < XSetTool::INVALID)
            {
                // builtin tool item
                image = xset_get_image(builtin_tool_icon[INT(set->tool)], (GtkIconSize)icon_size);
            }
            else
            {
                image =
                    xset_get_image(icon_name ? icon_name : "gtk-execute", (GtkIconSize)icon_size);
            }

            // cannot use gtk_tool_button_new because icon does not obey size
            // btn = GTK_WIDGET( gtk_toggle_tool_button_new() );
            // gtk_tool_button_set_icon_widget( GTK_TOOL_BUTTON( btn ), image );
            // gtk_tool_button_set_label( GTK_TOOL_BUTTON( btn ), set->menu_label );
            btn = gtk_toggle_button_new();
            gtk_widget_show(image);
            gtk_button_set_image(GTK_BUTTON(btn), image);
            gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btn), xset_get_b(set));
            gtk_widget_set_margin_start(btn, 0);
            gtk_widget_set_margin_end(btn, 0);
            gtk_widget_set_margin_top(btn, 0);
            gtk_widget_set_margin_bottom(btn, 0);
            gtk_widget_set_hexpand(btn, false);
            gtk_widget_set_vexpand(btn, false);
            set_gtk3_widget_padding(btn, 0, 0);
            gtk_button_set_always_show_image(GTK_BUTTON(btn), true);
            gtk_widget_set_margin_start(btn, 0);
            gtk_widget_set_margin_end(btn, 0);

            // create tool item containing an ebox to capture click on button
            item = GTK_WIDGET(gtk_tool_item_new());
            ebox = gtk_event_box_new();
            gtk_container_add(GTK_CONTAINER(item), ebox);
            gtk_container_add(GTK_CONTAINER(ebox), btn);
            gtk_event_box_set_visible_window(GTK_EVENT_BOX(ebox), false);
            gtk_event_box_set_above_child(GTK_EVENT_BOX(ebox), true);
            g_signal_connect(ebox,
                             "button-press-event",
                             G_CALLBACK(on_tool_icon_button_press),
                             set);
            g_object_set_data(G_OBJECT(ebox), "browser", file_browser);
            ptk_file_browser_add_toolbar_widget(set, btn);

            // tooltip
            if (show_tooltips)
            {
                str = clean_label(menu_label, false, false);
                gtk_widget_set_tooltip_text(ebox, str.data());
            }
            break;
        case XSetMenu::SUBMENU:
            menu_label = nullptr;
            // create a tool button
            set_child = nullptr;
            if (set->child && set->tool == XSetTool::CUSTOM)
            {
                set_child = xset_is(set->child);
            }

            if (!icon_name && set_child && set_child->icon)
            {
                // take the user icon from the first item in the submenu
                icon_name = set_child->icon;
            }
            else if (!icon_name && set->tool > XSetTool::CUSTOM && set->tool < XSetTool::INVALID)
            {
                icon_name = builtin_tool_icon[INT(set->tool)];
            }
            else if (!icon_name && set_child && set->tool == XSetTool::CUSTOM)
            {
                // take the auto icon from the first item in the submenu
                cmd_type = XSetCMD(xset_get_int(set_child, XSetVar::X));
                switch (cmd_type)
                {
                    case XSetCMD::APP:
                        // Application
                        new_menu_label = menu_label =
                            xset_custom_get_app_name_icon(set_child, &pixbuf, real_icon_size);
                        break;
                    case XSetCMD::BOOKMARK:
                    case XSetCMD::SCRIPT:
                    case XSetCMD::LINE:
                    case XSetCMD::INVALID:
                    default:
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
                    case XSetTool::BACK_MENU:
                        menu_label = (char*)builtin_tool_name[INT(XSetTool::BACK)];
                        break;
                    case XSetTool::FWD_MENU:
                        menu_label = (char*)builtin_tool_name[INT(XSetTool::FWD)];
                        break;
                    case XSetTool::CUSTOM:
                        if (set_child)
                        {
                            menu_label = set_child->menu_label;
                        }
                        break;
                    case XSetTool::NOT:
                    case XSetTool::DEVICES:
                    case XSetTool::BOOKMARKS:
                    case XSetTool::TREE:
                    case XSetTool::HOME:
                    case XSetTool::DEFAULT:
                    case XSetTool::UP:
                    case XSetTool::BACK:
                    case XSetTool::FWD:
                    case XSetTool::REFRESH:
                    case XSetTool::NEW_TAB:
                    case XSetTool::NEW_TAB_HERE:
                    case XSetTool::SHOW_HIDDEN:
                    case XSetTool::SHOW_THUMB:
                    case XSetTool::LARGE_ICONS:
                    case XSetTool::INVALID:
                    default:
                        if (!set->menu_label)
                        {
                            menu_label = (char*)xset_get_builtin_toolitem_label(set->tool);
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
                image =
                    xset_get_image(icon_name ? icon_name : "gtk-directory", (GtkIconSize)icon_size);
            }

            // cannot use gtk_tool_button_new because icon does not obey size
            // btn = GTK_WIDGET( gtk_tool_button_new( image, menu_label ) );
            btn = GTK_WIDGET(gtk_button_new());
            gtk_widget_show(image);
            gtk_button_set_image(GTK_BUTTON(btn), image);
            gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
            gtk_widget_set_margin_start(btn, 0);
            gtk_widget_set_margin_end(btn, 0);
            gtk_widget_set_margin_top(btn, 0);
            gtk_widget_set_margin_bottom(btn, 0);
            gtk_widget_set_hexpand(btn, false);
            gtk_widget_set_vexpand(btn, false);
            set_gtk3_widget_padding(btn, 0, 0);
            gtk_button_set_always_show_image(GTK_BUTTON(btn), true);
            gtk_widget_set_margin_start(btn, 0);
            gtk_widget_set_margin_end(btn, 0);

            // create eventbox for btn
            ebox = gtk_event_box_new();
            gtk_event_box_set_visible_window(GTK_EVENT_BOX(ebox), false);
            gtk_event_box_set_above_child(GTK_EVENT_BOX(ebox), true);
            gtk_container_add(GTK_CONTAINER(ebox), btn);
            g_signal_connect(G_OBJECT(ebox),
                             "button_press_event",
                             G_CALLBACK(on_tool_icon_button_press),
                             set);
            g_object_set_data(G_OBJECT(ebox), "browser", file_browser);
            ptk_file_browser_add_toolbar_widget(set, btn);

            // pack into hbox
            hbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0);
            gtk_box_pack_start(GTK_BOX(hbox), ebox, false, false, 0);
            // tooltip
            if (show_tooltips)
            {
                str = clean_label(menu_label, false, false);
                gtk_widget_set_tooltip_text(ebox, str.data());
            }
            std::free(new_menu_label);

            // reset menu_label for below
            menu_label = set->menu_label;
            if (!menu_label && set->tool > XSetTool::CUSTOM)
            {
                menu_label = (char*)xset_get_builtin_toolitem_label(set->tool);
            }

            ///////// create a menu_tool_button to steal the button from
            ebox = gtk_event_box_new();
            gtk_event_box_set_visible_window(GTK_EVENT_BOX(ebox), false);
            gtk_event_box_set_above_child(GTK_EVENT_BOX(ebox), true);
            GtkWidget* menu_btn;
            GtkWidget* hbox_menu;
            GList* children;

            menu_btn = GTK_WIDGET(gtk_menu_tool_button_new(nullptr, nullptr));
            hbox_menu = gtk_bin_get_child(GTK_BIN(menu_btn));
            children = gtk_container_get_children(GTK_CONTAINER(hbox_menu));
            btn = GTK_WIDGET(children->next->data);
            if (!btn || !GTK_IS_WIDGET(btn))
            {
                // failed so just create a button
                btn = GTK_WIDGET(gtk_button_new());
                gtk_button_set_label(GTK_BUTTON(btn), ".");
                gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
                gtk_container_add(GTK_CONTAINER(ebox), btn);
            }
            else
            {
                // steal the drop-down button
                gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(btn)), btn);
                gtk_container_add(GTK_CONTAINER(ebox), btn);

                gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
            }
            gtk_widget_set_margin_start(btn, 0);
            gtk_widget_set_margin_end(btn, 0);
            gtk_widget_set_margin_top(btn, 0);
            gtk_widget_set_margin_bottom(btn, 0);
            gtk_widget_set_hexpand(btn, false);
            gtk_widget_set_vexpand(btn, false);
            set_gtk3_widget_padding(btn, 0, 0);
            gtk_button_set_always_show_image(GTK_BUTTON(btn), true);
            gtk_widget_set_margin_start(btn, 0);
            gtk_widget_set_margin_end(btn, 0);

            g_list_free(children);
            gtk_widget_destroy(menu_btn);

            gtk_box_pack_start(GTK_BOX(hbox), ebox, false, false, 0);
            g_signal_connect(G_OBJECT(ebox),
                             "button_press_event",
                             G_CALLBACK(on_tool_menu_button_press),
                             set);
            g_object_set_data(G_OBJECT(ebox), "browser", file_browser);
            ptk_file_browser_add_toolbar_widget(set, btn);

            item = GTK_WIDGET(gtk_tool_item_new());
            gtk_container_add(GTK_CONTAINER(item), hbox);
            gtk_widget_show_all(item);

            // tooltip
            if (show_tooltips)
            {
                str = clean_label(menu_label, false, false);
                gtk_widget_set_tooltip_text(ebox, str.data());
            }
            break;
        case XSetMenu::SEP:
            // create tool item containing an ebox to capture click on sep
            btn = GTK_WIDGET(gtk_separator_tool_item_new());
            gtk_separator_tool_item_set_draw(GTK_SEPARATOR_TOOL_ITEM(btn), true);
            item = GTK_WIDGET(gtk_tool_item_new());
            ebox = gtk_event_box_new();
            gtk_container_add(GTK_CONTAINER(item), ebox);
            gtk_container_add(GTK_CONTAINER(ebox), btn);
            gtk_event_box_set_visible_window(GTK_EVENT_BOX(ebox), false);
            gtk_event_box_set_above_child(GTK_EVENT_BOX(ebox), true);
            g_signal_connect(ebox,
                             "button-press-event",
                             G_CALLBACK(on_tool_icon_button_press),
                             set);
            g_object_set_data(G_OBJECT(ebox), "browser", file_browser);
            break;
        case XSetMenu::NORMAL:
        case XSetMenu::RADIO:
        case XSetMenu::FILEDLG:
        case XSetMenu::FONTDLG:
        case XSetMenu::ICON:
        case XSetMenu::COLORDLG: // deprecated
        case XSetMenu::CONFIRM:
        case XSetMenu::RESERVED_03:
        case XSetMenu::RESERVED_04:
        case XSetMenu::RESERVED_05:
        case XSetMenu::RESERVED_06:
        case XSetMenu::RESERVED_07:
        case XSetMenu::RESERVED_08:
        case XSetMenu::RESERVED_09:
        case XSetMenu::RESERVED_10:
        default:
            return nullptr;
    }

    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(item), -1);

    // ztd::logger::info("    set={}   set->next={}", set->name, set->next);
    // next toolitem
    if (set->next)
    {
        set_next = xset_is(set->next);
        // ztd::logger::info("    NEXT {}", set_next->name);
        xset_add_toolitem(parent, file_browser, toolbar, icon_size, set_next, show_tooltips);
    }

    return item;
}

void
xset_fill_toolbar(GtkWidget* parent, PtkFileBrowser* file_browser, GtkWidget* toolbar,
                  xset_t set_parent, bool show_tooltips)
{
    static constexpr std::array<XSetTool, 7> default_tools{
        XSetTool::BOOKMARKS,
        XSetTool::TREE,
        XSetTool::NEW_TAB_HERE,
        XSetTool::BACK_MENU,
        XSetTool::FWD_MENU,
        XSetTool::UP,
        XSetTool::DEFAULT,
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
    set_parent->menu_style = XSetMenu::SUBMENU;

    const GtkIconSize icon_size = gtk_toolbar_get_icon_size(GTK_TOOLBAR(toolbar));

    xset_t set_child = nullptr;
    if (set_parent->child)
    {
        set_child = xset_is(set_parent->child);
    }
    if (!set_child)
    {
        // toolbar is empty - add default items
        set_child = xset_new_builtin_toolitem(
            (set_parent->xset_name == XSetName::TOOL_R) ? XSetTool::REFRESH : XSetTool::DEVICES);
        set_parent->child = ztd::strdup(set_child->name);
        set_child->parent = ztd::strdup(set_parent->name);
        if (set_parent->xset_name != XSetName::TOOL_R)
        {
            if (set_parent->xset_name == XSetName::TOOL_S)
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
    gtk_widget_set_margin_start(toolbar, 0);
    gtk_widget_set_margin_end(toolbar, 0);
    gtk_widget_set_margin_top(toolbar, 0);
    gtk_widget_set_margin_bottom(toolbar, 0);

    // remove padding from GTK3 toolbar - this works
    set_gtk3_widget_padding(toolbar, 0, 2);
    gtk_widget_set_margin_start(toolbar, 0);
    gtk_widget_set_margin_end(toolbar, 0);

    gtk_widget_show_all(toolbar);
}

void
xset_set_window_icon(GtkWindow* win)
{
    const char* name;
    xset_t set = xset_get(XSetName::MAIN_ICON);
    if (set->icon)
    {
        name = set->icon;
    }
    else if (geteuid() == 0)
    {
        name = "spacefm-root";
    }
    else
    {
        name = "spacefm";
    }
    GtkIconTheme* icon_theme = gtk_icon_theme_get_default();
    if (!icon_theme)
    {
        return;
    }
    GError* error = nullptr;
    GdkPixbuf* icon = gtk_icon_theme_load_icon(icon_theme, name, 48, (GtkIconLookupFlags)0, &error);
    if (icon)
    {
        gtk_window_set_icon(GTK_WINDOW(win), icon);
        g_object_unref(icon);
    }
    else if (error)
    {
        // An error occured on loading the icon
        ztd::logger::error("Unable to load the window icon '{}' in - xset_set_window_icon - {}",
                           name,
                           error->message);
        g_error_free(error);
    }
}
