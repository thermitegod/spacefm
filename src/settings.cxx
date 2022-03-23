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
#include <filesystem>

#include <array>
#include <vector>

#include <iostream>
#include <fstream>

#include <unistd.h>

#include <sys/stat.h>

#include <fmt/format.h>

#include <glibmm.h>

#include <gtk/gtk.h>

#include <gdk/gdkkeysyms.h>
#include <fcntl.h>

#include <exo/exo.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "settings.hxx"
#include "main-window.hxx"
#include "item-prop.hxx"

#include "autosave.hxx"
#include "extern.hxx"
#include "utils.hxx"

#include "vfs/vfs-app-desktop.hxx"
#include "vfs/vfs-utils.hxx"
#include "vfs/vfs-user-dir.hxx"

#include "ptk/ptk-utils.hxx"
#include "ptk/ptk-app-chooser.hxx"
#include "ptk/ptk-handler.hxx"
#include "ptk/ptk-file-menu.hxx"
#include "ptk/ptk-location-view.hxx"

#define CONFIG_VERSION "101" // 3.0.0

AppSettings app_settings = AppSettings();
ConfigSettings config_settings = ConfigSettings();

// MOD settings
static void xset_write(std::string& buf);
static void xset_parse(std::string& line);
static void read_root_settings();
static void xset_defaults();

std::vector<XSet*> xsets;
static std::vector<XSet*> keysets;
static XSet* set_clipboard = nullptr;
static bool clipboard_is_cut;
static XSet* set_last;

std::string settings_config_dir;
std::string settings_user_tmp_dir;

static XSetContext* xset_context = nullptr;
static XSet* book_icon_set_cached = nullptr;

EventHandler event_handler;

std::vector<std::string> xset_cmd_history;

typedef void (*SettingsParseFunc)(std::string& line);

static void xset_free_all();
static void xset_default_keys();
static char* xset_color_dialog(GtkWidget* parent, char* title, char* defcolor);
static bool xset_design_cb(GtkWidget* item, GdkEventButton* event, XSet* set);
static void xset_builtin_tool_activate(XSetTool tool_type, XSet* set, GdkEventButton* event);
static XSet* xset_new_builtin_toolitem(XSetTool tool_type);
static void xset_custom_insert_after(XSet* target, XSet* set);
static XSet* xset_custom_copy(XSet* set, bool copy_next, bool delete_set);
static XSetSetSet xset_set_set_encode(const std::string& var);
static void xset_free(XSet* set);
static void xset_remove(XSet* set);

static const char* enter_command_line =
    "Enter program or bash command line:\n\nUse:\n\t%%F\tselected files  or  %%f first selected "
    "file\n\t%%N\tselected filenames  or  %%n first selected filename\n\t%%d\tcurrent "
    "directory\n\t%%v\tselected device (eg /dev/sda1)\n\t%%m\tdevice mount point (eg "
    "/media/dvd);  %%l device label\n\t%%b\tselected bookmark\n\t%%t\tselected task directory;  "
    "%%p task pid\n\t%%a\tmenu item value\n\t$fm_panel, $fm_tab, etc";

static const char* icon_desc =
    "Enter an icon name, icon file path, or stock item name:\n\nOr click Choose to select an "
    "icon.  Not all icons may work properly due to various issues.";

static const char* enter_menu_name_new =
    "Enter new item name:\n\nPrecede a character with an underscore (_) to underline that "
    "character as a shortcut key if desired.\n\nTIP: To change this item later, right-click on "
    "the item to open the Design Menu.";

// clang-format off
// must match XSetTool:: enum
static const std::array<const char*, 18> builtin_tool_name
{
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
    "Large Icons"
};

// must match XSetTool:: enum
static const std::array<const char*, 18> builtin_tool_icon
{
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
    "zoom-in"
};

// must match XSetTool:: enum
static const std::array<const char*, 18> builtin_tool_shared_key
{
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
    "panel1_list_large"
};
//clang-format on

static void
parse_general_settings(std::string& line)
{
    std::size_t sep = line.find("=");
    if (sep == std::string::npos)
        return;

    line = ztd::strip(line);

    if (line.at(0) == '#')
        return;

    std::string token = line.substr(0, sep);
    std::string value = line.substr(sep + 1, std::string::npos - 1);

    // remove any quotes
    value = ztd::replace(value, "\"", "");

    if (value.empty())
        return;

    if (ztd::same(token, "show_thumbnail"))
        app_settings.show_thumbnail = std::stoi(value);
    else if (ztd::same(token, "max_thumb_size"))
        app_settings.max_thumb_size = std::stoi(value) << 10;
    else if (ztd::same(token, "big_icon_size"))
        app_settings.big_icon_size = std::stoi(value);
    else if (ztd::same(token, "small_icon_size"))
        app_settings.small_icon_size = std::stoi(value);
    else if (ztd::same(token, "tool_icon_size"))
        app_settings.tool_icon_size = std::stoi(value);
    else if (ztd::same(token, "single_click"))
        app_settings.single_click = std::stoi(value);
    else if (ztd::same(token, "no_single_hover"))
        app_settings.no_single_hover = std::stoi(value);
    else if (ztd::same(token, "sort_order"))
        app_settings.sort_order = std::stoi(value);
    else if (ztd::same(token, "sort_type"))
        app_settings.sort_type = std::stoi(value);
    else if (ztd::same(token, "use_si_prefix"))
        app_settings.use_si_prefix = std::stoi(value);
    else if (ztd::same(token, "no_execute"))
        app_settings.no_execute = std::stoi(value);
    else if (ztd::same(token, "no_confirm"))
        app_settings.no_confirm = std::stoi(value);
    else if (ztd::same(token, "no_confirm_trash"))
        app_settings.no_confirm_trash = std::stoi(value);
}

static void
parse_window_state(std::string& line)
{
    std::size_t sep = line.find("=");
    if (sep == std::string::npos)
        return;

    line = ztd::strip(line);

    if (line.at(0) == '#')
        return;

    std::string token = line.substr(0, sep);
    std::string value = line.substr(sep + 1, std::string::npos - 1);

    // remove any quotes
    value = ztd::replace(value, "\"", "");

    if (value.empty())
        return;

    if (ztd::same(token, "width"))
        app_settings.width = std::stoi(value);
    else if (ztd::same(token, "height"))
        app_settings.height = std::stoi(value);
    else if (ztd::same(token, "maximized"))
        app_settings.maximized = std::stoi(value);
}

static void
parse_interface_settings(std::string& line)
{
    std::size_t sep = line.find("=");
    if (sep == std::string::npos)
        return;

    line = ztd::strip(line);

    if (line.at(0) == '#')
        return;

    std::string token = line.substr(0, sep);
    std::string value = line.substr(sep + 1, std::string::npos - 1);

    // remove any quotes
    value = ztd::replace(value, "\"", "");

    if (value.empty())
        return;

    if (ztd::same(token, "always_show_tabs"))
        app_settings.always_show_tabs = std::stoi(value);
    else if (ztd::same(token, "show_close_tab_buttons"))
        app_settings.show_close_tab_buttons = std::stoi(value);
}

static void
parse_conf(std::string& etc_path, std::string& line)
{
    std::size_t sep = line.find("=");
    if (sep == std::string::npos)
        return;

    line = ztd::strip(line);

    if (line.at(0) == '#')
        return;

    std::string token = line.substr(0, sep);
    std::string value = line.substr(sep + 1, std::string::npos - 1);

    // remove any quotes
    value = ztd::replace(value, "\"", "");

    if (value.empty())
        return;

    if (ztd::same(token, "terminal_su") || ztd::same(token, "graphical_su"))
    {
        if (value.at(0) != '/' || !std::filesystem::exists(value))
            LOG_WARN("{}: {} '{}' file not found", etc_path, token, value);
        else if (ztd::same(token, "terminal_su"))
            config_settings.terminal_su = value.c_str();
    }
    else if (ztd::same(token, "font_view_icon"))
        config_settings.font_view_icon = value.c_str();
    else if (ztd::same(token, "font_view_compact"))
        config_settings.font_view_compact = value.c_str();
    else if (ztd::same(token, "font_general"))
        config_settings.font_general = value.c_str();
}

void
load_conf()
{
    std::string default_font = "Monospace 9";

    /* Set default config values */
    config_settings.terminal_su = nullptr;
    config_settings.tmp_dir = ztd::strdup(vfs_user_cache_dir());
    config_settings.font_view_icon = default_font.c_str();
    config_settings.font_view_compact = default_font.c_str();
    config_settings.font_general = default_font.c_str();

    // load spacefm.conf
    std::string config_path =
        Glib::build_filename(vfs_user_config_dir(), PACKAGE_NAME, "spacefm.conf");
    if (!std::filesystem::exists(config_path))
        config_path = Glib::build_filename(SYSCONFDIR, PACKAGE_NAME, "spacefm.conf");

    std::string line;
    std::ifstream file(config_path);
    if (file.is_open())
    {
        while (std::getline(file, line))
        {
            parse_conf(config_path, line);
        }
    }
    file.close();
}

void
load_settings(const char* config_dir)
{
    app_settings.load_saved_tabs = true;

    if (config_dir)
        settings_config_dir = config_dir;
    else
        settings_config_dir = Glib::build_filename(vfs_user_config_dir(), "spacefm");

    // MOD extra settings
    xset_defaults();

    const std::string session = Glib::build_filename(settings_config_dir, "session");

    std::string command;

    if (!std::filesystem::exists(settings_config_dir))
    {
        // copy /etc/xdg/spacefm
        std::string xdg_path = Glib::build_filename(settings_config_dir, "xdg", PACKAGE_NAME);
        if (std::filesystem::is_directory(xdg_path))
        {
            command = fmt::format("cp -r {} '{}'", xdg_path, settings_config_dir);
            Glib::spawn_command_line_sync(command);

            std::filesystem::permissions(settings_config_dir, std::filesystem::perms::owner_all);
        }
    }

    if (!std::filesystem::exists(settings_config_dir))
    {
        std::filesystem::create_directories(settings_config_dir);
        std::filesystem::permissions(settings_config_dir, std::filesystem::perms::owner_all);
    }

    if (config_settings.git_backed_settings)
    {
        if (Glib::find_program_in_path("git").empty())
        {
            LOG_ERROR("git backed settings enabled but git is not installed");
            config_settings.git_backed_settings = false;
        }
    }

    if (config_settings.git_backed_settings)
    {
        std::string git_path = Glib::build_filename(settings_config_dir, ".git");
        if (!std::filesystem::exists(git_path))
        {
            command = fmt::format("{} -c \"cd {} && git init && "
                                  "git config commit.gpgsign false\"",
                                  BASHPATH,
                                  settings_config_dir);
            Glib::spawn_command_line_sync(command);
            LOG_INFO("Initialized git repo at: {}", git_path);
        }
        else if (std::filesystem::exists(session))
        {
            command = fmt::format("{} -c \"cd {} && git add session && "
                                  "git commit -m 'Session File' 1>/dev/null\"",
                                  BASHPATH,
                                  settings_config_dir);
            Glib::spawn_command_line_sync(command);
            LOG_INFO("Updated git copy of: {}", session);
        }
        else if (std::filesystem::exists(git_path))
        {
            command = fmt::format("{} -c \"cd {} && git checkout session\"",
                                  BASHPATH,
                                  settings_config_dir);
            Glib::spawn_command_line_sync(command);
            LOG_INFO("Checked out: {}", session);
        }
    }

    if (std::filesystem::is_regular_file(session))
    {
        std::string line;
        std::ifstream file(session);
        if (file.is_open())
        {
            SettingsParseFunc func = nullptr;

            while (std::getline(file, line))
            {
                if (line.empty())
                    continue;

                if (line.at(0) == '[')
                {
                    if (ztd::same(line, "[General]"))
                        func = &parse_general_settings;
                    else if (ztd::same(line, "[Window]"))
                        func = &parse_window_state;
                    else if (ztd::same(line, "[Interface]"))
                        func = &parse_interface_settings;
                    else if (ztd::same(line, "[MOD]"))
                        func = &xset_parse;
                    else
                        func = nullptr;
                    continue;
                }

                if (func)
                    (*func)(line);
            }
        }
        file.close();
    }

    // MOD turn off fullscreen
    xset_set_b("main_full", false);

    app_settings.date_format = xset_get_s("date_format");
    if (app_settings.date_format.empty())
    {
        app_settings.date_format = "%Y-%m-%d %H:%M";
        xset_set("date_format", XSetSetSet::S, app_settings.date_format.c_str());
    }

    // MOD su command discovery (sets default)
    get_valid_su();

    // MOD terminal discovery
    char* main_terminal = xset_get_s("main_terminal");
    if (!main_terminal || main_terminal[0] == '\0')
    {
        for (const std::string& terminal: terminal_programs)
        {
            std::string term = Glib::find_program_in_path(terminal);
            if (term.empty())
                continue;

            xset_set("main_terminal", XSetSetSet::S, terminal.c_str());
            xset_set_b("main_terminal", true); // discovery
            break;
        }
    }

    // MOD editor discovery
    char* app_name = xset_get_s("editor");
    if (!app_name || app_name[0] == '\0')
    {
        VFSMimeType* mime_type = vfs_mime_type_get_from_type("text/plain");
        if (mime_type)
        {
            app_name = vfs_mime_type_get_default_action(mime_type);
            vfs_mime_type_unref(mime_type);
            if (app_name)
            {
                VFSAppDesktop desktop(app_name);
                xset_set("editor", XSetSetSet::S, desktop.get_exec());
            }
        }
    }

    // add default handlers
    ptk_handler_add_defaults(PtkHandlerMode::HANDLER_MODE_ARC, false, false);
    ptk_handler_add_defaults(PtkHandlerMode::HANDLER_MODE_FS, false, false);
    ptk_handler_add_defaults(PtkHandlerMode::HANDLER_MODE_NET, false, false);
    ptk_handler_add_defaults(PtkHandlerMode::HANDLER_MODE_FILE, false, false);

    // get root protected settings;wq
    read_root_settings();

    // set default keys
    xset_default_keys();

    // cache event handlers
    event_handler.win_focus = xset_get("evt_win_focus");
    event_handler.win_move = xset_get("evt_win_move");
    event_handler.win_click = xset_get("evt_win_click");
    event_handler.win_key = xset_get("evt_win_key");
    event_handler.win_close = xset_get("evt_win_close");
    event_handler.pnl_show = xset_get("evt_pnl_show");
    event_handler.pnl_focus = xset_get("evt_pnl_focus");
    event_handler.pnl_sel = xset_get("evt_pnl_sel");
    event_handler.tab_new = xset_get("evt_tab_new");
    event_handler.tab_chdir = xset_get("evt_tab_chdir");
    event_handler.tab_focus = xset_get("evt_tab_focus");
    event_handler.tab_close = xset_get("evt_tab_close");
    event_handler.device = xset_get("evt_device");

    // add default bookmarks
    ptk_bookmark_view_get_first_bookmark(nullptr);
}

void
autosave_settings()
{
    save_settings(nullptr);
}

void
save_settings(void* main_window_ptr)
{
    FMMainWindow* main_window;
    // LOG_INFO("save_settings");

    xset_set("config_version", XSetSetSet::S, CONFIG_VERSION);

    // save tabs
    bool save_tabs = xset_get_b("main_save_tabs");
    if (main_window_ptr)
        main_window = static_cast<FMMainWindow*>(main_window_ptr);
    else
        main_window = fm_main_window_get_last_active();

    if (GTK_IS_WIDGET(main_window))
    {
        if (save_tabs)
        {
            for (int p = 1; p < 5; p++)
            {
                XSet* set = xset_get_panel(p, "show");
                if (GTK_IS_NOTEBOOK(main_window->panel[p - 1]))
                {
                    int pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_window->panel[p - 1]));
                    if (pages) // panel was shown
                    {
                        if (set->s)
                        {
                            free(set->s);
                            set->s = nullptr;
                        }
                        std::string tabs;
                        for (int g = 0; g < pages; g++)
                        {
                            PtkFileBrowser* file_browser = PTK_FILE_BROWSER(
                                gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_window->panel[p - 1]),
                                                          g));
                            tabs = fmt::format("{}{}{}",
                                               tabs,
                                               CONFIG_FILE_TABS_DELIM,
                                               ptk_file_browser_get_cwd(file_browser));
                        }
                        set->s = ztd::strdup(tabs);

                        // save current tab
                        if (set->x)
                            free(set->x);

                        int current_page =
                            gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->panel[p - 1]));
                        set->x = ztd::strdup(current_page);
                    }
                }
            }
        }
        else
        {
            // clear saved tabs
            for (int p = 1; p < 5; p++)
            {
                XSet* set = xset_get_panel(p, "show");
                if (set->s)
                {
                    free(set->s);
                    set->s = nullptr;
                }
                if (set->x)
                {
                    free(set->x);
                    set->x = nullptr;
                }
            }
        }
    }

    /* save settings */
    if (!std::filesystem::exists(settings_config_dir))
    {
        std::filesystem::create_directories(settings_config_dir);
        std::filesystem::permissions(settings_config_dir, std::filesystem::perms::owner_all);
    }

    std::string buf = "";

    // clang-format off
    buf.append("[General]\n");
    buf.append(fmt::format("show_thumbnail=\"{:d}\"\n", app_settings.show_thumbnail));
    buf.append(fmt::format("max_thumb_size=\"{}\"\n", app_settings.max_thumb_size >> 10));
    buf.append(fmt::format("big_icon_size=\"{}\"\n", app_settings.big_icon_size));
    buf.append(fmt::format("small_icon_size=\"{}\"\n", app_settings.small_icon_size));
    buf.append(fmt::format("tool_icon_size=\"{}\"\n", app_settings.tool_icon_size));
    buf.append(fmt::format("single_click=\"{:d}\"\n", app_settings.single_click));
    buf.append(fmt::format("no_single_hover=\"{:d}\"\n", app_settings.no_single_hover));
    buf.append(fmt::format("sort_order=\"{}\"\n", app_settings.sort_order));
    buf.append(fmt::format("sort_type=\"{}\"\n", app_settings.sort_type));
    buf.append(fmt::format("use_si_prefix=\"{:d}\"\n", app_settings.use_si_prefix));
    buf.append(fmt::format("no_execute=\"{:d}\"\n", app_settings.no_execute));
    buf.append(fmt::format("no_confirm=\"{:d}\"\n", app_settings.no_confirm));
    buf.append(fmt::format("no_confirm_trash=\"{:d}\"\n", app_settings.no_confirm_trash));

    buf.append("\n[Window]\n");
    buf.append(fmt::format("width=\"{}\"\n", app_settings.width));
    buf.append(fmt::format("height=\"{}\"\n", app_settings.height));
    buf.append(fmt::format("maximized=\"{:d}\"\n", app_settings.maximized));

    buf.append("\n[Interface]\n");
    buf.append(fmt::format("always_show_tabs=\"{:d}\"\n", app_settings.always_show_tabs));
    buf.append(fmt::format("show_close_tab_buttons=\"{:d}\"\n", app_settings.show_close_tab_buttons));

    buf.append("\n[MOD]\n");
    xset_write(buf);
    // clang-format on

    // move
    std::string path = Glib::build_filename(settings_config_dir, "session");
    std::ofstream file(path);
    if (file.is_open())
        file << buf;
    else
        LOG_ERROR("saving session file failed");
    file.close();
}

void
free_settings()
{
    if (!xset_cmd_history.empty())
        xset_cmd_history.clear();

    xset_free_all();
}

const char*
xset_get_config_dir()
{
    return settings_config_dir.c_str();
}

const char*
xset_get_user_tmp_dir()
{
    if (settings_user_tmp_dir.empty() && std::filesystem::exists(settings_user_tmp_dir))
        return settings_user_tmp_dir.c_str();

    settings_user_tmp_dir = Glib::build_filename(config_settings.tmp_dir, PACKAGE_NAME);
    std::filesystem::create_directories(settings_user_tmp_dir);
    std::filesystem::permissions(settings_user_tmp_dir, std::filesystem::perms::owner_all);

    return settings_user_tmp_dir.c_str();
}

static void
xset_free_all()
{
    for (XSet* set: xsets)
    {
        if (set->ob2_data && Glib::str_has_prefix(set->name, "evt_"))
        {
            g_list_foreach((GList*)set->ob2_data, (GFunc)free, nullptr);
            g_list_free((GList*)set->ob2_data);
        }
        xset_free(set);
        g_slice_free(XSet, set);
    }
    xsets.clear();
    set_last = nullptr;

    if (xset_context)
    {
        xset_context_new();
        g_slice_free(XSetContext, xset_context);
        xset_context = nullptr;
    }
}

static void
xset_free(XSet* set)
{
    if (set->name)
        free(set->name);
    if (set->s)
        free(set->s);
    if (set->x)
        free(set->x);
    if (set->y)
        free(set->y);
    if (set->z)
        free(set->z);
    if (set->menu_label)
        free(set->menu_label);
    if (set->shared_key)
        free(set->shared_key);
    if (set->icon)
        free(set->icon);
    if (set->desc)
        free(set->desc);
    if (set->title)
        free(set->title);
    if (set->next)
        free(set->next);
    if (set->parent)
        free(set->parent);
    if (set->child)
        free(set->child);
    if (set->prev)
        free(set->prev);
    if (set->line)
        free(set->line);
    if (set->context)
        free(set->context);
    if (set->plugin)
    {
        if (set->plug_dir)
            free(set->plug_dir);
        if (set->plug_name)
            free(set->plug_name);
    }
}

static void
xset_remove(XSet* set)
{
    xset_free(set);
    g_slice_free(XSet, set);
    xsets.erase(std::remove(xsets.begin(), xsets.end(), set), xsets.end());
    set_last = nullptr;
}

static XSet*
xset_new(const std::string& name)
{
    XSet* set = g_slice_new(XSet);
    set->name = ztd::strdup(name);

    set->b = XSetB::XSET_B_UNSET;
    set->s = nullptr;
    set->x = nullptr;
    set->y = nullptr;
    set->z = nullptr;
    set->disable = false;
    set->menu_label = nullptr;
    set->menu_style = XSetMenu::NORMAL;
    set->cb_func = nullptr;
    set->cb_data = nullptr;
    set->ob1 = nullptr;
    set->ob1_data = nullptr;
    set->ob2 = nullptr;
    set->ob2_data = nullptr;
    set->key = 0;
    set->keymod = 0;
    set->shared_key = nullptr;
    set->icon = nullptr;
    set->desc = nullptr;
    set->title = nullptr;
    set->next = nullptr;
    set->context = nullptr;
    set->tool = XSetTool::NOT;
    set->lock = true;
    set->plugin = false;

    // custom ( !lock )
    set->prev = nullptr;
    set->parent = nullptr;
    set->child = nullptr;
    set->line = nullptr;
    set->task = false;
    set->task_pop = false;
    set->task_err = false;
    set->task_out = false;
    set->in_terminal = false;
    set->keep_terminal = false;
    set->scroll_lock = false;
    set->opener = 0;
    return set;
}

XSet*
xset_get(const std::string& name)
{
    for (XSet* set: xsets)
    {
        // check for existing xset
        if (ztd::same(name, set->name))
            return set;
    }

    XSet* set = xset_new(name);
    xsets.push_back(set);
    return set;
}

XSet*
xset_get_panel(int panel, const std::string& name)
{
    std::string fullname = fmt::format("panel{}_{}", panel, name);
    XSet* set = xset_get(fullname);
    return set;
}

XSet*
xset_get_panel_mode(int panel, const std::string& name, const char mode)
{
    // FMT BUG - need to use std::to_string on char
    // otherwise it gets ignored and not added to new string
    std::string fullname = fmt::format("panel{}_{}{}", panel, name, std::to_string(mode));
    XSet* set = xset_get(fullname);
    return set;
}

char*
xset_get_s(const std::string& name)
{
    XSet* set = xset_get(name);
    if (set)
        return set->s;
    else
        return nullptr;
}

char*
xset_get_s_panel(int panel, const std::string& name)
{
    std::string fullname = fmt::format("panel{}_{}", panel, name);
    return xset_get_s(fullname);
}

bool
xset_get_b(const std::string& name)
{
    XSet* set = xset_get(name);
    return (set->b == XSetB::XSET_B_TRUE);
}

bool
xset_get_b_panel(int panel, const std::string& name)
{
    std::string fullname = fmt::format("panel{}_{}", panel, name);
    return xset_get_b(fullname);
}

bool
xset_get_b_panel_mode(int panel, const std::string& name, const char mode)
{
    // FMT BUG - need to use std::to_string on char
    // otherwise it gets ignored and not added to new string
    std::string fullname = fmt::format("panel{}_{}{}", panel, name, std::to_string(mode));
    return xset_get_b(fullname);
}

static bool
xset_get_b_set(XSet* set)
{
    return (set->b == XSetB::XSET_B_TRUE);
}

XSet*
xset_is(const std::string& name)
{
    for (XSet* set: xsets)
    {
        // check for existing xset
        if (ztd::same(name, set->name))
            return set;
    }
    return nullptr;
}

XSet*
xset_set_b(const std::string& name, bool bval)
{
    XSet* set = xset_get(name);

    if (bval)
        set->b = XSetB::XSET_B_TRUE;
    else
        set->b = XSetB::XSET_B_FALSE;
    return set;
}

XSet*
xset_set_b_panel(int panel, const std::string& name, bool bval)
{
    std::string fullname = fmt::format("panel{}_{}", panel, name);
    XSet* set = xset_set_b(fullname, bval);
    return set;
}

XSet*
xset_set_b_panel_mode(int panel, const std::string& name, const char mode, bool bval)
{
    // FMT BUG - need to use std::to_string on char
    // otherwise it gets ignored and not added to new string
    std::string fullname = fmt::format("panel{}_{}{}", panel, name, std::to_string(mode));
    XSet* set = xset_set_b(fullname, bval);
    return set;
}

static int
xset_get_int_set(XSet* set, XSetSetSet var)
{
    if (!set)
        return -1;

    const char* varstring = nullptr;
    switch (var)
    {
        case XSetSetSet::S:
            varstring = set->s;
            break;
        case XSetSetSet::X:
            varstring = set->x;
            break;
        case XSetSetSet::Y:
            varstring = set->y;
            break;
        case XSetSetSet::Z:
            varstring = set->z;
            break;
        case XSetSetSet::KEY:
            return set->key;
        case XSetSetSet::KEYMOD:
            return set->keymod;
        case XSetSetSet::B:
        case XSetSetSet::STYLE:
        case XSetSetSet::DESC:
        case XSetSetSet::TITLE:
        case XSetSetSet::MENU_LABEL:
        case XSetSetSet::ICN:
        case XSetSetSet::MENU_LABEL_CUSTOM:
        case XSetSetSet::ICON:
        case XSetSetSet::SHARED_KEY:
        case XSetSetSet::NEXT:
        case XSetSetSet::PREV:
        case XSetSetSet::PARENT:
        case XSetSetSet::CHILD:
        case XSetSetSet::CONTEXT:
        case XSetSetSet::LINE:
        case XSetSetSet::TOOL:
        case XSetSetSet::TASK:
        case XSetSetSet::TASK_POP:
        case XSetSetSet::TASK_ERR:
        case XSetSetSet::TASK_OUT:
        case XSetSetSet::RUN_IN_TERMINAL:
        case XSetSetSet::KEEP_TERMINAL:
        case XSetSetSet::SCROLL_LOCK:
        case XSetSetSet::DISABLE:
        case XSetSetSet::OPENER:
        default:
            return -1;
    }

    if (!varstring)
        return 0;
    return strtol(varstring, nullptr, 10);
}

int
xset_get_int(const std::string& name, XSetSetSet var)
{
    XSet* set = xset_get(name);
    return xset_get_int_set(set, var);
}

int
xset_get_int_panel(int panel, const std::string& name, XSetSetSet var)
{
    std::string fullname = fmt::format("panel{}_{}", panel, name);
    return xset_get_int(fullname, var);
}

static XSet*
xset_is_main_bookmark(XSet* set)
{
    XSet* set_parent = nullptr;

    // is this xset in main_book ?  returns immediate parent set
    XSet* set_prev = set;
    while (set_prev)
    {
        if (set_prev->prev)
            set_prev = xset_is(set_prev->prev);
        else if (set_prev->parent)
        {
            set_prev = xset_is(set_prev->parent);
            if (!set_parent)
                set_parent = set_prev;
            if (set_prev && !g_strcmp0(set_prev->name, "main_book"))
            {
                // found bookmark in main_book tree
                return set_parent;
            }
        }
        else
            break;
    }
    return nullptr;
}

static void
xset_write_set(std::string& buf, XSet* set)
{
    if (set->plugin)
        return;
    if (set->s)
        buf.append(fmt::format("{}-s=\"{}\"\n", set->name, set->s));
    if (set->x)
        buf.append(fmt::format("{}-x=\"{}\"\n", set->name, set->x));
    if (set->y)
        buf.append(fmt::format("{}-y=\"{}\"\n", set->name, set->y));
    if (set->z)
        buf.append(fmt::format("{}-z=\"{}\"\n", set->name, set->z));
    if (set->key)
        buf.append(fmt::format("{}-key=\"{}\"\n", set->name, set->key));
    if (set->keymod)
        buf.append(fmt::format("{}-keymod=\"{}\"\n", set->name, set->keymod));
    // menu label
    if (set->menu_label)
    {
        if (set->lock)
        {
            // built-in
            if (set->in_terminal && set->menu_label && set->menu_label[0])
                // only save lbl if menu_label was customized
                buf.append(fmt::format("{}-menu_label=\"{}\"\n", set->name, set->menu_label));
        }
        else
        {
            // custom
            buf.append(fmt::format("{}-menu_label_custom=\"{}\"\n", set->name, set->menu_label));
        }
    }
    // icon
    if (set->lock)
    {
        // built-in
        if (set->keep_terminal)
            // only save icn if icon was customized
            buf.append(fmt::format("{}-icn=\"{}\"\n", set->name, set->icon ? set->icon : ""));
    }
    else if (set->icon)
    {
        // custom
        buf.append(fmt::format("{}-icon=\"{}\"\n", set->name, set->icon));
    }

    if (set->next)
        buf.append(fmt::format("{}-next=\"{}\"\n", set->name, set->next));
    if (set->child)
        buf.append(fmt::format("{}-child=\"{}\"\n", set->name, set->child));
    if (set->context)
        buf.append(fmt::format("{}-context=\"{}\"\n", set->name, set->context));
    if (set->b != XSetB::XSET_B_UNSET)
        buf.append(fmt::format("{}-b=\"{}\"\n", set->name, set->b));
    if (set->tool != XSetTool::NOT)
        buf.append(fmt::format("{}-tool=\"{}\"\n", set->name, static_cast<int>(set->tool)));

    if (!set->lock)
    {
        if (set->menu_style != XSetMenu::NORMAL)
            buf.append(
                fmt::format("{}-style=\"{}\"\n", set->name, static_cast<int>(set->menu_style)));
        if (set->desc)
            buf.append(fmt::format("{}-desc=\"{}\"\n", set->name, set->desc));
        if (set->title)
            buf.append(fmt::format("{}-title=\"{}\"\n", set->name, set->title));
        if (set->prev)
            buf.append(fmt::format("{}-prev=\"{}\"\n", set->name, set->prev));
        if (set->parent)
            buf.append(fmt::format("{}-parent=\"{}\"\n", set->name, set->parent));
        if (set->line)
            buf.append(fmt::format("{}-line=\"{}\"\n", set->name, set->line));
        if (set->task)
            buf.append(fmt::format("{}-task=\"{:d}\"\n", set->name, set->task));
        if (set->task_pop)
            buf.append(fmt::format("{}-task_pop=\"{:d}\"\n", set->name, set->task_pop));
        if (set->task_err)
            buf.append(fmt::format("{}-task_err=\"{:d}\"\n", set->name, set->task_err));
        if (set->task_out)
            buf.append(fmt::format("{}-task_out=\"{:d}\"\n", set->name, set->task_out));
        if (set->in_terminal)
            buf.append(fmt::format("{}-run_in_terminal=\"{:d}\"\n", set->name, set->in_terminal));
        if (set->keep_terminal)
            buf.append(fmt::format("{}-keep_terminal=\"{:d}\"\n", set->name, set->keep_terminal));
        if (set->scroll_lock)
            buf.append(fmt::format("{}-scroll_lock=\"{:d}\"\n", set->name, set->scroll_lock));
        if (set->opener != 0)
            buf.append(fmt::format("{}-opener=\"{}\"\n", set->name, set->opener));
    }
}

static void
xset_write(std::string& buf)
{
    for (XSet* set: xsets)
    {
        // hack to not save default handlers - this allows default handlers
        // to be updated more easily
        if (set->disable && set->name[0] == 'h' && Glib::str_has_prefix(set->name, "hand"))
            continue;
        xset_write_set(buf, set);
    }
}

static void
xset_parse(std::string& line)
{
    std::size_t sep = line.find("=");
    if (sep == std::string::npos)
        return;

    std::size_t sep2 = line.find("-");
    if (sep2 == std::string::npos)
        return;

    line = ztd::strip(line);

    if (line.at(0) == '#')
        return;

    std::string token = line.substr(0, sep2);
    std::string value = line.substr(sep + 1, std::string::npos - 1);
    std::string token_var = line.substr(sep2 + 1, sep - sep2 - 1);

    XSetSetSet var;
    try
    {
        var = xset_set_set_encode(token_var);
    }
    catch (const std::logic_error& e)
    {
        std::string msg = fmt::format("XSet parse error:\n\n{}", e.what());
        ptk_show_error(nullptr, "Error", e.what());
        return;
    }

    // remove any quotes
    value = ztd::replace(value, "\"", "");

    if (value.empty())
        return;

    if (ztd::startswith(token, "cstm_") || ztd::startswith(token, "hand_"))
    {
        // custom
        if (ztd::same(token, set_last->name))
        {
            xset_set_set(set_last, var, value.c_str());
        }
        else
        {
            set_last = xset_get(token);
            if (set_last->lock)
                set_last->lock = false;
            xset_set_set(set_last, var, value.c_str());
        }
    }
    else
    {
        // normal (lock)
        if (ztd::same(token, set_last->name))
        {
            xset_set_set(set_last, var, value.c_str());
        }
        else
        {
            set_last = xset_set(token, var, value.c_str());
        }
    }
}

XSet*
xset_set_cb(const std::string& name, GFunc cb_func, void* cb_data)
{
    XSet* set = xset_get(name);
    set->cb_func = cb_func;
    set->cb_data = cb_data;
    return set;
}

XSet*
xset_set_cb_panel(int panel, const std::string& name, GFunc cb_func, void* cb_data)
{
    std::string fullname = fmt::format("panel{}_{}", panel, name);
    XSet* set = xset_set_cb(fullname, cb_func, cb_data);
    return set;
}

XSet*
xset_set_ob1_int(XSet* set, const char* ob1, int ob1_int)
{
    if (set->ob1)
        free(set->ob1);
    set->ob1 = ztd::strdup(ob1);
    set->ob1_data = GINT_TO_POINTER(ob1_int);
    return set;
}

XSet*
xset_set_ob1(XSet* set, const char* ob1, void* ob1_data)
{
    if (set->ob1)
        free(set->ob1);
    set->ob1 = ztd::strdup(ob1);
    set->ob1_data = ob1_data;
    return set;
}

XSet*
xset_set_ob2(XSet* set, const char* ob2, void* ob2_data)
{
    if (set->ob2)
        free(set->ob2);
    set->ob2 = ztd::strdup(ob2);
    set->ob2_data = ob2_data;
    return set;
}

static XSetSetSet
xset_set_set_encode(const std::string& var)
{
    XSetSetSet tmp;
    if (ztd::same(var, "s"))
        tmp = XSetSetSet::S;
    else if (ztd::same(var, "b"))
        tmp = XSetSetSet::B;
    else if (ztd::same(var, "x"))
        tmp = XSetSetSet::X;
    else if (ztd::same(var, "y"))
        tmp = XSetSetSet::Y;
    else if (ztd::same(var, "z"))
        tmp = XSetSetSet::Z;
    else if (ztd::same(var, "key"))
        tmp = XSetSetSet::KEY;
    else if (ztd::same(var, "keymod"))
        tmp = XSetSetSet::KEYMOD;
    else if (ztd::same(var, "style"))
        tmp = XSetSetSet::STYLE;
    else if (ztd::same(var, "desc"))
        tmp = XSetSetSet::DESC;
    else if (ztd::same(var, "title"))
        tmp = XSetSetSet::TITLE;
    else if (ztd::same(var, "menu_label"))
        tmp = XSetSetSet::MENU_LABEL;
    else if (ztd::same(var, "icn"))
        tmp = XSetSetSet::ICN;
    else if (ztd::same(var, "menu_label_custom"))
        tmp = XSetSetSet::MENU_LABEL_CUSTOM;
    else if (ztd::same(var, "icon"))
        tmp = XSetSetSet::ICON;
    else if (ztd::same(var, "shared_key"))
        tmp = XSetSetSet::SHARED_KEY;
    else if (ztd::same(var, "next"))
        tmp = XSetSetSet::NEXT;
    else if (ztd::same(var, "prev"))
        tmp = XSetSetSet::PREV;
    else if (ztd::same(var, "parent"))
        tmp = XSetSetSet::PARENT;
    else if (ztd::same(var, "child"))
        tmp = XSetSetSet::CHILD;
    else if (ztd::same(var, "context"))
        tmp = XSetSetSet::CONTEXT;
    else if (ztd::same(var, "line"))
        tmp = XSetSetSet::LINE;
    else if (ztd::same(var, "tool"))
        tmp = XSetSetSet::TOOL;
    else if (ztd::same(var, "task"))
        tmp = XSetSetSet::TASK;
    else if (ztd::same(var, "task_pop"))
        tmp = XSetSetSet::TASK_POP;
    else if (ztd::same(var, "task_err"))
        tmp = XSetSetSet::TASK_ERR;
    else if (ztd::same(var, "task_out"))
        tmp = XSetSetSet::TASK_OUT;
    else if (ztd::same(var, "run_in_terminal"))
        tmp = XSetSetSet::RUN_IN_TERMINAL;
    else if (ztd::same(var, "keep_terminal"))
        tmp = XSetSetSet::KEEP_TERMINAL;
    else if (ztd::same(var, "scroll_lock"))
        tmp = XSetSetSet::SCROLL_LOCK;
    else if (ztd::same(var, "disable"))
        tmp = XSetSetSet::DISABLE;
    else if (ztd::same(var, "opener"))
        tmp = XSetSetSet::OPENER;
    // Deprecated config keys - start
    else if (ztd::same(var, "lbl"))
        tmp = XSetSetSet::MENU_LABEL;
    else if (ztd::same(var, "label"))
        tmp = XSetSetSet::MENU_LABEL_CUSTOM;
    else if (ztd::same(var, "cxt"))
        tmp = XSetSetSet::CONTEXT;
    else if (ztd::same(var, "term"))
        tmp = XSetSetSet::RUN_IN_TERMINAL;
    else if (ztd::same(var, "keep"))
        tmp = XSetSetSet::KEEP_TERMINAL;
    else if (ztd::same(var, "scroll"))
        tmp = XSetSetSet::SCROLL_LOCK;
    else if (ztd::same(var, "op"))
        tmp = XSetSetSet::OPENER;
    // Deprecated config keys - end
    else
    {
        std::string err_msg = fmt::format("Unknown XSet var {}", var);
        throw std::logic_error(err_msg);
    }
    return tmp;
}

XSet*
xset_set_set(XSet* set, XSetSetSet var, const char* value)
{
    if (!set)
        return nullptr;

    switch (var)
    {
        case XSetSetSet::S:
            if (set->s)
                free(set->s);
            set->s = ztd::strdup(value);
            break;
        case XSetSetSet::B:
            if (!strcmp(value, "1"))
                set->b = XSetB::XSET_B_TRUE;
            else
                set->b = XSetB::XSET_B_FALSE;
            break;
        case XSetSetSet::X:
            if (set->x)
                free(set->x);
            set->x = ztd::strdup(value);
            break;
        case XSetSetSet::Y:
            if (set->y)
                free(set->y);
            set->y = ztd::strdup(value);
            break;
        case XSetSetSet::Z:
            if (set->z)
                free(set->z);
            set->z = ztd::strdup(value);
            break;
        case XSetSetSet::KEY:
            set->key = strtol(value, nullptr, 10);
            break;
        case XSetSetSet::KEYMOD:
            set->keymod = strtol(value, nullptr, 10);
            break;
        case XSetSetSet::STYLE:
            set->menu_style = (XSetMenu)strtol(value, nullptr, 10);
            break;
        case XSetSetSet::DESC:
            if (set->desc)
                free(set->desc);
            set->desc = ztd::strdup(value);
            break;
        case XSetSetSet::TITLE:
            if (set->title)
                free(set->title);
            set->title = ztd::strdup(value);
            break;
        case XSetSetSet::MENU_LABEL:
            // lbl is only used >= 0.9.0 for changed lock default menu_label
            if (set->menu_label)
                free(set->menu_label);
            set->menu_label = ztd::strdup(value);
            if (set->lock)
                // indicate that menu label is not default and should be saved
                set->in_terminal = true;
            break;
        case XSetSetSet::ICN:
            // icn is only used >= 0.9.0 for changed lock default icon
            if (set->icon)
                free(set->icon);
            set->icon = ztd::strdup(value);
            if (set->lock)
                // indicate that icon is not default and should be saved
                set->keep_terminal = true;
            break;
        case XSetSetSet::MENU_LABEL_CUSTOM:
            // pre-0.9.0 menu_label or >= 0.9.0 custom item label
            // only save if custom or not default label
            if (!set->lock || g_strcmp0(set->menu_label, value))
            {
                if (set->menu_label)
                    free(set->menu_label);
                set->menu_label = ztd::strdup(value);
                if (set->lock)
                    // indicate that menu label is not default and should be saved
                    set->in_terminal = true;
            }
            break;
        case XSetSetSet::ICON:
            // pre-0.9.0 icon or >= 0.9.0 custom item icon
            // only save if custom or not default icon
            // also check that stock name does not match
            break;
        case XSetSetSet::SHARED_KEY:
            if (set->shared_key)
                free(set->shared_key);
            set->shared_key = ztd::strdup(value);
            break;
        case XSetSetSet::NEXT:
            if (set->next)
                free(set->next);
            set->next = ztd::strdup(value);
            break;
        case XSetSetSet::PREV:
            if (set->prev)
                free(set->prev);
            set->prev = ztd::strdup(value);
            break;
        case XSetSetSet::PARENT:
            if (set->parent)
                free(set->parent);
            set->parent = ztd::strdup(value);
            break;
        case XSetSetSet::CHILD:
            if (set->child)
                free(set->child);
            set->child = ztd::strdup(value);
            break;
        case XSetSetSet::CONTEXT:
            if (set->context)
                free(set->context);
            set->context = ztd::strdup(value);
            break;
        case XSetSetSet::LINE:
            if (set->line)
                free(set->line);
            set->line = ztd::strdup(value);
            break;
        case XSetSetSet::TOOL:
            set->tool = XSetTool(std::stoi(value));
            break;
        case XSetSetSet::TASK:
            if (std::stoi(value) == 1)
                set->task = true;
            else
                set->task = false;
            break;
        case XSetSetSet::TASK_POP:
            if (std::stoi(value) == 1)
                set->task_pop = true;
            else
                set->task_pop = false;
            break;
        case XSetSetSet::TASK_ERR:
            if (std::stoi(value) == 1)
                set->task_err = true;
            else
                set->task_err = false;
            break;
        case XSetSetSet::TASK_OUT:
            if (std::stoi(value) == 1)
                set->task_out = true;
            else
                set->task_out = false;
            break;
        case XSetSetSet::RUN_IN_TERMINAL:
            if (std::stoi(value) == 1)
                set->in_terminal = true;
            else
                set->in_terminal = false;
            break;
        case XSetSetSet::KEEP_TERMINAL:
            if (std::stoi(value) == 1)
                set->keep_terminal = true;
            else
                set->keep_terminal = false;
            break;
        case XSetSetSet::SCROLL_LOCK:
            if (std::stoi(value) == 1)
                set->scroll_lock = true;
            else
                set->scroll_lock = false;
            break;
        case XSetSetSet::DISABLE:
            if (std::stoi(value) == 1)
                set->disable = true;
            else
                set->disable = false;
            break;
        case XSetSetSet::OPENER:
            set->opener = std::stoi(value);
            break;
        default:
            break;
    }

    return set;
}

XSet*
xset_set(const std::string& name, XSetSetSet var, const char* value)
{
    XSet* set = xset_get(name);
    if (!set->lock || (var != XSetSetSet::STYLE && var != XSetSetSet::DESC &&
                       var != XSetSetSet::TITLE && var != XSetSetSet::SHARED_KEY))
        return xset_set_set(set, var, value);

    return set;
}

XSet*
xset_set_panel(int panel, const std::string& name, XSetSetSet var, const char* value)
{
    std::string fullname = fmt::format("panel{}_{}", panel, name);
    XSet* set = xset_set(fullname, var, value);
    return set;
}

XSet*
xset_find_custom(const std::string& search)
{
    // find a custom command or submenu by label or xset name
    std::string label = clean_label(search, true, false);

    for (XSet* set: xsets)
    {
        if (!set->lock && ((set->menu_style == XSetMenu::SUBMENU && set->child) ||
                           (set->menu_style < XSetMenu::SUBMENU &&
                            XSetCMD(xset_get_int_set(set, XSetSetSet::X)) <= XSetCMD::BOOKMARK)))
        {
            // custom submenu or custom command - label or name matches?
            std::string str = clean_label(set->menu_label, true, false);
            if (ztd::same(set->name, search) || ztd::same(str, label))
            {
                // match
                return set;
            }
        }
    }
    return nullptr;
}

bool
xset_opener(PtkFileBrowser* file_browser, const char job)
{ // find an opener for job
    XSet* set;
    XSet* mset;
    XSet* open_all_set;
    XSet* tset;
    XSet* open_all_tset;
    XSetContext* context = nullptr;
    int context_action;
    bool found = false;
    char pinned;

    for (XSet* set2: xsets)
    {
        if (!set2->lock && set2->opener == job && set2->tool == XSetTool::NOT &&
            set2->menu_style != XSetMenu::SUBMENU && set2->menu_style != XSetMenu::SEP)
        {
            if (set2->desc && !strcmp(set2->desc, "@plugin@mirror@"))
            {
                // is a plugin mirror
                mset = set2;
                if (!mset->shared_key)
                    continue;
                set2 = xset_is(mset->shared_key);
            }
            else if (set2->plugin && set2->shared_key)
            {
                // plugin with mirror - ignore to use mirror's context only
                continue;
            }
            else
                set = mset = set2;

            if (!context)
            {
                if (!(context = xset_context_new()))
                    return false;
                if (file_browser)
                    main_context_fill(file_browser, context);
                else
                    return false;

                if (!context->valid)
                    return false;

                // get mime type open_all_type set
                std::string str;
                char* open_all_name = ztd::strdup(context->var[ItemPropContext::CONTEXT_MIME]);
                if (open_all_name)
                    str = open_all_name;
                else
                    str = "";

                str = ztd::replace(str, "-", "_");
                str = ztd::replace(str, " ", "");
                open_all_set = xset_is(fmt::format("open_all_type_{}", str));
            }

            // test context
            if (mset->context)
            {
                context_action = xset_context_test(context, mset->context, false);
                if (context_action == ItemPropContextState::CONTEXT_HIDE ||
                    context_action == ItemPropContextState::CONTEXT_DISABLE)
                    continue;
            }

            // valid custom type?
            XSetCMD cmd_type = XSetCMD(xset_get_int_set(set, XSetSetSet::X));
            if (cmd_type != XSetCMD::APP && cmd_type != XSetCMD::LINE &&
                cmd_type != XSetCMD::SCRIPT)
                continue;

            // is set pinned to open_all_type for pre-context?
            pinned = 0;
            for (XSet* set3: xsets)
            {
                if (set3->next && Glib::str_has_prefix(set3->name, "open_all_type_"))
                {
                    tset = open_all_tset = set3;
                    while (tset->next)
                    {
                        if (!strcmp(set->name, tset->next))
                        {
                            // found pinned to open_all_type
                            if (open_all_tset == open_all_set)
                                // correct mime type
                                pinned = 2;
                            else
                                // wrong mime type
                                pinned = 1;
                            break;
                        }
                        if (tset->next)
                            tset = xset_is(tset->next);
                    }
                }
            }
            if (pinned == 1)
                continue;

            // valid
            found = true;
            set->browser = file_browser;
            std::string clean = clean_label(set->menu_label, false, false);
            LOG_INFO("Selected Menu Item '{}' As Handler", clean);
            xset_menu_cb(nullptr, set); // also does custom activate
        }
    }
    return found;
}

static void
write_root_saver(std::string& buf, const std::string& path, const char* name, const char* var,
                 const char* value)
{
    if (!value)
        return;

    std::string save;
    save = fmt::format("{}-{}={}", name, var, value);
    save = bash_quote(save);
    buf.append(fmt::format("echo {} >>| \"{}\"\n", save, path));
}

bool
write_root_settings(std::string& buf, const std::string& path)
{
    buf.append(fmt::format("\n#save root settings\nmkdir -p {}/{}\n"
                           "echo -e '#SpaceFM As-Root Session File\\n\\' >| '{}'\n",
                           SYSCONFDIR,
                           PACKAGE_NAME,
                           path));

    for (XSet* set: xsets)
    {
        if (set)
        {
            if (!strcmp(set->name, "root_editor") || !strcmp(set->name, "dev_back_part") ||
                !strcmp(set->name, "dev_rest_file") || !strcmp(set->name, "main_terminal") ||
                !strncmp(set->name, "dev_fmt_", 8) || !strncmp(set->name, "label_cmd_", 8))
            {
                write_root_saver(buf, path, set->name, "s", set->s);
                write_root_saver(buf, path, set->name, "x", set->x);
                write_root_saver(buf, path, set->name, "y", set->y);
                if (set->b != XSetB::XSET_B_UNSET)
                    buf.append(fmt::format("echo '{}-b={}' >>| \"{}\"\n", set->name, set->b, path));
            }
        }
    }

    buf.append(fmt::format("chmod -R go-w+rX {}/{}\n\n", SYSCONFDIR, PACKAGE_NAME));
    return true;
}

static void
read_root_settings()
{
    if (geteuid() == 0)
        return;

    std::string root_set_path =
        fmt::format("{}/{}/{}-as-root", SYSCONFDIR, PACKAGE_NAME, Glib::get_user_name());
    if (!std::filesystem::exists(root_set_path))
        root_set_path = fmt::format("{}/{}/{}-as-root", SYSCONFDIR, PACKAGE_NAME, geteuid());

    std::string line;
    std::ifstream file(root_set_path);

    if (!file.is_open())
    {
        if (std::filesystem::exists(root_set_path))
            LOG_WARN("Error reading root settings from {} Commands run as root may "
                     "present a security risk",
                     root_set_path);
        else
            LOG_WARN("No root settings found in {} Setting a root editor in "
                     "Preferences should remove this warning on startup.   Otherwise commands "
                     "run as root may present a security risk.",
                     root_set_path);
        return;
    }

    // clear settings
    for (XSet* set: xsets)
    {
        if (set)
        {
            if (!strcmp(set->name, "root_editor") || !strcmp(set->name, "dev_back_part") ||
                !strcmp(set->name, "dev_rest_file") || !strncmp(set->name, "dev_fmt_", 8) ||
                !strncmp(set->name, "label_cmd_", 8))
            {
                if (set->s)
                {
                    free(set->s);
                    set->s = nullptr;
                }
                if (set->x)
                {
                    free(set->x);
                    set->x = nullptr;
                }
                if (set->y)
                {
                    free(set->y);
                    set->y = nullptr;
                }
                set->b = XSetB::XSET_B_UNSET;
            }
        }
    }

    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        xset_parse(line);
    }

    file.close();
}

XSetContext*
xset_context_new()
{
    if (!xset_context)
    {
        xset_context = g_slice_new0(XSetContext);
        xset_context->valid = false;
        for (std::size_t i = 0; i < xset_context->var.size(); ++i)
            xset_context->var[i] = nullptr;
    }
    else
    {
        xset_context->valid = false;
        for (std::size_t i = 0; i < xset_context->var.size(); ++i)
        {
            if (xset_context->var[i])
                free(xset_context->var[i]);
            xset_context->var[i] = nullptr;
        }
    }
    return xset_context;
}

GtkWidget*
xset_get_image(const char* icon, GtkIconSize icon_size)
{
    /*
        GTK_ICON_SIZE_MENU,
        GTK_ICON_SIZE_SMALL_TOOLBAR,
        GTK_ICON_SIZE_LARGE_TOOLBAR,
        GTK_ICON_SIZE_BUTTON,
        GTK_ICON_SIZE_DND,
        GTK_ICON_SIZE_DIALOG
    */

    if (!(icon && icon[0]))
        return nullptr;

    if (!icon_size)
        icon_size = GTK_ICON_SIZE_MENU;

    return gtk_image_new_from_icon_name(icon, icon_size);
}

void
xset_add_menu(PtkFileBrowser* file_browser, GtkWidget* menu, GtkAccelGroup* accel_group,
              const char* elements)
{
    if (!elements)
        return;

    std::vector<std::string> split_elements = ztd::split(elements, " ");

    for (const std::string& element: split_elements)
    {
        XSet* set = xset_get(element);
        xset_add_menuitem(file_browser, menu, accel_group, set);
    }
}

static GtkWidget*
xset_new_menuitem(const char* label, const char* icon)
{
    GtkWidget* item;

    if (label && strstr(label, "\\_"))
    {
        // allow escape of underscore
        std::string str = clean_label(label, false, false);
        item = gtk_menu_item_new_with_label(str.c_str());
    }
    else
        item = gtk_menu_item_new_with_mnemonic(label);
    if (!(icon && icon[0]))
        return item;
    // GtkWidget* image = xset_get_image(icon, GTK_ICON_SIZE_MENU);

    return item;
}

char*
xset_custom_get_app_name_icon(XSet* set, GdkPixbuf** icon, int icon_size)
{
    char* menu_label = nullptr;
    GdkPixbuf* icon_new = nullptr;

    if (!set->lock && XSetCMD(xset_get_int_set(set, XSetSetSet::X)) == XSetCMD::APP)
    {
        if (set->z && Glib::str_has_suffix(set->z, ".desktop"))
        {
            VFSAppDesktop desktop(set->z);

            if (!(set->menu_label && set->menu_label[0]))
                menu_label = ztd::strdup(desktop.get_disp_name());
            if (set->icon)
                icon_new = vfs_load_icon(set->icon, icon_size);
            if (!icon_new)
                icon_new = desktop.get_icon(icon_size);
        }
        else
        {
            // not a desktop file - probably executable
            if (set->icon)
                icon_new = vfs_load_icon(set->icon, icon_size);
            if (!icon_new && set->z)
            {
                // guess icon name from executable name
                char* name = g_path_get_basename(set->z);
                if (name && name[0])
                    icon_new = vfs_load_icon(name, icon_size);
                free(name);
            }
        }

        if (!icon_new)
        {
            // fallback
            icon_new = vfs_load_icon("gtk-execute", icon_size);
        }
    }
    else
        LOG_WARN("xset_custom_get_app_name_icon set is not XSetCMD::APP");

    if (icon)
        *icon = icon_new;
    else if (icon_new)
        g_object_unref(icon_new);

    if (!menu_label)
    {
        menu_label = set->menu_label && set->menu_label[0] ? ztd::strdup(set->menu_label)
                                                           : ztd::strdup(set->z);
        if (!menu_label)
            menu_label = ztd::strdup("Application");
    }
    return menu_label;
}

GdkPixbuf*
xset_custom_get_bookmark_icon(XSet* set, int icon_size)
{
    GdkPixbuf* icon_new = nullptr;
    const char* icon1 = nullptr;
    const char* icon2 = nullptr;
    const char* icon3 = nullptr;

    if (!book_icon_set_cached)
        book_icon_set_cached = xset_get("book_icon");

    if (!set->lock && XSetCMD(xset_get_int_set(set, XSetSetSet::X)) == XSetCMD::BOOKMARK)
    {
        if (!set->icon && (set->z && (strstr(set->z, ":/") || Glib::str_has_prefix(set->z, "//"))))
        {
            // a bookmarked URL - show network icon
            XSet* set2 = xset_get("dev_icon_network");
            if (set2->icon)
                icon1 = ztd::strdup(set2->icon);
            else
                icon1 = ztd::strdup("gtk-network");
            icon2 = ztd::strdup("user-bookmarks");
            icon3 = ztd::strdup("gnome-fs-directory");
        }
        else if (set->z && (strstr(set->z, ":/") || Glib::str_has_prefix(set->z, "//")))
        {
            // a bookmarked URL - show custom or network icon
            icon1 = ztd::strdup(set->icon);
            XSet* set2 = xset_get("dev_icon_network");
            if (set2->icon)
                icon2 = ztd::strdup(set2->icon);
            else
                icon2 = ztd::strdup("gtk-network");
            icon3 = ztd::strdup("user-bookmarks");
        }
        else if (!set->icon && book_icon_set_cached->icon)
        {
            icon1 = ztd::strdup(book_icon_set_cached->icon);
            icon2 = ztd::strdup("user-bookmarks");
            icon3 = ztd::strdup("gnome-fs-directory");
        }
        else if (set->icon && book_icon_set_cached->icon)
        {
            icon1 = ztd::strdup(set->icon);
            icon2 = ztd::strdup(book_icon_set_cached->icon);
            icon3 = ztd::strdup("user-bookmarks");
        }
        else if (set->icon)
        {
            icon1 = ztd::strdup(set->icon);
            icon2 = ztd::strdup("user-bookmarks");
            icon3 = ztd::strdup("gnome-fs-directory");
        }
        else
        {
            icon1 = ztd::strdup("user-bookmarks");
            icon2 = ztd::strdup("gnome-fs-directory");
            icon3 = ztd::strdup("gtk-directory");
        }
        if (icon1)
            icon_new = vfs_load_icon(icon1, icon_size);
        if (!icon_new && icon2)
            icon_new = vfs_load_icon(icon2, icon_size);
        if (!icon_new && icon3)
            icon_new = vfs_load_icon(icon3, icon_size);
    }
    else
        LOG_WARN("xset_custom_get_bookmark_icon set is not XSetCMD::BOOKMARK");
    return icon_new;
}

GtkWidget*
xset_add_menuitem(PtkFileBrowser* file_browser, GtkWidget* menu, GtkAccelGroup* accel_group,
                  XSet* set)
{
    GtkWidget* item = nullptr;
    GtkWidget* submenu;
    XSet* keyset;
    XSet* set_next;
    char* icon_name = nullptr;
    char* context = nullptr;
    int context_action = ItemPropContextState::CONTEXT_SHOW;
    XSet* mset;
    char* icon_file = nullptr;
    // LOG_INFO("xset_add_menuitem {}", set->name);

    // plugin?
    mset = xset_get_plugin_mirror(set);
    if (set->plugin && set->shared_key)
    {
        icon_name = mset->icon;
        context = mset->context;
    }
    if (!icon_name)
        icon_name = set->icon;
    if (!icon_name)
    {
        if (set->plugin)
            icon_file = g_build_filename(set->plug_dir, set->plug_name, "icon", nullptr);
        else
            icon_file =
                g_build_filename(xset_get_config_dir(), "scripts", set->name, "icon", nullptr);
        if (!std::filesystem::exists(icon_file))
        {
            free(icon_file);
            icon_file = nullptr;
        }
        else
            icon_name = icon_file;
    }
    if (!context)
        context = set->context;

    // context?
    if (context && set->tool == XSetTool::NOT && xset_context && xset_context->valid &&
        !xset_get_b("context_dlg"))
        context_action = xset_context_test(xset_context, context, set->disable);

    if (context_action != ItemPropContextState::CONTEXT_HIDE)
    {
        if (set->tool != XSetTool::NOT && set->menu_style != XSetMenu::SUBMENU)
        {
            // item = xset_new_menuitem( set->menu_label, icon_name );
        }
        else if (set->menu_style != XSetMenu::NORMAL)
        {
            XSet* set_radio;
            switch (set->menu_style)
            {
                case XSetMenu::CHECK:
                    if (!(!set->lock &&
                          (XSetCMD(xset_get_int_set(set, XSetSetSet::X)) > XSetCMD::SCRIPT)))
                    {
                        item = gtk_check_menu_item_new_with_mnemonic(set->menu_label);
                        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),
                                                       mset->b == XSetB::XSET_B_TRUE);
                    }
                    break;
                case XSetMenu::RADIO:
                    if (set->ob2_data)
                        set_radio = XSET(set->ob2_data);
                    else
                        set_radio = set;
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
                        xset_add_menu(file_browser, submenu, accel_group, set->desc);
                    else if (set->child)
                    {
                        set_next = xset_get(set->child);
                        xset_add_menuitem(file_browser, submenu, accel_group, set_next);
                        GList* l = gtk_container_get_children(GTK_CONTAINER(submenu));
                        if (l)
                            g_list_free(l);
                        else
                        {
                            // Nothing was added to the menu (all items likely have
                            // invisible context) so destroy (hide) - issue #215
                            gtk_widget_destroy(item);
                            if (icon_file)
                                free(icon_file);

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
                case XSetMenu::COLORDLG:
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
            int icon_w;
            int icon_h;
            gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &icon_w, &icon_h);
            int icon_size = icon_w > icon_h ? icon_w : icon_h;

            GdkPixbuf* app_icon = nullptr;
            XSetCMD cmd_type = XSetCMD(xset_get_int_set(set, XSetSetSet::X));
            if (!set->lock && cmd_type == XSetCMD::APP)
            {
                // Application
                char* menu_label = xset_custom_get_app_name_icon(set, &app_icon, icon_size);
                item = xset_new_menuitem(menu_label, nullptr);
                free(menu_label);
            }
            else if (!set->lock && cmd_type == XSetCMD::BOOKMARK)
            {
                // Bookmark
                item = xset_new_menuitem(set->menu_label && set->menu_label[0] ? set->menu_label
                                                                               : set->z,
                                         nullptr);
                app_icon = xset_custom_get_bookmark_icon(set, icon_size);
            }
            else
                item = xset_new_menuitem(set->menu_label, icon_name);

            if (app_icon)
            {
                g_object_unref(app_icon);
            }
        }

        set->browser = file_browser;
        g_object_set_data(G_OBJECT(item), "menu", menu);
        g_object_set_data(G_OBJECT(item), "set", set);

        if (set->ob1)
            g_object_set_data(G_OBJECT(item), set->ob1, set->ob1_data);
        if (set->menu_style != XSetMenu::RADIO && set->ob2)
            g_object_set_data(G_OBJECT(item), set->ob2, set->ob2_data);

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
                keyset = xset_get(set->shared_key);
            else
                keyset = set;
            if (keyset->key > 0 && accel_group)
                gtk_widget_add_accelerator(item,
                                           "activate",
                                           accel_group,
                                           keyset->key,
                                           (GdkModifierType)keyset->keymod,
                                           GTK_ACCEL_VISIBLE);
        }
        // design mode callback
        g_signal_connect(item, "button-press-event", G_CALLBACK(xset_design_cb), set);
        g_signal_connect(item, "button-release-event", G_CALLBACK(xset_design_cb), set);

        gtk_widget_set_sensitive(item,
                                 context_action != ItemPropContextState::CONTEXT_DISABLE &&
                                     !set->disable);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }

    if (icon_file)
        free(icon_file);

    // next item
    if (set->next)
    {
        set_next = xset_get(set->next);
        xset_add_menuitem(file_browser, menu, accel_group, set_next);
    }
    return item;
}

char*
xset_custom_get_script(XSet* set, bool create)
{
    char* path;

    if ((strncmp(set->name, "cstm_", 5) && strncmp(set->name, "cust", 4) &&
         strncmp(set->name, "hand", 4)) ||
        (create && set->plugin))
        return nullptr;

    if (create)
    {
        path = g_build_filename(xset_get_config_dir(), "scripts", set->name, nullptr);
        if (!std::filesystem::exists(path))
        {
            std::filesystem::create_directories(path);
            std::filesystem::permissions(path, std::filesystem::perms::owner_all);
        }
        free(path);
    }

    if (set->plugin)
    {
        path = g_build_filename(set->plug_dir, set->plug_name, "exec.sh", nullptr);
    }
    else
    {
        path = g_build_filename(xset_get_config_dir(), "scripts", set->name, "exec.sh", nullptr);
    }

    if (create && !std::filesystem::exists(path))
    {
        std::ofstream file(path);
        if (file.is_open())
        {
            file << fmt::format("#!{}\n", BASHPATH);
            file << fmt::format("{}\n\n", SHELL_SETTINGS);
            file << fmt::format("#import file manager variables\n");
            file << fmt::format("$fm_import\n\n");
            file << fmt::format("#For all spacefm variables see man page: spacefm-scripts\n\n");
            file << fmt::format("#Start script\n");
            file << fmt::format("\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
            file << fmt::format("#End script\n");
            file << fmt::format("exit $?\n");
        }
        file.close();

        chmod(path, 0700);
    }
    return path;
}

static const std::string
xset_custom_new_name()
{
    std::string setname;

    while (true)
    {
        setname = fmt::format("cstm_{}", randhex8());
        if (!xset_is(setname))
        {
            std::string path1 = Glib::build_filename(xset_get_config_dir(), "scripts", setname);
            std::string path2 = Glib::build_filename(xset_get_config_dir(), "plugin-data", setname);

            // only use free xset name if no aux data dirs exist for that name too.
            if (!std::filesystem::exists(path1) && !std::filesystem::exists(path2))
                break;
        }
    };

    return setname;
}

static void
xset_custom_copy_files(XSet* src, XSet* dest)
{
    std::string path_src;
    std::string path_dest;
    std::string command;

    std::string* standard_output = nullptr;
    std::string* standard_error = nullptr;
    int exit_status;

    std::string msg;

    // LOG_INFO("xset_custom_copy_files( {}, {} )", src->name, dest->name);

    // copy command dir

    if (src->plugin)
        path_src = Glib::build_filename(src->plug_dir, src->plug_name);
    else
        path_src = Glib::build_filename(xset_get_config_dir(), "scripts", src->name);
    // LOG_INFO("    path_src={}", path_src);

    // LOG_INFO("    path_src EXISTS");
    path_dest = Glib::build_filename(xset_get_config_dir(), "scripts");
    std::filesystem::create_directories(path_dest);
    std::filesystem::permissions(path_dest, std::filesystem::perms::owner_all);
    path_dest = Glib::build_filename(xset_get_config_dir(), "scripts", dest->name);
    command = fmt::format("cp -a {} {}", path_src, path_dest);

    // LOG_INFO("    path_dest={}", path_dest );
    print_command(command);
    Glib::spawn_command_line_sync(command, standard_output, standard_error, &exit_status);
    LOG_INFO("{}{}", *standard_output, *standard_error);
    if (exit_status && WIFEXITED(exit_status))
    {
        msg = fmt::format("An error occured copying command files\n\n{}", *standard_error);
        xset_msg_dialog(nullptr, GTK_MESSAGE_ERROR, "Copy Command Error", GTK_BUTTONS_OK, msg);
    }
    command = fmt::format("chmod -R go-rwx {}", path_dest);
    print_command(command);
    Glib::spawn_command_line_sync(command);

    // copy data dir
    XSet* mset = xset_get_plugin_mirror(src);
    path_src = Glib::build_filename(xset_get_config_dir(), "plugin-data", mset->name);
    if (std::filesystem::is_directory(path_src))
    {
        path_dest = Glib::build_filename(xset_get_config_dir(), "plugin-data", dest->name);
        command = fmt::format("cp -a {} {}", path_src, path_dest);
        print_command(command);
        Glib::spawn_command_line_sync(command, standard_output, standard_error, &exit_status);
        LOG_INFO("{}{}", *standard_output, *standard_error);
        if (exit_status && WIFEXITED(exit_status))
        {
            msg = fmt::format("An error occured copying command data files\n\n{}", *standard_error);
            xset_msg_dialog(nullptr, GTK_MESSAGE_ERROR, "Copy Command Error", GTK_BUTTONS_OK, msg);
        }
        command = fmt::format("chmod -R go-rwx {}", path_dest);
        print_command(command);
        Glib::spawn_command_line_sync(command);
    }
}

static XSet*
xset_custom_copy(XSet* set, bool copy_next, bool delete_set)
{
    // LOG_INFO("xset_custom_copy( {}, {}, {})", set->name, copy_next ? "true" : "false",
    // delete_set ? "true" : "false");
    XSet* mset = set;
    // if a plugin with a mirror, get the mirror
    if (set->plugin && set->shared_key)
        mset = xset_get_plugin_mirror(set);

    XSet* newset = xset_custom_new();
    newset->menu_label = ztd::strdup(set->menu_label);
    newset->s = ztd::strdup(set->s);
    newset->x = ztd::strdup(set->x);
    newset->y = ztd::strdup(set->y);
    newset->z = ztd::strdup(set->z);
    newset->desc = ztd::strdup(set->desc);
    newset->title = ztd::strdup(set->title);
    newset->b = set->b;
    newset->menu_style = set->menu_style;
    newset->context = ztd::strdup(mset->context);
    newset->line = ztd::strdup(set->line);

    newset->task = mset->task;
    newset->task_pop = mset->task_pop;
    newset->task_err = mset->task_err;
    newset->task_out = mset->task_out;
    newset->in_terminal = mset->in_terminal;
    newset->keep_terminal = mset->keep_terminal;
    newset->scroll_lock = mset->scroll_lock;

    if (!mset->icon && set->plugin)
        newset->icon = ztd::strdup(set->icon);
    else
        newset->icon = ztd::strdup(mset->icon);

    xset_custom_copy_files(set, newset);
    newset->tool = set->tool;

    if (set->menu_style == XSetMenu::SUBMENU && set->child)
    {
        XSet* set_child = xset_get(set->child);
        // LOG_INFO("    copy submenu {}", set_child->name);
        XSet* newchild = xset_custom_copy(set_child, true, delete_set);
        newset->child = ztd::strdup(newchild->name);
        newchild->parent = ztd::strdup(newset->name);
    }

    if (copy_next && set->next)
    {
        XSet* set_next = xset_get(set->next);
        // LOG_INFO("    copy next {}", set_next->name);
        XSet* newnext = xset_custom_copy(set_next, true, delete_set);
        newnext->prev = ztd::strdup(newset->name);
        newset->next = ztd::strdup(newnext->name);
    }

    // when copying imported plugin file, discard mirror xset
    if (delete_set)
        xset_custom_delete(set, false);

    return newset;
}

void
clean_plugin_mirrors()
{ // remove plugin mirrors for non-existent plugins
    bool redo = true;

    while (redo)
    {
        redo = false;
        for (XSet* set: xsets)
        {
            if (set->desc && !strcmp(set->desc, "@plugin@mirror@"))
            {
                if (!set->shared_key || !xset_is(set->shared_key))
                {
                    xset_remove(set);
                    redo = true;
                    break;
                }
            }
        }
    }

    // remove plugin-data for non-existent xsets
    std::string path = Glib::build_filename(xset_get_config_dir(), "plugin-data");
    if (std::filesystem::is_directory(path))
    {
        std::string file_name;
        for (const auto& file: std::filesystem::directory_iterator(path))
        {
            file_name = std::filesystem::path(file).filename();
            if (Glib::str_has_prefix(file_name, "cstm_") && !xset_is(file_name))
            {
                std::string plugin_path = fmt::format("{}/{}", path, file_name);
                std::filesystem::remove_all(plugin_path);
                LOG_INFO("Removed {}/{}", path, file_name);
            }
        }
    }
}

static void
xset_set_plugin_mirror(XSet* pset)
{
    for (XSet* set: xsets)
    {
        if (set->desc && !strcmp(set->desc, "@plugin@mirror@"))
        {
            if (set->parent && set->child)
            {
                if (!strcmp(set->child, pset->plug_name) && !strcmp(set->parent, pset->plug_dir))
                {
                    if (set->shared_key)
                        free(set->shared_key);
                    set->shared_key = ztd::strdup(pset->name);
                    if (pset->shared_key)
                        free(pset->shared_key);
                    pset->shared_key = ztd::strdup(set->name);
                    return;
                }
            }
        }
    }
}

XSet*
xset_get_plugin_mirror(XSet* set)
{
    // plugin mirrors are custom xsets that save the user's key, icon
    // and run prefs for the plugin, if any
    if (!set->plugin)
        return set;
    if (set->shared_key)
        return xset_get(set->shared_key);

    XSet* newset = xset_custom_new();
    newset->desc = ztd::strdup("@plugin@mirror@");
    newset->parent = ztd::strdup(set->plug_dir);
    newset->child = ztd::strdup(set->plug_name);
    newset->shared_key = ztd::strdup(set->name); // this will not be saved
    newset->task = set->task;
    newset->task_pop = set->task_pop;
    newset->task_err = set->task_err;
    newset->task_out = set->task_out;
    newset->in_terminal = set->in_terminal;
    newset->keep_terminal = set->keep_terminal;
    newset->scroll_lock = set->scroll_lock;
    newset->context = ztd::strdup(set->context);
    newset->opener = set->opener;
    newset->b = set->b;
    newset->s = ztd::strdup(set->s);
    set->shared_key = ztd::strdup(newset->name);
    return newset;
}

static int
compare_plugin_sets(XSet* a, XSet* b)
{
    return g_utf8_collate(a->menu_label, b->menu_label);
}

std::vector<XSet*>
xset_get_plugins()
{ // return list of plugin sets sorted by menu_label
    std::vector<XSet*> plugins;

    for (XSet* set: xsets)
    {
        if (set->plugin && set->plugin_top && set->plug_dir)
            plugins.push_back(set);
    }
    sort(plugins.begin(), plugins.end(), compare_plugin_sets);
    return plugins;
}

void
xset_clear_plugins(std::vector<XSet*>& plugins)
{
    if (!plugins.empty())
    {
        for (XSet* set: plugins)
            xset_remove(set);
        plugins.clear();
    }
}

static XSet*
xset_get_by_plug_name(const char* plug_dir, const char* plug_name)
{
    if (!plug_name)
        return nullptr;

    for (XSet* set: xsets)
    {
        if (set->plugin && !strcmp(plug_name, set->plug_name) && !strcmp(plug_dir, set->plug_dir))
            return set;
    }

    // add new
    std::string setname = xset_custom_new_name();

    XSet* set = xset_new(setname);
    set->plug_dir = ztd::strdup(plug_dir);
    set->plug_name = ztd::strdup(plug_name);
    set->plugin = true;
    set->lock = false;
    xsets.push_back(set);
    return set;
}

static void
xset_parse_plugin(const char* plug_dir, const std::string& line, PluginUse use)
{
    std::size_t sep = line.find("=");
    if (sep == std::string::npos)
        return;

    std::size_t sep2 = line.find("-");
    if (sep2 == std::string::npos)
        return;

    std::string token = line.substr(0, sep2);
    std::string value = line.substr(sep + 1, std::string::npos - 1);
    std::string token_var = line.substr(sep2 + 1, sep - sep2 - 1);

    if (value.empty())
        return;

    const char* name = line.c_str();
    XSet* set;
    XSet* set2;

    // handler
    std::string prefix;
    switch (use)
    {
        case PluginUse::HAND_ARC:
            prefix = "hand_arc_";
            break;
        case PluginUse::HAND_FS:
            prefix = "hand_fs_";
            break;
        case PluginUse::HAND_NET:
            prefix = "hand_net_";
            break;
        case PluginUse::HAND_FILE:
            prefix = "hand_f_";
            break;
        case PluginUse::BOOKMARKS:
        case PluginUse::NORMAL:
        default:
            prefix = "cstm_";
            break;
    }

    if (!ztd::startswith(name, prefix))
        return;

    XSetSetSet var;
    try
    {
        var = xset_set_set_encode(token_var);
    }
    catch (const std::logic_error& e)
    {
        std::string msg = fmt::format("Plugin load error:\n\"{}\"\n{}", plug_dir, e.what());
        ptk_show_error(nullptr, "Error", e.what());
        return;
    }
    set = xset_get_by_plug_name(plug_dir, name);
    xset_set_set(set, var, value.c_str());

    if (use >= PluginUse::BOOKMARKS)
    {
        // map plug names to new set names (does not apply to handlers)
        if (set->prev && ztd::same(token_var, "prev"))
        {
            if (ztd::startswith(set->prev, "cstm_"))
            {
                set2 = xset_get_by_plug_name(plug_dir, set->prev);
                free(set->prev);
                set->prev = ztd::strdup(set2->name);
            }
            else
            {
                free(set->prev);
                set->prev = nullptr;
            }
        }
        else if (set->next && ztd::same(token_var, "next"))
        {
            if (ztd::startswith(set->next, "cstm_"))
            {
                set2 = xset_get_by_plug_name(plug_dir, set->next);
                free(set->next);
                set->next = ztd::strdup(set2->name);
            }
            else
            {
                free(set->next);
                set->next = nullptr;
            }
        }
        else if (set->parent && ztd::same(token_var, "parent"))
        {
            if (ztd::startswith(set->parent, "cstm_"))
            {
                set2 = xset_get_by_plug_name(plug_dir, set->parent);
                free(set->parent);
                set->parent = ztd::strdup(set2->name);
            }
            else
            {
                free(set->parent);
                set->parent = nullptr;
            }
        }
        else if (set->child && ztd::same(token_var, "child"))
        {
            if (ztd::startswith(set->child, "cstm_"))
            {
                set2 = xset_get_by_plug_name(plug_dir, set->child);
                free(set->child);
                set->child = ztd::strdup(set2->name);
            }
            else
            {
                free(set->child);
                set->child = nullptr;
            }
        }
    }
}

XSet*
xset_import_plugin(const char* plug_dir, PluginUse* use)
{
    bool func;

    if (use)
        *use = PluginUse::NORMAL;

    // clear all existing plugin sets with this plug_dir
    // ( keep the mirrors to retain user prefs )
    bool redo = true;
    while (redo)
    {
        redo = false;
        for (XSet* set: xsets)
        {
            if (set->plugin && !strcmp(plug_dir, set->plug_dir))
            {
                xset_remove(set);
                redo = true; // search list from start again due to changed list
                break;
            }
        }
    }

    // read plugin file into xsets
    bool plugin_good = false;
    std::string plugin = Glib::build_filename(plug_dir, "plugin");

    std::string line;
    std::ifstream file(plugin);
    if (!file.is_open())
    {
        LOG_WARN("Error reading plugin file {}", plugin);
        return nullptr;
    }

    if (file.is_open())
    {
        while (std::getline(file, line))
        {
            if (line.empty())
                continue;

            if (line.at(0) == '[')
            {
                if (ztd::same(line, "[Plugin]"))
                    func = true;
                else
                    func = false;
                continue;
            }

            if (func)
            {
                std::size_t sep = line.find("=");
                if (sep == std::string::npos)
                    continue;

                std::string token = line.substr(0, sep);
                std::string value = line.substr(sep + 1, std::string::npos - 1);

                if (use && *use == PluginUse::NORMAL)
                {
                    if (ztd::same(token, "main_book-child"))
                    {
                        // This plugin is an export of all bookmarks
                        *use = PluginUse::BOOKMARKS;
                    }
                    else if (token.substr(0, 5) == "hand_")
                    {
                        if (token.substr(0, 8) == "hand_fs_")
                            *use = PluginUse::HAND_FS;
                        else if (token.substr(0, 9) == "hand_arc_")
                            *use = PluginUse::HAND_ARC;
                        else if (token.substr(0, 9) == "hand_net_")
                            *use = PluginUse::HAND_NET;
                        else if (token.substr(0, 7) == "hand_f_")
                            *use = PluginUse::HAND_FILE;
                    }
                }
                xset_parse_plugin(plug_dir, token, use ? *use : PluginUse::NORMAL);
                if (!plugin_good)
                    plugin_good = true;
            }
        }
    }

    file.close();

    // clean plugin sets, set type
    bool top = true;
    XSet* rset = nullptr;
    for (XSet* set: xsets)
    {
        if (set->plugin && !strcmp(plug_dir, set->plug_dir))
        {
            set->key = 0;
            set->keymod = 0;
            set->tool = XSetTool::NOT;
            set->opener = 0;
            xset_set_plugin_mirror(set);
            if ((set->plugin_top = top))
            {
                top = false;
                rset = set;
            }
        }
    }
    return plugin_good ? rset : nullptr;
}

struct PluginData
{
    FMMainWindow* main_window;
    GtkWidget* handler_dlg;
    char* plug_dir;
    XSet* set;
    PluginJob job;
};

static void
on_install_plugin_cb(VFSFileTask* task, PluginData* plugin_data)
{
    (void)task;
    XSet* set;
    std::string msg;
    // LOG_INFO("on_install_plugin_cb");
    if (plugin_data->job == PluginJob::REMOVE) // uninstall
    {
        if (!std::filesystem::exists(plugin_data->plug_dir))
        {
            xset_custom_delete(plugin_data->set, false);
            clean_plugin_mirrors();
        }
    }
    else
    {
        char* plugin = g_build_filename(plugin_data->plug_dir, "plugin", nullptr);
        if (std::filesystem::exists(plugin))
        {
            PluginUse use = PluginUse::NORMAL;
            set = xset_import_plugin(plugin_data->plug_dir, &use);
            if (!set)
            {
                msg = fmt::format(
                    "The imported plugin directory does not contain a valid plugin.\n\n({}/)",
                    plugin_data->plug_dir);
                xset_msg_dialog(GTK_WIDGET(plugin_data->main_window),
                                GTK_MESSAGE_ERROR,
                                "Invalid Plugin",
                                GTK_BUTTONS_OK,
                                msg);
            }
            // TODO - switch
            else if (use == PluginUse::BOOKMARKS)
            {
                // bookmarks
                if (plugin_data->job != PluginJob::COPY || !plugin_data->set)
                {
                    // This dialog should never be seen - failsafe
                    xset_msg_dialog(
                        GTK_WIDGET(plugin_data->main_window),
                        GTK_MESSAGE_ERROR,
                        "Bookmarks",
                        GTK_BUTTONS_OK,
                        "This plugin file contains exported bookmarks which cannot be installed or "
                        "imported to the design clipboard.\n\nYou can import these directly into a "
                        "menu (select New|Import from the Design Menu).");
                }
                else
                {
                    // copy all bookmarks into menu
                    // paste after insert_set (plugin_data->set)
                    XSet* newset = xset_custom_copy(set, true, true);
                    // get last bookmark and toolbar if needed
                    set = newset;
                    do
                    {
                        if (plugin_data->set->tool != XSetTool::NOT)
                            set->tool = XSetTool::CUSTOM;
                        else
                            set->tool = XSetTool::NOT;
                        if (!set->next)
                            break;
                    } while ((set = xset_get(set->next)));
                    // set now points to last bookmark
                    newset->prev = ztd::strdup(plugin_data->set->name);
                    set->next = plugin_data->set->next; // steal
                    if (plugin_data->set->next)
                    {
                        XSet* set_next = xset_get(plugin_data->set->next);
                        free(set_next->prev);
                        set_next->prev = ztd::strdup(set->name); // last bookmark
                    }
                    plugin_data->set->next = ztd::strdup(newset->name);
                    // find parent
                    set = newset;
                    while (set->prev)
                        set = xset_get(set->prev);
                    if (set->parent)
                        main_window_bookmark_changed(set->parent);
                }
            }
            else if (use < PluginUse::BOOKMARKS)
            {
                // handler
                set->plugin_top = false; // prevent being added to Plugins menu
                if (plugin_data->job == PluginJob::INSTALL)
                {
                    // This dialog should never be seen - failsafe
                    xset_msg_dialog(plugin_data->main_window ? GTK_WIDGET(plugin_data->main_window)
                                                             : nullptr,
                                    GTK_MESSAGE_ERROR,
                                    "Handler Plugin",
                                    GTK_BUTTONS_OK,
                                    "This file contains a handler plugin which cannot be installed "
                                    "as a plugin.\n\nYou can import handlers from a handler "
                                    "configuration window, or use Plugins|Import.");
                }
                else
                {
                    // TODO
                    ptk_handler_import(static_cast<int>(use), plugin_data->handler_dlg, set);
                }
            }
            else if (plugin_data->job == PluginJob::COPY)
            {
                // copy
                set->plugin_top = false; // do not show tmp plugin in Plugins menu
                if (plugin_data->set)
                {
                    // paste after insert_set (plugin_data->set)
                    XSet* newset = xset_custom_copy(set, false, true);
                    newset->prev = ztd::strdup(plugin_data->set->name);
                    newset->next = plugin_data->set->next; // steal
                    if (plugin_data->set->next)
                    {
                        XSet* set_next = xset_get(plugin_data->set->next);
                        free(set_next->prev);
                        set_next->prev = ztd::strdup(newset->name);
                    }
                    plugin_data->set->next = ztd::strdup(newset->name);
                    if (plugin_data->set->tool != XSetTool::NOT)
                        newset->tool = XSetTool::CUSTOM;
                    else
                        newset->tool = XSetTool::NOT;
                    main_window_bookmark_changed(newset->name);
                }
                else
                {
                    // place on design clipboard
                    set_clipboard = set;
                    clipboard_is_cut = false;
                    if (xset_get_b("plug_cverb") || plugin_data->handler_dlg)
                    {
                        std::string label = clean_label(set->menu_label, false, false);
                        if (geteuid() == 0)
                            msg = fmt::format(
                                "The '{}' plugin has been copied to the design clipboard.  Use "
                                "View|Design Mode to paste it into a menu.\n\nBecause it has not "
                                "been installed, this plugin will not appear in the Plugins "
                                "menu.",
                                label);
                        else
                            msg = fmt::format(
                                "The '{}' plugin has been copied to the design clipboard.  Use "
                                "View|Design Mode to paste it into a menu.\n\nBecause it has not "
                                "been installed, this plugin will not appear in the Plugins "
                                "menu, and its contents are not protected by root (once pasted "
                                "it will be saved with normal ownership).\n\nIf this plugin "
                                "contains su commands or will be run as root, installing it to "
                                "and running it only from the Plugins menu is recommended to "
                                "improve your system security.",
                                label);
                        xset_msg_dialog(GTK_WIDGET(plugin_data->main_window),
                                        GTK_MESSAGE_INFO,
                                        "Copy Plugin",
                                        GTK_BUTTONS_OK,
                                        msg);
                    }
                }
            }
            clean_plugin_mirrors();
        }
        free(plugin);
    }
    free(plugin_data->plug_dir);
    g_slice_free(PluginData, plugin_data);
}

static void
xset_remove_plugin(GtkWidget* parent, PtkFileBrowser* file_browser, XSet* set)
{
    if (!file_browser || !set || !set->plugin_top || !set->plug_dir)
        return;

    if (!app_settings.no_confirm)
    {
        std::string msg;
        std::string label = clean_label(set->menu_label, false, false);
        msg = fmt::format("Uninstall the '{}' plugin?\n\n( {} )", label, set->plug_dir);
        if (xset_msg_dialog(parent,
                            GTK_MESSAGE_WARNING,
                            "Uninstall Plugin",
                            GTK_BUTTONS_YES_NO,
                            msg) != GTK_RESPONSE_YES)
        {
            return;
        }
    }
    PtkFileTask* ptask =
        ptk_file_exec_new("Uninstall Plugin", nullptr, parent, file_browser->task_view);

    const std::string plug_dir_q = bash_quote(set->plug_dir);

    ptask->task->exec_command = fmt::format("rm -rf {}", plug_dir_q);
    ptask->task->exec_sync = true;
    ptask->task->exec_popup = false;
    ptask->task->exec_show_output = false;
    ptask->task->exec_show_error = true;
    ptask->task->exec_export = false;
    ptask->task->exec_as_user = "root";

    PluginData* plugin_data = g_slice_new0(PluginData);
    plugin_data->main_window = nullptr;
    plugin_data->plug_dir = ztd::strdup(set->plug_dir);
    plugin_data->set = set;
    plugin_data->job = PluginJob::REMOVE;
    ptask->complete_notify = (GFunc)on_install_plugin_cb;
    ptask->user_data = plugin_data;

    ptk_file_task_run(ptask);
}

void
install_plugin_file(void* main_win, GtkWidget* handler_dlg, const std::string& path,
                    const std::string& plug_dir, PluginJob job, XSet* insert_set)
{
    std::string own;
    std::string plug_dir_q = bash_quote(plug_dir);
    std::string file_path_q = bash_quote(path);

    FMMainWindow* main_window = static_cast<FMMainWindow*>(main_win);
    // task
    PtkFileTask* ptask;
    ptask = ptk_file_exec_new("Install Plugin",
                              nullptr,
                              main_win ? GTK_WIDGET(main_window) : nullptr,
                              main_win ? main_window->task_view : nullptr);

    switch (job)
    {
        case PluginJob::INSTALL:
            // install
            own =
                fmt::format("chown -R root:root {} && chmod -R go+rX-w {}", plug_dir_q, plug_dir_q);
            ptask->task->exec_as_user = "root";
            break;
        case PluginJob::COPY:
            // copy to clipboard or import to menu
            own = fmt::format("chmod -R go+rX-w {}", plug_dir_q);
            break;
        case PluginJob::REMOVE:
        default:
            break;
    }

    std::string book = "";
    if (insert_set && !strcmp(insert_set->name, "main_book"))
    {
        // import bookmarks to end
        XSet* set = xset_get("main_book");
        if (set->child)
        {
            set = xset_is(set->child);
            while (set && set->next)
                set = xset_is(set->next);

            if (set)
            {
                insert_set = set;
            }
            else
            {
                // failsafe
                insert_set = nullptr;
            }
        }
        else
        {
            // failsafe
            insert_set = nullptr;
        }
    }
    if (job == PluginJob::INSTALL || !insert_set)
    {
        // prevent install of exported bookmarks or handler as plugin or design clipboard
        if (job == PluginJob::INSTALL)
            book = " || [ -e main_book ] || [ -d hand_* ]";
        else
            book = " || [ -e main_book ]";
    }

    ptask->task->exec_command = fmt::format(
        "rm -rf {} ; mkdir -p {} && cd {} && tar --exclude='/*' --keep-old-files -xf {} ; "
        "err=$? ; if [ $err -ne 0 ] || [ ! -e plugin ] {} ; then rm -rf {} ; echo 'Error "
        "installing "
        "plugin (invalid plugin file?)'; exit 1 ; fi ; {}",
        plug_dir_q,
        plug_dir_q,
        plug_dir_q,
        file_path_q,
        book,
        plug_dir_q,
        own);

    ptask->task->exec_sync = true;
    ptask->task->exec_popup = false;
    ptask->task->exec_show_output = false;
    ptask->task->exec_show_error = true;
    ptask->task->exec_export = false;

    PluginData* plugin_data = g_slice_new0(PluginData);
    plugin_data->main_window = main_window;
    plugin_data->handler_dlg = handler_dlg;
    plugin_data->plug_dir = ztd::strdup(plug_dir);
    plugin_data->job = job;
    plugin_data->set = insert_set;
    ptask->complete_notify = (GFunc)on_install_plugin_cb;
    ptask->user_data = plugin_data;

    ptk_file_task_run(ptask);
}

static bool
xset_custom_export_files(XSet* set, const char* plug_dir)
{
    char* path_src;
    char* path_dest;

    if (set->plugin)
    {
        path_src = g_build_filename(set->plug_dir, set->plug_name, nullptr);
        path_dest = g_build_filename(plug_dir, set->plug_name, nullptr);
    }
    else
    {
        path_src = g_build_filename(xset_get_config_dir(), "scripts", set->name, nullptr);
        path_dest = g_build_filename(plug_dir, set->name, nullptr);
    }

    if (!(std::filesystem::exists(path_src) && dir_has_files(path_src)))
    {
        if (!strcmp(set->name, "main_book"))
        {
            // exporting all bookmarks - create empty main_book dir
            std::filesystem::create_directories(path_dest);
            std::filesystem::permissions(path_dest, std::filesystem::perms::owner_all);
            if (!std::filesystem::exists(path_dest))
            {
                free(path_src);
                free(path_dest);
                return false;
            }
        }
        // skip empty or missing dirs
        free(path_src);
        free(path_dest);
        return true;
    }

    int exit_status;
    std::string command = fmt::format("cp -a {} {}", path_src, path_dest);
    free(path_src);
    free(path_dest);
    print_command(command);
    Glib::spawn_command_line_sync(command, nullptr, nullptr, &exit_status);

    return !!exit_status;
}

static bool
xset_custom_export_write(std::string& buf, XSet* set, const char* plug_dir)
{ // recursively write set, submenu sets, and next sets
    xset_write_set(buf, set);
    if (!xset_custom_export_files(set, plug_dir))
        return false;
    if (set->menu_style == XSetMenu::SUBMENU && set->child)
    {
        if (!xset_custom_export_write(buf, xset_get(set->child), plug_dir))
            return false;
    }
    if (set->next)
    {
        if (!xset_custom_export_write(buf, xset_get(set->next), plug_dir))
            return false;
    }
    return true;
}

void
xset_custom_export(GtkWidget* parent, PtkFileBrowser* file_browser, XSet* set)
{
    const char* deffolder;
    std::string deffile;

    std::string plug_dir_q;
    std::string path_q;

    // get new plugin filename
    XSet* save = xset_get("plug_cfile");
    if (save->s) //&& std::filesystem::is_directory(save->s)
        deffolder = save->s;
    else
    {
        if (!(deffolder = xset_get_s("go_set_default")))
            deffolder = ztd::strdup("/");
    }

    if (!set->plugin)
    {
        std::string s1 = clean_label(set->menu_label, true, false);
        std::string type;
        if (!strcmp(set->name, "main_book"))
            type = "bookmarks";
        else if (Glib::str_has_prefix(set->name, "hand_arc_"))
            type = "archive-handler";
        else if (Glib::str_has_prefix(set->name, "hand_fs_"))
            type = "device-handler";
        else if (Glib::str_has_prefix(set->name, "hand_net_"))
            type = "protocol-handler";
        else if (Glib::str_has_prefix(set->name, "hand_f_"))
            type = "file-handler";
        else
            type = "plugin";

        deffile = fmt::format("{}-{}-{}.tar.xz", s1, PACKAGE_NAME, type);
    }
    else
    {
        std::string s1 = g_path_get_basename(set->plug_dir);
        deffile = fmt::format("{}-{}-plugin.tar.xz", s1, PACKAGE_NAME);
    }

    char* path = xset_file_dialog(parent,
                                  GTK_FILE_CHOOSER_ACTION_SAVE,
                                  "Save As Plugin File",
                                  deffolder,
                                  deffile.c_str());
    if (!path)
        return;
    if (save->s)
        free(save->s);
    save->s = g_path_get_dirname(path);

    // get or create tmp plugin dir
    std::string plug_dir;
    if (!set->plugin)
    {
        char* s1 = (char*)xset_get_user_tmp_dir();
        if (!s1)
        {
            free(path);
            xset_msg_dialog(parent,
                            GTK_MESSAGE_ERROR,
                            "Export Error",
                            GTK_BUTTONS_OK,
                            "Unable to create temporary files");
            return;
        }
        while (true)
        {
            plug_dir = Glib::build_filename(s1, randhex8());
            if (!std::filesystem::exists(plug_dir))
                break;
        }
        std::filesystem::create_directories(plug_dir);
        std::filesystem::permissions(plug_dir, std::filesystem::perms::owner_all);

        // Create plugin file
        std::string plugin_path = Glib::build_filename(plug_dir, "plugin");

        std::string buf = "";

        buf.append("[Plugin]\n");
        xset_write_set(buf, xset_get("config_version"));

        char* s_prev = set->prev;
        char* s_next = set->next;
        char* s_parent = set->parent;
        set->prev = set->next = set->parent = nullptr;
        xset_write_set(buf, set);
        set->prev = s_prev;
        set->next = s_next;
        set->parent = s_parent;

        if (!xset_custom_export_files(set, plug_dir.c_str()))
        {
            if (!set->plugin)
            {
                std::filesystem::remove_all(plug_dir);
                LOG_INFO("Removed {}", plug_dir);
            }
            free(path);
            xset_msg_dialog(parent,
                            GTK_MESSAGE_ERROR,
                            "Export Error",
                            GTK_BUTTONS_OK,
                            "Unable to create temporary files");
            return;
        }
        if (set->menu_style == XSetMenu::SUBMENU && set->child)
        {
            if (!xset_custom_export_write(buf, xset_get(set->child), plug_dir.c_str()))
            {
                if (!set->plugin)
                {
                    std::filesystem::remove_all(plug_dir);
                    LOG_INFO("Removed {}", plug_dir);
                }
                free(path);
                xset_msg_dialog(parent,
                                GTK_MESSAGE_ERROR,
                                "Export Error",
                                GTK_BUTTONS_OK,
                                "Unable to create temporary files");
                return;
            }
        }
        buf.append("\n");

        std::ofstream file(path);
        if (file.is_open())
            file << buf;
        file.close();
    }
    else
        plug_dir = ztd::strdup(set->plug_dir);

    // tar and delete tmp files
    // task
    PtkFileTask* ptask;
    ptask = ptk_file_exec_new("Export Plugin",
                              plug_dir.c_str(),
                              parent,
                              file_browser ? file_browser->task_view : nullptr);

    plug_dir_q = bash_quote(plug_dir);
    path_q = bash_quote(path);
    if (!set->plugin)
        ptask->task->exec_command =
            fmt::format("tar --numeric-owner -cJf {} * ; err=$? ; rm -rf {} ; "
                        "if [ $err -ne 0 ];then rm -f {} ; fi ; exit $err",
                        path_q,
                        plug_dir_q,
                        path_q);
    else
        ptask->task->exec_command =
            fmt::format("tar --numeric-owner -cJf {} * ; err=$? ; "
                        "if [ $err -ne 0 ] ; then rm -f {} ; fi ; exit $err",
                        path_q,
                        path_q);
    ptask->task->exec_sync = true;
    ptask->task->exec_popup = false;
    ptask->task->exec_show_output = false;
    ptask->task->exec_show_error = true;
    ptask->task->exec_export = false;
    ptask->task->exec_browser = file_browser;
    ptk_file_task_run(ptask);

    free(path);
    return;
}

static void
open_spec(PtkFileBrowser* file_browser, const char* url, bool in_new_tab)
{
    bool new_window = false;
    if (!file_browser)
    {
        FMMainWindow* main_window = fm_main_window_get_on_current_desktop();
        if (!main_window)
        {
            main_window = FM_MAIN_WINDOW(fm_main_window_new());
            gtk_window_set_default_size(GTK_WINDOW(main_window),
                                        app_settings.width,
                                        app_settings.height);
            gtk_widget_show(GTK_WIDGET(main_window));
            new_window = !xset_get_b("main_save_tabs");
        }
        file_browser = PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
        gtk_window_present(GTK_WINDOW(main_window));
    }
    bool new_tab = !new_window && in_new_tab;

    // convert ~ to /home/user for smarter bookmarks
    std::string use_url;
    if (Glib::str_has_prefix(url, "~/") || !g_strcmp0(url, "~"))
        use_url = fmt::format("{}{}", vfs_user_home_dir(), url + 1);
    else
        use_url = url;

    if ((use_url[0] != '/' && strstr(use_url.c_str(), ":/")) || Glib::str_has_prefix(use_url, "//"))
    {
        // network
        if (file_browser)
            ptk_location_view_mount_network(file_browser, use_url.c_str(), new_tab, false);
        else
            open_in_prog(use_url.c_str());
    }
    else if (std::filesystem::is_directory(use_url))
    {
        // dir
        if (file_browser)
        {
            if (new_tab || g_strcmp0(ptk_file_browser_get_cwd(file_browser), use_url.c_str()))
                ptk_file_browser_emit_open(file_browser,
                                           use_url.c_str(),
                                           new_tab ? PtkOpenAction::PTK_OPEN_NEW_TAB
                                                   : PtkOpenAction::PTK_OPEN_DIR);
            gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view));
        }
        else
            open_in_prog(use_url.c_str());
    }
    else if (std::filesystem::exists(use_url))
    {
        // file - open dir and select file
        char* dir = g_path_get_dirname(use_url.c_str());
        if (dir && std::filesystem::is_directory(dir))
        {
            if (file_browser)
            {
                if (!new_tab && !strcmp(dir, ptk_file_browser_get_cwd(file_browser)))
                {
                    ptk_file_browser_select_file(file_browser, use_url.c_str());
                    gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view));
                }
                else
                {
                    file_browser->select_path = ztd::strdup(use_url);
                    ptk_file_browser_emit_open(file_browser,
                                               dir,
                                               new_tab ? PtkOpenAction::PTK_OPEN_NEW_TAB
                                                       : PtkOpenAction::PTK_OPEN_DIR);
                    if (new_tab)
                    {
                        FMMainWindow* main_window_last =
                            static_cast<FMMainWindow*>(file_browser->main_window);
                        file_browser =
                            main_window_last
                                ? PTK_FILE_BROWSER(
                                      fm_main_window_get_current_file_browser(main_window_last))
                                : nullptr;
                        if (file_browser)
                        {
                            // select path in new browser
                            file_browser->select_path = ztd::strdup(use_url);
                            // usually this is not ready but try anyway
                            ptk_file_browser_select_file(file_browser, use_url.c_str());
                        }
                    }
                }
            }
            else
                open_in_prog(dir);
        }
        free(dir);
    }
    else
    {
        std::string msg = fmt::format("Bookmark target '{}' is missing or invalid.", use_url);
        xset_msg_dialog(file_browser ? GTK_WIDGET(file_browser) : nullptr,
                        GTK_MESSAGE_ERROR,
                        "Invalid Bookmark Target",
                        GTK_BUTTONS_OK,
                        msg);
    }
}

static void
xset_custom_activate(GtkWidget* item, XSet* set)
{
    (void)item;
    GtkWidget* parent;
    GtkWidget* task_view = nullptr;
    const char* cwd;
    std::string value;
    XSet* mset;

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
        LOG_WARN("xset_custom_activate !browser !desktop");
        return;
    }

    // name
    if (!set->plugin && !(!set->lock && XSetCMD(xset_get_int_set(set, XSetSetSet::X)) >
                                            XSetCMD::SCRIPT /*app or bookmark*/))
    {
        if (!(set->menu_label && set->menu_label[0]) ||
            (set->menu_label && !strcmp(set->menu_label, "New _Command")))
        {
            if (!xset_text_dialog(parent,
                                  "Change Item Name",
                                  enter_menu_name_new,
                                  "",
                                  set->menu_label,
                                  &set->menu_label,
                                  "",
                                  false))
                return;
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
        case XSetMenu::COLORDLG:
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
    XSetCMD cmd_type = XSetCMD(xset_get_int_set(set, XSetSetSet::X));
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
                return;
            break;
        case XSetCMD::APP:
            // app or executable
            if (!(set->z && set->z[0]))
            {
                xset_item_prop_dlg(xset_context, set, 0);
                return;
            }
            else if (Glib::str_has_suffix(set->z, ".desktop"))
            {
                VFSAppDesktop desktop(set->z);
                if (desktop.get_exec() && desktop.get_exec()[0] != '\0')
                {
                    // get file list
                    GList* sel_files;
                    if (set->browser)
                    {
                        sel_files = ptk_file_browser_get_selected_files(set->browser);
                    }
                    else
                    {
                        sel_files = nullptr;
                        cwd = ztd::strdup("/");
                    }

                    std::vector<std::string> open_files;

                    GList* l;
                    for (l = sel_files; l; l = l->next)
                    {
                        std::string open_file = g_build_filename(
                            cwd,
                            vfs_file_info_get_name(static_cast<VFSFileInfo*>(l->data)),
                            nullptr);

                        open_files.push_back(open_file);
                    }

                    // open in app
                    try
                    {
                        desktop.open_files(cwd, open_files);
                    }
                    catch (const VFSAppDesktopException& e)
                    {
                        ptk_show_error(parent ? GTK_WINDOW(parent) : nullptr, "Error", e.what());
                    }

                    if (sel_files)
                    {
                        g_list_foreach(sel_files, (GFunc)vfs_file_info_unref, nullptr);
                        g_list_free(sel_files);
                    }
                }
                return;
            }
            else
            {
                command = bash_quote(set->z);
                app_no_sync = true;
            }
            break;
        case XSetCMD::BOOKMARK:
            // Bookmark
            if (!(set->z && set->z[0]))
            {
                xset_item_prop_dlg(xset_context, set, 0);
                return;
            }
            char* specs;
            specs = set->z;
            while (specs && (specs[0] == ' ' || specs[0] == ';'))
                specs++;
            if (specs && std::filesystem::exists(specs))
                open_spec(set->browser, specs, xset_get_b("book_newtab"));
            else
            {
                // parse semi-colon separated list
                char* sep;
                char* url;
                while (specs && specs[0])
                {
                    if ((sep = strchr(specs, ';')))
                        sep[0] = '\0';
                    url = ztd::strdup(specs);
                    url = g_strstrip(url);
                    if (url[0])
                        open_spec(set->browser, url, true);
                    free(url);
                    if (sep)
                    {
                        sep[0] = ';';
                        specs = sep + 1;
                    }
                    else
                        specs = nullptr;
                }
            }
            return;
        case XSetCMD::INVALID:
        default:
            return;
    }

    // task
    std::string task_name = clean_label(set->menu_label, false, false);
    PtkFileTask* ptask = ptk_file_exec_new(task_name, cwd, parent, task_view);
    // do not free cwd!
    ptask->task->exec_browser = set->browser;
    ptask->task->exec_command = command;
    ptask->task->exec_set = set;

    if (set->y && set->y[0] != '\0')
        ptask->task->exec_as_user = set->y;

    if (set->plugin && set->shared_key && mset->icon)
        ptask->task->exec_icon = mset->icon;
    else if (set->icon)
        ptask->task->exec_icon = set->icon;

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
xset_custom_delete(XSet* set, bool delete_next)
{
    if (set->menu_style == XSetMenu::SUBMENU && set->child)
    {
        XSet* set_child = xset_get(set->child);
        xset_custom_delete(set_child, true);
    }

    if (delete_next && set->next)
    {
        XSet* set_next = xset_get(set->next);
        xset_custom_delete(set_next, true);
    }

    if (set == set_clipboard)
        set_clipboard = nullptr;

    std::string path1 = Glib::build_filename(xset_get_config_dir(), "scripts", set->name);
    std::string path2 = Glib::build_filename(xset_get_config_dir(), "plugin-data", set->name);
    if (std::filesystem::exists(path1))
    {
        std::filesystem::remove_all(path1);
        LOG_INFO("Removed {}", path1);
    }

    if (std::filesystem::exists(path2))
    {
        std::filesystem::remove_all(path2);
        LOG_INFO("Removed {}", path2);
    }

    xset_remove(set);
}

XSet*
xset_custom_remove(XSet* set)
{
    XSet* set_prev;
    XSet* set_next;
    XSet* set_parent;
    XSet* set_child;

    /*
    LOG_INFO("xset_custom_remove {} ({})", set->name, set->menu_label );
    LOG_INFO("    set->parent = {}", set->parent );
    LOG_INFO("    set->prev   = {}", set->prev );
    LOG_INFO("    set->next   = {}", set->next );
    */
    if (set->prev)
    {
        set_prev = xset_get(set->prev);
        // LOG_INFO("        set->prev = {} ({})", set_prev->name, set_prev->menu_label);
        if (set_prev->next)
            free(set_prev->next);
        if (set->next)
            set_prev->next = ztd::strdup(set->next);
        else
            set_prev->next = nullptr;
    }
    if (set->next)
    {
        set_next = xset_get(set->next);
        if (set_next->prev)
            free(set_next->prev);
        if (set->prev)
            set_next->prev = ztd::strdup(set->prev);
        else
        {
            set_next->prev = nullptr;
            if (set->parent)
            {
                set_parent = xset_get(set->parent);
                if (set_parent->child)
                    free(set_parent->child);
                set_parent->child = ztd::strdup(set_next->name);
                set_next->parent = ztd::strdup(set->parent);
            }
        }
    }
    if (!set->prev && !set->next && set->parent)
    {
        set_parent = xset_get(set->parent);
        if (set->tool != XSetTool::NOT)
            set_child = xset_new_builtin_toolitem(XSetTool::HOME);
        else
        {
            set_child = xset_custom_new();
            set_child->menu_label = ztd::strdup("New _Command");
        }
        if (set_parent->child)
            free(set_parent->child);
        set_parent->child = ztd::strdup(set_child->name);
        set_child->parent = ztd::strdup(set->parent);
        return set_child;
    }
    return nullptr;
}

static void
xset_custom_insert_after(XSet* target, XSet* set)
{ // inserts single set 'set', no next
    XSet* target_next;

    if (!set)
    {
        LOG_WARN("xset_custom_insert_after set == nullptr");
        return;
    }
    if (!target)
    {
        LOG_WARN("xset_custom_insert_after target == nullptr");
        return;
    }

    if (set->parent)
    {
        free(set->parent);
        set->parent = nullptr;
    }

    free(set->prev);
    free(set->next);
    set->prev = ztd::strdup(target->name);
    set->next = target->next; // steal string
    if (target->next)
    {
        target_next = xset_get(target->next);
        if (target_next->prev)
            free(target_next->prev);
        target_next->prev = ztd::strdup(set->name);
    }
    target->next = ztd::strdup(set->name);
    if (target->tool != XSetTool::NOT)
    {
        if (set->tool < XSetTool::CUSTOM)
            set->tool = XSetTool::CUSTOM;
    }
    else
    {
        if (set->tool > XSetTool::CUSTOM)
            LOG_WARN("xset_custom_insert_after builtin tool inserted after non-tool");
        set->tool = XSetTool::NOT;
    }
}

static bool
xset_clipboard_in_set(XSet* set)
{ // look upward to see if clipboard is in set's tree
    if (!set_clipboard || set->lock)
        return false;
    if (set == set_clipboard)
        return true;

    if (set->parent)
    {
        XSet* set_parent = xset_get(set->parent);
        if (xset_clipboard_in_set(set_parent))
            return true;
    }

    if (set->prev)
    {
        XSet* set_prev = xset_get(set->prev);
        while (set_prev)
        {
            if (set_prev->parent)
            {
                XSet* set_prev_parent = xset_get(set_prev->parent);
                if (xset_clipboard_in_set(set_prev_parent))
                    return true;
                set_prev = nullptr;
            }
            else if (set_prev->prev)
                set_prev = xset_get(set_prev->prev);
            else
                set_prev = nullptr;
        }
    }
    return false;
}

XSet*
xset_custom_new()
{
    std::string setname = xset_custom_new_name();

    XSet* set;
    set = xset_get(setname);
    set->lock = false;
    set->keep_terminal = true;
    set->task = true;
    set->task_err = true;
    set->task_out = true;
    return set;
}

void
xset_edit(GtkWidget* parent, const char* path, bool force_root, bool no_root)
{
    bool as_root = false;
    bool terminal;
    GtkWidget* dlgparent = nullptr;
    if (!path)
        return;
    if (force_root && no_root)
        return;

    if (parent)
        dlgparent = gtk_widget_get_toplevel(GTK_WIDGET(parent));

    std::string editor;
    if (geteuid() != 0 && !force_root && (no_root || have_rw_access(path)))
    {
        editor = xset_get_s("editor");
        if (editor.empty() || editor.at(0) == '\0')
        {
            ptk_show_error(dlgparent ? GTK_WINDOW(dlgparent) : nullptr,
                           "Editor Not Set",
                           "Please set your editor in View|Preferences|Advanced");
            return;
        }
        terminal = xset_get_b("editor");
    }
    else
    {
        editor = xset_get_s("root_editor");
        if (editor.empty() || editor.at(0) == '\0')
        {
            ptk_show_error(dlgparent ? GTK_WINDOW(dlgparent) : nullptr,
                           "Root Editor Not Set",
                           "Please set root's editor in View|Preferences|Advanced");
            return;
        }
        as_root = true;
        terminal = xset_get_b("root_editor");
    }
    // replacements
    const std::string quoted_path = bash_quote(path);
    if (ztd::contains(editor, "%f"))
        editor = ztd::replace(editor, "%f", quoted_path);
    else if (ztd::contains(editor, "%F"))
        editor = ztd::replace(editor, "%F", quoted_path);
    else if (ztd::contains(editor, "%u"))
        editor = ztd::replace(editor, "%u", quoted_path);
    else if (ztd::contains(editor, "%U"))
        editor = ztd::replace(editor, "%U", quoted_path);
    else
        editor = fmt::format("{} {}", editor, quoted_path);
    editor = fmt::format("{} {}", editor, quoted_path);

    // task
    std::string task_name = fmt::format("Edit {}", path);
    std::string cwd = g_path_get_dirname(path);
    PtkFileTask* ptask = ptk_file_exec_new(task_name, cwd.c_str(), dlgparent, nullptr);
    ptask->task->exec_command = editor;
    ptask->task->exec_sync = false;
    ptask->task->exec_terminal = terminal;
    if (as_root)
        ptask->task->exec_as_user = "root";
    ptk_file_task_run(ptask);
}

const std::string
xset_get_keyname(XSet* set, int key_val, int key_mod)
{
    int keyval;
    int keymod;
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
        return "( none )";

    std::string mod = gdk_keyval_name(keyval);

#if 0
    if (mod && mod[0] && !mod[1] && g_ascii_isalpha(mod[0]))
        mod[0] = g_ascii_toupper(mod[0]);
    else if (!mod)
        mod = ztd::strdup("NA");
#endif

    if (keymod)
    {
        if (keymod & GDK_SUPER_MASK)
            mod = fmt::format("Super+{}", mod);
        if (keymod & GDK_HYPER_MASK)
            mod = fmt::format("Hyper+{}", mod);
        if (keymod & GDK_META_MASK)
            mod = fmt::format("Meta+{}", mod);
        if (keymod & GDK_MOD1_MASK)
            mod = fmt::format("Alt+{}", mod);
        if (keymod & GDK_CONTROL_MASK)
            mod = fmt::format("Ctrl+{}", mod);
        if (keymod & GDK_SHIFT_MASK)
            mod = fmt::format("Shift+{}", mod);
    }
    return mod;
}

static bool
on_set_key_keypress(GtkWidget* widget, GdkEventKey* event, GtkWidget* dlg)
{
    (void)widget;
    int* newkey = (int*)g_object_get_data(G_OBJECT(dlg), "newkey");
    int* newkeymod = (int*)g_object_get_data(G_OBJECT(dlg), "newkeymod");
    GtkWidget* btn = GTK_WIDGET(g_object_get_data(G_OBJECT(dlg), "btn"));
    XSet* set = XSET(g_object_get_data(G_OBJECT(dlg), "set"));
    XSet* keyset = nullptr;
    std::string keyname;

    unsigned int keymod = ptk_get_keymod(event->state);

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

#ifdef HAVE_NONLATIN
    unsigned int nonlatin_key = 0;
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
        keyset = xset_get(set->shared_key);

    for (XSet* set2: xsets)
    {
        if (set2 && set2 != set && set2->key > 0 && set2->key == event->keyval &&
            set2->keymod == keymod && set2 != keyset)
        {
            std::string name;
            if (set2->desc && !strcmp(set2->desc, "@plugin@mirror@") && set2->shared_key)
            {
                // set2 is plugin mirror
                XSet* rset = xset_get(set2->shared_key);
                if (rset->menu_label)
                    name = clean_label(rset->menu_label, false, false);
                else
                    name = "( no name )";
            }
            else if (set2->menu_label)
                name = clean_label(set2->menu_label, false, false);
            else
                name = "( no name )";

            keyname = xset_get_keyname(nullptr, event->keyval, keymod);
#ifdef HAVE_NONLATIN
            if (nonlatin_key == 0)
#endif
                gtk_message_dialog_format_secondary_text(
                    GTK_MESSAGE_DIALOG(dlg),
                    "\t%s\n\tKeycode: %#4x  Modifier: %#x\n\n%s is already assigned to "
                    "'%s'.\n\nPress a different key or click Set to replace the current key "
                    "assignment.",
                    keyname.c_str(),
                    event->keyval,
                    keymod,
                    keyname.c_str(),
                    name.c_str());
#ifdef HAVE_NONLATIN
            else
                gtk_message_dialog_format_secondary_text(
                    GTK_MESSAGE_DIALOG(dlg),
                    "\t%s\n\tKeycode: %#4x [%#4x]  Modifier: %#x\n\n%s is already assigned to "
                    "'%s'.\n\nPress a different key or click Set to replace the current key "
                    "assignment.",
                    keyname.c_str(),
                    event->keyval,
                    nonlatin_key,
                    keymod,
                    keyname.c_str(),
                    name.c_str());
#endif
            *newkey = event->keyval;
            *newkeymod = keymod;
            return true;
        }
    }
    keyname = xset_get_keyname(nullptr, event->keyval, keymod);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg),
                                             "\t%s\n\tKeycode: %#4x  Modifier: %#x",
                                             keyname.c_str(),
                                             event->keyval,
                                             keymod);
    *newkey = event->keyval;
    *newkeymod = keymod;
    return true;
}

void
xset_set_key(GtkWidget* parent, XSet* set)
{
    std::string name;
    std::string keymsg;
    XSet* keyset;
    unsigned int newkey = 0;
    unsigned int newkeymod = 0;
    GtkWidget* dlgparent = nullptr;

    if (set->menu_label)
        name = clean_label(set->menu_label, false, true);
    else if (set->tool > XSetTool::CUSTOM)
        name = xset_get_builtin_toolitem_label(set->tool);
    else if (Glib::str_has_prefix(set->name, "open_all_type_"))
    {
        keyset = xset_get("open_all");
        name = clean_label(keyset->menu_label, false, true);
        if (set->shared_key)
            free(set->shared_key);
        set->shared_key = ztd::strdup("open_all");
    }
    else
        name = "( no name )";

    keymsg = fmt::format("Press your key combination for item '{}' then click Set.  To "
                         "remove the current key assignment, click Unset.",
                         name);
    if (parent)
        dlgparent = gtk_widget_get_toplevel(parent);

    GtkWidget* dlg = gtk_message_dialog_new_with_markup(GTK_WINDOW(dlgparent),
                                                        GTK_DIALOG_MODAL,
                                                        GTK_MESSAGE_QUESTION,
                                                        GTK_BUTTONS_NONE,
                                                        keymsg.c_str(),
                                                        nullptr);
    xset_set_window_icon(GTK_WINDOW(dlg));

    GtkWidget* btn_cancel = gtk_button_new_with_label("Cancel");
    gtk_button_set_label(GTK_BUTTON(btn_cancel), "Cancel");
    gtk_dialog_add_action_widget(GTK_DIALOG(dlg), btn_cancel, GTK_RESPONSE_CANCEL);

    GtkWidget* btn_unset = gtk_button_new_with_label("NO");
    gtk_button_set_label(GTK_BUTTON(btn_unset), "Unset");
    gtk_dialog_add_action_widget(GTK_DIALOG(dlg), btn_unset, GTK_RESPONSE_NO);

    if (set->shared_key)
        keyset = xset_get(set->shared_key);
    else
        keyset = set;
    if (keyset->key <= 0)
        gtk_widget_set_sensitive(btn_unset, false);

    GtkWidget* btn = gtk_button_new_with_label("Apply");
    gtk_button_set_label(GTK_BUTTON(btn), "Set");
    gtk_dialog_add_action_widget(GTK_DIALOG(dlg), btn, GTK_RESPONSE_OK);
    gtk_widget_set_sensitive(btn, false);

    g_object_set_data(G_OBJECT(dlg), "set", set);
    g_object_set_data(G_OBJECT(dlg), "newkey", &newkey);
    g_object_set_data(G_OBJECT(dlg), "newkeymod", &newkeymod);
    g_object_set_data(G_OBJECT(dlg), "btn", btn);
    g_object_set_data(G_OBJECT(dlg), "btn_unset", btn_unset);
    g_signal_connect(dlg, "key_press_event", G_CALLBACK(on_set_key_keypress), dlg);
    gtk_widget_show_all(dlg);
    gtk_window_set_title(GTK_WINDOW(dlg), "Set Key");

    int response = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
    if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_NO)
    {
        if (response == GTK_RESPONSE_OK && (newkey || newkeymod))
        {
            // clear duplicate key assignments
            for (XSet* set2: xsets)
            {
                if (set2 && set2->key > 0 && set2->key == newkey && set2->keymod == newkeymod)
                {
                    set2->key = 0;
                    set2->keymod = 0;
                }
            }
        }
        else if (response == GTK_RESPONSE_NO)
        {
            newkey = 0; // unset
            newkeymod = 0;
        }
        // plugin? set shared_key to mirror if not
        if (set->plugin && !set->shared_key)
            xset_get_plugin_mirror(set);
        // set new key
        if (set->shared_key)
            keyset = xset_get(set->shared_key);
        else
            keyset = set;
        keyset->key = newkey;
        keyset->keymod = newkeymod;
    }
}

static void
xset_design_job(GtkWidget* item, XSet* set)
{
    XSet* newset;
    XSet* mset;
    XSet* childset;
    XSet* set_next;
    std::string msg;
    int response;
    char* folder;
    char* file = nullptr;
    std::string file2;
    std::string plug_dir;
    char* custom_file;
    char* cscript;
    char* name;
    char* prog;
    int buttons;
    GtkWidget* dlgparent = nullptr;
    GtkWidget* dlg;
    GtkClipboard* clip;
    GtkWidget* parent = nullptr;
    bool update_toolbars = false;

    parent = gtk_widget_get_toplevel(GTK_WIDGET(set->browser));

    XSetTool tool_type;
    XSetJob job = XSetJob(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "job")));
    XSetCMD cmd_type = XSetCMD(xset_get_int_set(set, XSetSetSet::X));

    // LOG_INFO("activate job {} {}", job, set->name);
    switch (job)
    {
        case XSetJob::KEY:
            xset_set_key(parent, set);
            break;
        case XSetJob::ICON:
            mset = xset_get_plugin_mirror(set);
            char* old_icon;
            old_icon = ztd::strdup(mset->icon);
            // Note: xset_text_dialog uses the title passed to know this is an
            // icon chooser, so it adds a Choose button.  If you change the title,
            // change xset_text_dialog.
            xset_text_dialog(parent, "Set Icon", icon_desc, "", mset->icon, &mset->icon, "", false);
            if (set->lock && g_strcmp0(old_icon, mset->icon))
            {
                // built-in icon has been changed from default, save it
                set->keep_terminal = true;
            }
            free(old_icon);
            break;
        case XSetJob::LABEL:
            break;
        case XSetJob::EDIT:
            if (cmd_type == XSetCMD::SCRIPT)
            {
                // script
                cscript = xset_custom_get_script(set, !set->plugin);
                if (!cscript)
                    break;
                xset_edit(parent, cscript, false, true);
                free(cscript);
            }
            break;
        case XSetJob::EDIT_ROOT:
            if (cmd_type == XSetCMD::SCRIPT)
            {
                // script
                cscript = xset_custom_get_script(set, !set->plugin);
                if (!cscript)
                    break;
                xset_edit(parent, cscript, true, false);
                free(cscript);
            }
            break;
        case XSetJob::COPYNAME:
            clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
            if (cmd_type == XSetCMD::LINE)
            {
                // line
                gtk_clipboard_set_text(clip, set->line, -1);
            }
            else if (cmd_type == XSetCMD::SCRIPT)
            {
                // script
                cscript = xset_custom_get_script(set, true);
                if (!cscript)
                    break;
                gtk_clipboard_set_text(clip, cscript, -1);
                free(cscript);
            }
            else if (cmd_type == XSetCMD::APP)
            {
                // custom
                gtk_clipboard_set_text(clip, set->z, -1);
            }
            break;
        case XSetJob::LINE:
            if (xset_text_dialog(parent,
                                 "Edit Command Line",
                                 enter_command_line,
                                 "",
                                 set->line,
                                 &set->line,
                                 "",
                                 false))
                xset_set_set(set, XSetSetSet::X, "0");
            break;
        case XSetJob::SCRIPT:
            xset_set_set(set, XSetSetSet::X, "1");
            cscript = xset_custom_get_script(set, true);
            if (!cscript)
                break;
            xset_edit(parent, cscript, false, false);
            free(cscript);
            break;
        case XSetJob::CUSTOM:
            if (set->z && set->z[0] != '\0')
            {
                folder = g_path_get_dirname(set->z);
                file = g_path_get_basename(set->z);
            }
            else
            {
                folder = ztd::strdup("/usr/bin");
                file = nullptr;
            }
            if ((custom_file = xset_file_dialog(parent,
                                                GTK_FILE_CHOOSER_ACTION_OPEN,
                                                "Choose Custom Executable",
                                                folder,
                                                file)))
            {
                xset_set_set(set, XSetSetSet::X, "2");
                xset_set_set(set, XSetSetSet::Z, custom_file);
                free(custom_file);
            }
            free(file);
            free(folder);
            break;
        case XSetJob::USER:
            if (!set->plugin)
                xset_text_dialog(
                    parent,
                    "Run As User",
                    "Run this command as username:\n\n( Leave blank for current user )",
                    "",
                    set->y,
                    &set->y,
                    "",
                    false);
            break;
        case XSetJob::BOOKMARK:
        case XSetJob::APP:
        case XSetJob::COMMAND:
            if (Glib::str_has_prefix(set->name, "open_all_type_"))
            {
                name = set->name + 14;
                msg = fmt::format(
                    "You are adding a custom command to the Default menu item.  This item will "
                    "automatically have a pre-context - it will only appear when the MIME type "
                    "of the first selected file matches the current type '{}'.\n\nAdd commands "
                    "or menus here which you only want to appear for this one MIME type.",
                    name[0] == '\0' ? "(none)" : name);
                if (xset_msg_dialog(parent,
                                    GTK_MESSAGE_INFO,
                                    "New Context Command",
                                    GTK_BUTTONS_OK_CANCEL,
                                    msg) != GTK_RESPONSE_OK)
                {
                    break;
                }
            }
            switch (job)
            {
                case XSetJob::COMMAND:
                    name = ztd::strdup("New _Command");
                    if (!xset_text_dialog(parent,
                                          "Set Item Name",
                                          enter_menu_name_new,
                                          "",
                                          name,
                                          &name,
                                          "",
                                          false))
                    {
                        free(name);
                        break;
                    }
                    file = nullptr;
                    break;
                case XSetJob::APP:
                {
                    VFSMimeType* mime_type;
                    mime_type = vfs_mime_type_get_from_type(
                        xset_context && xset_context->var[ItemPropContext::CONTEXT_MIME] &&
                                xset_context->var[ItemPropContext::CONTEXT_MIME][0]
                            ? xset_context->var[ItemPropContext::CONTEXT_MIME]
                            : XDG_MIME_TYPE_UNKNOWN);
                    file = (char*)ptk_choose_app_for_mime_type(GTK_WINDOW(parent),
                                                               mime_type,
                                                               true,
                                                               false,
                                                               false,
                                                               false);
                    vfs_mime_type_unref(mime_type);
                    if (!(file && file[0]))
                    {
                        free(file);
                        break;
                    }
                    name = nullptr;
                    break;
                }
                case XSetJob::BOOKMARK:
                    if (set->browser)
                        folder = (char*)ptk_file_browser_get_cwd(set->browser);
                    else
                        folder = nullptr;
                    file = xset_file_dialog(parent,
                                            GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                            "Choose Directory",
                                            folder,
                                            nullptr);
                    if (!(file && file[0]))
                    {
                        free(file);
                        break;
                    }
                    name = g_path_get_basename(file);
                    break;
                case XSetJob::KEY:
                case XSetJob::ICON:
                case XSetJob::LABEL:
                case XSetJob::EDIT:
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
                case XSetJob::SUBMENU:
                case XSetJob::SUBMENU_BOOK:
                case XSetJob::SEP:
                case XSetJob::ADD_TOOL:
                case XSetJob::IMPORT_FILE:
                case XSetJob::IMPORT_GTK:
                case XSetJob::CUT:
                case XSetJob::COPY:
                case XSetJob::PASTE:
                case XSetJob::REMOVE:
                case XSetJob::REMOVE_BOOK:
                case XSetJob::NORMAL:
                case XSetJob::CHECK:
                case XSetJob::CONFIRM:
                case XSetJob::DIALOG:
                case XSetJob::MESSAGE:
                case XSetJob::COPYNAME:
                case XSetJob::PROP:
                case XSetJob::PROP_CMD:
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

            // add new menu item
            newset = xset_custom_new();
            xset_custom_insert_after(set, newset);

            newset->z = file;
            newset->menu_label = name;
            newset->browser = set->browser;

            switch (job)
            {
                case XSetJob::COMMAND:
                    xset_item_prop_dlg(xset_context, newset, 2);
                    break;
                case XSetJob::APP:
                    free(newset->x);
                    newset->x = ztd::strdup("2"); // XSetCMD::APP
                    // unset these to save session space
                    newset->task = false;
                    newset->task_err = false;
                    newset->task_out = false;
                    newset->keep_terminal = false;
                    break;
                case XSetJob::BOOKMARK:
                    free(newset->x);
                    newset->x = ztd::strdup("3"); // XSetCMD::BOOKMARK
                    // unset these to save session space
                    newset->task = false;
                    newset->task_err = false;
                    newset->task_out = false;
                    newset->keep_terminal = false;
                    break;
                case XSetJob::KEY:
                case XSetJob::ICON:
                case XSetJob::LABEL:
                case XSetJob::EDIT:
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
                case XSetJob::SUBMENU:
                case XSetJob::SUBMENU_BOOK:
                case XSetJob::SEP:
                case XSetJob::ADD_TOOL:
                case XSetJob::IMPORT_FILE:
                case XSetJob::IMPORT_GTK:
                case XSetJob::CUT:
                case XSetJob::COPY:
                case XSetJob::PASTE:
                case XSetJob::REMOVE:
                case XSetJob::REMOVE_BOOK:
                case XSetJob::NORMAL:
                case XSetJob::CHECK:
                case XSetJob::CONFIRM:
                case XSetJob::DIALOG:
                case XSetJob::MESSAGE:
                case XSetJob::COPYNAME:
                case XSetJob::PROP:
                case XSetJob::PROP_CMD:
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
            main_window_bookmark_changed(newset->name);
            break;
        case XSetJob::SUBMENU:
        case XSetJob::SUBMENU_BOOK:
            if (Glib::str_has_prefix(set->name, "open_all_type_"))
            {
                name = set->name + 14;
                msg = fmt::format(
                    "You are adding a custom submenu to the Default menu item.  This item will "
                    "automatically have a pre-context - it will only appear when the MIME type "
                    "of the first selected file matches the current type '{}'.\n\nAdd commands "
                    "or menus here which you only want to appear for this one MIME type.",
                    name[0] == '\0' ? "(none)" : name);
                if (xset_msg_dialog(parent,
                                    GTK_MESSAGE_INFO,
                                    "New Context Submenu",
                                    GTK_BUTTONS_OK_CANCEL,
                                    msg) != GTK_RESPONSE_OK)
                {
                    break;
                }
            }
            name = nullptr;
            if (!xset_text_dialog(
                    parent,
                    "Set Submenu Name",
                    "Enter submenu name:\n\nPrecede a character with an underscore (_) "
                    "to underline that character as a shortcut key if desired.",
                    "",
                    "New _Submenu",
                    &name,
                    "",
                    false) ||
                !name)
                break;

            // add new submenu
            newset = xset_custom_new();
            newset->menu_label = name;
            newset->menu_style = XSetMenu::SUBMENU;
            xset_custom_insert_after(set, newset);

            // add submenu child
            childset = xset_custom_new();
            newset->child = ztd::strdup(childset->name);
            childset->parent = ztd::strdup(newset->name);
            if (job == XSetJob::SUBMENU_BOOK || xset_is_main_bookmark(set))
            {
                // adding new submenu from a bookmark - fill with bookmark
                folder = set->browser ? (char*)ptk_file_browser_get_cwd(set->browser)
                                      : (char*)vfs_user_desktop_dir().c_str();
                childset->menu_label = g_path_get_basename(folder);
                childset->z = ztd::strdup(folder);
                childset->x = ztd::strdup(static_cast<int>(XSetCMD::BOOKMARK));
                // unset these to save session space
                childset->task = false;
                childset->task_err = false;
                childset->task_out = false;
                childset->keep_terminal = false;
            }
            else
                childset->menu_label = ztd::strdup("New _Command");
            main_window_bookmark_changed(newset->name);
            break;
        case XSetJob::SEP:
            newset = xset_custom_new();
            newset->menu_style = XSetMenu::SEP;
            xset_custom_insert_after(set, newset);
            main_window_bookmark_changed(newset->name);
            break;
        case XSetJob::ADD_TOOL:
            tool_type = XSetTool(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "tool_type")));
            if (tool_type < XSetTool::DEVICES || tool_type >= XSetTool::INVALID ||
                set->tool == XSetTool::NOT)
                break;
            newset = xset_new_builtin_toolitem(tool_type);
            if (newset)
                xset_custom_insert_after(set, newset);
            break;
        case XSetJob::IMPORT_FILE:
            // get file path
            XSet* save;
            save = xset_get("plug_ifile");
            if (save->s) //&& std::filesystem::is_directory(save->s)
                folder = save->s;
            else
            {
                if (!(folder = xset_get_s("go_set_default")))
                    folder = ztd::strdup("/");
            }
            file = xset_file_dialog(GTK_WIDGET(parent),
                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                    "Choose Plugin File",
                                    folder,
                                    nullptr);
            if (!file)
                break;
            if (save->s)
                free(save->s);
            save->s = g_path_get_dirname(file);

            // Make Plugin Dir
            const char* user_tmp;
            user_tmp = xset_get_user_tmp_dir();
            if (!user_tmp)
            {
                xset_msg_dialog(GTK_WIDGET(parent),
                                GTK_MESSAGE_ERROR,
                                "Error Creating Temp Directory",
                                GTK_BUTTONS_OK,
                                "Unable to create temporary directory");
                free(file);
                break;
            }
            while (std::filesystem::exists(plug_dir))
            {
                plug_dir = Glib::build_filename(user_tmp, randhex8());
                if (!std::filesystem::exists(plug_dir))
                    break;
            }
            install_plugin_file(set->browser ? set->browser->main_window : nullptr,
                                nullptr,
                                file,
                                plug_dir,
                                PluginJob::COPY,
                                set);
            free(file);
            break;
        case XSetJob::IMPORT_GTK:
            // both GTK2 and GTK3 now use new location?
            file2 = Glib::build_filename(vfs_user_config_dir(), "gtk-3.0", "bookmarks");
            if (!std::filesystem::exists(file2))
                file2 = Glib::build_filename(vfs_user_home_dir(), ".gtk-bookmarks");
            msg = fmt::format("GTK bookmarks ({}) will be imported into the current or selected "
                              "submenu.  Note that importing large numbers of bookmarks (eg more "
                              "than 500) may impact performance.",
                              file2);
            if (xset_msg_dialog(parent,
                                GTK_MESSAGE_QUESTION,
                                "Import GTK Bookmarks",
                                GTK_BUTTONS_OK_CANCEL,
                                msg) != GTK_RESPONSE_OK)
            {
                break;
            }
            ptk_bookmark_view_import_gtk(file2.c_str(), set);
            break;
        case XSetJob::CUT:
            set_clipboard = set;
            clipboard_is_cut = true;
            break;
        case XSetJob::COPY:
            set_clipboard = set;
            clipboard_is_cut = false;

            // if copy bookmark, put target on real clipboard
            if (!set->lock && set->z && set->menu_style < XSetMenu::SUBMENU &&
                XSetCMD(xset_get_int_set(set, XSetSetSet::X)) == XSetCMD::BOOKMARK)
            {
                clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
                gtk_clipboard_set_text(clip, set->z, -1);
                clip = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
                gtk_clipboard_set_text(clip, set->z, -1);
            }
            break;
        case XSetJob::PASTE:
            if (!set_clipboard)
                break;
            if (set_clipboard->tool > XSetTool::CUSTOM && set->tool == XSetTool::NOT)
                // failsafe - disallow pasting a builtin tool to a menu
                break;
            if (clipboard_is_cut)
            {
                update_toolbars = set_clipboard->tool != XSetTool::NOT;
                if (!update_toolbars && set_clipboard->parent)
                {
                    newset = xset_get(set_clipboard->parent);
                    if (newset->tool != XSetTool::NOT)
                        // we are cutting the first item in a tool submenu
                        update_toolbars = true;
                }
                xset_custom_remove(set_clipboard);
                xset_custom_insert_after(set, set_clipboard);

                main_window_bookmark_changed(set_clipboard->name);
                set_clipboard = nullptr;

                if (!set->lock)
                {
                    // update parent for bookmarks
                    newset = set;
                    while (newset->prev)
                        newset = xset_get(newset->prev);
                    if (newset->parent)
                        main_window_bookmark_changed(newset->parent);
                }
            }
            else
            {
                newset = xset_custom_copy(set_clipboard, false, false);
                xset_custom_insert_after(set, newset);
                main_window_bookmark_changed(newset->name);
            }
            break;
        case XSetJob::REMOVE:
        case XSetJob::REMOVE_BOOK:
            if (set->plugin)
            {
                xset_remove_plugin(parent, set->browser, set);
                break;
            }
            if (set->menu_label && set->menu_label[0])
                name = ztd::strdup(clean_label(set->menu_label, false, false));
            else
            {
                if (!set->lock && set->z && set->menu_style < XSetMenu::SUBMENU &&
                    (cmd_type == XSetCMD::BOOKMARK || cmd_type == XSetCMD::APP))
                    name = ztd::strdup(set->z);
                else
                    name = ztd::strdup("( no name )");
            }
            if (set->child && set->menu_style == XSetMenu::SUBMENU)
            {
                msg = fmt::format(
                    "Permanently remove the '{}' SUBMENU AND ALL ITEMS WITHIN IT?\n\nThis action "
                    "will delete all settings and files associated with these items.",
                    name);
                buttons = GTK_BUTTONS_YES_NO;
            }
            else
            {
                msg = fmt::format("Permanently remove the '{}' item?\n\nThis action will delete "
                                  "all settings and files associated with this item.",
                                  name);
                buttons = GTK_BUTTONS_OK_CANCEL;
            }
            free(name);
            bool is_bookmark_or_app;
            is_bookmark_or_app = !set->lock && set->menu_style < XSetMenu::SUBMENU &&
                                 (cmd_type == XSetCMD::BOOKMARK || cmd_type == XSetCMD::APP) &&
                                 set->tool <= XSetTool::CUSTOM;
            if (set->menu_style != XSetMenu::SEP && !app_settings.no_confirm &&
                !is_bookmark_or_app && set->tool <= XSetTool::CUSTOM)
            {
                if (parent)
                    dlgparent = gtk_widget_get_toplevel(parent);
                dlg = gtk_message_dialog_new(GTK_WINDOW(dlgparent),
                                             GTK_DIALOG_MODAL,
                                             GTK_MESSAGE_WARNING,
                                             (GtkButtonsType)buttons,
                                             msg.c_str(),
                                             nullptr);
                xset_set_window_icon(GTK_WINDOW(dlg));
                gtk_window_set_title(GTK_WINDOW(dlg), "Confirm Remove");
                gtk_widget_show_all(dlg);
                response = gtk_dialog_run(GTK_DIALOG(dlg));
                gtk_widget_destroy(dlg);
                if (response != GTK_RESPONSE_OK && response != GTK_RESPONSE_YES)
                    break;
            }

            // remove
            name = ztd::strdup(set->name);
            prog = ztd::strdup(set->parent);

            if (job == XSetJob::REMOVE && set->parent /* maybe only item in sub */
                && xset_is_main_bookmark(set))
                job = XSetJob::REMOVE_BOOK;

            if (set->parent && (set_next = xset_is(set->parent)) &&
                set_next->tool == XSetTool::CUSTOM && set_next->menu_style == XSetMenu::SUBMENU)
                // this set is first item in custom toolbar submenu
                update_toolbars = true;

            childset = xset_custom_remove(set);

            if (childset && job == XSetJob::REMOVE_BOOK)
            {
                // added a new default item to submenu from a bookmark
                folder = set->browser ? (char*)ptk_file_browser_get_cwd(set->browser)
                                      : (char*)vfs_user_desktop_dir().c_str();
                free(childset->menu_label);
                childset->menu_label = g_path_get_basename(folder);
                childset->z = ztd::strdup(folder);
                childset->x = ztd::strdup(static_cast<int>(XSetCMD::BOOKMARK));
                // unset these to save session space
                childset->task = false;
                childset->task_err = false;
                childset->task_out = false;
                childset->keep_terminal = false;
            }
            else if (set->tool != XSetTool::NOT)
            {
                update_toolbars = true;
                free(name);
                free(prog);
                name = prog = nullptr;
            }
            else
            {
                free(prog);
                prog = nullptr;
            }

            xset_custom_delete(set, false);
            set = nullptr;

            if (prog)
                main_window_bookmark_changed(prog);
            else if (name)
                main_window_bookmark_changed(name);
            free(name);
            free(prog);
            break;
        case XSetJob::EXPORT:
            if ((!set->lock || !g_strcmp0(set->name, "main_book")) && set->tool <= XSetTool::CUSTOM)
                xset_custom_export(parent, set->browser, set);
            break;
        case XSetJob::NORMAL:
            set->menu_style = XSetMenu::NORMAL;
            break;
        case XSetJob::CHECK:
            set->menu_style = XSetMenu::CHECK;
            break;
        case XSetJob::CONFIRM:
            if (!set->desc)
                set->desc = ztd::strdup("Are you sure?");
            if (xset_text_dialog(parent,
                                 "Dialog Message",
                                 "Enter the message to be displayed in this "
                                 "dialog:\n\nUse:\n\t\\n\tnewline\n\t\\t\ttab",
                                 "",
                                 set->desc,
                                 &set->desc,
                                 "",
                                 false))
                set->menu_style = XSetMenu::CONFIRM;
            break;
        case XSetJob::DIALOG:
            if (xset_text_dialog(parent,
                                 "Dialog Message",
                                 "Enter the message to be displayed in this "
                                 "dialog:\n\nUse:\n\t\\n\tnewline\n\t\\t\ttab",
                                 "",
                                 set->desc,
                                 &set->desc,
                                 "",
                                 false))
                set->menu_style = XSetMenu::STRING;
            break;
        case XSetJob::MESSAGE:
            xset_text_dialog(parent,
                             "Dialog Message",
                             "Enter the message to be displayed in this "
                             "dialog:\n\nUse:\n\t\\n\tnewline\n\t\\t\ttab",
                             "",
                             set->desc,
                             &set->desc,
                             "",
                             false);
            break;
        case XSetJob::PROP:
            xset_item_prop_dlg(xset_context, set, 0);
            break;
        case XSetJob::PROP_CMD:
            xset_item_prop_dlg(xset_context, set, 2);
            break;
        case XSetJob::IGNORE_CONTEXT:
            xset_set_b("context_dlg", !xset_get_b("context_dlg"));
            break;
        case XSetJob::BROWSE_FILES:
            if (set->tool > XSetTool::CUSTOM)
                break;
            if (set->plugin)
            {
                folder = g_build_filename(set->plug_dir, "files", nullptr);
                if (!std::filesystem::exists(folder))
                {
                    free(folder);
                    folder = g_build_filename(set->plug_dir, set->plug_name, nullptr);
                }
            }
            else
            {
                folder = g_build_filename(xset_get_config_dir(), "scripts", set->name, nullptr);
            }
            if (!std::filesystem::exists(folder) && !set->plugin)
            {
                std::filesystem::create_directories(folder);
                std::filesystem::permissions(folder, std::filesystem::perms::owner_all);
            }

            if (set->browser)
            {
                ptk_file_browser_emit_open(set->browser, folder, PtkOpenAction::PTK_OPEN_DIR);
            }
            break;
        case XSetJob::BROWSE_DATA:
            if (set->tool > XSetTool::CUSTOM)
                break;
            if (set->plugin)
            {
                mset = xset_get_plugin_mirror(set);
                folder =
                    g_build_filename(xset_get_config_dir(), "plugin-data", mset->name, nullptr);
            }
            else
                folder = g_build_filename(xset_get_config_dir(), "plugin-data", set->name, nullptr);
            if (!std::filesystem::exists(folder))
            {
                std::filesystem::create_directories(folder);
                std::filesystem::permissions(folder, std::filesystem::perms::owner_all);
            }

            if (set->browser)
            {
                ptk_file_browser_emit_open(set->browser, folder, PtkOpenAction::PTK_OPEN_DIR);
            }
            break;
        case XSetJob::BROWSE_PLUGIN:
            if (set->plugin && set->plug_dir)
            {
                if (set->browser)
                {
                    ptk_file_browser_emit_open(set->browser,
                                               set->plug_dir,
                                               PtkOpenAction::PTK_OPEN_DIR);
                }
            }
            break;
        case XSetJob::TERM:
            mset = xset_get_plugin_mirror(set);
            if (mset->in_terminal)
                mset->in_terminal = false;
            else
            {
                mset->in_terminal = true;
                mset->task = false;
            }
            break;
        case XSetJob::KEEP:
            mset = xset_get_plugin_mirror(set);
            if (mset->keep_terminal)
                mset->keep_terminal = false;
            else
                mset->keep_terminal = true;
            break;
        case XSetJob::TASK:
            mset = xset_get_plugin_mirror(set);
            if (mset->task)
                mset->task = false;
            else
                mset->task = true;
            break;
        case XSetJob::POP:
            mset = xset_get_plugin_mirror(set);
            if (mset->task_pop)
                mset->task_pop = false;
            else
                mset->task_pop = true;
            break;
        case XSetJob::ERR:
            mset = xset_get_plugin_mirror(set);
            if (mset->task_err)
                mset->task_err = false;
            else
                mset->task_err = true;
            break;
        case XSetJob::OUT:
            mset = xset_get_plugin_mirror(set);
            if (mset->task_out)
                mset->task_out = false;
            else
                mset->task_out = true;
            break;
        case XSetJob::SCROLL:
            mset = xset_get_plugin_mirror(set);
            if (mset->scroll_lock)
                mset->scroll_lock = false;
            else
                mset->scroll_lock = true;
            break;
        case XSetJob::TOOLTIPS:
            set_next = xset_get_panel(1, "tool_l");
            set_next->b =
                set_next->b == XSetB::XSET_B_TRUE ? XSetB::XSET_B_UNSET : XSetB::XSET_B_TRUE;
            break;
        case XSetJob::HELP:
        case XSetJob::HELP_NEW:
        case XSetJob::HELP_ADD:
        case XSetJob::HELP_BROWSE:
        case XSetJob::HELP_STYLE:
        case XSetJob::HELP_BOOK:
        case XSetJob::INVALID:
        default:
            break;
    }

    if (set && (!set->lock || !strcmp(set->name, "main_book")))
    {
        main_window_bookmark_changed(set->name);
        if (set->parent && (set_next = xset_is(set->parent)) &&
            set_next->tool == XSetTool::CUSTOM && set_next->menu_style == XSetMenu::SUBMENU)
            // this set is first item in custom toolbar submenu
            update_toolbars = true;
    }

    if ((set && !set->lock && set->tool != XSetTool::NOT) || update_toolbars)
        main_window_rebuild_all_toolbars(set ? set->browser : nullptr);

    // autosave
    autosave_request_add();
}

static bool
xset_job_is_valid(XSet* set, XSetJob job)
{
    bool no_remove = false;
    bool no_paste = false;
    bool open_all = false;

    if (!set)
        return false;

    if (set->plugin)
    {
        if (!set->plug_dir)
            return false;
        if (!set->plugin_top)
            no_remove = true;
    }

    // control open_all item
    if (Glib::str_has_prefix(set->name, "open_all_type_"))
        open_all = true;

    switch (job)
    {
        case XSetJob::KEY:
            return set->menu_style < XSetMenu::SUBMENU;
        case XSetJob::ICON:
            return ((set->menu_style == XSetMenu::NORMAL || set->menu_style == XSetMenu::STRING ||
                     set->menu_style == XSetMenu::FONTDLG ||
                     set->menu_style == XSetMenu::COLORDLG ||
                     set->menu_style == XSetMenu::SUBMENU || set->tool != XSetTool::NOT) &&
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
            if (!set_clipboard)
                no_paste = true;
            else if (set->plugin)
                no_paste = true;
            else if (set == set_clipboard && clipboard_is_cut)
                // do not allow cut paste to self
                no_paste = true;
            else if (set_clipboard->tool > XSetTool::CUSTOM && set->tool == XSetTool::NOT)
                // do not allow paste of builtin tool item to menu
                no_paste = true;
            else if (set_clipboard->menu_style == XSetMenu::SUBMENU)
                // do not allow paste of submenu to self or below
                no_paste = xset_clipboard_in_set(set);
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
        case XSetJob::IMPORT_GTK:
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
xset_design_menu_keypress(GtkWidget* widget, GdkEventKey* event, XSet* set)
{
    XSetJob job = XSetJob::INVALID;

    GtkWidget* item = gtk_menu_shell_get_selected_item(GTK_MENU_SHELL(widget));
    if (!item)
        return false;

    unsigned int keymod = ptk_get_keymod(event->state);

#ifdef HAVE_NONLATIN
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
                    if (XSetCMD(xset_get_int_set(set, XSetSetSet::X)) == XSetCMD::SCRIPT)
                        job = XSetJob::EDIT;
                    else
                        job = XSetJob::PROP_CMD;
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
        case GDK_CONTROL_MASK:
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
                        job = XSetJob::EDIT;
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
                                    G_SIGNAL_MATCH_FUNC,
                                    0,
                                    0,
                                    nullptr,
                                    (void*)xset_design_job,
                                    nullptr);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), true);
    g_signal_handlers_unblock_matched(item,
                                      G_SIGNAL_MATCH_FUNC,
                                      0,
                                      0,
                                      nullptr,
                                      (void*)xset_design_job,
                                      nullptr);
}

static GtkWidget*
xset_design_additem(GtkWidget* menu, const char* label, XSetJob job, XSet* set)
{
    GtkWidget* item;
    item = gtk_menu_item_new_with_mnemonic(label);

    g_object_set_data(G_OBJECT(item), "job", GINT_TO_POINTER(static_cast<int>(job)));
    gtk_container_add(GTK_CONTAINER(menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(xset_design_job), set);
    return item;
}

GtkWidget*
xset_design_show_menu(GtkWidget* menu, XSet* set, XSet* book_insert, unsigned int button,
                      uint32_t time)
{
    GtkWidget* newitem;
    GtkWidget* submenu;
    GtkWidget* submenu2;
    bool no_remove = false;
    bool no_paste = false;
    // bool open_all = false;

    // XSet* mset;
    XSet* insert_set;

    // book_insert is a bookmark set to be used for Paste, etc
    insert_set = book_insert ? book_insert : set;
    // to signal this is a bookmark, pass book_insert = set
    bool is_bookmark = !!book_insert;
    bool show_keys = !is_bookmark && set->tool == XSetTool::NOT;

    // if (set->plugin && set->shared_key)
    //     mset = xset_get_plugin_mirror(set);
    // else
    //     mset = set;

    if (set->plugin)
    {
        if (set->plug_dir)
        {
            if (!set->plugin_top)
                no_remove = true;
        }
        else
            no_remove = true;
    }

    if (!set_clipboard)
        no_paste = true;
    else if (insert_set->plugin)
        no_paste = true;
    else if (insert_set == set_clipboard && clipboard_is_cut)
        // do not allow cut paste to self
        no_paste = true;
    else if (set_clipboard->tool > XSetTool::CUSTOM && insert_set->tool == XSetTool::NOT)
        // do not allow paste of builtin tool item to menu
        no_paste = true;
    else if (set_clipboard->menu_style == XSetMenu::SUBMENU)
        // do not allow paste of submenu to self or below
        no_paste = xset_clipboard_in_set(insert_set);

    // control open_all item
    // if (Glib::str_has_prefix(set->name, "open_all_type_"))
    //    open_all = true;

    GtkWidget* design_menu = gtk_menu_new();
    GtkAccelGroup* accel_group = gtk_accel_group_new();

    // Cut
    newitem = xset_design_additem(design_menu, "Cu_t", XSetJob::CUT, set);
    gtk_widget_set_sensitive(newitem, !set->lock && !set->plugin);
    if (show_keys)
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_x,
                                   GDK_CONTROL_MASK,
                                   GTK_ACCEL_VISIBLE);

    // Copy
    newitem = xset_design_additem(design_menu, "_Copy", XSetJob::COPY, set);
    gtk_widget_set_sensitive(newitem, !set->lock);
    if (show_keys)
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_c,
                                   GDK_CONTROL_MASK,
                                   GTK_ACCEL_VISIBLE);

    // Paste
    newitem = xset_design_additem(design_menu, "_Paste", XSetJob::PASTE, insert_set);
    gtk_widget_set_sensitive(newitem, !no_paste);
    if (show_keys)
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_v,
                                   GDK_CONTROL_MASK,
                                   GTK_ACCEL_VISIBLE);

    // Remove
    newitem = xset_design_additem(design_menu,
                                  "_Remove",
                                  is_bookmark ? XSetJob::REMOVE_BOOK : XSetJob::REMOVE,
                                  set);
    gtk_widget_set_sensitive(newitem, !set->lock && !no_remove);
    if (show_keys)
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_Delete,
                                   (GdkModifierType)0,
                                   GTK_ACCEL_VISIBLE);

    // Export
    newitem = xset_design_additem(design_menu, "E_xport", XSetJob::EXPORT, set);
    gtk_widget_set_sensitive(
        newitem,
        (!set->lock && set->menu_style < XSetMenu::SEP && set->tool <= XSetTool::CUSTOM) ||
            !g_strcmp0(set->name, "main_book"));

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
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_Insert,
                                   (GdkModifierType)0,
                                   GTK_ACCEL_VISIBLE);

    // New > Submenu
    newitem = xset_design_additem(submenu,
                                  "Sub_menu",
                                  is_bookmark ? XSetJob::SUBMENU_BOOK : XSetJob::SUBMENU,
                                  insert_set);

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
    if (is_bookmark)
        newitem = xset_design_additem(submenu2, "_GTK Bookmarks", XSetJob::IMPORT_GTK, set);

    if (insert_set->tool != XSetTool::NOT)
    {
        // "Add" submenu for builtin tool items
        newitem = gtk_menu_item_new_with_mnemonic("_Add");
        submenu = gtk_menu_new();
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(newitem), submenu);
        gtk_container_add(GTK_CONTAINER(design_menu), newitem);
        g_object_set_data(G_OBJECT(newitem), "job", GINT_TO_POINTER(XSetJob::HELP_ADD));
        g_signal_connect(submenu, "key_press_event", G_CALLBACK(xset_design_menu_keypress), set);

        for (std::size_t i = static_cast<int>(XSetTool::DEVICES); i < builtin_tool_name.size(); i++)
        {
            newitem =
                xset_design_additem(submenu, builtin_tool_name[i], XSetJob::ADD_TOOL, insert_set);
            g_object_set_data(G_OBJECT(newitem), "tool_type", GINT_TO_POINTER(i));
        }
    }

    // Separator
    gtk_container_add(GTK_CONTAINER(design_menu), gtk_separator_menu_item_new());

    // Help
    newitem = xset_design_additem(design_menu,
                                  "_Help",
                                  is_bookmark ? XSetJob::HELP_BOOK : XSetJob::HELP,
                                  set);
    gtk_widget_set_sensitive(newitem, !set->lock || set->line);
    if (show_keys)
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_F1,
                                   (GdkModifierType)0,
                                   GTK_ACCEL_VISIBLE);

    // Tooltips (toolbar)
    if (set->tool != XSetTool::NOT)
    {
        newitem = xset_design_additem(design_menu, "T_ooltips", XSetJob::TOOLTIPS, set);
        if (!xset_get_b_panel(1, "tool_l"))
            set_check_menu_item_block(newitem);
    }

    // Key
    newitem = xset_design_additem(design_menu, "_Key Shortcut", XSetJob::KEY, set);
    gtk_widget_set_sensitive(newitem, (set->menu_style < XSetMenu::SUBMENU));
    if (show_keys)
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_k,
                                   GDK_CONTROL_MASK,
                                   GTK_ACCEL_VISIBLE);

    // Edit (script)
    if (!set->lock && set->menu_style < XSetMenu::SUBMENU && set->tool <= XSetTool::CUSTOM)
    {
        if (XSetCMD(xset_get_int_set(set, XSetSetSet::X)) == XSetCMD::SCRIPT)
        {
            char* script = xset_custom_get_script(set, false);
            if (script)
            {
                if (geteuid() != 0 && have_rw_access(script))
                {
                    // edit as user
                    newitem = xset_design_additem(design_menu, "_Edit Script", XSetJob::EDIT, set);
                    if (show_keys)
                        gtk_widget_add_accelerator(newitem,
                                                   "activate",
                                                   accel_group,
                                                   GDK_KEY_F4,
                                                   (GdkModifierType)0,
                                                   GTK_ACCEL_VISIBLE);
                }
                else
                {
                    // edit as root
                    newitem =
                        xset_design_additem(design_menu, "E_dit As Root", XSetJob::EDIT_ROOT, set);
                    if (geteuid() == 0 && show_keys)
                        gtk_widget_add_accelerator(newitem,
                                                   "activate",
                                                   accel_group,
                                                   GDK_KEY_F4,
                                                   (GdkModifierType)0,
                                                   GTK_ACCEL_VISIBLE);
                }
                free(script);
            }
        }
        else if (XSetCMD(xset_get_int_set(set, XSetSetSet::X)) == XSetCMD::LINE)
        {
            // edit command line
            newitem = xset_design_additem(design_menu, "_Edit Command", XSetJob::PROP_CMD, set);
            if (show_keys)
                gtk_widget_add_accelerator(newitem,
                                           "activate",
                                           accel_group,
                                           GDK_KEY_F4,
                                           (GdkModifierType)0,
                                           GTK_ACCEL_VISIBLE);
        }
    }

    // Properties
    newitem = xset_design_additem(design_menu, "_Properties", XSetJob::PROP, set);
    if (show_keys)
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_F3,
                                   (GdkModifierType)0,
                                   GTK_ACCEL_VISIBLE);

    // show menu
    gtk_widget_show_all(GTK_WIDGET(design_menu));
    /* sfm 1.0.6 passing button (3) here when menu == nullptr causes items in New
     * submenu to not activate with some trackpads (eg two-finger right-click)
     * to open original design menu.  Affected only bookmarks pane and toolbar
     * where menu == nullptr.  So pass 0 for button if !menu. */

    // FIXME does not destroy parent popup
    // gtk_menu_popup_at_pointer(GTK_MENU(design_menu), nullptr);
    gtk_menu_popup(GTK_MENU(design_menu),
                   menu ? GTK_WIDGET(menu) : nullptr,
                   nullptr,
                   nullptr,
                   nullptr,
                   menu ? button : 0,
                   time);
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
xset_design_cb(GtkWidget* item, GdkEventButton* event, XSet* set)
{
    XSetJob job = XSetJob::INVALID;

    GtkWidget* menu = item ? GTK_WIDGET(g_object_get_data(G_OBJECT(item), "menu")) : nullptr;
    unsigned int keymod = ptk_get_keymod(event->state);

    if (event->type == GDK_BUTTON_RELEASE)
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
                gtk_menu_shell_deactivate(GTK_MENU_SHELL(menu));
            gtk_menu_item_activate(GTK_MENU_ITEM(item));
            return true;
        }
        // true for issue #521 where a right-click also left-clicks the first
        // menu item in some GTK2/3 themes.
        return true;
    }
    else if (event->type != GDK_BUTTON_PRESS)
        return false;

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
                            xset_menu_cb(nullptr, set);
                        else
                            xset_builtin_tool_activate(set->tool, set, event);
                        return true;
                    }
                    break;
                case GDK_CONTROL_MASK:
                    // ctrl
                    job = XSetJob::COPY;
                    break;
                case GDK_MOD1_MASK:
                    // alt
                    job = XSetJob::CUT;
                    break;
                case GDK_SHIFT_MASK:
                    // shift
                    job = XSetJob::PASTE;
                    break;
                case (GDK_CONTROL_MASK | GDK_SHIFT_MASK):
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
                        if (XSetCMD(xset_get_int_set(set, XSetSetSet::X)) == XSetCMD::SCRIPT)
                            job = XSetJob::EDIT;
                        else
                            job = XSetJob::PROP_CMD;
                    }
                    break;
                case GDK_CONTROL_MASK:
                    // ctrl
                    job = XSetJob::KEY;
                    break;
                case GDK_MOD1_MASK:
                    // alt
                    job = XSetJob::HELP;
                    break;
                case GDK_SHIFT_MASK:
                    // shift
                    job = XSetJob::ICON;
                    break;
                case (GDK_CONTROL_MASK | GDK_SHIFT_MASK):
                    // ctrl + shift
                    job = XSetJob::REMOVE;
                    break;
                case (GDK_CONTROL_MASK | GDK_MOD1_MASK):
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
                gtk_menu_shell_deactivate(GTK_MENU_SHELL(menu));
            g_object_set_data(G_OBJECT(item), "job", GINT_TO_POINTER(job));
            xset_design_job(item, set);
        }
        else
            xset_design_show_menu(menu, set, nullptr, event->button, event->time);
        return true;
    }
    return false; // true will not stop activate on button-press (will on release)
}

bool
xset_menu_keypress(GtkWidget* widget, GdkEventKey* event, void* user_data)
{
    (void)user_data;
    XSetJob job = XSetJob::INVALID;
    XSet* set;

    GtkWidget* item = gtk_menu_shell_get_selected_item(GTK_MENU_SHELL(widget));
    if (item)
    {
        set = XSET(g_object_get_data(G_OBJECT(item), "set"));
        if (!set)
            return false;
    }
    else
        return false;

    unsigned int keymod = ptk_get_keymod(event->state);

#ifdef HAVE_NONLATIN
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
                    if (XSetCMD(xset_get_int_set(set, XSetSetSet::X)) == XSetCMD::SCRIPT)
                        job = XSetJob::EDIT;
                    else
                        job = XSetJob::PROP_CMD;
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
        case GDK_CONTROL_MASK:
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
                        if (XSetCMD(xset_get_int_set(set, XSetSetSet::X)) == XSetCMD::SCRIPT)
                            job = XSetJob::EDIT;
                        else
                            job = XSetJob::PROP_CMD;
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
            xset_design_show_menu(widget, set, nullptr, 0, event->time);
        return true;
    }
    return false;
}

void
xset_menu_cb(GtkWidget* item, XSet* set)
{
    GtkWidget* parent;
    GFunc cb_func = nullptr;
    void* cb_data = nullptr;
    std::string title;
    XSet* mset; // mirror set or set
    XSet* rset; // real set

    if (item)
    {
        if (set->lock && set->menu_style == XSetMenu::RADIO && GTK_IS_CHECK_MENU_ITEM(item) &&
            !gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)))
            return;

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
    else if (!set->lock && set->desc && !strcmp(set->desc, "@plugin@mirror@") && set->shared_key)
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

    switch (rset->menu_style)
    {
        case XSetMenu::NORMAL:
            if (cb_func)
                cb_func(item, cb_data);
            else if (!rset->lock)
                xset_custom_activate(item, rset);
            break;
        case XSetMenu::SEP:
            break;
        case XSetMenu::CHECK:
            if (mset->b == XSetB::XSET_B_TRUE)
                mset->b = XSetB::XSET_B_FALSE;
            else
                mset->b = XSetB::XSET_B_TRUE;
            if (cb_func)
                cb_func(item, cb_data);
            else if (!rset->lock)
                xset_custom_activate(item, rset);
            if (set->tool == XSetTool::CUSTOM)
                ptk_file_browser_update_toolbar_widgets(set->browser, set, XSetTool::INVALID);
            break;
        case XSetMenu::STRING:
        case XSetMenu::CONFIRM:
        {
            std::string msg = rset->desc;
            char* default_str = nullptr;
            if (rset->title && rset->lock)
                title = ztd::strdup(rset->title);
            else
                title = clean_label(rset->menu_label, false, false);
            if (rset->lock)
                default_str = rset->z;
            else
            {
                msg = ztd::replace(msg, "\\n", "\n");
                msg = ztd::replace(msg, "\\t", "\t");
            }
            if (rset->menu_style == XSetMenu::CONFIRM)
            {
                if (xset_msg_dialog(parent,
                                    GTK_MESSAGE_QUESTION,
                                    title,
                                    GTK_BUTTONS_OK_CANCEL,
                                    msg) == GTK_RESPONSE_OK)
                {
                    if (cb_func)
                        cb_func(item, cb_data);
                    else if (!set->lock)
                        xset_custom_activate(item, rset);
                }
            }
            else if (xset_text_dialog(parent,
                                      title,
                                      msg,
                                      "",
                                      mset->s,
                                      &mset->s,
                                      default_str,
                                      false))
            {
                if (cb_func)
                    cb_func(item, cb_data);
                else if (!set->lock)
                    xset_custom_activate(item, rset);
            }
        }
        break;
        case XSetMenu::RADIO:
            if (mset->b != XSetB::XSET_B_TRUE)
                mset->b = XSetB::XSET_B_TRUE;
            if (cb_func)
                cb_func(item, cb_data);
            else if (!rset->lock)
                xset_custom_activate(item, rset);
            break;
        case XSetMenu::FONTDLG:
            break;
        case XSetMenu::FILEDLG:
            // test purpose only
            {
                char* file;
                file = xset_file_dialog(parent,
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        rset->title,
                                        rset->s,
                                        "foobar.xyz");
                // LOG_INFO("file={}", file);
                free(file);
            }
            break;
        case XSetMenu::ICON:
            // Note: xset_text_dialog uses the title passed to know this is an
            // icon chooser, so it adds a Choose button.  If you change the title,
            // change xset_text_dialog.
            if (xset_text_dialog(parent,
                                 rset->title ? rset->title : "Set Icon",
                                 rset->desc ? rset->desc : icon_desc,
                                 "",
                                 rset->icon,
                                 &rset->icon,
                                 "",
                                 false))
            {
                if (rset->lock)
                    rset->keep_terminal = true; // trigger save of changed icon
                if (cb_func)
                    cb_func(item, cb_data);
            }
            break;
        case XSetMenu::COLORDLG:
        {
            char* scolor;
            scolor = xset_color_dialog(parent, rset->title, rset->s);
            if (rset->s)
                free(rset->s);
            rset->s = scolor;
            if (cb_func)
                cb_func(item, cb_data);
            break;
        }
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
                cb_func(item, cb_data);
            else if (!set->lock)
                xset_custom_activate(item, rset);
            break;
    }

    if (rset->menu_style != XSetMenu::NORMAL)
        autosave_request_add();
}

int
xset_msg_dialog(GtkWidget* parent, GtkMessageType action, const std::string& title,
                GtkButtonsType buttons, const std::string& msg1)
{
    std::string msg2;
    return xset_msg_dialog(parent, action, title, buttons, msg1, msg2);
}

int
xset_msg_dialog(GtkWidget* parent, GtkMessageType action, const std::string& title,
                GtkButtonsType buttons, const std::string& msg1, const std::string& msg2)
{
    GtkWidget* dlgparent = nullptr;

    if (parent)
        dlgparent = gtk_widget_get_toplevel(parent);

    GtkWidget* dlg =
        gtk_message_dialog_new(GTK_WINDOW(dlgparent),
                               GtkDialogFlags(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                               action,
                               buttons,
                               msg1.c_str(),
                               nullptr);

    if (action == GTK_MESSAGE_INFO)
        xset_set_window_icon(GTK_WINDOW(dlg));
    gtk_window_set_role(GTK_WINDOW(dlg), "msg_dialog");

    if (!msg2.empty())
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), msg2.c_str(), nullptr);

    gtk_window_set_title(GTK_WINDOW(dlg), title.c_str());

    gtk_widget_show_all(dlg);
    int res = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
    return res;
}

static void
on_multi_input_insert(GtkTextBuffer* buf)
{ // remove linefeeds from pasted text
    GtkTextIter iter, siter;
    // bool changed = false;
    int x;

    // buffer contains linefeeds?
    gtk_text_buffer_get_start_iter(buf, &siter);
    gtk_text_buffer_get_end_iter(buf, &iter);
    char* all = gtk_text_buffer_get_text(buf, &siter, &iter, false);
    if (!strchr(all, '\n'))
    {
        free(all);
        return;
    }
    free(all);

    // delete selected text that was pasted over
    if (gtk_text_buffer_get_selection_bounds(buf, &siter, &iter))
        gtk_text_buffer_delete(buf, &siter, &iter);

    GtkTextMark* insert = gtk_text_buffer_get_insert(buf);
    gtk_text_buffer_get_iter_at_mark(buf, &iter, insert);
    gtk_text_buffer_get_start_iter(buf, &siter);
    char* b = gtk_text_buffer_get_text(buf, &siter, &iter, false);
    gtk_text_buffer_get_end_iter(buf, &siter);
    char* a = gtk_text_buffer_get_text(buf, &iter, &siter, false);

    if (strchr(b, '\n'))
    {
        x = 0;
        while (b[x] != '\0')
        {
            if (b[x] == '\n')
                b[x] = ' ';
            x++;
        }
    }
    if (strchr(a, '\n'))
    {
        x = 0;
        while (a[x] != '\0')
        {
            if (a[x] == '\n')
                a[x] = ' ';
            x++;
        }
    }

    g_signal_handlers_block_matched(buf,
                                    G_SIGNAL_MATCH_FUNC,
                                    0,
                                    0,
                                    nullptr,
                                    (void*)on_multi_input_insert,
                                    nullptr);

    gtk_text_buffer_set_text(buf, b, -1);
    gtk_text_buffer_get_end_iter(buf, &iter);
    GtkTextMark* mark = gtk_text_buffer_create_mark(buf, nullptr, &iter, true);
    gtk_text_buffer_get_end_iter(buf, &iter);
    gtk_text_buffer_insert(buf, &iter, a, -1);
    gtk_text_buffer_get_iter_at_mark(buf, &iter, mark);
    gtk_text_buffer_place_cursor(buf, &iter);

    g_signal_handlers_unblock_matched(buf,
                                      G_SIGNAL_MATCH_FUNC,
                                      0,
                                      0,
                                      nullptr,
                                      (void*)on_multi_input_insert,
                                      nullptr);

    free(a);
    free(b);
}

char*
multi_input_get_text(GtkWidget* input)
{ // returns a new allocated string or nullptr if input is empty
    GtkTextIter iter, siter;

    if (!GTK_IS_TEXT_VIEW(input))
        return nullptr;

    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(input));
    gtk_text_buffer_get_start_iter(buf, &siter);
    gtk_text_buffer_get_end_iter(buf, &iter);
    char* ret = gtk_text_buffer_get_text(buf, &siter, &iter, false);
    if (ret && ret[0] == '\0')
    {
        free(ret);
        ret = nullptr;
    }
    return ret;
}

void
multi_input_select_region(GtkWidget* input, int start, int end)
{
    GtkTextIter iter, siter;

    if (start < 0 || !GTK_IS_TEXT_VIEW(input))
        return;

    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(input));

    gtk_text_buffer_get_iter_at_offset(buf, &siter, start);

    if (end < 0)
        gtk_text_buffer_get_end_iter(buf, &iter);
    else
        gtk_text_buffer_get_iter_at_offset(buf, &iter, end);

    gtk_text_buffer_select_range(buf, &iter, &siter);
}

GtkTextView*
multi_input_new(GtkScrolledWindow* scrolled, const char* text)
{
    GtkTextIter iter;

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    GtkTextView* input = GTK_TEXT_VIEW(gtk_text_view_new());
    // ubuntu shows input too small so use mininum height
    gtk_widget_set_size_request(GTK_WIDGET(input), -1, 50);
    gtk_widget_set_size_request(GTK_WIDGET(scrolled), -1, 50);

    gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(input));
    GtkTextBuffer* buf = gtk_text_view_get_buffer(input);
    gtk_text_view_set_wrap_mode(input, GTK_WRAP_CHAR); // GTK_WRAP_WORD_CHAR

    if (text)
        gtk_text_buffer_set_text(buf, text, -1);
    gtk_text_buffer_get_end_iter(buf, &iter);
    gtk_text_buffer_place_cursor(buf, &iter);
    GtkTextMark* insert = gtk_text_buffer_get_insert(buf);
    gtk_text_view_scroll_to_mark(input, insert, 0.0, false, 0, 0);
    gtk_text_view_set_accepts_tab(input, false);

    g_signal_connect_after(G_OBJECT(buf),
                           "insert-text",
                           G_CALLBACK(on_multi_input_insert),
                           nullptr);

    return input;
}

static bool
on_input_keypress(GtkWidget* widget, GdkEventKey* event, GtkWidget* dlg)
{
    (void)widget;
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter)
    {
        gtk_dialog_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
        return true;
    }
    return false;
}

char*
xset_icon_chooser_dialog(GtkWindow* parent, const char* def_icon)
{
    GtkAllocation allocation;
    char* icon = nullptr;

    // set busy cursor
    GdkCursor* cursor =
        gdk_cursor_new_for_display(gtk_widget_get_display(GTK_WIDGET(parent)), GDK_WATCH);
    if (cursor)
    {
        gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(parent)), cursor);
        g_object_unref(cursor);
        while (gtk_events_pending())
            gtk_main_iteration();
    }

    // btn_icon_choose clicked - preparing the exo icon chooser dialog
    GtkWidget* icon_chooser = exo_icon_chooser_dialog_new("Choose Icon",
                                                          GTK_WINDOW(parent),
                                                          "Cancel",
                                                          GTK_RESPONSE_CANCEL,
                                                          "OK",
                                                          GTK_RESPONSE_ACCEPT,
                                                          nullptr);
    // Set icon chooser dialog size
    int width = xset_get_int("main_icon", XSetSetSet::X);
    int height = xset_get_int("main_icon", XSetSetSet::Y);
    if (width && height)
        gtk_window_set_default_size(GTK_WINDOW(icon_chooser), width, height);

    // Load current icon
    if (def_icon && def_icon[0])
        exo_icon_chooser_dialog_set_icon(EXO_ICON_CHOOSER_DIALOG(icon_chooser), def_icon);

    // Prompting user to pick icon
    int response_icon_chooser = gtk_dialog_run(GTK_DIALOG(icon_chooser));
    if (response_icon_chooser == GTK_RESPONSE_ACCEPT)
    {
        /* Fetching selected icon */
        icon = exo_icon_chooser_dialog_get_icon(EXO_ICON_CHOOSER_DIALOG(icon_chooser));
    }

    // Save icon chooser dialog size
    gtk_widget_get_allocation(GTK_WIDGET(icon_chooser), &allocation);
    if (allocation.width && allocation.height)
    {
        std::string str;

        str = fmt::format("{}", allocation.width);
        xset_set("main_icon", XSetSetSet::X, str.c_str());
        str = fmt::format("{}", allocation.height);
        xset_set("main_icon", XSetSetSet::Y, str.c_str());
    }
    gtk_widget_destroy(icon_chooser);

    // remove busy cursor
    gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(parent)), nullptr);

    return icon;
}

bool
xset_text_dialog(GtkWidget* parent, const std::string& title, const std::string& msg1,
                 const std::string& msg2, const char* defstring, char** answer,
                 const std::string& defreset, bool edit_care)
{
    GtkTextIter iter;
    GtkTextIter siter;
    GtkAllocation allocation;
    int width;
    int height;
    GtkWidget* dlgparent = nullptr;

    if (parent)
        dlgparent = gtk_widget_get_toplevel(parent);

    GtkWidget* dlg = gtk_message_dialog_new(GTK_WINDOW(dlgparent),
                                            GTK_DIALOG_MODAL,
                                            GTK_MESSAGE_QUESTION,
                                            GTK_BUTTONS_NONE,
                                            msg1.c_str(),
                                            nullptr);
    xset_set_window_icon(GTK_WINDOW(dlg));
    gtk_window_set_role(GTK_WINDOW(dlg), "text_dialog");

    width = xset_get_int("text_dlg", XSetSetSet::S);
    height = xset_get_int("text_dlg", XSetSetSet::Z);
    if (width && height)
        gtk_window_set_default_size(GTK_WINDOW(dlg), width, height);
    else
        gtk_window_set_default_size(GTK_WINDOW(dlg), 600, 400);
    // gtk_widget_set_size_request( GTK_WIDGET( dlg ), 600, 400 );

    gtk_window_set_resizable(GTK_WINDOW(dlg), true);

    if (!msg2.empty())
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), msg2.c_str(), nullptr);

    // input view
    GtkScrolledWindow* scroll_input =
        GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(nullptr, nullptr));
    GtkTextView* input = multi_input_new(scroll_input, defstring);
    GtkTextBuffer* buf = gtk_text_view_get_buffer(input);

    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg))),
                       GTK_WIDGET(scroll_input),
                       true,
                       true,
                       4);

    g_signal_connect(G_OBJECT(input), "key-press-event", G_CALLBACK(on_input_keypress), dlg);

    // buttons
    GtkWidget* btn_edit;
    GtkWidget* btn_default = nullptr;
    GtkWidget* btn_icon_choose = nullptr;

    if (edit_care)
    {
        btn_edit = gtk_toggle_button_new_with_mnemonic("_Edit");
        gtk_dialog_add_action_widget(GTK_DIALOG(dlg), btn_edit, GTK_RESPONSE_YES);
        gtk_widget_set_focus_on_click(GTK_WIDGET(btn_edit), false);
        gtk_text_view_set_editable(input, false);
    }

    /* Special hack to add an icon chooser button when this dialog is called
     * to set icons - see xset_menu_cb() and set init "main_icon"
     * and xset_design_job */
    if (ztd::same(title, "Set Icon") || ztd::same(title, "Set Window Icon"))
    {
        btn_icon_choose = gtk_button_new_with_mnemonic("C_hoose");
        gtk_dialog_add_action_widget(GTK_DIALOG(dlg), btn_icon_choose, GTK_RESPONSE_ACCEPT);
        gtk_widget_set_focus_on_click(GTK_WIDGET(btn_icon_choose), false);
    }

    if (!defreset.empty())
    {
        btn_default = gtk_button_new_with_mnemonic("_Default");
        gtk_dialog_add_action_widget(GTK_DIALOG(dlg), btn_default, GTK_RESPONSE_NO);
        gtk_widget_set_focus_on_click(GTK_WIDGET(btn_default), false);
    }

    GtkWidget* btn_cancel = gtk_button_new_with_label("Cancel");
    gtk_dialog_add_action_widget(GTK_DIALOG(dlg), btn_cancel, GTK_RESPONSE_CANCEL);

    GtkWidget* btn_ok = gtk_button_new_with_label("OK");
    gtk_dialog_add_action_widget(GTK_DIALOG(dlg), btn_ok, GTK_RESPONSE_OK);

    // show
    gtk_widget_show_all(dlg);

    gtk_window_set_title(GTK_WINDOW(dlg), title.c_str());

    if (edit_care)
    {
        gtk_widget_grab_focus(btn_ok);
        if (btn_default)
            gtk_widget_set_sensitive(btn_default, false);
    }

    char* ans;
    char* trim_ans;
    int response;
    char* icon;
    bool ret = false;
    while ((response = gtk_dialog_run(GTK_DIALOG(dlg))))
    {
        bool exit_loop = false;
        switch (response)
        {
            case GTK_RESPONSE_OK:
                gtk_text_buffer_get_start_iter(buf, &siter);
                gtk_text_buffer_get_end_iter(buf, &iter);
                ans = gtk_text_buffer_get_text(buf, &siter, &iter, false);
                if (strchr(ans, '\n'))
                {
                    ptk_show_error(GTK_WINDOW(dlgparent),
                                   "Error",
                                   "Your input is invalid because it contains linefeeds");
                }
                else
                {
                    if (*answer)
                        free(*answer);
                    trim_ans = ztd::strdup(ans);
                    trim_ans = g_strstrip(trim_ans);
                    if (ans && trim_ans[0] != '\0')
                        *answer = g_filename_from_utf8(trim_ans, -1, nullptr, nullptr, nullptr);
                    else
                        *answer = nullptr;
                    if (ans)
                    {
                        free(trim_ans);
                        free(ans);
                    }
                    ret = true;
                    exit_loop = true;
                    break;
                }

                break;
            case GTK_RESPONSE_YES:
                // btn_edit clicked
                gtk_text_view_set_editable(
                    input,
                    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn_edit)));
                if (btn_default)
                    gtk_widget_set_sensitive(
                        btn_default,
                        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn_edit)));
                exit_loop = true;
                break;
            case GTK_RESPONSE_ACCEPT:
                // get current icon
                gtk_text_buffer_get_start_iter(buf, &siter);
                gtk_text_buffer_get_end_iter(buf, &iter);
                icon = gtk_text_buffer_get_text(buf, &siter, &iter, false);

                // show icon chooser
                char* new_icon;
                new_icon = xset_icon_chooser_dialog(GTK_WINDOW(dlg), icon);
                free(icon);
                if (new_icon)
                {
                    gtk_text_buffer_set_text(buf, new_icon, -1);
                    free(new_icon);
                }
                exit_loop = true;
                break;
            case GTK_RESPONSE_NO:
                // btn_default clicked
                gtk_text_buffer_set_text(buf, defreset.c_str(), -1);
                exit_loop = true;
                break;
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            default:
                exit_loop = true;
                break;
        }
        if (exit_loop)
            break;
    }

    gtk_widget_get_allocation(GTK_WIDGET(dlg), &allocation);
    width = allocation.width;
    height = allocation.height;
    if (width && height)
    {
        std::string str;

        str = fmt::format("{}", width);
        xset_set("text_dlg", XSetSetSet::S, str.c_str());

        str = fmt::format("{}", height);
        xset_set("text_dlg", XSetSetSet::Z, str.c_str());
    }
    gtk_widget_destroy(dlg);
    return ret;
}

char*
xset_file_dialog(GtkWidget* parent, GtkFileChooserAction action, const char* title,
                 const char* deffolder, const char* deffile)
{
    char* path;
    /*  Actions:
     *      GTK_FILE_CHOOSER_ACTION_OPEN
     *      GTK_FILE_CHOOSER_ACTION_SAVE
     *      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
     *      GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER  */
    GtkWidget* dlgparent = parent ? gtk_widget_get_toplevel(parent) : nullptr;
    GtkWidget* dlg = gtk_file_chooser_dialog_new(title,
                                                 dlgparent ? GTK_WINDOW(dlgparent) : nullptr,
                                                 action,
                                                 "Cancel",
                                                 GTK_RESPONSE_CANCEL,
                                                 "OK",
                                                 GTK_RESPONSE_OK,
                                                 nullptr);
    // gtk_file_chooser_set_action( GTK_FILE_CHOOSER(dlg), GTK_FILE_CHOOSER_ACTION_SAVE );
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dlg), true);
    xset_set_window_icon(GTK_WINDOW(dlg));
    gtk_window_set_role(GTK_WINDOW(dlg), "file_dialog");

    if (deffolder)
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), deffolder);
    else
    {
        path = xset_get_s("go_set_default");
        if (path && path[0] != '\0')
            gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), path);
        else
        {
            path = (char*)vfs_user_home_dir().c_str();
            gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), path);
        }
    }
    if (deffile)
    {
        if (action == GTK_FILE_CHOOSER_ACTION_SAVE ||
            action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER)
            gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), deffile);
        else
        {
            path = g_build_filename(deffolder, deffile, nullptr);
            gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dlg), path);
            free(path);
        }
    }

    int width = xset_get_int("file_dlg", XSetSetSet::X);
    int height = xset_get_int("file_dlg", XSetSetSet::Y);
    if (width && height)
    {
        // filechooser will not honor default size or size request ?
        gtk_widget_show_all(dlg);
        gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER_ALWAYS);
        gtk_window_resize(GTK_WINDOW(dlg), width, height);
        while (gtk_events_pending())
            gtk_main_iteration();
        gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
    }

    int response = gtk_dialog_run(GTK_DIALOG(dlg));

    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(dlg), &allocation);
    width = allocation.width;
    height = allocation.height;
    if (width && height)
    {
        std::string str;

        str = fmt::format("{}", width);
        xset_set("file_dlg", XSetSetSet::X, str.c_str());
        str = fmt::format("{}", height);
        xset_set("file_dlg", XSetSetSet::Y, str.c_str());
    }

    if (response == GTK_RESPONSE_OK)
    {
        char* dest = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        gtk_widget_destroy(dlg);
        return dest;
    }
    gtk_widget_destroy(dlg);
    return nullptr;
}

static char*
xset_color_dialog(GtkWidget* parent, char* title, char* defcolor)
{
    (void)parent;
    GdkRGBA color;
    char* scolor = nullptr;
    GtkWidget* dlg = gtk_color_chooser_dialog_new(title, nullptr);
    GtkWidget* color_sel = nullptr;
    GtkWidget* help_button;

    g_object_get(G_OBJECT(dlg), "help-button", &help_button, nullptr);

    gtk_button_set_label(GTK_BUTTON(help_button), "_Unset");

    xset_set_window_icon(GTK_WINDOW(dlg));
    gtk_window_set_role(GTK_WINDOW(dlg), "color_dialog");

    if (defcolor && defcolor[0] != '\0')
    {
        /*
        if (gdk_rgba_parse(defcolor, &color))
        {
            // LOG_INFO("        gdk_rgba_to_string = {}", gdk_rgba_to_string(&color));
            gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(color_sel), &color);
        }
        */
        gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(color_sel), &color);
    }

    gtk_widget_show_all(dlg);
    int response = gtk_dialog_run(GTK_DIALOG(dlg));

    switch (response)
    {
        case GTK_RESPONSE_OK:
            gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(color_sel), &color);
            scolor = gdk_rgba_to_string(&color);
            break;
        default:
            // cancel, delete_event
            scolor = ztd::strdup(defcolor);
            break;
    }

    gtk_widget_destroy(dlg);
    return scolor;
}

static void
xset_builtin_tool_activate(XSetTool tool_type, XSet* set, GdkEventButton* event)
{
    XSet* set2;
    int p;
    char mode;
    PtkFileBrowser* file_browser = nullptr;
    FMMainWindow* main_window = fm_main_window_get_last_active();

    // set may be a submenu that does not match tool_type
    if (!(set && !set->lock && tool_type > XSetTool::CUSTOM))
    {
        LOG_WARN("xset_builtin_tool_activate invalid");
        return;
    }
    // LOG_INFO("xset_builtin_tool_activate  {}", set->menu_label);

    // get current browser, panel, and mode
    if (main_window)
    {
        file_browser = PTK_FILE_BROWSER(fm_main_window_get_current_file_browser(main_window));
        p = file_browser->mypanel;
        mode = main_window->panel_context[p - 1];
    }
    if (!PTK_IS_FILE_BROWSER(file_browser))
        return;

    switch (tool_type)
    {
        case XSetTool::DEVICES:
            set2 = xset_get_panel_mode(p, "show_devmon", mode);
            set2->b = set2->b == XSetB::XSET_B_TRUE ? XSetB::XSET_B_UNSET : XSetB::XSET_B_TRUE;
            update_views_all_windows(nullptr, file_browser);
            break;
        case XSetTool::BOOKMARKS:
            set2 = xset_get_panel_mode(p, "show_book", mode);
            set2->b = set2->b == XSetB::XSET_B_TRUE ? XSetB::XSET_B_UNSET : XSetB::XSET_B_TRUE;
            update_views_all_windows(nullptr, file_browser);
            if (file_browser->side_book)
            {
                ptk_bookmark_view_chdir(GTK_TREE_VIEW(file_browser->side_book), file_browser, true);
                gtk_widget_grab_focus(GTK_WIDGET(file_browser->side_book));
            }
            break;
        case XSetTool::TREE:
            set2 = xset_get_panel_mode(p, "show_dirtree", mode);
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
            set2 = xset_get_panel(p, "show_hidden");
            set2->b = set2->b == XSetB::XSET_B_TRUE ? XSetB::XSET_B_UNSET : XSetB::XSET_B_TRUE;
            ptk_file_browser_show_hidden_files(file_browser, set2->b);
            break;
        case XSetTool::SHOW_THUMB:
            main_window_toggle_thumbnails_all_windows();
            break;
        case XSetTool::LARGE_ICONS:
            if (file_browser->view_mode != PtkFBViewMode::PTK_FB_ICON_VIEW)
            {
                xset_set_b_panel(p, "list_large", !file_browser->large_icons);
                on_popup_list_large(nullptr, file_browser);
            }
            break;
        case XSetTool::NOT:
        case XSetTool::CUSTOM:
        case XSetTool::INVALID:
        default:
            LOG_WARN("xset_builtin_tool_activate invalid tool_type");
    }
}

const char*
xset_get_builtin_toolitem_label(XSetTool tool_type)
{
    if (tool_type < XSetTool::DEVICES || tool_type >= XSetTool::INVALID)
        return nullptr;
    return builtin_tool_name[static_cast<int>(tool_type)];
}

static XSet*
xset_new_builtin_toolitem(XSetTool tool_type)
{
    if (tool_type < XSetTool::DEVICES || tool_type >= XSetTool::INVALID)
        return nullptr;

    XSet* set = xset_custom_new();
    set->tool = tool_type;
    set->task = false;
    set->task_err = false;
    set->task_out = false;
    set->keep_terminal = false;

    return set;
}

static bool
on_tool_icon_button_press(GtkWidget* widget, GdkEventButton* event, XSet* set)
{
    XSetJob job = XSetJob::INVALID;

    // LOG_INFO("on_tool_icon_button_press  {}   button = {}", set->menu_label, event->button);
    if (event->type != GDK_BUTTON_PRESS)
        return false;
    unsigned int keymod = ptk_get_keymod(event->state);

    // get and focus browser
    PtkFileBrowser* file_browser =
        static_cast<PtkFileBrowser*>(g_object_get_data(G_OBJECT(widget), "browser"));
    if (!PTK_IS_FILE_BROWSER(file_browser))
        return true;
    ptk_file_browser_focus_me(file_browser);
    set->browser = file_browser;

    // get context
    XSetContext* context = xset_context_new();
    main_context_fill(file_browser, context);
    if (!context->valid)
        return true;

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
                                XSet* set_child = xset_is(set->child);
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
                            xset_builtin_tool_activate(XSetTool::BACK, set, event);
                        else if (set->tool == XSetTool::FWD_MENU)
                            xset_builtin_tool_activate(XSetTool::FWD, set, event);
                        else if (set->tool != XSetTool::NOT)
                            xset_builtin_tool_activate(set->tool, set, event);
                        return true;
                    }
                    else // if ( event->button == 3 )
                    {
                        // right-click show design menu for submenu set
                        xset_design_cb(nullptr, event, set);
                        return true;
                    }
                    break;
                case GDK_CONTROL_MASK:
                    // ctrl
                    job = XSetJob::COPY;
                    break;
                case GDK_MOD1_MASK:
                    // alt
                    job = XSetJob::CUT;
                    break;
                case GDK_SHIFT_MASK:
                    // shift
                    job = XSetJob::PASTE;
                    break;
                case (GDK_CONTROL_MASK | GDK_SHIFT_MASK):
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
                        XSetCMD(xset_get_int_set(set, XSetSetSet::X)) == XSetCMD::SCRIPT)
                        job = XSetJob::EDIT;
                    else
                        job = XSetJob::PROP_CMD;
                    break;
                case GDK_CONTROL_MASK:
                    // ctrl
                    job = XSetJob::KEY;
                    break;
                case GDK_MOD1_MASK:
                    // alt
                    break;
                case GDK_SHIFT_MASK:
                    // shift
                    job = XSetJob::ICON;
                    break;
                case (GDK_CONTROL_MASK | GDK_SHIFT_MASK):
                    // ctrl + shift
                    job = XSetJob::REMOVE;
                    break;
                case (GDK_CONTROL_MASK | GDK_MOD1_MASK):
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
on_tool_menu_button_press(GtkWidget* widget, GdkEventButton* event, XSet* set)
{
    // LOG_INFO("on_tool_menu_button_press  {}   button = {}", set->menu_label, event->button);
    if (event->type != GDK_BUTTON_PRESS)
        return false;
    unsigned int keymod = ptk_get_keymod(event->state);
    if (keymod != 0 || event->button != 1)
        return on_tool_icon_button_press(widget, event, set);

    // get and focus browser
    PtkFileBrowser* file_browser =
        static_cast<PtkFileBrowser*>(g_object_get_data(G_OBJECT(widget), "browser"));
    if (!PTK_IS_FILE_BROWSER(file_browser))
        return true;
    ptk_file_browser_focus_me(file_browser);

    // get context
    XSetContext* context = xset_context_new();
    main_context_fill(file_browser, context);
    if (!context->valid)
        return true;

    if (event->button == 1)
    {
        if (set->tool == XSetTool::CUSTOM)
        {
            // show custom submenu
            XSet* set_child;
            if (!(set && !set->lock && set->child && set->menu_style == XSetMenu::SUBMENU &&
                  (set_child = xset_is(set->child))))
                return true;
            GtkWidget* menu = gtk_menu_new();
            GtkAccelGroup* accel_group = gtk_accel_group_new();
            xset_add_menuitem(file_browser, menu, accel_group, set_child);
            gtk_widget_show_all(GTK_WIDGET(menu));
            gtk_menu_popup_at_pointer(GTK_MENU(menu), nullptr);
        }
        else
            xset_builtin_tool_activate(set->tool, set, event);
        return true;
    }
    return true;
}

static void
set_gtk3_widget_padding(GtkWidget* widget, int left_right, int top_bottom)
{
    std::string str = fmt::format("GtkWidget {{ padding-left: {}px; padding-right: {}px; "
                                  "padding-top: {}px; padding-bottom: {}px; }}",
                                  left_right,
                                  left_right,
                                  top_bottom,
                                  top_bottom);

    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(provider), str.c_str(), -1, nullptr);
    GtkStyleContext* context = gtk_widget_get_style_context(widget);
    gtk_style_context_add_provider(context,
                                   GTK_STYLE_PROVIDER(provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static GtkWidget*
xset_add_toolitem(GtkWidget* parent, PtkFileBrowser* file_browser, GtkWidget* toolbar,
                  int icon_size, XSet* set, bool show_tooltips)
{
    if (!set)
        return nullptr;

    if (set->lock)
        return nullptr;

    if (set->tool == XSetTool::NOT)
    {
        LOG_WARN("xset_add_toolitem set->tool == XSetTool::NOT");
        set->tool = XSetTool::CUSTOM;
    }

    GtkWidget* image = nullptr;
    GtkWidget* item = nullptr;
    GtkWidget* btn;
    XSet* set_next;
    char* new_menu_label = nullptr;
    GdkPixbuf* pixbuf = nullptr;
    char* icon_file = nullptr;
    std::string str;

    // get real icon size from gtk icon size
    int icon_w, icon_h;
    gtk_icon_size_lookup((GtkIconSize)icon_size, &icon_w, &icon_h);
    int real_icon_size = icon_w > icon_h ? icon_w : icon_h;

    set->browser = file_browser;

    // builtin toolitems set shared_key on build
    if (set->tool >= XSetTool::INVALID)
    {
        // looks like an unknown built-in toolitem from a future version - skip
        if (set->next)
        {
            set_next = xset_is(set->next);
            // LOG_INFO("    NEXT {}", set_next->name);
            xset_add_toolitem(parent, file_browser, toolbar, icon_size, set_next, show_tooltips);
        }
        return item;
    }
    if (set->tool > XSetTool::CUSTOM && set->tool < XSetTool::INVALID && !set->shared_key)
        set->shared_key = ztd::strdup(builtin_tool_shared_key[static_cast<int>(set->tool)]);

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
        icon_file = g_build_filename(xset_get_config_dir(), "scripts", set->name, "icon", nullptr);
        if (!std::filesystem::exists(icon_file))
        {
            free(icon_file);
            icon_file = nullptr;
        }
        else
            icon_name = icon_file;
    }

    char* menu_label;
    menu_label = set->menu_label;
    if (!menu_label && set->tool > XSetTool::CUSTOM)
        menu_label = (char*)xset_get_builtin_toolitem_label(set->tool);

    if (menu_style == XSetMenu::NORMAL)
        menu_style = XSetMenu::STRING;

    GtkWidget* ebox;
    GtkWidget* hbox;
    XSet* set_child;
    XSetCMD cmd_type;

    switch (menu_style)
    {
        case XSetMenu::STRING:
            // normal item
            cmd_type = XSetCMD(xset_get_int_set(set, XSetSetSet::X));
            if (set->tool > XSetTool::CUSTOM)
            {
                // builtin tool item
                if (icon_name)
                    image = xset_get_image(icon_name, (GtkIconSize)icon_size);
                else if (set->tool > XSetTool::CUSTOM && set->tool < XSetTool::INVALID)
                    image = xset_get_image(builtin_tool_icon[static_cast<int>(set->tool)],
                                           (GtkIconSize)icon_size);
            }
            else if (!set->lock && cmd_type == XSetCMD::APP)
            {
                // Application
                new_menu_label = xset_custom_get_app_name_icon(set, &pixbuf, real_icon_size);
            }
            else if (!set->lock && cmd_type == XSetCMD::BOOKMARK)
            {
                // Bookmark
                pixbuf = xset_custom_get_bookmark_icon(set, real_icon_size);
                if (!(set->menu_label && set->menu_label[0]))
                    new_menu_label = ztd::strdup(set->z);
            }

            if (pixbuf)
            {
                image = gtk_image_new_from_pixbuf(pixbuf);
                g_object_unref(pixbuf);
            }
            if (!image)
                image =
                    xset_get_image(icon_name ? icon_name : "gtk-execute", (GtkIconSize)icon_size);
            if (!new_menu_label)
                new_menu_label = ztd::strdup(menu_label);

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
                gtk_widget_set_tooltip_text(ebox, str.c_str());
            }
            free(new_menu_label);
            break;
        case XSetMenu::CHECK:
            if (!icon_name && set->tool > XSetTool::CUSTOM && set->tool < XSetTool::INVALID)
                // builtin tool item
                image = xset_get_image(builtin_tool_icon[static_cast<int>(set->tool)],
                                       (GtkIconSize)icon_size);
            else
                image =
                    xset_get_image(icon_name ? icon_name : "gtk-execute", (GtkIconSize)icon_size);

            // cannot use gtk_tool_button_new because icon does not obey size
            // btn = GTK_WIDGET( gtk_toggle_tool_button_new() );
            // gtk_tool_button_set_icon_widget( GTK_TOOL_BUTTON( btn ), image );
            // gtk_tool_button_set_label( GTK_TOOL_BUTTON( btn ), set->menu_label );
            btn = gtk_toggle_button_new();
            gtk_widget_show(image);
            gtk_button_set_image(GTK_BUTTON(btn), image);
            gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btn), xset_get_b_set(set));
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
                gtk_widget_set_tooltip_text(ebox, str.c_str());
            }
            break;
        case XSetMenu::SUBMENU:
            menu_label = nullptr;
            // create a tool button
            set_child = nullptr;
            if (set->child && set->tool == XSetTool::CUSTOM)
                set_child = xset_is(set->child);

            if (!icon_name && set_child && set_child->icon)
                // take the user icon from the first item in the submenu
                icon_name = set_child->icon;
            else if (!icon_name && set->tool > XSetTool::CUSTOM && set->tool < XSetTool::INVALID)
                icon_name = builtin_tool_icon[static_cast<int>(set->tool)];
            else if (!icon_name && set_child && set->tool == XSetTool::CUSTOM)
            {
                // take the auto icon from the first item in the submenu
                cmd_type = XSetCMD(xset_get_int_set(set_child, XSetSetSet::X));
                switch (cmd_type)
                {
                    case XSetCMD::APP:
                        // Application
                        new_menu_label = menu_label =
                            xset_custom_get_app_name_icon(set_child, &pixbuf, real_icon_size);
                        break;
                    case XSetCMD::BOOKMARK:
                        // Bookmark
                        pixbuf = xset_custom_get_bookmark_icon(set_child, real_icon_size);
                        if (!(set_child->menu_label && set_child->menu_label[0]))
                            menu_label = set_child->z;
                        break;
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
                        menu_label = (char*)builtin_tool_name[static_cast<int>(XSetTool::BACK)];
                        break;
                    case XSetTool::FWD_MENU:
                        menu_label = (char*)builtin_tool_name[static_cast<int>(XSetTool::FWD)];
                        break;
                    case XSetTool::CUSTOM:
                        if (set_child)
                            menu_label = set_child->menu_label;
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
                            menu_label = (char*)xset_get_builtin_toolitem_label(set->tool);
                        else
                            menu_label = set->menu_label;
                        break;
                }
            }

            if (!image)
                image =
                    xset_get_image(icon_name ? icon_name : "gtk-directory", (GtkIconSize)icon_size);

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
            hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
            gtk_box_pack_start(GTK_BOX(hbox), ebox, false, false, 0);
            // tooltip
            if (show_tooltips)
            {
                str = clean_label(menu_label, false, false);
                gtk_widget_set_tooltip_text(ebox, str.c_str());
            }
            free(new_menu_label);

            // reset menu_label for below
            menu_label = set->menu_label;
            if (!menu_label && set->tool > XSetTool::CUSTOM)
                menu_label = (char*)xset_get_builtin_toolitem_label(set->tool);

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
                gtk_widget_reparent(btn, ebox);
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
                gtk_widget_set_tooltip_text(ebox, str.c_str());
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
        case XSetMenu::COLORDLG:
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

    free(icon_file);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(item), -1);

    // LOG_INFO("    set={}   set->next={}", set->name, set->next);
    // next toolitem
    if (set->next)
    {
        set_next = xset_is(set->next);
        // LOG_INFO("    NEXT {}", set_next->name);
        xset_add_toolitem(parent, file_browser, toolbar, icon_size, set_next, show_tooltips);
    }

    return item;
}

void
xset_fill_toolbar(GtkWidget* parent, PtkFileBrowser* file_browser, GtkWidget* toolbar,
                  XSet* set_parent, bool show_tooltips)
{
    const std::array<XSetTool, 7> default_tools{XSetTool::BOOKMARKS,
                                                XSetTool::TREE,
                                                XSetTool::NEW_TAB_HERE,
                                                XSetTool::BACK_MENU,
                                                XSetTool::FWD_MENU,
                                                XSetTool::UP,
                                                XSetTool::DEFAULT};
    int i;
    int stop_b4;
    XSet* set;
    XSet* set_target;

    // LOG_INFO("xset_fill_toolbar {}", set_parent->name);
    if (!(file_browser && toolbar && set_parent))
        return;

    set_parent->lock = true;
    set_parent->menu_style = XSetMenu::SUBMENU;

    GtkIconSize icon_size = gtk_toolbar_get_icon_size(GTK_TOOLBAR(toolbar));

    XSet* set_child = nullptr;
    if (set_parent->child)
        set_child = xset_is(set_parent->child);
    if (!set_child)
    {
        // toolbar is empty - add default items
        set_child = xset_new_builtin_toolitem(
            strstr(set_parent->name, "tool_r") ? XSetTool::REFRESH : XSetTool::DEVICES);
        set_parent->child = ztd::strdup(set_child->name);
        set_child->parent = ztd::strdup(set_parent->name);
        if (!strstr(set_parent->name, "tool_r"))
        {
            if (strstr(set_parent->name, "tool_s"))
                stop_b4 = 2;
            else
                stop_b4 = default_tools.size();
            set_target = set_child;
            for (i = 0; i < stop_b4; i++)
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
    XSet* set = xset_get("main_icon");
    if (set->icon)
        name = set->icon;
    else if (geteuid() == 0)
        name = "spacefm-root";
    else
        name = "spacefm";
    GtkIconTheme* icon_theme = gtk_icon_theme_get_default();
    if (!icon_theme)
        return;
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
        LOG_ERROR("Unable to load the window icon '{}' in - xset_set_window_icon - {}",
                  name,
                  error->message);
        g_error_free(error);
    }
}

static void
xset_defaults()
{
    XSet* set;

    // set_last must be set (to anything)
    set_last = xset_get("separator");
    set_last->menu_style = XSetMenu::SEP;

    // separator
    set = xset_get("separator");
    set->menu_style = XSetMenu::SEP;

    // dev menu
    set = xset_set("dev_menu_remove", XSetSetSet::MENU_LABEL, "Remo_ve / Eject");
    xset_set_set(set, XSetSetSet::ICN, "gtk-disconnect");

    set = xset_set("dev_menu_unmount", XSetSetSet::MENU_LABEL, "_Unmount");
    xset_set_set(set, XSetSetSet::ICN, "gtk-remove");

    set = xset_set("dev_menu_open", XSetSetSet::MENU_LABEL, "_Open");
    xset_set_set(set, XSetSetSet::ICN, "gtk-open");

    set = xset_set("dev_menu_tab", XSetSetSet::MENU_LABEL, "Open In _Tab");
    xset_set_set(set, XSetSetSet::ICN, "gtk-add");

    set = xset_set("dev_menu_mount", XSetSetSet::MENU_LABEL, "_Mount");
    xset_set_set(set, XSetSetSet::ICN, "drive-removable-media");

    set = xset_set("dev_menu_mark", XSetSetSet::MENU_LABEL, "_Bookmark");
    xset_set_set(set, XSetSetSet::ICN, "gtk-add");

    set = xset_set("dev_prop", XSetSetSet::MENU_LABEL, "_Properties");
    xset_set_set(set, XSetSetSet::ICN, "gtk-properties");

    set = xset_set("dev_menu_settings", XSetSetSet::MENU_LABEL, "Setti_ngs");
    xset_set_set(set, XSetSetSet::ICN, "gtk-properties");
    set->menu_style = XSetMenu::SUBMENU;

    // dev settings
    set = xset_set("dev_show", XSetSetSet::MENU_LABEL, "S_how");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "dev_show_internal_drives dev_show_empty dev_show_partition_tables dev_show_net "
                 "dev_show_file dev_ignore_udisks_hide dev_show_hide_volumes dev_dispname");

    set = xset_set("dev_show_internal_drives", XSetSetSet::MENU_LABEL, "_Internal Drives");
    set->menu_style = XSetMenu::CHECK;
    set->b = geteuid() == 0 ? XSetB::XSET_B_TRUE : XSetB::XSET_B_FALSE;

    set = xset_set("dev_show_empty", XSetSetSet::MENU_LABEL, "_Empty Drives");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE; // geteuid() == 0 ? XSetB::XSET_B_TRUE : XSetB::XSET_B_UNSET;

    set = xset_set("dev_show_partition_tables", XSetSetSet::MENU_LABEL, "_Partition Tables");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set("dev_show_net", XSetSetSet::MENU_LABEL, "Mounted _Networks");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("dev_show_file", XSetSetSet::MENU_LABEL, "Mounted _Other");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("dev_show_hide_volumes", XSetSetSet::MENU_LABEL, "_Volumes...");
    xset_set_set(set, XSetSetSet::TITLE, "Show/Hide Volumes");
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "To force showing or hiding of some volumes, overriding other settings, you can specify "
        "the devices, volume labels, or device IDs in the space-separated list "
        "below.\n\nExample:  +/dev/sdd1 -Label With Space +ata-OCZ-part4\nThis would cause "
        "/dev/sdd1 and the OCZ device to be shown, and the volume with label \"Label With "
        "Space\" to be hidden.\n\nThere must be a space between entries and a plus or minus sign "
        "directly before each item.  This list is case-sensitive.\n\n");

    set = xset_set("dev_ignore_udisks_hide", XSetSetSet::MENU_LABEL, "Ignore _Hide Policy");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set("dev_dispname", XSetSetSet::MENU_LABEL, "_Display Name");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Set Display Name Format");
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "Enter device display name format:\n\nUse:\n\t%%v\tdevice filename (eg "
                 "sdd1)\n\t%%s\ttotal size (eg 800G)\n\t%%t\tfstype (eg ext4)\n\t%%l\tvolume "
                 "label (eg Label or [no media])\n\t%%m\tmount point if mounted, or "
                 "---\n\t%%i\tdevice ID\n\t%%n\tmajor:minor device numbers (eg 15:3)\n");
    xset_set_set(set, XSetSetSet::S, "%v %s %l %m");
    xset_set_set(set, XSetSetSet::Z, "%v %s %l %m");
    xset_set_set(set, XSetSetSet::ICN, "gtk-edit");

    set = xset_set("dev_menu_auto", XSetSetSet::MENU_LABEL, "_Auto Mount");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "dev_automount_optical dev_automount_removable dev_ignore_udisks_nopolicy "
                 "dev_automount_volumes dev_automount_dirs dev_auto_open dev_unmount_quit");

    set = xset_set("dev_automount_optical", XSetSetSet::MENU_LABEL, "Mount _Optical");
    set->b = geteuid() == 0 ? XSetB::XSET_B_FALSE : XSetB::XSET_B_TRUE;
    set->menu_style = XSetMenu::CHECK;

    set = xset_set("dev_automount_removable", XSetSetSet::MENU_LABEL, "_Mount Removable");
    set->b = geteuid() == 0 ? XSetB::XSET_B_FALSE : XSetB::XSET_B_TRUE;
    set->menu_style = XSetMenu::CHECK;

    set = xset_set("dev_automount_volumes", XSetSetSet::MENU_LABEL, "Mount _Volumes...");
    xset_set_set(set, XSetSetSet::TITLE, "Auto-Mount Volumes");
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "To force or prevent automounting of some volumes, overriding other settings, you can "
        "specify the devices, volume labels, or device IDs in the space-separated list "
        "below.\n\nExample:  +/dev/sdd1 -Label With Space +ata-OCZ-part4\nThis would cause "
        "/dev/sdd1 and the OCZ device to be auto-mounted when detected, and the volume with "
        "label \"Label With Space\" to be ignored.\n\nThere must be a space between entries and "
        "a plus or minus sign directly before each item.  This list is case-sensitive.\n\n");

    set = xset_set("dev_automount_dirs", XSetSetSet::MENU_LABEL, "Mount _Dirs...");
    xset_set_set(set, XSetSetSet::TITLE, "Automatic Mount Point Dirs");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "Enter the directory where SpaceFM should automatically create mount point directories "
        "for fuse and similar filesystems (%%a in handler commands).  This directory must be "
        "user-writable (do NOT use /media), and empty subdirectories will be removed.  If left "
        "blank, ~/.cache/spacefm/ (or $XDG_CACHE_HOME/spacefm/) is used.  The following "
        "variables are recognized: $USER $UID $HOME $XDG_RUNTIME_DIR $XDG_CACHE_HOME\n\nNote "
        "that some handlers or mount programs may not obey this setting.\n");

    set = xset_set("dev_auto_open", XSetSetSet::MENU_LABEL, "Open _Tab");
    set->b = XSetB::XSET_B_TRUE;
    set->menu_style = XSetMenu::CHECK;

    set = xset_set("dev_unmount_quit", XSetSetSet::MENU_LABEL, "_Unmount On Exit");
    set->b = XSetB::XSET_B_UNSET;
    set->menu_style = XSetMenu::CHECK;

    set = xset_set("dev_exec", XSetSetSet::MENU_LABEL, "Auto _Run");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "dev_exec_fs dev_exec_audio dev_exec_video separator dev_exec_insert "
                 "dev_exec_unmount dev_exec_remove");
    xset_set_set(set, XSetSetSet::ICN, "gtk-execute");

    set = xset_set("dev_exec_fs", XSetSetSet::MENU_LABEL, "On _Mount");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Auto Run On Mount");
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "Enter program or bash command line to be run automatically after a removable "
                 "drive or data disc is auto-mounted:\n\nUse:\n\t%%v\tdevice (eg "
                 "/dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_set("dev_exec_audio", XSetSetSet::MENU_LABEL, "On _Audio CD");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Auto Run On Audio CD");
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "Enter program or bash command line to be run automatically when an audio CD is "
                 "inserted in a qualified device:\n\nUse:\n\t%%v\tdevice (eg "
                 "/dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_set("dev_exec_video", XSetSetSet::MENU_LABEL, "On _Video DVD");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Auto Run On Video DVD");
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "Enter program or bash command line to be run automatically when a video DVD is "
                 "auto-mounted:\n\nUse:\n\t%%v\tdevice (eg /dev/sda1)\n\t%%l\tdevice "
                 "label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_set("dev_exec_insert", XSetSetSet::MENU_LABEL, "On _Insert");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Auto Run On Insert");
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "Enter program or bash command line to be run automatically when any device is "
                 "inserted:\n\nUse:\n\t%%v\tdevice added (eg /dev/sda1)\n\t%%l\tdevice "
                 "label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_set("dev_exec_unmount", XSetSetSet::MENU_LABEL, "On _Unmount");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Auto Run On Unmount");
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "Enter program or bash command line to be run automatically when any device is "
                 "unmounted by any means:\n\nUse:\n\t%%v\tdevice unmounted (eg "
                 "/dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_set("dev_exec_remove", XSetSetSet::MENU_LABEL, "On _Remove");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Auto Run On Remove");
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "Enter program or bash command line to be run automatically when any device is removed "
        "(ejection of media does not qualify):\n\nUse:\n\t%%v\tdevice removed (eg "
        "/dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_set("dev_ignore_udisks_nopolicy", XSetSetSet::MENU_LABEL, "Ignore _No Policy");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set("dev_mount_options", XSetSetSet::MENU_LABEL, "_Mount Options");
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "Enter your comma- or space-separated list of default mount options below (%%o in "
        "handlers).\n\nIn addition to regular options, you can also specify options to be added "
        "or removed for a specific filesystem type by using the form OPTION+FSTYPE or "
        "OPTION-FSTYPE.\n\nExample:  nosuid, sync+vfat, sync+ntfs, noatime, noatime-ext4\nThis "
        "will add nosuid and noatime for all filesystem types, add sync for vfat and ntfs only, "
        "and remove noatime for ext4.\n\nNote: Some options, such as nosuid, may be added by the "
        "mount program even if you do not include them.  Options in fstab take precedence.  "
        "pmount and some handlers may ignore options set here.");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Default Mount Options");
    xset_set_set(set, XSetSetSet::S, "noexec, nosuid, noatime");
    xset_set_set(set, XSetSetSet::Z, "noexec, nosuid, noatime");
    xset_set_set(set, XSetSetSet::ICN, "gtk-edit");

    set = xset_set("dev_change", XSetSetSet::MENU_LABEL, "_Change Detection");
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "Enter your comma- or space-separated list of filesystems which should NOT be monitored "
        "for file changes.  This setting only affects non-block devices (such as nfs or fuse), "
        "and is usually used to prevent SpaceFM becoming unresponsive with network filesystems.  "
        "Loading of thumbnails and subdirectory sizes will also be disabled.");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Change Detection Blacklist");
    xset_set_set(set, XSetSetSet::ICN, "gtk-edit");
    set->s = ztd::strdup("cifs curlftpfs ftpfs fuse.sshfs nfs smbfs");
    set->z = ztd::strdup(set->s);

    set = xset_set("dev_fs_cnf", XSetSetSet::MENU_LABEL, "_Device Handlers");
    xset_set_set(set, XSetSetSet::ICON, "gtk-preferences");

    set = xset_set("dev_net_cnf", XSetSetSet::MENU_LABEL, "_Protocol Handlers");
    xset_set_set(set, XSetSetSet::ICON, "gtk-preferences");

    // dev icons
    set = xset_set("dev_icon", XSetSetSet::MENU_LABEL, "_Icon");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "dev_icon_internal_mounted dev_icon_internal_unmounted separator dev_icon_remove_mounted "
        "dev_icon_remove_unmounted separator dev_icon_optical_mounted dev_icon_optical_media "
        "dev_icon_optical_nomedia dev_icon_audiocd separator dev_icon_floppy_mounted "
        "dev_icon_floppy_unmounted separator dev_icon_network dev_icon_file");

    set = xset_set("dev_icon_audiocd", XSetSetSet::MENU_LABEL, "Audio CD");
    set->menu_style = XSetMenu::ICON;
    xset_set_set(set, XSetSetSet::ICN, "gtk-cdrom");

    set = xset_set("dev_icon_optical_mounted", XSetSetSet::MENU_LABEL, "Optical Mounted");
    set->menu_style = XSetMenu::ICON;
    xset_set_set(set, XSetSetSet::ICN, "gtk-cdrom");

    set = xset_set("dev_icon_optical_media", XSetSetSet::MENU_LABEL, "Optical Has Media");
    set->menu_style = XSetMenu::ICON;
    xset_set_set(set, XSetSetSet::ICN, "gtk-yes");

    set = xset_set("dev_icon_optical_nomedia", XSetSetSet::MENU_LABEL, "Optical No Media");
    set->menu_style = XSetMenu::ICON;
    xset_set_set(set, XSetSetSet::ICN, "gtk-close");

    set = xset_set("dev_icon_floppy_mounted", XSetSetSet::MENU_LABEL, "Floppy Mounted");
    set->menu_style = XSetMenu::ICON;
    xset_set_set(set, XSetSetSet::ICN, "gtk-floppy");

    set = xset_set("dev_icon_floppy_unmounted", XSetSetSet::MENU_LABEL, "Floppy Unmounted");
    set->menu_style = XSetMenu::ICON;
    xset_set_set(set, XSetSetSet::ICN, "gtk-floppy");

    set = xset_set("dev_icon_remove_mounted", XSetSetSet::MENU_LABEL, "Removable Mounted");
    set->menu_style = XSetMenu::ICON;
    xset_set_set(set, XSetSetSet::ICN, "gtk-add");

    set = xset_set("dev_icon_remove_unmounted", XSetSetSet::MENU_LABEL, "Removable Unmounted");
    set->menu_style = XSetMenu::ICON;
    xset_set_set(set, XSetSetSet::ICN, "gtk-remove");

    set = xset_set("dev_icon_internal_mounted", XSetSetSet::MENU_LABEL, "Internal Mounted");
    set->menu_style = XSetMenu::ICON;
    xset_set_set(set, XSetSetSet::ICN, "gtk-open");

    set = xset_set("dev_icon_internal_unmounted", XSetSetSet::MENU_LABEL, "Internal Unmounted");
    set->menu_style = XSetMenu::ICON;
    xset_set_set(set, XSetSetSet::ICN, "gtk-harddisk");

    set = xset_set("dev_icon_network", XSetSetSet::MENU_LABEL, "Mounted Network");
    set->menu_style = XSetMenu::ICON;
    xset_set_set(set, XSetSetSet::ICN, "gtk-network");

    set = xset_set("dev_icon_file", XSetSetSet::MENU_LABEL, "Mounted Other");
    set->menu_style = XSetMenu::ICON;
    xset_set_set(set, XSetSetSet::ICN, "gtk-file");

    set = xset_set("book_open", XSetSetSet::MENU_LABEL, "_Open");
    xset_set_set(set, XSetSetSet::ICN, "gtk-open");

    set = xset_set("book_settings", XSetSetSet::MENU_LABEL, "_Settings");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set, XSetSetSet::ICN, "gtk-properties");

    set = xset_set("book_icon", XSetSetSet::MENU_LABEL, "Bookmark _Icon");
    set->menu_style = XSetMenu::ICON;
    // do not set a default icon for book_icon

    set = xset_set("book_menu_icon", XSetSetSet::MENU_LABEL, "Sub_menu Icon");
    set->menu_style = XSetMenu::ICON;
    // do not set a default icon for book_menu_icon

    set = xset_set("book_show", XSetSetSet::MENU_LABEL, "_Show Bookmarks");
    set->menu_style = XSetMenu::CHECK;
    xset_set_set(set, XSetSetSet::SHARED_KEY, "panel1_show_book");

    set = xset_set("book_add", XSetSetSet::MENU_LABEL, "New _Bookmark");
    xset_set_set(set, XSetSetSet::ICN, "gtk-jump-to");

    set = xset_set("main_book", XSetSetSet::MENU_LABEL, "_Bookmarks");
    xset_set_set(set, XSetSetSet::ICN, "gtk-directory");
    set->menu_style = XSetMenu::SUBMENU;

    // Rename/Move Dialog
    set = xset_set("move_name", XSetSetSet::MENU_LABEL, "_Name");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set("move_filename", XSetSetSet::MENU_LABEL, "F_ilename");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("move_parent", XSetSetSet::MENU_LABEL, "_Parent");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set("move_path", XSetSetSet::MENU_LABEL, "P_ath");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("move_type", XSetSetSet::MENU_LABEL, "Typ_e");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("move_target", XSetSetSet::MENU_LABEL, "Ta_rget");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("move_template", XSetSetSet::MENU_LABEL, "Te_mplate");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("move_option", XSetSetSet::MENU_LABEL, "_Option");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set, XSetSetSet::DESC, "move_copy move_link move_copyt move_linkt move_as_root");

    set = xset_set("move_copy", XSetSetSet::MENU_LABEL, "_Copy");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("move_link", XSetSetSet::MENU_LABEL, "_Link");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("move_copyt", XSetSetSet::MENU_LABEL, "Copy _Target");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set("move_linkt", XSetSetSet::MENU_LABEL, "Lin_k Target");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set("move_as_root", XSetSetSet::MENU_LABEL, "_As Root");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("move_dlg_help", XSetSetSet::MENU_LABEL, "_Help");
    xset_set_set(set, XSetSetSet::ICN, "gtk-help");

    set = xset_set("move_dlg_confirm_create", XSetSetSet::MENU_LABEL, "_Confirm Create");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    // status bar
    set = xset_set("status_middle", XSetSetSet::MENU_LABEL, "_Middle Click");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set, XSetSetSet::DESC, "status_name status_path status_info status_hide");

    set = xset_set("status_name", XSetSetSet::MENU_LABEL, "Copy _Name");
    set->menu_style = XSetMenu::RADIO;

    set = xset_set("status_path", XSetSetSet::MENU_LABEL, "Copy _Path");
    set->menu_style = XSetMenu::RADIO;

    set = xset_set("status_info", XSetSetSet::MENU_LABEL, "File _Info");
    set->menu_style = XSetMenu::RADIO;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("status_hide", XSetSetSet::MENU_LABEL, "_Hide Panel");
    set->menu_style = XSetMenu::RADIO;

    // MAIN WINDOW MENUS

    // File
    set = xset_set("main_new_window", XSetSetSet::MENU_LABEL, "New _Window");
    xset_set_set(set, XSetSetSet::ICN, "spacefm");

    set = xset_set("main_root_window", XSetSetSet::MENU_LABEL, "R_oot Window");
    xset_set_set(set, XSetSetSet::ICN, "gtk-dialog-warning");

    set = xset_set("main_search", XSetSetSet::MENU_LABEL, "_File Search");
    xset_set_set(set, XSetSetSet::ICN, "gtk-find");

    set = xset_set("main_terminal", XSetSetSet::MENU_LABEL, "_Terminal");
    set->b = XSetB::XSET_B_UNSET; // discovery notification

    set = xset_set("main_root_terminal", XSetSetSet::MENU_LABEL, "_Root Terminal");
    xset_set_set(set, XSetSetSet::ICN, "gtk-dialog-warning");

    // was previously used for 'Save Session' < 0.9.4 as XSetMenu::NORMAL
    set = xset_set("main_save_session", XSetSetSet::MENU_LABEL, "Open _URL");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::ICN, "gtk-network");
    xset_set_set(set, XSetSetSet::TITLE, "Open URL");
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "Enter URL in the "
                 "format:\n\tPROTOCOL://USERNAME:PASSWORD@HOST:PORT/SHARE\n\nExamples:\n\tftp://"
                 "mirrors.kernel.org\n\tsmb://user:pass@10.0.0.1:50/docs\n\tssh://"
                 "user@sys.domain\n\tmtp://\n\nIncluding a password is unsafe.  To bookmark a "
                 "URL, right-click on the mounted network in Devices and select Bookmark.\n");
    set->line = nullptr;

    set = xset_set("main_save_tabs", XSetSetSet::MENU_LABEL, "Save Ta_bs");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("main_exit", XSetSetSet::MENU_LABEL, "E_xit");
    xset_set_set(set, XSetSetSet::ICN, "gtk-quit");

    // View
    set = xset_set("panel1_show", XSetSetSet::MENU_LABEL, "Panel _1");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("panel2_show", XSetSetSet::MENU_LABEL, "Panel _2");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set("panel3_show", XSetSetSet::MENU_LABEL, "Panel _3");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set("panel4_show", XSetSetSet::MENU_LABEL, "Panel _4");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set("main_pbar", XSetSetSet::MENU_LABEL, "Panel _Bar");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("main_focus_panel", XSetSetSet::MENU_LABEL, "F_ocus");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "panel_prev panel_next panel_hide panel_1 panel_2 panel_3 panel_4");
    xset_set_set(set, XSetSetSet::ICN, "gtk-go-forward");

    xset_set("panel_prev", XSetSetSet::MENU_LABEL, "_Prev");
    xset_set("panel_next", XSetSetSet::MENU_LABEL, "_Next");
    xset_set("panel_hide", XSetSetSet::MENU_LABEL, "_Hide");
    xset_set("panel_1", XSetSetSet::MENU_LABEL, "Panel _1");
    xset_set("panel_2", XSetSetSet::MENU_LABEL, "Panel _2");
    xset_set("panel_3", XSetSetSet::MENU_LABEL, "Panel _3");
    xset_set("panel_4", XSetSetSet::MENU_LABEL, "Panel _4");

    set = xset_set("main_auto", XSetSetSet::MENU_LABEL, "_Event Manager");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set, XSetSetSet::DESC, "auto_inst auto_win auto_pnl auto_tab evt_device");
    xset_set_set(set, XSetSetSet::ICN, "gtk-execute");

    set = xset_set("auto_inst", XSetSetSet::MENU_LABEL, "_Instance");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set, XSetSetSet::DESC, "evt_start evt_exit");

    set = xset_set("evt_start", XSetSetSet::MENU_LABEL, "_Startup");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Set Instance Startup Command");
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "Enter program or bash command line to be run automatically when a SpaceFM "
                 "instance starts:\n\nUse:\n\t%%e\tevent type  (evt_start)\n");

    set = xset_set("evt_exit", XSetSetSet::MENU_LABEL, "_Exit");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Set Instance Exit Command");
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "Enter program or bash command line to be run automatically when a SpaceFM "
                 "instance exits:\n\nUse:\n\t%%e\tevent type  (evt_exit)\n");

    set = xset_set("auto_win", XSetSetSet::MENU_LABEL, "_Window");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "evt_win_new evt_win_focus evt_win_move evt_win_click evt_win_key evt_win_close");

    set = xset_set("evt_win_new", XSetSetSet::MENU_LABEL, "_New");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Set New Window Command");
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "Enter program or bash command line to be run automatically whenever a new SpaceFM "
        "window is opened:\n\nUse:\n\t%%e\tevent type  (evt_win_new)\n\t%%w\twindow id  (see "
        "spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg $fm_pwd, etc) "
        "can be used in this command.");

    set = xset_set("evt_win_focus", XSetSetSet::MENU_LABEL, "_Focus");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Set Window Focus Command");
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "Enter program or bash command line to be run automatically whenever a SpaceFM "
                 "window gets focus:\n\nUse:\n\t%%e\tevent type  (evt_win_focus)\n\t%%w\twindow "
                 "id  (see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables "
                 "(eg $fm_pwd, etc) can be used in this command.");

    set = xset_set("evt_win_move", XSetSetSet::MENU_LABEL, "_Move/Resize");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Set Window Move/Resize Command");
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "Enter program or bash command line to be run automatically whenever a SpaceFM window is "
        "moved or resized:\n\nUse:\n\t%%e\tevent type  (evt_win_move)\n\t%%w\twindow id  (see "
        "spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg $fm_pwd, etc) "
        "can be used in this command.\n\nNote: This command may be run multiple times during "
        "resize.");

    set = xset_set("evt_win_click", XSetSetSet::MENU_LABEL, "_Click");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Set Click Command");
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "Enter program or bash command line to be run automatically whenever the mouse is "
        "clicked:\n\nUse:\n\t%%e\tevent type  (evt_win_click)\n\t%%w\twindow id  (see spacefm -s "
        "help)\n\t%%p\tpanel\n\t%%t\ttab\n\t%%b\tbutton  (mouse button pressed)\n\t%%m\tmodifier "
        " (modifier keys)\n\t%%f\tfocus  (element which received the click)\n\nExported bash "
        "variables (eg $fm_pwd, etc) can be used in this command when no asterisk prefix is "
        "used.\n\nPrefix your command with an asterisk (*) and conditionally return exit status "
        "0 to inhibit the default handler.  For example:\n*if [ \"%%b\" != \"2\" ];then exit 1; "
        "fi; spacefm -g --label \"\\nMiddle button was clicked in %%f\" --button ok &");

    set = xset_set("evt_win_key", XSetSetSet::MENU_LABEL, "_Keypress");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Set Window Keypress Command");
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "Enter program or bash command line to be run automatically whenever a key is "
        "pressed:\n\nUse:\n\t%%e\tevent type  (evt_win_key)\n\t%%w\twindow id  (see spacefm -s "
        "help)\n\t%%p\tpanel\n\t%%t\ttab\n\t%%k\tkey code  (key pressed)\n\t%%m\tmodifier  "
        "(modifier keys)\n\nExported bash variables (eg $fm_pwd, etc) can be used in this "
        "command when no asterisk prefix is used.\n\nPrefix your command with an asterisk (*) "
        "and conditionally return exit status 0 to inhibit the default handler.  For "
        "example:\n*if [ \"%%k\" != \"0xffc5\" ];then exit 1; fi; spacefm -g --label \"\\nKey "
        "F8 was pressed.\" --button ok &");

    set = xset_set("evt_win_close", XSetSetSet::MENU_LABEL, "Cl_ose");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Set Window Close Command");
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "Enter program or bash command line to be run automatically whenever a SpaceFM "
                 "window is closed:\n\nUse:\n\t%%e\tevent type  (evt_win_close)\n\t%%w\twindow "
                 "id  (see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables "
                 "(eg $fm_pwd, etc) can be used in this command.");

    set = xset_set("auto_pnl", XSetSetSet::MENU_LABEL, "_Panel");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set, XSetSetSet::DESC, "evt_pnl_focus evt_pnl_show evt_pnl_sel");

    set = xset_set("evt_pnl_focus", XSetSetSet::MENU_LABEL, "_Focus");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Set Panel Focus Command");
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "Enter program or bash command line to be run automatically whenever a panel "
                 "gets focus:\n\nUse:\n\t%%e\tevent type  (evt_pnl_focus)\n\t%%w\twindow id  "
                 "(see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg "
                 "$fm_pwd, etc) can be used in this command.");

    set = xset_set("evt_pnl_show", XSetSetSet::MENU_LABEL, "_Show");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Set Panel Show Command");
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "Enter program or bash command line to be run automatically whenever a panel or panel "
        "element is shown or hidden:\n\nUse:\n\t%%e\tevent type  (evt_pnl_show)\n\t%%w\twindow "
        "id  (see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\t%%f\tfocus  (element shown or "
        "hidden)\n\t%%v\tvisible  (1 or 0)\n\nExported bash variables (eg $fm_pwd, etc) can be "
        "used in this command.");

    set = xset_set("evt_pnl_sel", XSetSetSet::MENU_LABEL, "S_elect");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Set Panel Select Command");
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "Enter program or bash command line to be run automatically whenever the file selection "
        "changes:\n\nUse:\n\t%%e\tevent type  (evt_pnl_sel)\n\t%%w\twindow id  (see spacefm -s "
        "help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg $fm_pwd, etc) can be "
        "used in this command.\n\nPrefix your command with an asterisk (*) and conditionally "
        "return exit status 0 to inhibit the default handler.");

    set = xset_set("auto_tab", XSetSetSet::MENU_LABEL, "_Tab");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set, XSetSetSet::DESC, "evt_tab_new evt_tab_chdir evt_tab_focus evt_tab_close");

    set = xset_set("evt_tab_new", XSetSetSet::MENU_LABEL, "_New");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Set New Tab Command");
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "Enter program or bash command line to be run automatically whenever a new tab "
                 "is opened:\n\nUse:\n\t%%e\tevent type  (evt_tab_new)\n\t%%w\twindow id  (see "
                 "spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg "
                 "$fm_pwd, etc) can be used in this command.");

    set = xset_set("evt_tab_chdir", XSetSetSet::MENU_LABEL, "_Change Dir");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Set Tab Change Dir Command");
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "Enter program or bash command line to be run automatically whenever a tab changes to a "
        "different directory:\n\nUse:\n\t%%e\tevent type  (evt_tab_chdir)\n\t%%w\twindow id  "
        "(see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\t%%d\tnew directory\n\nExported bash "
        "variables (eg $fm_pwd, etc) can be used in this command.");

    set = xset_set("evt_tab_focus", XSetSetSet::MENU_LABEL, "_Focus");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Set Tab Focus Command");
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "Enter program or bash command line to be run automatically whenever a tab gets "
                 "focus:\n\nUse:\n\t%%e\tevent type  (evt_tab_focus)\n\t%%w\twindow id  (see "
                 "spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg "
                 "$fm_pwd, etc) can be used in this command.");

    set = xset_set("evt_tab_close", XSetSetSet::MENU_LABEL, "_Close");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Set Tab Close Command");
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "Enter program or bash command line to be run automatically whenever a tab is "
                 "closed:\n\nUse:\n\t%%e\tevent type  (evt_tab_close)\n\t%%w\twindow id  (see "
                 "spacefm -s help)\n\t%%p\tpanel\n\t%%t\tclosed tab");

    set = xset_set("evt_device", XSetSetSet::MENU_LABEL, "_Device");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Set Device Command");
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "Enter program or bash command line to be run automatically whenever a device "
                 "state changes:\n\nUse:\n\t%%e\tevent type  (evt_device)\n\t%%f\tdevice "
                 "file\n\t%%v\tchange  (added|removed|changed)\n");

    set = xset_set("main_title", XSetSetSet::MENU_LABEL, "Wi_ndow Title");
    set->menu_style = XSetMenu::STRING;
    xset_set_set(set, XSetSetSet::TITLE, "Set Window Title Format");
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "Set window title format:\n\nUse:\n\t%%n\tcurrent directory name (eg "
        "bin)\n\t%%d\tcurrent "
        "directory path (eg /usr/bin)\n\t%%p\tcurrent panel number (1-4)\n\t%%t\tcurrent tab "
        "number\n\t%%P\ttotal number of panels visible\n\t%%T\ttotal number of tabs in current "
        "panel\n\t*\tasterisk shown if tasks running in window");
    xset_set_set(set, XSetSetSet::S, "%d");
    xset_set_set(set, XSetSetSet::Z, "%d");

    set = xset_set("main_icon", XSetSetSet::MENU_LABEL, "_Window Icon");
    set->menu_style = XSetMenu::ICON;
    // Note: xset_text_dialog uses the title passed to know this is an
    // icon chooser, so it adds a Choose button.  If you change the title,
    // change xset_text_dialog.
    set->title = ztd::strdup("Set Window Icon");
    set->desc = ztd::strdup("Enter an icon name, icon file path, or stock item name:\n\nOr click "
                            "Choose to select an icon.  Not all icons may work properly due to "
                            "various issues.\n\nProvided alternate SpaceFM "
                            "icons:\n\tspacefm-[48|128]-[cube|pyramid]-[blue|green|red]\n\tspacefm-"
                            "48-folder-[blue|red]\n\nFor example: spacefm-48-pyramid-green");
    // x and y store global icon chooser dialog size

    set = xset_set("main_full", XSetSetSet::MENU_LABEL, "_Fullscreen");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set("main_design_mode", XSetSetSet::MENU_LABEL, "_Design Mode");
    xset_set_set(set, XSetSetSet::ICN, "gtk-help");

    set = xset_set("main_prefs", XSetSetSet::MENU_LABEL, "_Preferences");
    xset_set_set(set, XSetSetSet::ICN, "gtk-preferences");

    set = xset_set("main_tool", XSetSetSet::MENU_LABEL, "_Tool");
    set->menu_style = XSetMenu::SUBMENU;

    set = xset_get("root_bar"); // in Preferences
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("view_thumb",
                   XSetSetSet::MENU_LABEL,
                   "_Thumbnails (global)"); // in View|Panel View|Style
    set->menu_style = XSetMenu::CHECK;

    // Plugins
    set = xset_set("plug_install", XSetSetSet::MENU_LABEL, "_Install");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set, XSetSetSet::DESC, "plug_ifile");
    xset_set_set(set, XSetSetSet::ICN, "gtk-add");

    set = xset_set("plug_ifile", XSetSetSet::MENU_LABEL, "_File");
    xset_set_set(set, XSetSetSet::ICN, "gtk-file");

    set = xset_set("plug_copy", XSetSetSet::MENU_LABEL, "_Import");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set, XSetSetSet::DESC, "plug_cfile separator plug_cverb");
    xset_set_set(set, XSetSetSet::ICN, "gtk-copy");

    set = xset_set("plug_cfile", XSetSetSet::MENU_LABEL, "_File");
    xset_set_set(set, XSetSetSet::ICN, "gtk-file");
    set = xset_set("plug_cverb", XSetSetSet::MENU_LABEL, "_Verbose");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    // Help
    set = xset_set("main_about", XSetSetSet::MENU_LABEL, "_About");
    xset_set_set(set, XSetSetSet::ICN, "gtk-about");

    set = xset_set("main_dev", XSetSetSet::MENU_LABEL, "_Show Devices");
    xset_set_set(set, XSetSetSet::SHARED_KEY, "panel1_show_devmon");
    set->menu_style = XSetMenu::CHECK;

    // Tasks
    set = xset_set("main_tasks", XSetSetSet::MENU_LABEL, "_Task Manager");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "task_show_manager task_hide_manager separator task_columns task_popups task_errors "
        "task_queue");

    set = xset_set("task_col_status", XSetSetSet::MENU_LABEL, "_Status");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("0");   // column position
    set->y = ztd::strdup("130"); // column width

    set = xset_set("task_col_count", XSetSetSet::MENU_LABEL, "_Count");
    set->menu_style = XSetMenu::CHECK;
    set->x = ztd::strdup("1");

    set = xset_set("task_col_path", XSetSetSet::MENU_LABEL, "_Directory");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("2");

    set = xset_set("task_col_file", XSetSetSet::MENU_LABEL, "_Item");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("3");

    set = xset_set("task_col_to", XSetSetSet::MENU_LABEL, "_To");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("4");

    set = xset_set("task_col_progress", XSetSetSet::MENU_LABEL, "_Progress");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("5");
    set->y = ztd::strdup("100");

    set = xset_set("task_col_total", XSetSetSet::MENU_LABEL, "T_otal");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("6");
    set->y = ztd::strdup("120");

    set = xset_set("task_col_started", XSetSetSet::MENU_LABEL, "Sta_rted");
    set->menu_style = XSetMenu::CHECK;
    set->x = ztd::strdup("7");

    set = xset_set("task_col_elapsed", XSetSetSet::MENU_LABEL, "_Elapsed");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("8");
    set->y = ztd::strdup("70");

    set = xset_set("task_col_curspeed", XSetSetSet::MENU_LABEL, "C_urrent Speed");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("9");

    set = xset_set("task_col_curest", XSetSetSet::MENU_LABEL, "Current Re_main");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("10");

    set = xset_set("task_col_avgspeed", XSetSetSet::MENU_LABEL, "_Average Speed");
    set->menu_style = XSetMenu::CHECK;
    set->x = ztd::strdup("11");
    set->y = ztd::strdup("60");

    set = xset_set("task_col_avgest", XSetSetSet::MENU_LABEL, "A_verage Remain");
    set->menu_style = XSetMenu::CHECK;
    set->x = ztd::strdup("12");
    set->y = ztd::strdup("65");

    set = xset_set("task_col_reorder", XSetSetSet::MENU_LABEL, "Reor_der");

    set = xset_set("task_stop", XSetSetSet::MENU_LABEL, "_Stop");
    xset_set_set(set, XSetSetSet::ICN, "gtk-stop");
    set = xset_set("task_pause", XSetSetSet::MENU_LABEL, "Pa_use");
    xset_set_set(set, XSetSetSet::ICN, "gtk-media-pause");
    set = xset_set("task_que", XSetSetSet::MENU_LABEL, "_Queue");
    xset_set_set(set, XSetSetSet::ICN, "gtk-add");
    set = xset_set("task_resume", XSetSetSet::MENU_LABEL, "_Resume");
    xset_set_set(set, XSetSetSet::ICN, "gtk-media-play");
    set = xset_set("task_showout", XSetSetSet::MENU_LABEL, "Sho_w Output");

    set = xset_set("task_all", XSetSetSet::MENU_LABEL, "_All Tasks");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "task_stop_all task_pause_all task_que_all task_resume_all");

    set = xset_set("task_stop_all", XSetSetSet::MENU_LABEL, "_Stop");
    xset_set_set(set, XSetSetSet::ICN, "gtk-stop");
    set = xset_set("task_pause_all", XSetSetSet::MENU_LABEL, "Pa_use");
    xset_set_set(set, XSetSetSet::ICN, "gtk-media-pause");
    set = xset_set("task_que_all", XSetSetSet::MENU_LABEL, "_Queue");
    xset_set_set(set, XSetSetSet::ICN, "gtk-add");
    set = xset_set("task_resume_all", XSetSetSet::MENU_LABEL, "_Resume");
    xset_set_set(set, XSetSetSet::ICN, "gtk-media-play");

    set = xset_set("task_show_manager", XSetSetSet::MENU_LABEL, "Show _Manager");
    set->menu_style = XSetMenu::RADIO;
    set->b = XSetB::XSET_B_FALSE;
    // set->x  used for task man height >=0.9.4

    set = xset_set("task_hide_manager", XSetSetSet::MENU_LABEL, "Auto-_Hide Manager");
    set->menu_style = XSetMenu::RADIO;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("task_columns", XSetSetSet::MENU_LABEL, "_Columns");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "task_col_count task_col_path task_col_file task_col_to task_col_progress task_col_total "
        "task_col_started task_col_elapsed task_col_curspeed task_col_curest task_col_avgspeed "
        "task_col_avgest separator task_col_reorder");

    set = xset_set("task_popups", XSetSetSet::MENU_LABEL, "_Popups");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "task_pop_all task_pop_top task_pop_above task_pop_stick separator task_pop_detail "
        "task_pop_over task_pop_err");

    set = xset_set("task_pop_all", XSetSetSet::MENU_LABEL, "Popup _All Tasks");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_FALSE;

    set = xset_set("task_pop_top", XSetSetSet::MENU_LABEL, "Stay On _Top");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_FALSE;

    set = xset_set("task_pop_above", XSetSetSet::MENU_LABEL, "A_bove Others");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_FALSE;

    set = xset_set("task_pop_stick", XSetSetSet::MENU_LABEL, "All _Workspaces");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_FALSE;

    set = xset_set("task_pop_detail", XSetSetSet::MENU_LABEL, "_Detailed Stats");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_FALSE;

    set = xset_set("task_pop_over", XSetSetSet::MENU_LABEL, "_Overwrite Option");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("task_pop_err", XSetSetSet::MENU_LABEL, "_Error Option");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("task_errors", XSetSetSet::MENU_LABEL, "Err_ors");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set, XSetSetSet::DESC, "task_err_first task_err_any task_err_cont");

    set = xset_set("task_err_first", XSetSetSet::MENU_LABEL, "Stop If _First");
    set->menu_style = XSetMenu::RADIO;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("task_err_any", XSetSetSet::MENU_LABEL, "Stop On _Any");
    set->menu_style = XSetMenu::RADIO;
    set->b = XSetB::XSET_B_FALSE;

    set = xset_set("task_err_cont", XSetSetSet::MENU_LABEL, "_Continue");
    set->menu_style = XSetMenu::RADIO;
    set->b = XSetB::XSET_B_FALSE;

    set = xset_set("task_queue", XSetSetSet::MENU_LABEL, "Qu_eue");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set, XSetSetSet::DESC, "task_q_new task_q_smart task_q_pause");

    set = xset_set("task_q_new", XSetSetSet::MENU_LABEL, "_Queue New Tasks");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("task_q_smart", XSetSetSet::MENU_LABEL, "_Smart Queue");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("task_q_pause", XSetSetSet::MENU_LABEL, "_Pause On Error");
    set->menu_style = XSetMenu::CHECK;

    // PANELS COMMON
    xset_set("date_format", XSetSetSet::S, "%Y-%m-%d %H:%M");

    set = xset_set("con_open", XSetSetSet::MENU_LABEL, "_Open");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set, XSetSetSet::ICN, "gtk-open");

    set = xset_set("open_execute", XSetSetSet::MENU_LABEL, "E_xecute");
    xset_set_set(set, XSetSetSet::ICN, "gtk-execute");

    set = xset_set("open_edit", XSetSetSet::MENU_LABEL, "Edi_t");
    xset_set_set(set, XSetSetSet::ICN, "gtk-edit");

    set = xset_set("open_edit_root", XSetSetSet::MENU_LABEL, "Edit As _Root");
    xset_set_set(set, XSetSetSet::ICN, "gtk-dialog-warning");

    set = xset_set("open_other", XSetSetSet::MENU_LABEL, "_Choose...");
    xset_set_set(set, XSetSetSet::ICN, "gtk-open");

    set = xset_set("open_hand", XSetSetSet::MENU_LABEL, "File _Handlers...");
    xset_set_set(set, XSetSetSet::ICN, "gtk-preferences");

    set = xset_set("open_all", XSetSetSet::MENU_LABEL, "Open With _Default"); // virtual

    set = xset_set("open_in_tab", XSetSetSet::MENU_LABEL, "In _Tab");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "opentab_new opentab_prev opentab_next opentab_1 opentab_2 opentab_3 opentab_4 "
                 "opentab_5 opentab_6 opentab_7 opentab_8 opentab_9 opentab_10");

    xset_set("opentab_new", XSetSetSet::MENU_LABEL, "N_ew");
    xset_set("opentab_prev", XSetSetSet::MENU_LABEL, "_Prev");
    xset_set("opentab_next", XSetSetSet::MENU_LABEL, "_Next");
    xset_set("opentab_1", XSetSetSet::MENU_LABEL, "Tab _1");
    xset_set("opentab_2", XSetSetSet::MENU_LABEL, "Tab _2");
    xset_set("opentab_3", XSetSetSet::MENU_LABEL, "Tab _3");
    xset_set("opentab_4", XSetSetSet::MENU_LABEL, "Tab _4");
    xset_set("opentab_5", XSetSetSet::MENU_LABEL, "Tab _5");
    xset_set("opentab_6", XSetSetSet::MENU_LABEL, "Tab _6");
    xset_set("opentab_7", XSetSetSet::MENU_LABEL, "Tab _7");
    xset_set("opentab_8", XSetSetSet::MENU_LABEL, "Tab _8");
    xset_set("opentab_9", XSetSetSet::MENU_LABEL, "Tab _9");
    xset_set("opentab_10", XSetSetSet::MENU_LABEL, "Tab 1_0");

    set = xset_set("open_in_panel", XSetSetSet::MENU_LABEL, "In _Panel");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "open_in_panelprev open_in_panelnext open_in_panel1 open_in_panel2 open_in_panel3 "
                 "open_in_panel4");

    xset_set("open_in_panelprev", XSetSetSet::MENU_LABEL, "_Prev");
    xset_set("open_in_panelnext", XSetSetSet::MENU_LABEL, "_Next");
    xset_set("open_in_panel1", XSetSetSet::MENU_LABEL, "Panel _1");
    xset_set("open_in_panel2", XSetSetSet::MENU_LABEL, "Panel _2");
    xset_set("open_in_panel3", XSetSetSet::MENU_LABEL, "Panel _3");
    xset_set("open_in_panel4", XSetSetSet::MENU_LABEL, "Panel _4");

    set = xset_set("arc_extract", XSetSetSet::MENU_LABEL, "_Extract");
    xset_set_set(set, XSetSetSet::ICN, "gtk-convert");

    set = xset_set("arc_extractto", XSetSetSet::MENU_LABEL, "Extract _To");
    xset_set_set(set, XSetSetSet::ICN, "gtk-convert");

    set = xset_set("arc_list", XSetSetSet::MENU_LABEL, "_List Contents");
    xset_set_set(set, XSetSetSet::ICN, "gtk-file");

    set = xset_set("arc_default", XSetSetSet::MENU_LABEL, "_Archive Defaults");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "arc_conf2 separator arc_def_open arc_def_ex arc_def_exto arc_def_list separator "
                 "arc_def_parent arc_def_write");

    set = xset_set("arc_def_open", XSetSetSet::MENU_LABEL, "_Open With App");
    set->menu_style = XSetMenu::RADIO;

    set = xset_set("arc_def_ex", XSetSetSet::MENU_LABEL, "_Extract");
    set->menu_style = XSetMenu::RADIO;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("arc_def_exto", XSetSetSet::MENU_LABEL, "Extract _To");
    set->menu_style = XSetMenu::RADIO;

    set = xset_set("arc_def_list", XSetSetSet::MENU_LABEL, "_List Contents");
    set->menu_style = XSetMenu::RADIO;

    set = xset_set("arc_def_parent", XSetSetSet::MENU_LABEL, "_Create Subdirectory");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("arc_def_write", XSetSetSet::MENU_LABEL, "_Write Access");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("arc_conf2", XSetSetSet::MENU_LABEL, "Archive _Handlers");
    xset_set_set(set, XSetSetSet::ICON, "gtk-preferences");

    set = xset_set("open_new", XSetSetSet::MENU_LABEL, "_New");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "new_file new_directory new_link new_archive separator tab_new tab_new_here new_bookmark");
    xset_set_set(set, XSetSetSet::ICN, "gtk-new");

    set = xset_set("new_file", XSetSetSet::MENU_LABEL, "_File");
    xset_set_set(set, XSetSetSet::ICN, "gtk-file");

    set = xset_set("new_directory", XSetSetSet::MENU_LABEL, "Dir_ectory");
    xset_set_set(set, XSetSetSet::ICN, "gtk-directory");

    set = xset_set("new_link", XSetSetSet::MENU_LABEL, "_Link");
    xset_set_set(set, XSetSetSet::ICN, "gtk-file");

    set = xset_set("new_bookmark", XSetSetSet::MENU_LABEL, "_Bookmark");
    xset_set_set(set, XSetSetSet::SHARED_KEY, "book_add");
    xset_set_set(set, XSetSetSet::ICN, "gtk-jump-to");

    set = xset_set("new_archive", XSetSetSet::MENU_LABEL, "_Archive");
    xset_set_set(set, XSetSetSet::ICN, "gtk-save-as");

    set = xset_get("arc_dlg");
    set->b = XSetB::XSET_B_TRUE; // Extract To - Create Subdirectory
    set->z = ztd::strdup("1");   // Extract To - Write Access

    set = xset_set("tab_new", XSetSetSet::MENU_LABEL, "_Tab");
    xset_set_set(set, XSetSetSet::ICN, "gtk-add");
    set = xset_set("tab_new_here", XSetSetSet::MENU_LABEL, "Tab _Here");
    xset_set_set(set, XSetSetSet::ICN, "gtk-add");

    set = xset_set("new_app", XSetSetSet::MENU_LABEL, "_Desktop Application");
    xset_set_set(set, XSetSetSet::ICN, "gtk-add");

    set = xset_set("con_go", XSetSetSet::MENU_LABEL, "_Go");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "go_back go_forward go_up go_home go_default go_set_default edit_canon separator "
                 "go_tab go_focus");
    xset_set_set(set, XSetSetSet::ICN, "gtk-go-forward");

    set = xset_set("go_back", XSetSetSet::MENU_LABEL, "_Back");
    xset_set_set(set, XSetSetSet::ICN, "gtk-go-back");
    set = xset_set("go_forward", XSetSetSet::MENU_LABEL, "_Forward");
    xset_set_set(set, XSetSetSet::ICN, "gtk-go-forward");
    set = xset_set("go_up", XSetSetSet::MENU_LABEL, "_Up");
    xset_set_set(set, XSetSetSet::ICN, "gtk-go-up");
    set = xset_set("go_home", XSetSetSet::MENU_LABEL, "_Home");
    xset_set_set(set, XSetSetSet::ICN, "gtk-home");
    set = xset_set("go_default", XSetSetSet::MENU_LABEL, "_Default");
    xset_set_set(set, XSetSetSet::ICN, "gtk-home");

    set = xset_set("go_set_default", XSetSetSet::MENU_LABEL, "_Set Default");
    xset_set_set(set, XSetSetSet::ICN, "gtk-save");

    set = xset_set("edit_canon", XSetSetSet::MENU_LABEL, "Re_al Path");

    set = xset_set("go_focus", XSetSetSet::MENU_LABEL, "Fo_cus");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "focus_path_bar focus_filelist focus_dirtree focus_book focus_device");

    set = xset_set("focus_path_bar", XSetSetSet::MENU_LABEL, "_Path Bar");
    xset_set_set(set, XSetSetSet::ICN, "gtk-dialog-question");
    set = xset_set("focus_filelist", XSetSetSet::MENU_LABEL, "_File List");
    xset_set_set(set, XSetSetSet::ICN, "gtk-file");
    set = xset_set("focus_dirtree", XSetSetSet::MENU_LABEL, "_Tree");
    xset_set_set(set, XSetSetSet::ICN, "gtk-directory");
    set = xset_set("focus_book", XSetSetSet::MENU_LABEL, "_Bookmarks");
    xset_set_set(set, XSetSetSet::ICN, "gtk-jump-to");
    set = xset_set("focus_device", XSetSetSet::MENU_LABEL, "De_vices");
    xset_set_set(set, XSetSetSet::ICN, "gtk-harddisk");

    set = xset_set("go_tab", XSetSetSet::MENU_LABEL, "_Tab");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "tab_prev tab_next tab_restore tab_close tab_1 tab_2 tab_3 tab_4 tab_5 tab_6 "
                 "tab_7 tab_8 tab_9 tab_10");

    xset_set("tab_prev", XSetSetSet::MENU_LABEL, "_Prev");
    xset_set("tab_next", XSetSetSet::MENU_LABEL, "_Next");
    xset_set("tab_restore", XSetSetSet::MENU_LABEL, "_Restore");
    xset_set("tab_close", XSetSetSet::MENU_LABEL, "_Close");
    xset_set("tab_1", XSetSetSet::MENU_LABEL, "Tab _1");
    xset_set("tab_2", XSetSetSet::MENU_LABEL, "Tab _2");
    xset_set("tab_3", XSetSetSet::MENU_LABEL, "Tab _3");
    xset_set("tab_4", XSetSetSet::MENU_LABEL, "Tab _4");
    xset_set("tab_5", XSetSetSet::MENU_LABEL, "Tab _5");
    xset_set("tab_6", XSetSetSet::MENU_LABEL, "Tab _6");
    xset_set("tab_7", XSetSetSet::MENU_LABEL, "Tab _7");
    xset_set("tab_8", XSetSetSet::MENU_LABEL, "Tab _8");
    xset_set("tab_9", XSetSetSet::MENU_LABEL, "Tab _9");
    xset_set("tab_10", XSetSetSet::MENU_LABEL, "Tab 1_0");

    set = xset_set("con_view", XSetSetSet::MENU_LABEL, "_View");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set, XSetSetSet::ICN, "gtk-preferences");

    set = xset_set("view_list_style", XSetSetSet::MENU_LABEL, "Styl_e");
    set->menu_style = XSetMenu::SUBMENU;

    set = xset_set("view_columns", XSetSetSet::MENU_LABEL, "C_olumns");
    set->menu_style = XSetMenu::SUBMENU;

    set = xset_set("view_reorder_col", XSetSetSet::MENU_LABEL, "_Reorder");

    set = xset_set("rubberband", XSetSetSet::MENU_LABEL, "_Rubberband Select");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("view_sortby", XSetSetSet::MENU_LABEL, "_Sort");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "sortby_name sortby_size sortby_type sortby_perm sortby_owner sortby_date separator "
        "sortby_ascend sortby_descend separator sortx_alphanum sortx_case separator " // sortx_natural
        "sortx_directories sortx_files sortx_mix separator sortx_hidfirst sortx_hidlast");

    set = xset_set("sortby_name", XSetSetSet::MENU_LABEL, "_Name");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set("sortby_size", XSetSetSet::MENU_LABEL, "_Size");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set("sortby_type", XSetSetSet::MENU_LABEL, "_Type");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set("sortby_perm", XSetSetSet::MENU_LABEL, "_Permission");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set("sortby_owner", XSetSetSet::MENU_LABEL, "_Owner");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set("sortby_date", XSetSetSet::MENU_LABEL, "_Modified");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set("sortby_ascend", XSetSetSet::MENU_LABEL, "_Ascending");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set("sortby_descend", XSetSetSet::MENU_LABEL, "_Descending");
    set->menu_style = XSetMenu::RADIO;

    set = xset_set("sortx_alphanum", XSetSetSet::MENU_LABEL, "Alphanumeric");
    set->menu_style = XSetMenu::CHECK;
#if 0
    set = xset_set("sortx_natural", XSetSetSet::MENU_LABEL, "Nat_ural");
    set->menu_style = XSetMenu::CHECK;
#endif
    set = xset_set("sortx_case", XSetSetSet::MENU_LABEL, "_Case Sensitive");
    set->menu_style = XSetMenu::CHECK;
    set = xset_set("sortx_directories", XSetSetSet::MENU_LABEL, "Directories Fi_rst");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set("sortx_files", XSetSetSet::MENU_LABEL, "F_iles First");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set("sortx_mix", XSetSetSet::MENU_LABEL, "Mi_xed");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set("sortx_hidfirst", XSetSetSet::MENU_LABEL, "_Hidden First");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set("sortx_hidlast", XSetSetSet::MENU_LABEL, "Hidden _Last");
    set->menu_style = XSetMenu::RADIO;

    set = xset_set("view_refresh", XSetSetSet::MENU_LABEL, "Re_fresh");
    xset_set_set(set, XSetSetSet::ICN, "gtk-refresh");

    set = xset_set("path_seek", XSetSetSet::MENU_LABEL, "Auto See_k");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("path_hand", XSetSetSet::MENU_LABEL, "_Protocol Handlers");
    xset_set_set(set, XSetSetSet::ICN, "gtk-preferences");
    xset_set_set(set, XSetSetSet::SHARED_KEY, "dev_net_cnf");
    // set->s was custom protocol handler in sfm<=0.9.3 - retained

    set = xset_set("path_help", XSetSetSet::MENU_LABEL, "Path Bar _Help");
    xset_set_set(set, XSetSetSet::ICN, "gtk-help");

    // EDIT
    set = xset_set("edit_cut", XSetSetSet::MENU_LABEL, "Cu_t");
    xset_set_set(set, XSetSetSet::ICN, "gtk-cut");

    set = xset_set("edit_copy", XSetSetSet::MENU_LABEL, "_Copy");
    xset_set_set(set, XSetSetSet::ICN, "gtk-copy");

    set = xset_set("edit_paste", XSetSetSet::MENU_LABEL, "_Paste");
    xset_set_set(set, XSetSetSet::ICN, "gtk-paste");

    set = xset_set("edit_rename", XSetSetSet::MENU_LABEL, "_Rename");
    xset_set_set(set, XSetSetSet::ICN, "gtk-edit");

    set = xset_set("edit_delete", XSetSetSet::MENU_LABEL, "_Delete");
    xset_set_set(set, XSetSetSet::ICN, "gtk-delete");

    set = xset_set("edit_trash", XSetSetSet::MENU_LABEL, "_Trash");
    xset_set_set(set, XSetSetSet::ICN, "gtk-delete");

    set = xset_set("edit_submenu", XSetSetSet::MENU_LABEL, "_Actions");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "copy_name copy_parent copy_path separator paste_link paste_target paste_as separator "
        "copy_to "
        "move_to edit_root edit_hide separator select_all select_patt select_invert select_un");
    xset_set_set(set, XSetSetSet::ICN, "gtk-edit");

    set = xset_set("copy_name", XSetSetSet::MENU_LABEL, "Copy _Name");
    xset_set_set(set, XSetSetSet::ICN, "gtk-copy");

    set = xset_set("copy_path", XSetSetSet::MENU_LABEL, "Copy _Path");
    xset_set_set(set, XSetSetSet::ICN, "gtk-copy");

    set = xset_set("copy_parent", XSetSetSet::MENU_LABEL, "Copy Pa_rent");
    xset_set_set(set, XSetSetSet::ICN, "gtk-copy");

    set = xset_set("paste_link", XSetSetSet::MENU_LABEL, "Paste _Link");
    xset_set_set(set, XSetSetSet::ICN, "gtk-paste");

    set = xset_set("paste_target", XSetSetSet::MENU_LABEL, "Paste _Target");
    xset_set_set(set, XSetSetSet::ICN, "gtk-paste");

    set = xset_set("paste_as", XSetSetSet::MENU_LABEL, "Paste _As");
    xset_set_set(set, XSetSetSet::ICN, "gtk-paste");

    set = xset_set("copy_to", XSetSetSet::MENU_LABEL, "_Copy To");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set, XSetSetSet::DESC, "copy_loc copy_loc_last separator copy_tab copy_panel");

    set = xset_set("copy_loc", XSetSetSet::MENU_LABEL, "L_ocation");
    set = xset_set("copy_loc_last", XSetSetSet::MENU_LABEL, "L_ast Location");
    xset_set_set(set, XSetSetSet::ICN, "gtk-redo");

    set = xset_set("copy_tab", XSetSetSet::MENU_LABEL, "_Tab");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "copy_tab_prev copy_tab_next copy_tab_1 copy_tab_2 copy_tab_3 copy_tab_4 "
                 "copy_tab_5 copy_tab_6 copy_tab_7 copy_tab_8 copy_tab_9 copy_tab_10");

    xset_set("copy_tab_prev", XSetSetSet::MENU_LABEL, "_Prev");
    xset_set("copy_tab_next", XSetSetSet::MENU_LABEL, "_Next");
    xset_set("copy_tab_1", XSetSetSet::MENU_LABEL, "Tab _1");
    xset_set("copy_tab_2", XSetSetSet::MENU_LABEL, "Tab _2");
    xset_set("copy_tab_3", XSetSetSet::MENU_LABEL, "Tab _3");
    xset_set("copy_tab_4", XSetSetSet::MENU_LABEL, "Tab _4");
    xset_set("copy_tab_5", XSetSetSet::MENU_LABEL, "Tab _5");
    xset_set("copy_tab_6", XSetSetSet::MENU_LABEL, "Tab _6");
    xset_set("copy_tab_7", XSetSetSet::MENU_LABEL, "Tab _7");
    xset_set("copy_tab_8", XSetSetSet::MENU_LABEL, "Tab _8");
    xset_set("copy_tab_9", XSetSetSet::MENU_LABEL, "Tab _9");
    xset_set("copy_tab_10", XSetSetSet::MENU_LABEL, "Tab 1_0");

    set = xset_set("copy_panel", XSetSetSet::MENU_LABEL, "_Panel");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "copy_panel_prev copy_panel_next copy_panel_1 copy_panel_2 copy_panel_3 copy_panel_4");

    xset_set("copy_panel_prev", XSetSetSet::MENU_LABEL, "_Prev");
    xset_set("copy_panel_next", XSetSetSet::MENU_LABEL, "_Next");
    xset_set("copy_panel_1", XSetSetSet::MENU_LABEL, "Panel _1");
    xset_set("copy_panel_2", XSetSetSet::MENU_LABEL, "Panel _2");
    xset_set("copy_panel_3", XSetSetSet::MENU_LABEL, "Panel _3");
    xset_set("copy_panel_4", XSetSetSet::MENU_LABEL, "Panel _4");

    set = xset_set("move_to", XSetSetSet::MENU_LABEL, "_Move To");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set, XSetSetSet::DESC, "move_loc move_loc_last separator move_tab move_panel");

    set = xset_set("move_loc", XSetSetSet::MENU_LABEL, "_Location");
    set = xset_set("move_loc_last", XSetSetSet::MENU_LABEL, "L_ast Location");
    xset_set_set(set, XSetSetSet::ICN, "gtk-redo");
    set = xset_set("move_tab", XSetSetSet::MENU_LABEL, "_Tab");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "move_tab_prev move_tab_next move_tab_1 move_tab_2 move_tab_3 move_tab_4 "
                 "move_tab_5 move_tab_6 move_tab_7 move_tab_8 move_tab_9 move_tab_10");

    xset_set("move_tab_prev", XSetSetSet::MENU_LABEL, "_Prev");
    xset_set("move_tab_next", XSetSetSet::MENU_LABEL, "_Next");
    xset_set("move_tab_1", XSetSetSet::MENU_LABEL, "Tab _1");
    xset_set("move_tab_2", XSetSetSet::MENU_LABEL, "Tab _2");
    xset_set("move_tab_3", XSetSetSet::MENU_LABEL, "Tab _3");
    xset_set("move_tab_4", XSetSetSet::MENU_LABEL, "Tab _4");
    xset_set("move_tab_5", XSetSetSet::MENU_LABEL, "Tab _5");
    xset_set("move_tab_6", XSetSetSet::MENU_LABEL, "Tab _6");
    xset_set("move_tab_7", XSetSetSet::MENU_LABEL, "Tab _7");
    xset_set("move_tab_8", XSetSetSet::MENU_LABEL, "Tab _8");
    xset_set("move_tab_9", XSetSetSet::MENU_LABEL, "Tab _9");
    xset_set("move_tab_10", XSetSetSet::MENU_LABEL, "Tab 1_0");

    set = xset_set("move_panel", XSetSetSet::MENU_LABEL, "_Panel");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "move_panel_prev move_panel_next move_panel_1 move_panel_2 move_panel_3 move_panel_4");

    xset_set("move_panel_prev", XSetSetSet::MENU_LABEL, "_Prev");
    xset_set("move_panel_next", XSetSetSet::MENU_LABEL, "_Next");
    xset_set("move_panel_1", XSetSetSet::MENU_LABEL, "Panel _1");
    xset_set("move_panel_2", XSetSetSet::MENU_LABEL, "Panel _2");
    xset_set("move_panel_3", XSetSetSet::MENU_LABEL, "Panel _3");
    xset_set("move_panel_4", XSetSetSet::MENU_LABEL, "Panel _4");

    set = xset_set("edit_hide", XSetSetSet::MENU_LABEL, "_Hide");

    set = xset_set("select_all", XSetSetSet::MENU_LABEL, "_Select All");
    xset_set_set(set, XSetSetSet::ICN, "gtk-select-all");

    set = xset_set("select_un", XSetSetSet::MENU_LABEL, "_Unselect All");

    set = xset_set("select_invert", XSetSetSet::MENU_LABEL, "_Invert Selection");

    set = xset_set("select_patt", XSetSetSet::MENU_LABEL, "S_elect By Pattern");

    set = xset_set("edit_root", XSetSetSet::MENU_LABEL, "R_oot");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set, XSetSetSet::DESC, "root_copy_loc root_move2 root_delete");
    xset_set_set(set, XSetSetSet::ICN, "gtk-dialog-warning");

    set = xset_set("root_copy_loc", XSetSetSet::MENU_LABEL, "_Copy To");
    set = xset_set("root_move2", XSetSetSet::MENU_LABEL, "Move _To");
    set = xset_set("root_delete", XSetSetSet::MENU_LABEL, "_Delete");
    xset_set_set(set, XSetSetSet::ICN, "gtk-delete");

    // Properties
    set = xset_set("con_prop", XSetSetSet::MENU_LABEL, "Propert_ies");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set, XSetSetSet::DESC, "");
    xset_set_set(set, XSetSetSet::ICN, "gtk-properties");

    set = xset_set("prop_info", XSetSetSet::MENU_LABEL, "_Info");
    xset_set_set(set, XSetSetSet::ICN, "gtk-dialog-info");

    set = xset_set("prop_perm", XSetSetSet::MENU_LABEL, "_Permissions");
    xset_set_set(set, XSetSetSet::ICN, "dialog-password");

    set = xset_set("prop_quick", XSetSetSet::MENU_LABEL, "_Quick");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "perm_r perm_rw perm_rwx perm_r_r perm_rw_r perm_rw_rw perm_rwxr_x perm_rwxrwx "
                 "perm_r_r_r perm_rw_r_r perm_rw_rw_rw perm_rwxr_r perm_rwxr_xr_x perm_rwxrwxrwx "
                 "perm_rwxrwxrwt perm_unstick perm_stick perm_recurs");

    xset_set("perm_r", XSetSetSet::MENU_LABEL, "r--------");
    xset_set("perm_rw", XSetSetSet::MENU_LABEL, "rw-------");
    xset_set("perm_rwx", XSetSetSet::MENU_LABEL, "rwx------");
    xset_set("perm_r_r", XSetSetSet::MENU_LABEL, "r--r-----");
    xset_set("perm_rw_r", XSetSetSet::MENU_LABEL, "rw-r-----");
    xset_set("perm_rw_rw", XSetSetSet::MENU_LABEL, "rw-rw----");
    xset_set("perm_rwxr_x", XSetSetSet::MENU_LABEL, "rwxr-x---");
    xset_set("perm_rwxrwx", XSetSetSet::MENU_LABEL, "rwxrwx---");
    xset_set("perm_r_r_r", XSetSetSet::MENU_LABEL, "r--r--r--");
    xset_set("perm_rw_r_r", XSetSetSet::MENU_LABEL, "rw-r--r--");
    xset_set("perm_rw_rw_rw", XSetSetSet::MENU_LABEL, "rw-rw-rw-");
    xset_set("perm_rwxr_r", XSetSetSet::MENU_LABEL, "rwxr--r--");
    xset_set("perm_rwxr_xr_x", XSetSetSet::MENU_LABEL, "rwxr-xr-x");
    xset_set("perm_rwxrwxrwx", XSetSetSet::MENU_LABEL, "rwxrwxrwx");
    xset_set("perm_rwxrwxrwt", XSetSetSet::MENU_LABEL, "rwxrwxrwt");
    xset_set("perm_unstick", XSetSetSet::MENU_LABEL, "-t");
    xset_set("perm_stick", XSetSetSet::MENU_LABEL, "+t");

    set = xset_set("perm_recurs", XSetSetSet::MENU_LABEL, "_Recursive");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "perm_go_w perm_go_rwx perm_ugo_w perm_ugo_rx perm_ugo_rwx");

    xset_set("perm_go_w", XSetSetSet::MENU_LABEL, "go-w");
    xset_set("perm_go_rwx", XSetSetSet::MENU_LABEL, "go-rwx");
    xset_set("perm_ugo_w", XSetSetSet::MENU_LABEL, "ugo+w");
    xset_set("perm_ugo_rx", XSetSetSet::MENU_LABEL, "ugo+rX");
    xset_set("perm_ugo_rwx", XSetSetSet::MENU_LABEL, "ugo+rwX");

    set = xset_set("prop_root", XSetSetSet::MENU_LABEL, "_Root");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "rperm_rw rperm_rwx rperm_rw_r rperm_rw_rw rperm_rwxr_x rperm_rwxrwx rperm_rw_r_r "
                 "rperm_rw_rw_rw rperm_rwxr_r rperm_rwxr_xr_x rperm_rwxrwxrwx rperm_rwxrwxrwt "
                 "rperm_unstick rperm_stick rperm_recurs rperm_own");
    xset_set_set(set, XSetSetSet::ICN, "gtk-dialog-warning");

    xset_set("rperm_rw", XSetSetSet::MENU_LABEL, "rw-------");
    xset_set("rperm_rwx", XSetSetSet::MENU_LABEL, "rwx------");
    xset_set("rperm_rw_r", XSetSetSet::MENU_LABEL, "rw-r-----");
    xset_set("rperm_rw_rw", XSetSetSet::MENU_LABEL, "rw-rw----");
    xset_set("rperm_rwxr_x", XSetSetSet::MENU_LABEL, "rwxr-x---");
    xset_set("rperm_rwxrwx", XSetSetSet::MENU_LABEL, "rwxrwx---");
    xset_set("rperm_rw_r_r", XSetSetSet::MENU_LABEL, "rw-r--r--");
    xset_set("rperm_rw_rw_rw", XSetSetSet::MENU_LABEL, "rw-rw-rw-");
    xset_set("rperm_rwxr_r", XSetSetSet::MENU_LABEL, "rwxr--r--");
    xset_set("rperm_rwxr_xr_x", XSetSetSet::MENU_LABEL, "rwxr-xr-x");
    xset_set("rperm_rwxrwxrwx", XSetSetSet::MENU_LABEL, "rwxrwxrwx");
    xset_set("rperm_rwxrwxrwt", XSetSetSet::MENU_LABEL, "rwxrwxrwt");
    xset_set("rperm_unstick", XSetSetSet::MENU_LABEL, "-t");
    xset_set("rperm_stick", XSetSetSet::MENU_LABEL, "+t");

    set = xset_set("rperm_recurs", XSetSetSet::MENU_LABEL, "_Recursive");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(set,
                 XSetSetSet::DESC,
                 "rperm_go_w rperm_go_rwx rperm_ugo_w rperm_ugo_rx rperm_ugo_rwx");

    xset_set("rperm_go_w", XSetSetSet::MENU_LABEL, "go-w");
    xset_set("rperm_go_rwx", XSetSetSet::MENU_LABEL, "go-rwx");
    xset_set("rperm_ugo_w", XSetSetSet::MENU_LABEL, "ugo+w");
    xset_set("rperm_ugo_rx", XSetSetSet::MENU_LABEL, "ugo+rX");
    xset_set("rperm_ugo_rwx", XSetSetSet::MENU_LABEL, "ugo+rwX");

    set = xset_set("rperm_own", XSetSetSet::MENU_LABEL, "_Owner");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "own_myuser own_myuser_users own_user1 own_user1_users own_user2 own_user2_users own_root "
        "own_root_users own_root_myuser own_root_user1 own_root_user2 own_recurs");

    xset_set("own_myuser", XSetSetSet::MENU_LABEL, "myuser");
    xset_set("own_myuser_users", XSetSetSet::MENU_LABEL, "myuser:users");
    xset_set("own_user1", XSetSetSet::MENU_LABEL, "user1");
    xset_set("own_user1_users", XSetSetSet::MENU_LABEL, "user1:users");
    xset_set("own_user2", XSetSetSet::MENU_LABEL, "user2");
    xset_set("own_user2_users", XSetSetSet::MENU_LABEL, "user2:users");
    xset_set("own_root", XSetSetSet::MENU_LABEL, "root");
    xset_set("own_root_users", XSetSetSet::MENU_LABEL, "root:users");
    xset_set("own_root_myuser", XSetSetSet::MENU_LABEL, "root:myuser");
    xset_set("own_root_user1", XSetSetSet::MENU_LABEL, "root:user1");
    xset_set("own_root_user2", XSetSetSet::MENU_LABEL, "root:user2");

    set = xset_set("own_recurs", XSetSetSet::MENU_LABEL, "_Recursive");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_set(
        set,
        XSetSetSet::DESC,
        "rown_myuser rown_myuser_users rown_user1 rown_user1_users rown_user2 rown_user2_users "
        "rown_root rown_root_users rown_root_myuser rown_root_user1 rown_root_user2");

    xset_set("rown_myuser", XSetSetSet::MENU_LABEL, "myuser");
    xset_set("rown_myuser_users", XSetSetSet::MENU_LABEL, "myuser:users");
    xset_set("rown_user1", XSetSetSet::MENU_LABEL, "user1");
    xset_set("rown_user1_users", XSetSetSet::MENU_LABEL, "user1:users");
    xset_set("rown_user2", XSetSetSet::MENU_LABEL, "user2");
    xset_set("rown_user2_users", XSetSetSet::MENU_LABEL, "user2:users");
    xset_set("rown_root", XSetSetSet::MENU_LABEL, "root");
    xset_set("rown_root_users", XSetSetSet::MENU_LABEL, "root:users");
    xset_set("rown_root_myuser", XSetSetSet::MENU_LABEL, "root:myuser");
    xset_set("rown_root_user1", XSetSetSet::MENU_LABEL, "root:user1");
    xset_set("rown_root_user2", XSetSetSet::MENU_LABEL, "root:user2");

    // PANELS
    int p;
    for (p = 1; p < 5; p++)
    {
        set = xset_set_panel(p, "show_toolbox", XSetSetSet::MENU_LABEL, "_Toolbar");
        set->menu_style = XSetMenu::CHECK;
        set->b = XSetB::XSET_B_TRUE;
        if (p != 1)
            xset_set_set(set, XSetSetSet::SHARED_KEY, "panel1_show_toolbox");

        set = xset_set_panel(p, "show_devmon", XSetSetSet::MENU_LABEL, "_Devices");
        set->menu_style = XSetMenu::CHECK;
        set->b = XSetB::XSET_B_UNSET;
        if (p != 1)
            xset_set_set(set, XSetSetSet::SHARED_KEY, "panel1_show_devmon");

        set = xset_set_panel(p, "show_dirtree", XSetSetSet::MENU_LABEL, "T_ree");
        set->menu_style = XSetMenu::CHECK;
        set->b = XSetB::XSET_B_TRUE;
        if (p != 1)
            xset_set_set(set, XSetSetSet::SHARED_KEY, "panel1_show_dirtree");

        set = xset_set_panel(p, "show_book", XSetSetSet::MENU_LABEL, "_Bookmarks");
        set->menu_style = XSetMenu::CHECK;
        set->b = XSetB::XSET_B_UNSET;
        if (p != 1)
            xset_set_set(set, XSetSetSet::SHARED_KEY, "panel1_show_book");

        set = xset_set_panel(p, "show_sidebar", XSetSetSet::MENU_LABEL, "_Side Toolbar");
        set->menu_style = XSetMenu::CHECK;
        set->b = XSetB::XSET_B_UNSET;
        if (p != 1)
            xset_set_set(set, XSetSetSet::SHARED_KEY, "panel1_show_sidebar");

        set = xset_set_panel(p, "list_detailed", XSetSetSet::MENU_LABEL, "_Detailed");
        set->menu_style = XSetMenu::RADIO;
        set->b = XSetB::XSET_B_TRUE;
        if (p != 1)
            xset_set_set(set, XSetSetSet::SHARED_KEY, "panel1_list_detailed");

        set = xset_set_panel(p, "list_icons", XSetSetSet::MENU_LABEL, "_Icons");
        set->menu_style = XSetMenu::RADIO;
        if (p != 1)
            xset_set_set(set, XSetSetSet::SHARED_KEY, "panel1_list_icons");

        set = xset_set_panel(p, "list_compact", XSetSetSet::MENU_LABEL, "_Compact");
        set->menu_style = XSetMenu::RADIO;
        if (p != 1)
            xset_set_set(set, XSetSetSet::SHARED_KEY, "panel1_list_compact");

        set = xset_set_panel(p, "list_large", XSetSetSet::MENU_LABEL, "_Large Icons");
        set->menu_style = XSetMenu::CHECK;
        if (p != 1)
            xset_set_set(set, XSetSetSet::SHARED_KEY, "panel1_list_large");

        set = xset_set_panel(p, "show_hidden", XSetSetSet::MENU_LABEL, "_Hidden Files");
        set->menu_style = XSetMenu::CHECK;
        if (p != 1)
            xset_set_set(set, XSetSetSet::SHARED_KEY, "panel1_show_hidden");

        set = xset_set_panel(p, "icon_tab", XSetSetSet::MENU_LABEL, "_Icon");
        set->menu_style = XSetMenu::ICON;
        xset_set_set(set, XSetSetSet::ICN, "gtk-directory");

        set = xset_set_panel(p, "icon_status", XSetSetSet::MENU_LABEL, "_Icon");
        set->menu_style = XSetMenu::ICON;
        xset_set_set(set, XSetSetSet::ICN, "gtk-yes");
        if (p != 1)
            xset_set_set(set, XSetSetSet::SHARED_KEY, "panel1_icon_status");

        set = xset_set_panel(p, "detcol_name", XSetSetSet::MENU_LABEL, "_Name");
        set->menu_style = XSetMenu::CHECK;
        set->b = XSetB::XSET_B_TRUE; // visible
        set->x = ztd::strdup("0");   // position

        set = xset_set_panel(p, "detcol_size", XSetSetSet::MENU_LABEL, "_Size");
        set->menu_style = XSetMenu::CHECK;
        set->b = XSetB::XSET_B_TRUE;
        set->x = ztd::strdup("1");
        if (p != 1)
            xset_set_set(set, XSetSetSet::SHARED_KEY, "panel1_detcol_size");

        set = xset_set_panel(p, "detcol_type", XSetSetSet::MENU_LABEL, "_Type");
        set->menu_style = XSetMenu::CHECK;
        set->x = ztd::strdup("2");
        if (p != 1)
            xset_set_set(set, XSetSetSet::SHARED_KEY, "panel1_detcol_type");

        set = xset_set_panel(p, "detcol_perm", XSetSetSet::MENU_LABEL, "_Permission");
        set->menu_style = XSetMenu::CHECK;
        set->x = ztd::strdup("3");
        if (p != 1)
            xset_set_set(set, XSetSetSet::SHARED_KEY, "panel1_detcol_perm");

        set = xset_set_panel(p, "detcol_owner", XSetSetSet::MENU_LABEL, "_Owner");
        set->menu_style = XSetMenu::CHECK;
        set->x = ztd::strdup("4");
        if (p != 1)
            xset_set_set(set, XSetSetSet::SHARED_KEY, "panel1_detcol_owner");

        set = xset_set_panel(p, "detcol_date", XSetSetSet::MENU_LABEL, "_Modified");
        set->menu_style = XSetMenu::CHECK;
        set->x = ztd::strdup("5");
        if (p != 1)
            xset_set_set(set, XSetSetSet::SHARED_KEY, "panel1_detcol_date");

        set = xset_get_panel(p, "sort_extra");
        set->b = XSetB::XSET_B_TRUE;               // sort_natural
        set->x = ztd::strdup(XSetB::XSET_B_FALSE); // sort_case
        set->y = ztd::strdup("1");                 // PTKFileListSortDir::PTK_LIST_SORT_DIR_FIRST
        set->z = ztd::strdup(XSetB::XSET_B_TRUE);  // sort_hidden_first

        set = xset_set_panel(p, "book_fol", XSetSetSet::MENU_LABEL, "Follow _Dir");
        set->menu_style = XSetMenu::CHECK;
        set->b = XSetB::XSET_B_TRUE;
        if (p != 1)
            xset_set_set(set, XSetSetSet::SHARED_KEY, "panel1_book_fol");
    }

    // speed
    set = xset_set("book_newtab", XSetSetSet::MENU_LABEL, "_New Tab");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set("book_single", XSetSetSet::MENU_LABEL, "_Single Click");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("dev_newtab", XSetSetSet::MENU_LABEL, "_New Tab");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set("dev_single", XSetSetSet::MENU_LABEL, "_Single Click");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    // mark all labels and icons as default
    for (XSet* set2: xsets)
    {
        if (set2->lock)
        {
            if (set2->in_terminal)
                set2->in_terminal = false;
            if (set2->keep_terminal)
                set2->keep_terminal = false;
        }
    }
}

static void
def_key(const char* name, unsigned int key, unsigned int keymod)
{
    XSet* set = xset_get(name);

    // key already set or unset?
    if (set->key != 0 || key == 0)
        return;

    // key combo already in use?
    for (XSet* set2: keysets)
    {
        if (set2->key == key && set2->keymod == keymod)
            return;
    }
    set->key = key;
    set->keymod = keymod;
}

static void
xset_default_keys()
{
    // read all currently set or unset keys
    for (XSet* set: xsets)
    {
        if (set->key)
            keysets.push_back(set);
    }

    def_key("tab_prev", GDK_KEY_Tab, (GDK_SHIFT_MASK | GDK_CONTROL_MASK));
    def_key("tab_next", GDK_KEY_Tab, GDK_CONTROL_MASK);
    def_key("tab_new", GDK_KEY_t, GDK_CONTROL_MASK);
    def_key("tab_restore", GDK_KEY_T, (GDK_SHIFT_MASK | GDK_CONTROL_MASK));
    def_key("tab_close", GDK_KEY_w, GDK_CONTROL_MASK);
    def_key("tab_1", GDK_KEY_1, GDK_MOD1_MASK);
    def_key("tab_2", GDK_KEY_2, GDK_MOD1_MASK);
    def_key("tab_3", GDK_KEY_3, GDK_MOD1_MASK);
    def_key("tab_4", GDK_KEY_4, GDK_MOD1_MASK);
    def_key("tab_5", GDK_KEY_5, GDK_MOD1_MASK);
    def_key("tab_6", GDK_KEY_6, GDK_MOD1_MASK);
    def_key("tab_7", GDK_KEY_7, GDK_MOD1_MASK);
    def_key("tab_8", GDK_KEY_8, GDK_MOD1_MASK);
    def_key("tab_9", GDK_KEY_9, GDK_MOD1_MASK);
    def_key("tab_10", GDK_KEY_0, GDK_MOD1_MASK);
    def_key("edit_cut", GDK_KEY_x, GDK_CONTROL_MASK);
    def_key("edit_copy", GDK_KEY_c, GDK_CONTROL_MASK);
    def_key("edit_paste", GDK_KEY_v, GDK_CONTROL_MASK);
    def_key("edit_rename", GDK_KEY_F2, 0);
    def_key("edit_delete", GDK_KEY_Delete, GDK_SHIFT_MASK);
    def_key("edit_trash", GDK_KEY_Delete, 0);
    def_key("copy_name", GDK_KEY_C, (GDK_SHIFT_MASK | GDK_MOD1_MASK));
    def_key("copy_path", GDK_KEY_C, (GDK_SHIFT_MASK | GDK_CONTROL_MASK));
    def_key("paste_link", GDK_KEY_V, (GDK_SHIFT_MASK | GDK_CONTROL_MASK));
    def_key("paste_as", GDK_KEY_A, (GDK_SHIFT_MASK | GDK_CONTROL_MASK));
    def_key("select_all", GDK_KEY_A, GDK_CONTROL_MASK);
    def_key("main_terminal", GDK_KEY_F4, 0);
    def_key("go_default", GDK_KEY_Escape, 0);
    def_key("go_back", GDK_KEY_Left, GDK_MOD1_MASK);
    def_key("go_forward", GDK_KEY_Right, GDK_MOD1_MASK);
    def_key("go_up", GDK_KEY_Up, GDK_MOD1_MASK);
    def_key("focus_path_bar", GDK_KEY_l, GDK_CONTROL_MASK);
    def_key("view_refresh", GDK_KEY_F5, 0);
    def_key("prop_info", GDK_KEY_Return, GDK_MOD1_MASK);
    def_key("prop_perm", GDK_KEY_p, GDK_CONTROL_MASK);
    def_key("panel1_show_hidden", GDK_KEY_h, GDK_CONTROL_MASK);
    def_key("book_new", GDK_KEY_d, GDK_CONTROL_MASK);
    def_key("new_file", GDK_KEY_f, (GDK_SHIFT_MASK | GDK_CONTROL_MASK));
    def_key("new_directory", GDK_KEY_n, (GDK_SHIFT_MASK | GDK_CONTROL_MASK));
    def_key("new_link", GDK_KEY_l, (GDK_SHIFT_MASK | GDK_CONTROL_MASK));
    def_key("main_new_window", GDK_KEY_n, GDK_CONTROL_MASK);
    def_key("open_all", GDK_KEY_F6, 0);
    def_key("main_full", GDK_KEY_F11, 0);
    def_key("panel1_show", GDK_KEY_1, GDK_CONTROL_MASK);
    def_key("panel2_show", GDK_KEY_2, GDK_CONTROL_MASK);
    def_key("panel3_show", GDK_KEY_3, GDK_CONTROL_MASK);
    def_key("panel4_show", GDK_KEY_4, GDK_CONTROL_MASK);
    // def_key("main_help", GDK_KEY_F1, 0);
    def_key("main_exit", GDK_KEY_q, GDK_CONTROL_MASK);
    def_key("main_prefs", GDK_KEY_F12, 0);
    def_key("book_add", GDK_KEY_d, GDK_CONTROL_MASK);
}
