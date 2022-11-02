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

#include <unistd.h>

#include <sys/stat.h>

#include <fmt/format.h>

#include <glibmm.h>
#include <glibmm/convert.h>

#include <gtk/gtk.h>

#include <gdk/gdkkeysyms.h>
#include <fcntl.h>

#include <exo/exo.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include <toml.hpp> // toml11

#include "types.hxx"

#include "settings.hxx"
#include "main-window.hxx"
#include "item-prop.hxx"

#include "scripts.hxx"

#include "settings/app.hxx"
#include "settings/etc.hxx"
#include "settings/names.hxx"

#include "autosave.hxx"
#include "extern.hxx"
#include "write.hxx"
#include "utils.hxx"

#include "vfs/vfs-app-desktop.hxx"
#include "vfs/vfs-utils.hxx"
#include "vfs/vfs-user-dir.hxx"

#include "ptk/ptk-utils.hxx"
#include "ptk/ptk-app-chooser.hxx"
#include "ptk/ptk-handler.hxx"
#include "ptk/ptk-file-menu.hxx"
#include "ptk/ptk-location-view.hxx"

// MOD settings
static void xset_defaults();

static std::vector<xset_t> keysets;
static xset_t set_clipboard = nullptr;
static bool clipboard_is_cut;

std::string settings_config_dir;
std::string settings_user_tmp_dir;

static XSetContext* xset_context = nullptr;

EventHandler event_handler;

std::vector<std::string> xset_cmd_history;

#ifdef HAVE_DEPRECATED_INI_LOADING
static void xset_parse(std::string& line);

using SettingsParseFunc = void (*)(std::string& line);
#endif

static void xset_free_all();
static void xset_default_keys();
static bool xset_design_cb(GtkWidget* item, GdkEventButton* event, xset_t set);
static void xset_builtin_tool_activate(XSetTool tool_type, xset_t set, GdkEventButton* event);
static xset_t xset_new_builtin_toolitem(XSetTool tool_type);
static void xset_custom_insert_after(xset_t target, xset_t set);
static xset_t xset_custom_copy(xset_t set, bool copy_next, bool delete_set);
static void xset_remove(xset_t set);

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

/**
 *  TOML data structure types for serialization
 */

// map<var, value>
using setvars_t = std::map<std::string, std::string>;
// map<xset_name, setvars_t>
using xsetpak_t = std::map<std::string, setvars_t>;

static const xsetpak_t xset_pack_sets();

#ifdef HAVE_DEPRECATED_INI_LOADING // Deprecated INI loader - start
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

    if (ztd::same(token, INI_KEY_SHOW_THUMBNAIL))
        app_settings.set_show_thumbnail(std::stoi(value));
    else if (ztd::same(token, INI_KEY_MAX_THUMB_SIZE))
        app_settings.set_max_thumb_size(std::stoi(value) << 10);
    else if (ztd::same(token, INI_KEY_ICON_SIZE_BIG))
        app_settings.set_icon_size_big(std::stoi(value));
    else if (ztd::same(token, INI_KEY_ICON_SIZE_SMALL))
        app_settings.set_icon_size_small(std::stoi(value));
    else if (ztd::same(token, INI_KEY_ICON_SIZE_TOOL))
        app_settings.set_icon_size_tool(std::stoi(value));
    else if (ztd::same(token, INI_KEY_SINGLE_CLICK))
        app_settings.set_single_click(std::stoi(value));
    else if (ztd::same(token, INI_KEY_NO_SINGLE_HOVER))
        app_settings.set_single_hover(std::stoi(value));
    else if (ztd::same(token, INI_KEY_SORT_ORDER))
        app_settings.set_sort_order(std::stoi(value));
    else if (ztd::same(token, INI_KEY_SORT_TYPE))
        app_settings.set_sort_type(std::stoi(value));
    else if (ztd::same(token, INI_KEY_USE_SI_PREFIX))
        app_settings.set_use_si_prefix(std::stoi(value));
    else if (ztd::same(token, INI_KEY_NO_EXECUTE))
        app_settings.set_click_executes(!std::stoi(value));
    else if (ztd::same(token, INI_KEY_NO_CONFIRM))
        app_settings.set_confirm(!std::stoi(value));
    else if (ztd::same(token, INI_KEY_NO_CONFIRM_TRASH))
        app_settings.set_confirm_trash(!std::stoi(value));
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

    if (ztd::same(token, INI_KEY_WIDTH))
        app_settings.set_width(std::stoi(value));
    else if (ztd::same(token, INI_KEY_HEIGHT))
        app_settings.set_height(std::stoi(value));
    else if (ztd::same(token, INI_KEY_MAXIMIZED))
        app_settings.set_maximized(std::stoi(value));
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

    if (ztd::same(token, INI_KEY_SHOW_TABS))
        app_settings.set_always_show_tabs(std::stoi(value));
    else if (ztd::same(token, INI_KEY_SHOW_CLOSE))
        app_settings.set_show_close_tab_buttons(std::stoi(value));
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

    XSetVar var;
    try
    {
        var = xset_get_xsetvar_from_name(token_var);
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

    xset_t set = xset_get(token);
    if (ztd::startswith(set->name, "cstm_") || ztd::startswith(set->name, "hand_"))
    { // custom
        if (set->lock)
            set->lock = false;
    }
    else
    { // normal (lock)
        xset_set_var(set, var, value);
    }
}
#endif // Deprecated INI loader - end

static std::uint64_t
get_config_file_version(const toml::value& data)
{
    const auto& version = toml::find(data, TOML_SECTION_VERSION);

    const auto config_version = toml::find<std::uint64_t>(version, TOML_KEY_VERSION);
    return config_version;
}

static void
config_parse_general(const toml::value& toml_data, std::uint64_t version)
{
    (void)version;

    const auto& section = toml::find(toml_data, TOML_SECTION_GENERAL);

    const auto show_thumbnail = toml::find<bool>(section, TOML_KEY_SHOW_THUMBNAIL);
    app_settings.set_show_thumbnail(show_thumbnail);

    const auto max_thumb_size = toml::find<std::uint64_t>(section, TOML_KEY_MAX_THUMB_SIZE);
    app_settings.set_max_thumb_size(max_thumb_size << 10);

    const auto icon_size_big = toml::find<std::uint64_t>(section, TOML_KEY_ICON_SIZE_BIG);
    app_settings.set_icon_size_big(icon_size_big);

    const auto icon_size_small = toml::find<std::uint64_t>(section, TOML_KEY_ICON_SIZE_SMALL);
    app_settings.set_icon_size_small(icon_size_small);

    const auto icon_size_tool = toml::find<std::uint64_t>(section, TOML_KEY_ICON_SIZE_TOOL);
    app_settings.set_icon_size_tool(icon_size_tool);

    const auto single_click = toml::find<bool>(section, TOML_KEY_SINGLE_CLICK);
    app_settings.set_single_click(single_click);

    const auto single_hover = toml::find<bool>(section, TOML_KEY_SINGLE_HOVER);
    app_settings.set_single_hover(single_hover);

    const auto sort_order = toml::find<std::uint64_t>(section, TOML_KEY_SORT_ORDER);
    app_settings.set_sort_order(sort_order);

    const auto sort_type = toml::find<std::uint64_t>(section, TOML_KEY_SORT_TYPE);
    app_settings.set_sort_type(sort_type);

    const auto use_si_prefix = toml::find<bool>(section, TOML_KEY_USE_SI_PREFIX);
    app_settings.set_use_si_prefix(use_si_prefix);

    const auto click_executes = toml::find<bool>(section, TOML_KEY_CLICK_EXECUTE);
    app_settings.set_click_executes(click_executes);

    const auto confirm = toml::find<bool>(section, TOML_KEY_CONFIRM);
    app_settings.set_confirm(confirm);

    const auto confirm_delete = toml::find<bool>(section, TOML_KEY_CONFIRM_DELETE);
    app_settings.set_confirm_delete(confirm_delete);

    const auto confirm_trash = toml::find<bool>(section, TOML_KEY_CONFIRM_TRASH);
    app_settings.set_confirm_trash(confirm_trash);
}

static void
config_parse_window(const toml::value& toml_data, std::uint64_t version)
{
    (void)version;

    const auto& section = toml::find(toml_data, TOML_SECTION_WINDOW);

    const auto height = toml::find<std::uint64_t>(section, TOML_KEY_HEIGHT);
    app_settings.set_height(height);

    const auto width = toml::find<std::uint64_t>(section, TOML_KEY_WIDTH);
    app_settings.set_width(width);

    const auto maximized = toml::find<bool>(section, TOML_KEY_MAXIMIZED);
    app_settings.set_maximized(maximized);
}

static void
config_parse_interface(const toml::value& toml_data, std::uint64_t version)
{
    (void)version;

    const auto& section = toml::find(toml_data, TOML_SECTION_INTERFACE);

    const auto always_show_tabs = toml::find<bool>(section, TOML_KEY_SHOW_TABS);
    app_settings.set_always_show_tabs(always_show_tabs);

    const auto show_close_tab_buttons = toml::find<bool>(section, TOML_KEY_SHOW_CLOSE);
    app_settings.set_show_close_tab_buttons(show_close_tab_buttons);
}

static void
config_parse_xset(const toml::value& toml_data, std::uint64_t version)
{
    (void)version;

    // loop over all of [[XSet]]
    for (const auto& section: toml::find<toml::array>(toml_data, TOML_SECTION_XSET))
    {
        // get [XSet.name] and all vars
        for (const auto& [toml_name, toml_vars]: section.as_table())
        {
            // get var and value
            for (const auto& [toml_var, toml_value]: toml_vars.as_table())
            {
                std::stringstream ss_name;
                std::stringstream ss_var;
                std::stringstream ss_value;

                ss_name << toml_name;
                ss_var << toml_var;
                ss_value << toml_value;

                const std::string name{ss_name.str()};
                const std::string setvar{ss_var.str()};
                const std::string value{ztd::strip(ss_value.str(), "\"")};

                // LOG_INFO("name: {} | var: {} | value: {}", name, setvar, value);

                XSetVar var;
                try
                {
                    var = xset_get_xsetvar_from_name(setvar);
                }
                catch (const std::logic_error& e)
                {
                    const std::string msg = fmt::format("XSet parse error:\n\n{}", e.what());
                    ptk_show_error(nullptr, "Error", e.what());
                    return;
                }

                xset_t set = xset_get(name);
                if (ztd::startswith(set->name, "cstm_") || ztd::startswith(set->name, "hand_"))
                { // custom
                    if (set->lock)
                        set->lock = false;
                    xset_set_var(set, var, value);
                }
                else
                { // normal (lock)
                    xset_set_var(set, var, value);
                }
            }
        }
    }
}

void
load_settings()
{
    app_settings.set_load_saved_tabs(true);

    settings_config_dir = vfs_user_get_config_dir();

    // MOD extra settings
    xset_defaults();

#ifdef HAVE_DEPRECATED_INI_LOADING
    // choose which config file to load
    std::string conf_ini = Glib::build_filename(settings_config_dir, CONFIG_FILE_INI_FILENAME);
    std::string conf_toml = Glib::build_filename(settings_config_dir, CONFIG_FILE_FILENAME);
    std::string& session = conf_toml;
    bool load_deprecated_ini_config = false;
    if (std::filesystem::exists(conf_ini) && !std::filesystem::exists(conf_toml))
    {
        LOG_WARN("INI config files are deprecated, loading support will be removed");
        load_deprecated_ini_config = true;
        session = conf_ini;
    }
#else
    const std::string session = Glib::build_filename(settings_config_dir, CONFIG_FILE_FILENAME);
#endif

    if (!std::filesystem::exists(settings_config_dir))
    {
        // copy /etc/xdg/spacefm
        std::string xdg_path = Glib::build_filename(settings_config_dir, "xdg", PACKAGE_NAME);
        if (std::filesystem::is_directory(xdg_path))
        {
            const std::string command = fmt::format("cp -r {} '{}'", xdg_path, settings_config_dir);
            Glib::spawn_command_line_sync(command);

            std::filesystem::permissions(settings_config_dir, std::filesystem::perms::owner_all);
        }
    }

    if (!std::filesystem::exists(settings_config_dir))
    {
        std::filesystem::create_directories(settings_config_dir);
        std::filesystem::permissions(settings_config_dir, std::filesystem::perms::owner_all);
    }

    bool git_backed_settings = etc_settings.get_git_backed_settings();

    if (git_backed_settings)
    {
        if (Glib::find_program_in_path("git").empty())
        {
            LOG_ERROR("git backed settings enabled but git is not installed");
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

            Glib::spawn_command_line_sync(command_args);
        }
    }

    if (std::filesystem::is_regular_file(session))
    {
#ifdef HAVE_DEPRECATED_INI_LOADING
        if (!load_deprecated_ini_config)
        { // TOML
#endif
            // LOG_INFO("Parse TOML");

            toml::value toml_data;
            try
            {
                toml_data = toml::parse(session);
                // DEBUG
                // std::cout << "###### TOML PARSE ######" << "\n\n";
                // std::cout << toml_data << "\n\n";
            }
            catch (const toml::syntax_error& e)
            {
                LOG_ERROR("Config file parsing failed: {}", e.what());
                return;
            }

            const std::uint64_t version = get_config_file_version(toml_data);

            config_parse_general(toml_data, version);
            config_parse_window(toml_data, version);
            config_parse_interface(toml_data, version);
            config_parse_xset(toml_data, version);
#ifdef HAVE_DEPRECATED_INI_LOADING
        }
        else
        { // INI
            // LOG_INFO("Parse INI");

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
                        if (ztd::same(line, INI_SECTION_GENERAL))
                            func = &parse_general_settings;
                        else if (ztd::same(line, INI_SECTION_WINDOW))
                            func = &parse_window_state;
                        else if (ztd::same(line, INI_SECTION_INTERFACE))
                            func = &parse_interface_settings;
                        else if (ztd::same(line, INI_SECTION_MOD))
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
#endif
    }
    else
    {
        LOG_INFO("No config file found, using defaults.");
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
    char* main_terminal = xset_get_s(XSetName::MAIN_TERMINAL);
    if (!main_terminal || main_terminal[0] == '\0')
    {
        for (std::string_view terminal: terminal_programs)
        {
            const std::string term = Glib::find_program_in_path(terminal.data());
            if (term.empty())
                continue;

            xset_set(XSetName::MAIN_TERMINAL, XSetVar::S, terminal);
            xset_set_b(XSetName::MAIN_TERMINAL, true); // discovery
            break;
        }
    }

    // MOD editor discovery
    char* app_name = xset_get_s(XSetName::EDITOR);
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
                xset_set(XSetName::EDITOR, XSetVar::S, desktop.get_exec());
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
    event_handler.win_focus = xset_get(XSetName::EVT_WIN_FOCUS);
    event_handler.win_move = xset_get(XSetName::EVT_WIN_MOVE);
    event_handler.win_click = xset_get(XSetName::EVT_WIN_CLICK);
    event_handler.win_key = xset_get(XSetName::EVT_WIN_KEY);
    event_handler.win_close = xset_get(XSetName::EVT_WIN_CLOSE);
    event_handler.pnl_show = xset_get(XSetName::EVT_PNL_SHOW);
    event_handler.pnl_focus = xset_get(XSetName::EVT_PNL_FOCUS);
    event_handler.pnl_sel = xset_get(XSetName::EVT_PNL_SEL);
    event_handler.tab_new = xset_get(XSetName::EVT_TAB_NEW);
    event_handler.tab_chdir = xset_get(XSetName::EVT_TAB_CHDIR);
    event_handler.tab_focus = xset_get(XSetName::EVT_TAB_FOCUS);
    event_handler.tab_close = xset_get(XSetName::EVT_TAB_CLOSE);
    event_handler.device = xset_get(XSetName::EVT_DEVICE);
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

    // save tabs
    bool save_tabs = xset_get_b(XSetName::MAIN_SAVE_TABS);
    if (main_window_ptr)
        main_window = FM_MAIN_WINDOW(main_window_ptr);
    else
        main_window = fm_main_window_get_last_active();

    if (GTK_IS_WIDGET(main_window))
    {
        if (save_tabs)
        {
            for (panel_t p: PANELS)
            {
                xset_t set = xset_get_panel(p, XSetPanel::SHOW);
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
                        for (int g = 0; g < pages; ++g)
                        {
                            PtkFileBrowser* file_browser = PTK_FILE_BROWSER_REINTERPRET(
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
            for (panel_t p: PANELS)
            {
                xset_t set = xset_get_panel(p, XSetPanel::SHOW);
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

    // new values get appened at the top of the file,
    // declare in reverse order
    const toml::value toml_data = toml::value{
        {TOML_SECTION_VERSION,
         toml::value{
             {TOML_KEY_VERSION, CONFIG_FILE_VERSION},
         }},

        {TOML_SECTION_GENERAL,
         toml::value{
             {TOML_KEY_SHOW_THUMBNAIL, app_settings.get_show_thumbnail()},
             {TOML_KEY_MAX_THUMB_SIZE, app_settings.get_max_thumb_size() >> 10},
             {TOML_KEY_ICON_SIZE_BIG, app_settings.get_icon_size_big()},
             {TOML_KEY_ICON_SIZE_SMALL, app_settings.get_icon_size_small()},
             {TOML_KEY_ICON_SIZE_TOOL, app_settings.get_icon_size_tool()},
             {TOML_KEY_SINGLE_CLICK, app_settings.get_single_click()},
             {TOML_KEY_SINGLE_HOVER, app_settings.get_single_hover()},
             {TOML_KEY_SORT_ORDER, app_settings.get_sort_order()},
             {TOML_KEY_SORT_TYPE, app_settings.get_sort_type()},
             {TOML_KEY_USE_SI_PREFIX, app_settings.get_use_si_prefix()},
             {TOML_KEY_CLICK_EXECUTE, app_settings.get_click_executes()},
             {TOML_KEY_CONFIRM, app_settings.get_confirm()},
             {TOML_KEY_CONFIRM_DELETE, app_settings.get_confirm_delete()},
             {TOML_KEY_CONFIRM_TRASH, app_settings.get_confirm_trash()},
         }},

        {TOML_SECTION_WINDOW,
         toml::value{
             {TOML_KEY_HEIGHT, app_settings.get_height()},
             {TOML_KEY_WIDTH, app_settings.get_width()},
             {TOML_KEY_MAXIMIZED, app_settings.get_maximized()},
         }},

        {TOML_SECTION_INTERFACE,
         toml::value{
             {TOML_KEY_SHOW_TABS, app_settings.get_always_show_tabs()},
             {TOML_KEY_SHOW_CLOSE, app_settings.get_show_close_tab_buttons()},
         }},

        {TOML_SECTION_XSET,
         toml::value{
             xset_pack_sets(),
         }},
    };

    const std::string toml_path = Glib::build_filename(settings_config_dir, CONFIG_FILE_FILENAME);

    write_file(toml_path, toml_data);

    // DEBUG
    // std::cout << "###### TOML DUMP ######" << "\n\n";
    // std::cout << toml_data << "\n\n";
}

static const setvars_t
xset_pack_set(xset_t set)
{
    setvars_t setvars;
    // hack to not save default handlers - this allows default handlers
    // to be updated more easily
    if (set->disable && set->name[0] == 'h' && ztd::startswith(set->name, "hand"))
        return setvars;

    if (set->plugin)
        return setvars;

    if (set->s)
        setvars.insert({xset_get_name_from_xsetvar(XSetVar::S), fmt::format("{}", set->s)});
    if (set->x)
        setvars.insert({xset_get_name_from_xsetvar(XSetVar::X), fmt::format("{}", set->x)});
    if (set->y)
        setvars.insert({xset_get_name_from_xsetvar(XSetVar::Y), fmt::format("{}", set->y)});
    if (set->z)
        setvars.insert({xset_get_name_from_xsetvar(XSetVar::Z), fmt::format("{}", set->z)});
    if (set->key)
        setvars.insert({xset_get_name_from_xsetvar(XSetVar::KEY), fmt::format("{}", set->key)});
    if (set->keymod)
        setvars.insert(
            {xset_get_name_from_xsetvar(XSetVar::KEYMOD), fmt::format("{}", set->keymod)});
    // menu label
    if (set->menu_label)
    {
        if (set->lock)
        { // built-in
            if (set->in_terminal && set->menu_label && set->menu_label[0])
            { // only save lbl if menu_label was customized
                setvars.insert({xset_get_name_from_xsetvar(XSetVar::MENU_LABEL),
                                fmt::format("{}", set->menu_label)});
            }
        }
        else
        { // custom
            setvars.insert({xset_get_name_from_xsetvar(XSetVar::MENU_LABEL_CUSTOM),
                            fmt::format("{}", set->menu_label)});
        }
    }
    // icon
    if (set->lock)
    { // built-in
        if (set->keep_terminal)
        { // only save icn if icon was customized
            setvars.insert(
                {xset_get_name_from_xsetvar(XSetVar::ICN), fmt::format("{}", set->icon)});
        }
    }
    else if (set->icon)
    { // custom
        setvars.insert({xset_get_name_from_xsetvar(XSetVar::ICON), fmt::format("{}", set->icon)});
    }

    if (set->next)
        setvars.insert({xset_get_name_from_xsetvar(XSetVar::NEXT), fmt::format("{}", set->next)});
    if (set->child)
        setvars.insert({xset_get_name_from_xsetvar(XSetVar::CHILD), fmt::format("{}", set->child)});
    if (set->context)
        setvars.insert(
            {xset_get_name_from_xsetvar(XSetVar::CONTEXT), fmt::format("{}", set->context)});
    if (set->b != XSetB::XSET_B_UNSET)
        setvars.insert({xset_get_name_from_xsetvar(XSetVar::B), fmt::format("{}", set->b)});
    if (set->tool != XSetTool::NOT)
        setvars.insert(
            {xset_get_name_from_xsetvar(XSetVar::TOOL), fmt::format("{}", INT(set->tool))});

    if (!set->lock)
    {
        if (set->menu_style != XSetMenu::NORMAL)
            setvars.insert({xset_get_name_from_xsetvar(XSetVar::STYLE),
                            fmt::format("{}", INT(set->menu_style))});
        if (set->desc)
            setvars.insert(
                {xset_get_name_from_xsetvar(XSetVar::DESC), fmt::format("{}", set->desc)});
        if (set->title)
            setvars.insert(
                {xset_get_name_from_xsetvar(XSetVar::TITLE), fmt::format("{}", set->title)});
        if (set->prev)
            setvars.insert(
                {xset_get_name_from_xsetvar(XSetVar::PREV), fmt::format("{}", set->prev)});
        if (set->parent)
            setvars.insert(
                {xset_get_name_from_xsetvar(XSetVar::PARENT), fmt::format("{}", set->parent)});
        if (set->line)
            setvars.insert(
                {xset_get_name_from_xsetvar(XSetVar::LINE), fmt::format("{}", set->line)});
        if (set->task)
            setvars.insert(
                {xset_get_name_from_xsetvar(XSetVar::TASK), fmt::format("{:d}", set->task)});
        if (set->task_pop)
            setvars.insert({xset_get_name_from_xsetvar(XSetVar::TASK_POP),
                            fmt::format("{:d}", set->task_pop)});
        if (set->task_err)
            setvars.insert({xset_get_name_from_xsetvar(XSetVar::TASK_ERR),
                            fmt::format("{:d}", set->task_err)});
        if (set->task_out)
            setvars.insert({xset_get_name_from_xsetvar(XSetVar::TASK_OUT),
                            fmt::format("{:d}", set->task_out)});
        if (set->in_terminal)
            setvars.insert({xset_get_name_from_xsetvar(XSetVar::RUN_IN_TERMINAL),
                            fmt::format("{:d}", set->in_terminal)});
        if (set->keep_terminal)
            setvars.insert({xset_get_name_from_xsetvar(XSetVar::KEEP_TERMINAL),
                            fmt::format("{:d}", set->keep_terminal)});
        if (set->scroll_lock)
            setvars.insert({xset_get_name_from_xsetvar(XSetVar::SCROLL_LOCK),
                            fmt::format("{:d}", set->scroll_lock)});
        if (set->opener != 0)
            setvars.insert(
                {xset_get_name_from_xsetvar(XSetVar::OPENER), fmt::format("{}", set->opener)});
    }

    return setvars;
}

static const xsetpak_t
xset_pack_sets()
{
    // this is stupid, but it works.
    // trying to .push_back() a toml::value into a toml::value
    // segfaults with toml::detail::throw_bad_cast.
    //
    // So the whole toml::value has to get created in one go,
    // so construct a map that toml::value can then consume.

    // map layout <XSet->name, <XSet->var, XSet->value>>
    xsetpak_t xsetpak;

    for (xset_t set: xsets)
    {
        setvars_t setvars = xset_pack_set(set);

        if (!setvars.empty())
            xsetpak.insert({fmt::format("{}", set->name), setvars});
    }

    return xsetpak;
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

    settings_user_tmp_dir = Glib::build_filename(etc_settings.get_tmp_dir(), PACKAGE_NAME);
    std::filesystem::create_directories(settings_user_tmp_dir);
    std::filesystem::permissions(settings_user_tmp_dir, std::filesystem::perms::owner_all);

    return settings_user_tmp_dir.c_str();
}

static void
xset_free_all()
{
    while (true)
    {
        if (xsets.empty())
            break;

        xset_t set = xsets.back();
        xsets.pop_back();

        if (set->ob2_data && ztd::startswith(set->name, "evt_"))
        {
            g_list_foreach((GList*)set->ob2_data, (GFunc)free, nullptr);
            g_list_free((GList*)set->ob2_data);
        }

        delete set;
    }

    if (xset_context)
        delete xset_context;
}

static void
xset_remove(xset_t set)
{
    xsets.erase(std::remove(xsets.begin(), xsets.end(), set), xsets.end());

    delete set;
}

xset_t
xset_find_custom(std::string_view search)
{
    // find a custom command or submenu by label or xset name
    const std::string label = clean_label(search, true, false);

    for (xset_t set: xsets)
    {
        if (!set->lock && ((set->menu_style == XSetMenu::SUBMENU && set->child) ||
                           (set->menu_style < XSetMenu::SUBMENU &&
                            XSetCMD(xset_get_int_set(set, XSetVar::X)) <= XSetCMD::BOOKMARK)))
        {
            // custom submenu or custom command - label or name matches?
            const std::string str = clean_label(set->menu_label, true, false);
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
    xset_t set;
    xset_t mset;
    xset_t open_all_set;
    xset_t tset;
    xset_t open_all_tset;
    XSetContext* context = nullptr;
    int context_action;
    bool found = false;
    char pinned;

    for (xset_t set2: xsets)
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
            XSetCMD cmd_type = XSetCMD(xset_get_int_set(set, XSetVar::X));
            if (cmd_type != XSetCMD::APP && cmd_type != XSetCMD::LINE &&
                cmd_type != XSetCMD::SCRIPT)
                continue;

            // is set pinned to open_all_type for pre-context?
            pinned = 0;
            for (xset_t set3: xsets)
            {
                if (set3->next && ztd::startswith(set3->name, "open_all_type_"))
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
            const std::string clean = clean_label(set->menu_label, false, false);
            LOG_INFO("Selected Menu Item '{}' As Handler", clean);
            xset_menu_cb(nullptr, set); // also does custom activate
        }
    }
    return found;
}

XSetContext::XSetContext()
{
    // LOG_INFO("XSetContext Constructor");

    this->valid = false;
    for (std::size_t i = 0; i < this->var.size(); ++i)
        this->var[i] = nullptr;
}

XSetContext::~XSetContext()
{
    // LOG_INFO("XSetContext Destructor");

    this->valid = false;
    for (std::size_t i = 0; i < this->var.size(); ++i)
    {
        if (this->var[i])
            free(this->var[i]);
        this->var[i] = nullptr;
    }
}

XSetContext*
xset_context_new()
{
    if (xset_context)
        delete xset_context;
    xset_context = new XSetContext();
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

    const std::vector<std::string> split_elements = ztd::split(elements, " ");

    for (std::string_view element: split_elements)
    {
        xset_t set = xset_get(element);
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
        const std::string str = clean_label(label, false, false);
        item = gtk_menu_item_new_with_label(str.c_str());
    }
    else
    {
        item = gtk_menu_item_new_with_mnemonic(label);
    }
    if (!(icon && icon[0]))
        return item;
    // GtkWidget* image = xset_get_image(icon, GTK_ICON_SIZE_MENU);

    return item;
}

char*
xset_custom_get_app_name_icon(xset_t set, GdkPixbuf** icon, int icon_size)
{
    char* menu_label = nullptr;
    GdkPixbuf* icon_new = nullptr;

    if (!set->lock && XSetCMD(xset_get_int_set(set, XSetVar::X)) == XSetCMD::APP)
    {
        if (set->z && ztd::endswith(set->z, ".desktop"))
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
                const std::string name = Glib::path_get_basename(set->z);
                icon_new = vfs_load_icon(name.c_str(), icon_size);
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
    int context_action = ItemPropContextState::CONTEXT_SHOW;
    xset_t mset;
    std::string icon_file;
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
            icon_file = Glib::build_filename(set->plug_dir, set->plug_name, "icon");
        else
            icon_file = Glib::build_filename(xset_get_config_dir(), "scripts", set->name, "icon");

        if (std::filesystem::exists(icon_file))
            icon_name = ztd::strdup(icon_file);
    }
    if (!context)
        context = set->context;

    // context?
    if (context && set->tool == XSetTool::NOT && xset_context && xset_context->valid &&
        !xset_get_b(XSetName::CONTEXT_DLG))
        context_action = xset_context_test(xset_context, context, set->disable);

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
                    if (!(!set->lock &&
                          (XSetCMD(xset_get_int_set(set, XSetVar::X)) > XSetCMD::SCRIPT)))
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
            int icon_w;
            int icon_h;
            gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &icon_w, &icon_h);
            int icon_size = icon_w > icon_h ? icon_w : icon_h;

            GdkPixbuf* app_icon = nullptr;
            XSetCMD cmd_type = XSetCMD(xset_get_int_set(set, XSetVar::X));
            if (!set->lock && cmd_type == XSetCMD::APP)
            {
                // Application
                char* menu_label = xset_custom_get_app_name_icon(set, &app_icon, icon_size);
                item = xset_new_menuitem(menu_label, nullptr);
                free(menu_label);
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

    // next item
    if (set->next)
    {
        set_next = xset_get(set->next);
        xset_add_menuitem(file_browser, menu, accel_group, set_next);
    }
    return item;
}

char*
xset_custom_get_script(xset_t set, bool create)
{
    std::string path;

    if ((strncmp(set->name, "cstm_", 5) && strncmp(set->name, "cust", 4) &&
         strncmp(set->name, "hand", 4)) ||
        (create && set->plugin))
        return nullptr;

    if (create)
    {
        path = Glib::build_filename(xset_get_config_dir(), "scripts", set->name);
        if (!std::filesystem::exists(path))
        {
            std::filesystem::create_directories(path);
            std::filesystem::permissions(path, std::filesystem::perms::owner_all);
        }
    }

    if (set->plugin)
    {
        path = Glib::build_filename(set->plug_dir, set->plug_name, "exec.sh");
    }
    else
    {
        path = Glib::build_filename(xset_get_config_dir(), "scripts", set->name, "exec.sh");
    }

    if (create && !std::filesystem::exists(path))
    {
        std::string data;
        data.append(fmt::format("#!{}\n", BASH_PATH));
        data.append(fmt::format("{}\n\n", SHELL_SETTINGS));
        data.append("#import file manager variables\n");
        data.append("$fm_import\n\n");
        data.append("#For all spacefm variables see man page: spacefm-scripts\n\n");
        data.append("#Start script\n");
        data.append("\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
        data.append("#End script\n");
        data.append("exit $?\n");

        write_file(path, data);

        if (std::filesystem::exists(path))
            std::filesystem::permissions(path, std::filesystem::perms::owner_all);
    }
    return ztd::strdup(path);
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
            const std::string path1 =
                Glib::build_filename(xset_get_config_dir(), "scripts", setname);
            const std::string path2 =
                Glib::build_filename(xset_get_config_dir(), "plugin-data", setname);

            // only use free xset name if no aux data dirs exist for that name too.
            if (!std::filesystem::exists(path1) && !std::filesystem::exists(path2))
                break;
        }
    };

    return setname;
}

static void
xset_custom_copy_files(xset_t src, xset_t dest)
{
    std::string path_src;
    std::string path_dest;
    std::string command;

    std::string* standard_output = nullptr;
    std::string* standard_error = nullptr;
    int exit_status;

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
        const std::string msg =
            fmt::format("An error occured copying command files\n\n{}", *standard_error);
        xset_msg_dialog(nullptr, GTK_MESSAGE_ERROR, "Copy Command Error", GTK_BUTTONS_OK, msg);
    }
    command = fmt::format("chmod -R go-rwx {}", path_dest);
    print_command(command);
    Glib::spawn_command_line_sync(command);

    // copy data dir
    xset_t mset = xset_get_plugin_mirror(src);
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
            const std::string msg =
                fmt::format("An error occured copying command data files\n\n{}", *standard_error);
            xset_msg_dialog(nullptr, GTK_MESSAGE_ERROR, "Copy Command Error", GTK_BUTTONS_OK, msg);
        }
        command = fmt::format("chmod -R go-rwx {}", path_dest);
        print_command(command);
        Glib::spawn_command_line_sync(command);
    }
}

static xset_t
xset_custom_copy(xset_t set, bool copy_next, bool delete_set)
{
    // LOG_INFO("xset_custom_copy( {}, {}, {})", set->name, copy_next ? "true" : "false",
    // delete_set ? "true" : "false");
    xset_t mset = set;
    // if a plugin with a mirror, get the mirror
    if (set->plugin && set->shared_key)
        mset = xset_get_plugin_mirror(set);

    xset_t newset = xset_custom_new();
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
        xset_t set_child = xset_get(set->child);
        // LOG_INFO("    copy submenu {}", set_child->name);
        xset_t newchild = xset_custom_copy(set_child, true, delete_set);
        newset->child = ztd::strdup(newchild->name);
        newchild->parent = ztd::strdup(newset->name);
    }

    if (copy_next && set->next)
    {
        xset_t set_next = xset_get(set->next);
        // LOG_INFO("    copy next {}", set_next->name);
        xset_t newnext = xset_custom_copy(set_next, true, delete_set);
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
        for (xset_t set: xsets)
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
    const std::string path = Glib::build_filename(xset_get_config_dir(), "plugin-data");
    if (std::filesystem::is_directory(path))
    {
        for (const auto& file: std::filesystem::directory_iterator(path))
        {
            const std::string file_name = std::filesystem::path(file).filename();
            if (ztd::startswith(file_name, "cstm_") && !xset_is(file_name))
            {
                const std::string plugin_path = fmt::format("{}/{}", path, file_name);
                std::filesystem::remove_all(plugin_path);
                LOG_INFO("Removed {}/{}", path, file_name);
            }
        }
    }
}

static void
xset_set_plugin_mirror(xset_t pset)
{
    for (xset_t set: xsets)
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

xset_t
xset_get_plugin_mirror(xset_t set)
{
    // plugin mirrors are custom xsets that save the user's key, icon
    // and run prefs for the plugin, if any
    if (!set->plugin)
        return set;
    if (set->shared_key)
        return xset_get(set->shared_key);

    xset_t newset = xset_custom_new();
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
compare_plugin_sets(xset_t a, xset_t b)
{
    return g_utf8_collate(a->menu_label, b->menu_label);
}

const std::vector<xset_t>
xset_get_plugins()
{ // return list of plugin sets sorted by menu_label
    std::vector<xset_t> plugins;

    for (xset_t set: xsets)
    {
        if (set->plugin && set->plugin_top && set->plug_dir)
            plugins.push_back(set);
    }
    sort(plugins.begin(), plugins.end(), compare_plugin_sets);
    return plugins;
}

void
xset_clear_plugins(const std::vector<xset_t>& plugins)
{
    if (plugins.empty())
        return;

    for (xset_t set: plugins)
        xset_remove(set);
}

static xset_t
xset_get_by_plug_name(std::string_view plug_dir, std::string_view plug_name)
{
    if (plug_name.empty())
        return nullptr;

    for (xset_t set: xsets)
    {
        if (set->plugin && ztd::same(plug_name, set->plug_name) &&
            ztd::same(plug_dir, set->plug_dir))
            return set;
    }

    // add new
    const std::string setname = xset_custom_new_name();

    xset_t set = xset_new(setname, XSetName::CUSTOM);
    set->plug_dir = ztd::strdup(plug_dir  .data());
    set->plug_name = ztd::strdup(plug_name.data());
    set->plugin = true;
    set->lock = false;
    xsets.push_back(set);

    return set;
}

static void
xset_parse_plugin(std::string_view plug_dir, std::string_view name, std::string_view setvar,
                  std::string_view value, PluginUse use)
{
    if (value.empty())
        return;

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

    XSetVar var;
    try
    {
        var = xset_get_xsetvar_from_name(setvar);
    }
    catch (const std::logic_error& e)
    {
        const std::string msg = fmt::format("Plugin load error:\n\"{}\"\n{}", plug_dir, e.what());
        LOG_ERROR("{}", msg);
        ptk_show_error(nullptr, "Plugin Load Error", msg);
        return;
    }

    xset_t set;
    xset_t set2;

    set = xset_get_by_plug_name(plug_dir, name);
    xset_set_var(set, var, value.data());

    if (use >= PluginUse::BOOKMARKS)
    {
        // map plug names to new set names (does not apply to handlers)
        if (set->prev && var == XSetVar::PREV)
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
        else if (set->next && var == XSetVar::NEXT)
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
        else if (set->parent && var == XSetVar::PARENT)
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
        else if (set->child && var == XSetVar::CHILD)
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

xset_t
xset_import_plugin(const char* plug_dir, PluginUse* use)
{
    if (use)
        *use = PluginUse::NORMAL;

    // clear all existing plugin sets with this plug_dir
    // ( keep the mirrors to retain user prefs )
    bool redo = true;
    while (redo)
    {
        redo = false;
        for (xset_t set: xsets)
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
    const std::string plugin = Glib::build_filename(plug_dir, PLUGIN_FILE_FILENAME);

    if (!std::filesystem::exists(plugin))
        return nullptr;

    toml::value toml_data;
    try
    {
        toml_data = toml::parse(plugin);
        // DEBUG
        // std::cout << "###### TOML PARSE ######" << "\n\n";
        // std::cout << toml_data << "\n\n";
    }
    catch (const toml::syntax_error& e)
    {
        const std::string msg =
            fmt::format("Plugin file parsing failed:\n\"{}\"\n{}", plugin, e.what());
        LOG_ERROR("{}", msg);
        ptk_show_error(nullptr, "Plugin Load Error", msg);
        return nullptr;
    }

    bool plugin_good = false;

    // const std::uint64_t version = get_config_file_version(toml_data);

    // loop over all of [[Plugin]]
    for (const auto& section: toml::find<toml::array>(toml_data, PLUGIN_FILE_SECTION_PLUGIN))
    {
        // get [Plugin.name] and all vars
        for (const auto& [toml_name, toml_vars]: section.as_table())
        {
            // get var and value
            for (const auto& [toml_var, toml_value]: toml_vars.as_table())
            {
                std::stringstream ss_name;
                std::stringstream ss_var;
                std::stringstream ss_value;

                ss_name << toml_name;
                ss_var << toml_var;
                ss_value << toml_value;

                const std::string name{ss_name.str()};
                const std::string var{ss_var.str()};
                const std::string value{ztd::strip(ss_value.str(), "\"")};

                // LOG_INFO("name: {} | var: {} | value: {}", name, var, value);

                if (use && *use == PluginUse::NORMAL)
                {
                    if (ztd::startswith(name, "hand_"))
                    {
                        if (ztd::startswith(name, "hand_fs_"))
                            *use = PluginUse::HAND_FS;
                        else if (ztd::startswith(name, "hand_arc_"))
                            *use = PluginUse::HAND_ARC;
                        else if (ztd::startswith(name, "hand_net_"))
                            *use = PluginUse::HAND_NET;
                        else if (ztd::startswith(name, "hand_f_"))
                            *use = PluginUse::HAND_FILE;
                    }
                }
                xset_parse_plugin(plug_dir, name, var, value, use ? *use : PluginUse::NORMAL);
                if (!plugin_good)
                    plugin_good = true;
            }
        }
    }

    // clean plugin sets, set type
    bool top = true;
    xset_t rset = nullptr;
    for (xset_t set: xsets)
    {
        if (set->plugin && ztd::same(plug_dir, set->plug_dir))
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
    PluginData();
    ~PluginData();

    FMMainWindow* main_window;
    GtkWidget* handler_dlg;
    char* plug_dir;
    xset_t set;
    PluginJob job;
};

PluginData::PluginData()
{
    this->main_window = nullptr;
    this->handler_dlg = nullptr;
    this->plug_dir = nullptr;
    this->set = nullptr;
}

PluginData::~PluginData()
{
    if (this->plug_dir)
        free(this->plug_dir);
}

static void
on_install_plugin_cb(VFSFileTask* task, PluginData* plugin_data)
{
    (void)task;
    xset_t set;
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
        const std::string plugin =
            Glib::build_filename(plugin_data->plug_dir, PLUGIN_FILE_FILENAME);
        if (std::filesystem::exists(plugin))
        {
            PluginUse use = PluginUse::NORMAL;
            set = xset_import_plugin(plugin_data->plug_dir, &use);
            if (!set)
            {
                const std::string msg = fmt::format(
                    "The imported plugin directory does not contain a valid plugin.\n\n({}/)",
                    plugin_data->plug_dir);
                xset_msg_dialog(GTK_WIDGET(plugin_data->main_window),
                                GTK_MESSAGE_ERROR,
                                "Invalid Plugin",
                                GTK_BUTTONS_OK,
                                msg);
            }
            else if (use != PluginUse::BOOKMARKS)
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
                    ptk_handler_import(INT(use), plugin_data->handler_dlg, set);
                }
            }
            else if (plugin_data->job == PluginJob::COPY)
            {
                // copy
                set->plugin_top = false; // do not show tmp plugin in Plugins menu
                if (plugin_data->set)
                {
                    // paste after insert_set (plugin_data->set)
                    xset_t newset = xset_custom_copy(set, false, true);
                    newset->prev = ztd::strdup(plugin_data->set->name);
                    newset->next = plugin_data->set->next; // steal
                    if (plugin_data->set->next)
                    {
                        xset_t set_next = xset_get(plugin_data->set->next);
                        free(set_next->prev);
                        set_next->prev = ztd::strdup(newset->name);
                    }
                    plugin_data->set->next = ztd::strdup(newset->name);
                    if (plugin_data->set->tool != XSetTool::NOT)
                        newset->tool = XSetTool::CUSTOM;
                    else
                        newset->tool = XSetTool::NOT;
                }
                else
                {
                    // place on design clipboard
                    set_clipboard = set;
                    clipboard_is_cut = false;
                    if (xset_get_b(XSetName::PLUG_CVERB) || plugin_data->handler_dlg)
                    {
                        std::string msg;
                        const std::string label = clean_label(set->menu_label, false, false);
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
    }

    delete plugin_data;
}

static void
xset_remove_plugin(GtkWidget* parent, PtkFileBrowser* file_browser, xset_t set)
{
    if (!file_browser || !set || !set->plugin_top || !set->plug_dir)
        return;

    if (app_settings.get_confirm())
    {
        const std::string label = clean_label(set->menu_label, false, false);
        const std::string msg =
            fmt::format("Uninstall the '{}' plugin?\n\n( {} )", label, set->plug_dir);
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

    PluginData* plugin_data = new PluginData;
    plugin_data->plug_dir = ztd::strdup(set->plug_dir);
    plugin_data->set = set;
    plugin_data->job = PluginJob::REMOVE;
    ptask->complete_notify = (GFunc)on_install_plugin_cb;
    ptask->user_data = plugin_data;

    ptk_file_task_run(ptask);
}

void
install_plugin_file(void* main_win, GtkWidget* handler_dlg, std::string_view path,
                    std::string_view plug_dir, PluginJob job, xset_t insert_set)
{
    std::string own;
    const std::string plug_dir_q = bash_quote(plug_dir);
    const std::string file_path_q = bash_quote(path);

    FMMainWindow* main_window = FM_MAIN_WINDOW(main_win);
    // task
    PtkFileTask* ptask = ptk_file_exec_new("Install Plugin",
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

    PluginData* plugin_data = new PluginData;
    plugin_data->main_window = main_window;
    plugin_data->handler_dlg = handler_dlg;
    plugin_data->plug_dir = ztd::strdup(plug_dir.data());
    plugin_data->job = job;
    plugin_data->set = insert_set;
    ptask->complete_notify = (GFunc)on_install_plugin_cb;
    ptask->user_data = plugin_data;

    ptk_file_task_run(ptask);
}

static bool
xset_custom_export_files(xset_t set, std::string_view plug_dir)
{
    std::string path_src;
    std::string path_dest;

    if (set->plugin)
    {
        path_src = Glib::build_filename(set->plug_dir, set->plug_name);
        path_dest = Glib::build_filename(plug_dir.data(), set->plug_name);
    }
    else
    {
        path_src = Glib::build_filename(xset_get_config_dir(), "scripts", set->name);
        path_dest = Glib::build_filename(plug_dir.data(), set->name);
    }

    if (!(std::filesystem::exists(path_src) && dir_has_files(path_src)))
    {
        // skip empty or missing dirs
        return true;
    }

    int exit_status;
    const std::string command = fmt::format("cp -a {} {}", path_src, path_dest);
    print_command(command);
    Glib::spawn_command_line_sync(command, nullptr, nullptr, &exit_status);

    return !!exit_status;
}

static bool
xset_custom_export_write(xsetpak_t& xsetpak, xset_t set, std::string_view plug_dir)
{ // recursively write set, submenu sets, and next sets
    xsetpak_t xsetpak_local{{fmt::format("{}", set->name), xset_pack_set(set)}};

    xsetpak.insert(xsetpak_local.begin(), xsetpak_local.end());

    if (!xset_custom_export_files(set, plug_dir))
        return false;
    if (set->menu_style == XSetMenu::SUBMENU && set->child)
    {
        if (!xset_custom_export_write(xsetpak, xset_get(set->child), plug_dir))
            return false;
    }
    if (set->next)
    {
        if (!xset_custom_export_write(xsetpak, xset_get(set->next), plug_dir))
            return false;
    }
    return true;
}

void
xset_custom_export(GtkWidget* parent, PtkFileBrowser* file_browser, xset_t set)
{
    const char* deffolder;
    std::string deffile;

    // get new plugin filename
    xset_t save = xset_get(XSetName::PLUG_CFILE);
    if (save->s) //&& std::filesystem::is_directory(save->s)
    {
        deffolder = save->s;
    }
    else
    {
        if (!(deffolder = xset_get_s(XSetName::GO_SET_DEFAULT)))
            deffolder = ztd::strdup("/");
    }

    if (!set->plugin)
    {
        const std::string s1 = clean_label(set->menu_label, true, false);
        std::string type;
        if (ztd::startswith(set->name, "hand_arc_"))
            type = "archive-handler";
        else if (ztd::startswith(set->name, "hand_fs_"))
            type = "device-handler";
        else if (ztd::startswith(set->name, "hand_net_"))
            type = "protocol-handler";
        else if (ztd::startswith(set->name, "hand_f_"))
            type = "file-handler";
        else
            type = "plugin";

        deffile = fmt::format("{}-{}-{}.tar.xz", s1, PACKAGE_NAME, type);
    }
    else
    {
        const std::string s1 = Glib::path_get_basename(set->plug_dir);
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
    save->s = ztd::strdup(Glib::path_get_dirname(path));

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
        // const std::string plugin_path = Glib::build_filename(plug_dir, PLUGIN_FILE_FILENAME);

        // Do not want to store plugins prev/next/parent
        // TODO - Better way
        char* s_prev = set->prev;
        char* s_next = set->next;
        char* s_parent = set->parent;
        set->prev = nullptr;
        set->next = nullptr;
        set->parent = nullptr;
        xsetpak_t xsetpak{{fmt::format("{}", set->name), xset_pack_set(set)}};
        set->prev = s_prev;
        set->next = s_next;
        set->parent = s_parent;
        //

        if (!xset_custom_export_files(set, plug_dir))
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
            if (!xset_custom_export_write(xsetpak, xset_get(set->child), plug_dir))
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

        // Plugin TOML
        const toml::value toml_data = toml::value{
            {TOML_SECTION_VERSION,
             toml::value{
                 {TOML_KEY_VERSION, CONFIG_FILE_VERSION},
             }},

            {PLUGIN_FILE_SECTION_PLUGIN,
             toml::value{
                 xsetpak,
             }},
        };

        write_file(path, toml_data);
    }
    else
    {
        plug_dir = ztd::strdup(set->plug_dir);
    }

    // tar and delete tmp files
    // task
    PtkFileTask* ptask;
    ptask = ptk_file_exec_new("Export Plugin",
                              plug_dir.c_str(),
                              parent,
                              file_browser ? file_browser->task_view : nullptr);

    const std::string plug_dir_q = bash_quote(plug_dir);
    const std::string path_q = bash_quote(path);
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
xset_custom_activate(GtkWidget* item, xset_t set)
{
    (void)item;
    GtkWidget* parent;
    GtkWidget* task_view = nullptr;
    const char* cwd;
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
        LOG_WARN("xset_custom_activate !browser !desktop");
        return;
    }

    // name
    if (!set->plugin && !(!set->lock && XSetCMD(xset_get_int_set(set, XSetVar::X)) >
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
    XSetCMD cmd_type = XSetCMD(xset_get_int_set(set, XSetVar::X));
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
            else if (ztd::endswith(set->z, ".desktop"))
            {
                VFSAppDesktop desktop(set->z);
                if (desktop.get_exec() && desktop.get_exec()[0] != '\0')
                {
                    // get file list
                    std::vector<VFSFileInfo*> sel_files;
                    if (set->browser)
                    {
                        sel_files = ptk_file_browser_get_selected_files(set->browser);
                    }
                    else
                    {
                        cwd = ztd::strdup("/");
                    }

                    std::vector<std::string> open_files;

                    for (VFSFileInfo* file: sel_files)
                    {
                        const std::string open_file =
                            Glib::build_filename(cwd, vfs_file_info_get_name(file));

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

                    vfs_file_info_list_free(sel_files);
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
xset_custom_delete(xset_t set, bool delete_next)
{
    if (set->menu_style == XSetMenu::SUBMENU && set->child)
    {
        xset_t set_child = xset_get(set->child);
        xset_custom_delete(set_child, true);
    }

    if (delete_next && set->next)
    {
        xset_t set_next = xset_get(set->next);
        xset_custom_delete(set_next, true);
    }

    if (set == set_clipboard)
        set_clipboard = nullptr;

    const std::string path1 = Glib::build_filename(xset_get_config_dir(), "scripts", set->name);
    const std::string path2 = Glib::build_filename(xset_get_config_dir(), "plugin-data", set->name);
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

xset_t
xset_custom_remove(xset_t set)
{
    xset_t set_prev;
    xset_t set_next;
    xset_t set_parent;
    xset_t set_child;

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
xset_custom_insert_after(xset_t target, xset_t set)
{ // inserts single set 'set', no next
    xset_t target_next;

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
xset_clipboard_in_set(xset_t set)
{ // look upward to see if clipboard is in set's tree
    if (!set_clipboard || set->lock)
        return false;
    if (set == set_clipboard)
        return true;

    if (set->parent)
    {
        xset_t set_parent = xset_get(set->parent);
        if (xset_clipboard_in_set(set_parent))
            return true;
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

xset_t
xset_custom_new()
{
    const std::string setname = xset_custom_new_name();

    xset_t set;
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
    const std::string task_name = fmt::format("Edit {}", path);
    const std::string cwd = Glib::path_get_dirname(path);
    PtkFileTask* ptask = ptk_file_exec_new(task_name, cwd.c_str(), dlgparent, nullptr);
    ptask->task->exec_command = editor;
    ptask->task->exec_sync = false;
    ptask->task->exec_terminal = terminal;
    if (as_root)
        ptask->task->exec_as_user = "root";
    ptk_file_task_run(ptask);
}

const std::string
xset_get_keyname(xset_t set, int key_val, int key_mod)
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
    xset_t set = XSET(g_object_get_data(G_OBJECT(dlg), "set"));
    xset_t keyset = nullptr;
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

    for (xset_t set2: xsets)
    {
        if (set2 && set2 != set && set2->key > 0 && set2->key == event->keyval &&
            set2->keymod == keymod && set2 != keyset)
        {
            std::string name;
            if (set2->desc && !strcmp(set2->desc, "@plugin@mirror@") && set2->shared_key)
            {
                // set2 is plugin mirror
                xset_t rset = xset_get(set2->shared_key);
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
xset_set_key(GtkWidget* parent, xset_t set)
{
    std::string name;
    xset_t keyset;
    unsigned int newkey = 0;
    unsigned int newkeymod = 0;
    GtkWidget* dlgparent = nullptr;

    if (set->menu_label)
        name = clean_label(set->menu_label, false, true);
    else if (set->tool > XSetTool::CUSTOM)
        name = xset_get_builtin_toolitem_label(set->tool);
    else if (ztd::startswith(set->name, "open_all_type_"))
    {
        keyset = xset_get(XSetName::OPEN_ALL);
        name = clean_label(keyset->menu_label, false, true);
        if (set->shared_key)
            free(set->shared_key);
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
            for (xset_t set2: xsets)
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
xset_design_job(GtkWidget* item, xset_t set)
{
    xset_t newset;
    xset_t mset;
    xset_t childset;
    xset_t set_next;
    std::string msg;
    int response;
    char* folder;
    std::string folder2;
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
    XSetCMD cmd_type = XSetCMD(xset_get_int_set(set, XSetVar::X));

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
                xset_set_var(set, XSetVar::X, "0");
            break;
        case XSetJob::SCRIPT:
            xset_set_var(set, XSetVar::X, "1");
            cscript = xset_custom_get_script(set, true);
            if (!cscript)
                break;
            xset_edit(parent, cscript, false, false);
            free(cscript);
            break;
        case XSetJob::CUSTOM:
            if (set->z && set->z[0] != '\0')
            {
                folder2 = Glib::path_get_dirname(set->z);
                file2 = ztd::strdup(Glib::path_get_basename(set->z));
            }
            else
            {
                folder2 = "/usr/bin";
            }
            if ((custom_file = xset_file_dialog(parent,
                                                GTK_FILE_CHOOSER_ACTION_OPEN,
                                                "Choose Custom Executable",
                                                folder2.c_str(),
                                                file2.c_str())))
            {
                xset_set_var(set, XSetVar::X, "2");
                xset_set_var(set, XSetVar::Z, custom_file);
                free(custom_file);
            }
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
            if (ztd::startswith(set->name, "open_all_type_"))
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
            break;
        case XSetJob::SUBMENU:
        case XSetJob::SUBMENU_BOOK:
            if (ztd::startswith(set->name, "open_all_type_"))
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
            childset->menu_label = ztd::strdup("New _Command");
            break;
        case XSetJob::SEP:
            newset = xset_custom_new();
            newset->menu_style = XSetMenu::SEP;
            xset_custom_insert_after(set, newset);
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
            xset_t save;
            save = xset_get(XSetName::PLUG_IFILE);
            if (save->s) //&& std::filesystem::is_directory(save->s)
                folder = save->s;
            else
            {
                if (!(folder = xset_get_s(XSetName::GO_SET_DEFAULT)))
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
            save->s = ztd::strdup(Glib::path_get_dirname(file));

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
        case XSetJob::CUT:
            set_clipboard = set;
            clipboard_is_cut = true;
            break;
        case XSetJob::COPY:
            set_clipboard = set;
            clipboard_is_cut = false;
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

                set_clipboard = nullptr;
            }
            else
            {
                newset = xset_custom_copy(set_clipboard, false, false);
                xset_custom_insert_after(set, newset);
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
                    cmd_type == XSetCMD::APP)
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
            bool is_app;
            is_app = !set->lock && set->menu_style < XSetMenu::SUBMENU &&
                     cmd_type == XSetCMD::APP && set->tool <= XSetTool::CUSTOM;
            if (set->menu_style != XSetMenu::SEP && app_settings.get_confirm() && !is_app &&
                set->tool <= XSetTool::CUSTOM)
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

            if (set->parent && (set_next = xset_is(set->parent)) &&
                set_next->tool == XSetTool::CUSTOM && set_next->menu_style == XSetMenu::SUBMENU)
                // this set is first item in custom toolbar submenu
                update_toolbars = true;

            childset = xset_custom_remove(set);

            if (set->tool != XSetTool::NOT)
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
                free(prog);
            if (name)
                free(name);
            break;
        case XSetJob::EXPORT:
            if ((!set->lock || set->xset_name == XSetName::MAIN_BOOK) &&
                set->tool <= XSetTool::CUSTOM)
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
            xset_set_b(XSetName::CONTEXT_DLG, !xset_get_b(XSetName::CONTEXT_DLG));
            break;
        case XSetJob::BROWSE_FILES:
            if (set->tool > XSetTool::CUSTOM)
                break;
            if (set->plugin)
            {
                folder2 = Glib::build_filename(set->plug_dir, "files");
                if (!std::filesystem::exists(folder2))
                    folder2 = Glib::build_filename(set->plug_dir, set->plug_name);
            }
            else
            {
                folder2 = Glib::build_filename(xset_get_config_dir(), "scripts", set->name);
            }
            if (!std::filesystem::exists(folder2) && !set->plugin)
            {
                std::filesystem::create_directories(folder2);
                std::filesystem::permissions(folder2, std::filesystem::perms::owner_all);
            }

            if (set->browser)
            {
                ptk_file_browser_emit_open(set->browser,
                                           folder2.c_str(),
                                           PtkOpenAction::PTK_OPEN_DIR);
            }
            break;
        case XSetJob::BROWSE_DATA:
            if (set->tool > XSetTool::CUSTOM)
                break;
            if (set->plugin)
            {
                mset = xset_get_plugin_mirror(set);
                folder2 = Glib::build_filename(xset_get_config_dir(), "plugin-data", mset->name);
            }
            else
            {
                folder2 = Glib::build_filename(xset_get_config_dir(), "plugin-data", set->name);
            }
            if (!std::filesystem::exists(folder2))
            {
                std::filesystem::create_directories(folder2);
                std::filesystem::permissions(folder2, std::filesystem::perms::owner_all);
            }

            if (set->browser)
            {
                ptk_file_browser_emit_open(set->browser,
                                           folder2.c_str(),
                                           PtkOpenAction::PTK_OPEN_DIR);
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
            set_next = xset_get_panel(1, XSetPanel::TOOL_L);
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

    if ((set && !set->lock && set->tool != XSetTool::NOT) || update_toolbars)
        main_window_rebuild_all_toolbars(set ? set->browser : nullptr);

    // autosave
    autosave_request_add();
}

static bool
xset_job_is_valid(xset_t set, XSetJob job)
{
    bool no_remove = false;
    bool no_paste = false;
    bool open_all = false;

    if (!set)
        return false;

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
        open_all = true;

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
                    if (XSetCMD(xset_get_int_set(set, XSetVar::X)) == XSetCMD::SCRIPT)
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
xset_design_show_menu(GtkWidget* menu, xset_t set, xset_t book_insert, unsigned int button,
                      std::time_t time)
{
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
    // if (ztd::startswith(set->name, "open_all_type_"))
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
    newitem = xset_design_additem(design_menu, "_Remove", XSetJob::REMOVE, set);
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
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_Insert,
                                   (GdkModifierType)0,
                                   GTK_ACCEL_VISIBLE);

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

        for (std::size_t i = INT(XSetTool::DEVICES); i < builtin_tool_name.size(); ++i)
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
        if (!xset_get_b_panel(1, XSetPanel::TOOL_L))
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
        if (XSetCMD(xset_get_int_set(set, XSetVar::X)) == XSetCMD::SCRIPT)
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
        else if (XSetCMD(xset_get_int_set(set, XSetVar::X)) == XSetCMD::LINE)
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
xset_design_cb(GtkWidget* item, GdkEventButton* event, xset_t set)
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
                        if (XSetCMD(xset_get_int_set(set, XSetVar::X)) == XSetCMD::SCRIPT)
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
    xset_t set;

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
                    if (XSetCMD(xset_get_int_set(set, XSetVar::X)) == XSetCMD::SCRIPT)
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
                        if (XSetCMD(xset_get_int_set(set, XSetVar::X)) == XSetCMD::SCRIPT)
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
                cb_func(item, cb_data);
            else if (!set->lock)
                xset_custom_activate(item, rset);
            break;
    }

    if (rset->menu_style != XSetMenu::NORMAL)
        autosave_request_add();
}

int
xset_msg_dialog(GtkWidget* parent, GtkMessageType action, std::string_view title,
                GtkButtonsType buttons, std::string_view msg1)
{
    std::string msg2;
    return xset_msg_dialog(parent, action, title, buttons, msg1, msg2);
}

int
xset_msg_dialog(GtkWidget* parent, GtkMessageType action, std::string_view title,
                GtkButtonsType buttons, std::string_view msg1, std::string_view msg2)
{
    GtkWidget* dlgparent = nullptr;

    if (parent)
        dlgparent = gtk_widget_get_toplevel(parent);

    GtkWidget* dlg =
        gtk_message_dialog_new(GTK_WINDOW(dlgparent),
                               GtkDialogFlags(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                               action,
                               buttons,
                               msg1.data(),
                               nullptr);

    if (action == GTK_MESSAGE_INFO)
        xset_set_window_icon(GTK_WINDOW(dlg));
    gtk_window_set_role(GTK_WINDOW(dlg), "msg_dialog");

    if (!msg2.empty())
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), msg2.data(), nullptr);

    gtk_window_set_title(GTK_WINDOW(dlg), title.data());

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
    int width = xset_get_int(XSetName::MAIN_ICON, XSetVar::X);
    int height = xset_get_int(XSetName::MAIN_ICON, XSetVar::Y);
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
        xset_set(XSetName::MAIN_ICON, XSetVar::X, std::to_string(allocation.width));
        xset_set(XSetName::MAIN_ICON, XSetVar::Y, std::to_string(allocation.height));
    }
    gtk_widget_destroy(icon_chooser);

    // remove busy cursor
    gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(parent)), nullptr);

    return icon;
}

bool
xset_text_dialog(GtkWidget* parent, std::string_view title, std::string_view msg1,
                 std::string_view msg2, const char* defstring, char** answer,
                 std::string_view defreset, bool edit_care)
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
                                            msg1.data(),
                                            nullptr);
    xset_set_window_icon(GTK_WINDOW(dlg));
    gtk_window_set_role(GTK_WINDOW(dlg), "text_dialog");

    width = xset_get_int(XSetName::TEXT_DLG, XSetVar::S);
    height = xset_get_int(XSetName::TEXT_DLG, XSetVar::Z);
    if (width && height)
        gtk_window_set_default_size(GTK_WINDOW(dlg), width, height);
    else
        gtk_window_set_default_size(GTK_WINDOW(dlg), 600, 400);
    // gtk_widget_set_size_request( GTK_WIDGET( dlg ), 600, 400 );

    gtk_window_set_resizable(GTK_WINDOW(dlg), true);

    if (!msg2.empty())
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), msg2.data(), nullptr);

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

    gtk_window_set_title(GTK_WINDOW(dlg), title.data());

    if (edit_care)
    {
        gtk_widget_grab_focus(btn_ok);
        if (btn_default)
            gtk_widget_set_sensitive(btn_default, false);
    }

    std::string ans;
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
                if (ztd::contains(ans, "\n"))
                {
                    ptk_show_error(GTK_WINDOW(dlgparent),
                                   "Error",
                                   "Your input is invalid because it contains linefeeds");
                }
                else
                {
                    if (*answer)
                        free(*answer);

                    ans = ztd::strip(ans);
                    if (ans.empty())
                        *answer = nullptr;
                    else
                        *answer = ztd::strdup(Glib::filename_from_utf8(ans));

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
                gtk_text_buffer_set_text(buf, defreset.data(), -1);
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
        xset_set(XSetName::TEXT_DLG, XSetVar::S, std::to_string(width));
        xset_set(XSetName::TEXT_DLG, XSetVar::Z, std::to_string(height));
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
        path = xset_get_s(XSetName::GO_SET_DEFAULT);
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
            const std::string path2 = Glib::build_filename(deffolder, deffile);
            gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dlg), path2.c_str());
        }
    }

    int width = xset_get_int(XSetName::FILE_DLG, XSetVar::X);
    int height = xset_get_int(XSetName::FILE_DLG, XSetVar::Y);
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
        xset_set(XSetName::FILE_DLG, XSetVar::X, std::to_string(width));
        xset_set(XSetName::FILE_DLG, XSetVar::Y, std::to_string(height));
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

static void
xset_builtin_tool_activate(XSetTool tool_type, xset_t set, GdkEventButton* event)
{
    xset_t set2;
    panel_t p;
    MainWindowPanel mode;
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
        file_browser =
            PTK_FILE_BROWSER_REINTERPRET(fm_main_window_get_current_file_browser(main_window));
        p = file_browser->mypanel;
        mode = main_window->panel_context.at(p);
    }
    if (!PTK_IS_FILE_BROWSER(file_browser))
        return;

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
            LOG_WARN("xset_builtin_tool_activate invalid tool_type");
    }
}

const char*
xset_get_builtin_toolitem_label(XSetTool tool_type)
{
    if (tool_type < XSetTool::DEVICES || tool_type >= XSetTool::INVALID)
        return nullptr;
    return builtin_tool_name[INT(tool_type)];
}

static xset_t
xset_new_builtin_toolitem(XSetTool tool_type)
{
    if (tool_type < XSetTool::DEVICES || tool_type >= XSetTool::INVALID)
        return nullptr;

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

    // LOG_INFO("on_tool_icon_button_press  {}   button = {}", set->menu_label, event->button);
    if (event->type != GDK_BUTTON_PRESS)
        return false;
    unsigned int keymod = ptk_get_keymod(event->state);

    // get and focus browser
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER(g_object_get_data(G_OBJECT(widget), "browser"));
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
                        XSetCMD(xset_get_int_set(set, XSetVar::X)) == XSetCMD::SCRIPT)
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
on_tool_menu_button_press(GtkWidget* widget, GdkEventButton* event, xset_t set)
{
    // LOG_INFO("on_tool_menu_button_press  {}   button = {}", set->menu_label, event->button);
    if (event->type != GDK_BUTTON_PRESS)
        return false;
    unsigned int keymod = ptk_get_keymod(event->state);
    if (keymod != 0 || event->button != 1)
        return on_tool_icon_button_press(widget, event, set);

    // get and focus browser
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER(g_object_get_data(G_OBJECT(widget), "browser"));
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
            xset_t set_child;
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
    const std::string str = fmt::format("GtkWidget {{ padding-left: {}px; padding-right: {}px; "
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
                  int icon_size, xset_t set, bool show_tooltips)
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
    xset_t set_next;
    char* new_menu_label = nullptr;
    GdkPixbuf* pixbuf = nullptr;
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
        set->shared_key = ztd::strdup(builtin_tool_shared_key[INT(set->tool)]);

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
        const std::string icon_file =
            Glib::build_filename(xset_get_config_dir(), "scripts", set->name, "icon");
        if (std::filesystem::exists(icon_file))
            icon_name = ztd::strdup(icon_file);
    }

    char* menu_label;
    menu_label = set->menu_label;
    if (!menu_label && set->tool > XSetTool::CUSTOM)
        menu_label = (char*)xset_get_builtin_toolitem_label(set->tool);

    if (menu_style == XSetMenu::NORMAL)
        menu_style = XSetMenu::STRING;

    GtkWidget* ebox;
    GtkWidget* hbox;
    xset_t set_child;
    XSetCMD cmd_type;

    switch (menu_style)
    {
        case XSetMenu::STRING:
            // normal item
            cmd_type = XSetCMD(xset_get_int_set(set, XSetVar::X));
            if (set->tool > XSetTool::CUSTOM)
            {
                // builtin tool item
                if (icon_name)
                    image = xset_get_image(icon_name, (GtkIconSize)icon_size);
                else if (set->tool > XSetTool::CUSTOM && set->tool < XSetTool::INVALID)
                    image =
                        xset_get_image(builtin_tool_icon[INT(set->tool)], (GtkIconSize)icon_size);
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
                image = xset_get_image(builtin_tool_icon[INT(set->tool)], (GtkIconSize)icon_size);
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
                icon_name = builtin_tool_icon[INT(set->tool)];
            else if (!icon_name && set_child && set->tool == XSetTool::CUSTOM)
            {
                // take the auto icon from the first item in the submenu
                cmd_type = XSetCMD(xset_get_int_set(set_child, XSetVar::X));
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
    int stop_b4;
    xset_t set;
    xset_t set_target;

    // LOG_INFO("xset_fill_toolbar {}", set_parent->name);
    if (!(file_browser && toolbar && set_parent))
        return;

    set_parent->lock = true;
    set_parent->menu_style = XSetMenu::SUBMENU;

    GtkIconSize icon_size = gtk_toolbar_get_icon_size(GTK_TOOLBAR(toolbar));

    xset_t set_child = nullptr;
    if (set_parent->child)
        set_child = xset_is(set_parent->child);
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
                stop_b4 = 2;
            else
                stop_b4 = default_tools.size();
            set_target = set_child;
            for (int i = 0; i < stop_b4; ++i)
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
    xset_t set;

    // separator
    set = xset_get(XSetName::SEPARATOR);
    set->menu_style = XSetMenu::SEP;

    // dev menu
    set = xset_set(XSetName::DEV_MENU_REMOVE, XSetVar::MENU_LABEL, "Remo_ve / Eject");
    xset_set_var(set, XSetVar::ICN, "gtk-disconnect");

    set = xset_set(XSetName::DEV_MENU_UNMOUNT, XSetVar::MENU_LABEL, "_Unmount");
    xset_set_var(set, XSetVar::ICN, "gtk-remove");

    set = xset_set(XSetName::DEV_MENU_OPEN, XSetVar::MENU_LABEL, "_Open");
    xset_set_var(set, XSetVar::ICN, "gtk-open");

    set = xset_set(XSetName::DEV_MENU_TAB, XSetVar::MENU_LABEL, "Open In _Tab");
    xset_set_var(set, XSetVar::ICN, "gtk-add");

    set = xset_set(XSetName::DEV_MENU_MOUNT, XSetVar::MENU_LABEL, "_Mount");
    xset_set_var(set, XSetVar::ICN, "drive-removable-media");

    set = xset_set(XSetName::DEV_MENU_MARK, XSetVar::MENU_LABEL, "_Bookmark");
    xset_set_var(set, XSetVar::ICN, "gtk-add");

    set = xset_set(XSetName::DEV_PROP, XSetVar::MENU_LABEL, "_Properties");
    xset_set_var(set, XSetVar::ICN, "gtk-properties");

    set = xset_set(XSetName::DEV_MENU_SETTINGS, XSetVar::MENU_LABEL, "Setti_ngs");
    xset_set_var(set, XSetVar::ICN, "gtk-properties");
    set->menu_style = XSetMenu::SUBMENU;

    // dev settings
    set = xset_set(XSetName::DEV_SHOW, XSetVar::MENU_LABEL, "S_how");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "dev_show_internal_drives dev_show_empty dev_show_partition_tables dev_show_net "
                 "dev_show_file dev_ignore_udisks_hide dev_show_hide_volumes dev_dispname");

    set = xset_set(XSetName::DEV_SHOW_INTERNAL_DRIVES, XSetVar::MENU_LABEL, "_Internal Drives");
    set->menu_style = XSetMenu::CHECK;
    set->b = geteuid() == 0 ? XSetB::XSET_B_TRUE : XSetB::XSET_B_FALSE;

    set = xset_set(XSetName::DEV_SHOW_EMPTY, XSetVar::MENU_LABEL, "_Empty Drives");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE; // geteuid() == 0 ? XSetB::XSET_B_TRUE : XSetB::XSET_B_UNSET;

    set = xset_set(XSetName::DEV_SHOW_PARTITION_TABLES, XSetVar::MENU_LABEL, "_Partition Tables");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::DEV_SHOW_NET, XSetVar::MENU_LABEL, "Mounted _Networks");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::DEV_SHOW_FILE, XSetVar::MENU_LABEL, "Mounted _Other");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::DEV_SHOW_HIDE_VOLUMES, XSetVar::MENU_LABEL, "_Volumes...");
    xset_set_var(set, XSetVar::TITLE, "Show/Hide Volumes");
    xset_set_var(
        set,
        XSetVar::DESC,
        "To force showing or hiding of some volumes, overriding other settings, you can specify "
        "the devices, volume labels, or device IDs in the space-separated list "
        "below.\n\nExample:  +/dev/sdd1 -Label With Space +ata-OCZ-part4\nThis would cause "
        "/dev/sdd1 and the OCZ device to be shown, and the volume with label \"Label With "
        "Space\" to be hidden.\n\nThere must be a space between entries and a plus or minus sign "
        "directly before each item.  This list is case-sensitive.\n\n");

    set = xset_set(XSetName::DEV_IGNORE_UDISKS_HIDE, XSetVar::MENU_LABEL, "Ignore _Hide Policy");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::DEV_DISPNAME, XSetVar::MENU_LABEL, "_Display Name");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Display Name Format");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter device display name format:\n\nUse:\n\t%%v\tdevice filename (eg "
                 "sdd1)\n\t%%s\ttotal size (eg 800G)\n\t%%t\tfstype (eg ext4)\n\t%%l\tvolume "
                 "label (eg Label or [no media])\n\t%%m\tmount point if mounted, or "
                 "---\n\t%%i\tdevice ID\n\t%%n\tmajor:minor device numbers (eg 15:3)\n");
    xset_set_var(set, XSetVar::S, "%v %s %l %m");
    xset_set_var(set, XSetVar::Z, "%v %s %l %m");
    xset_set_var(set, XSetVar::ICN, "gtk-edit");

    set = xset_set(XSetName::DEV_MENU_AUTO, XSetVar::MENU_LABEL, "_Auto Mount");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "dev_automount_optical dev_automount_removable dev_ignore_udisks_nopolicy "
                 "dev_automount_volumes dev_automount_dirs dev_auto_open dev_unmount_quit");

    set = xset_set(XSetName::DEV_AUTOMOUNT_OPTICAL, XSetVar::MENU_LABEL, "Mount _Optical");
    set->b = geteuid() == 0 ? XSetB::XSET_B_FALSE : XSetB::XSET_B_TRUE;
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::DEV_AUTOMOUNT_REMOVABLE, XSetVar::MENU_LABEL, "_Mount Removable");
    set->b = geteuid() == 0 ? XSetB::XSET_B_FALSE : XSetB::XSET_B_TRUE;
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::DEV_AUTOMOUNT_VOLUMES, XSetVar::MENU_LABEL, "Mount _Volumes...");
    xset_set_var(set, XSetVar::TITLE, "Auto-Mount Volumes");
    xset_set_var(
        set,
        XSetVar::DESC,
        "To force or prevent automounting of some volumes, overriding other settings, you can "
        "specify the devices, volume labels, or device IDs in the space-separated list "
        "below.\n\nExample:  +/dev/sdd1 -Label With Space +ata-OCZ-part4\nThis would cause "
        "/dev/sdd1 and the OCZ device to be auto-mounted when detected, and the volume with "
        "label \"Label With Space\" to be ignored.\n\nThere must be a space between entries and "
        "a plus or minus sign directly before each item.  This list is case-sensitive.\n\n");

    set = xset_set(XSetName::DEV_AUTOMOUNT_DIRS, XSetVar::MENU_LABEL, "Mount _Dirs...");
    xset_set_var(set, XSetVar::TITLE, "Automatic Mount Point Dirs");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(
        set,
        XSetVar::DESC,
        "Enter the directory where SpaceFM should automatically create mount point directories "
        "for fuse and similar filesystems (%%a in handler commands).  This directory must be "
        "user-writable (do NOT use /media), and empty subdirectories will be removed.  If left "
        "blank, ~/.cache/spacefm/ (or $XDG_CACHE_HOME/spacefm/) is used.  The following "
        "variables are recognized: $USER $UID $HOME $XDG_RUNTIME_DIR $XDG_CACHE_HOME\n\nNote "
        "that some handlers or mount programs may not obey this setting.\n");

    set = xset_set(XSetName::DEV_AUTO_OPEN, XSetVar::MENU_LABEL, "Open _Tab");
    set->b = XSetB::XSET_B_TRUE;
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::DEV_UNMOUNT_QUIT, XSetVar::MENU_LABEL, "_Unmount On Exit");
    set->b = XSetB::XSET_B_UNSET;
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::DEV_EXEC, XSetVar::MENU_LABEL, "Auto _Run");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "dev_exec_fs dev_exec_audio dev_exec_video separator dev_exec_insert "
                 "dev_exec_unmount dev_exec_remove");
    xset_set_var(set, XSetVar::ICN, "gtk-execute");

    set = xset_set(XSetName::DEV_EXEC_FS, XSetVar::MENU_LABEL, "On _Mount");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Auto Run On Mount");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or bash command line to be run automatically after a removable "
                 "drive or data disc is auto-mounted:\n\nUse:\n\t%%v\tdevice (eg "
                 "/dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_set(XSetName::DEV_EXEC_AUDIO, XSetVar::MENU_LABEL, "On _Audio CD");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Auto Run On Audio CD");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or bash command line to be run automatically when an audio CD is "
                 "inserted in a qualified device:\n\nUse:\n\t%%v\tdevice (eg "
                 "/dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_set(XSetName::DEV_EXEC_VIDEO, XSetVar::MENU_LABEL, "On _Video DVD");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Auto Run On Video DVD");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or bash command line to be run automatically when a video DVD is "
                 "auto-mounted:\n\nUse:\n\t%%v\tdevice (eg /dev/sda1)\n\t%%l\tdevice "
                 "label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_set(XSetName::DEV_EXEC_INSERT, XSetVar::MENU_LABEL, "On _Insert");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Auto Run On Insert");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or bash command line to be run automatically when any device is "
                 "inserted:\n\nUse:\n\t%%v\tdevice added (eg /dev/sda1)\n\t%%l\tdevice "
                 "label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_set(XSetName::DEV_EXEC_UNMOUNT, XSetVar::MENU_LABEL, "On _Unmount");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Auto Run On Unmount");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or bash command line to be run automatically when any device is "
                 "unmounted by any means:\n\nUse:\n\t%%v\tdevice unmounted (eg "
                 "/dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_set(XSetName::DEV_EXEC_REMOVE, XSetVar::MENU_LABEL, "On _Remove");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Auto Run On Remove");
    xset_set_var(
        set,
        XSetVar::DESC,
        "Enter program or bash command line to be run automatically when any device is removed "
        "(ejection of media does not qualify):\n\nUse:\n\t%%v\tdevice removed (eg "
        "/dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_set(XSetName::DEV_IGNORE_UDISKS_NOPOLICY, XSetVar::MENU_LABEL, "Ignore _No Policy");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::DEV_MOUNT_OPTIONS, XSetVar::MENU_LABEL, "_Mount Options");
    xset_set_var(
        set,
        XSetVar::DESC,
        "Enter your comma- or space-separated list of default mount options below (%%o in "
        "handlers).\n\nIn addition to regular options, you can also specify options to be added "
        "or removed for a specific filesystem type by using the form OPTION+FSTYPE or "
        "OPTION-FSTYPE.\n\nExample:  nosuid, sync+vfat, sync+ntfs, noatime, noatime-ext4\nThis "
        "will add nosuid and noatime for all filesystem types, add sync for vfat and ntfs only, "
        "and remove noatime for ext4.\n\nNote: Some options, such as nosuid, may be added by the "
        "mount program even if you do not include them.  Options in fstab take precedence.  "
        "pmount and some handlers may ignore options set here.");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Default Mount Options");
    xset_set_var(set, XSetVar::S, "noexec, nosuid, noatime");
    xset_set_var(set, XSetVar::Z, "noexec, nosuid, noatime");
    xset_set_var(set, XSetVar::ICN, "gtk-edit");

    set = xset_set(XSetName::DEV_CHANGE, XSetVar::MENU_LABEL, "_Change Detection");
    xset_set_var(
        set,
        XSetVar::DESC,
        "Enter your comma- or space-separated list of filesystems which should NOT be monitored "
        "for file changes.  This setting only affects non-block devices (such as nfs or fuse), "
        "and is usually used to prevent SpaceFM becoming unresponsive with network filesystems.  "
        "Loading of thumbnails and subdirectory sizes will also be disabled.");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Change Detection Blacklist");
    xset_set_var(set, XSetVar::ICN, "gtk-edit");
    set->s = ztd::strdup("cifs curlftpfs ftpfs fuse.sshfs nfs smbfs");
    set->z = ztd::strdup(set->s);

    set = xset_set(XSetName::DEV_FS_CNF, XSetVar::MENU_LABEL, "_Device Handlers");
    xset_set_var(set, XSetVar::ICON, "gtk-preferences");

    set = xset_set(XSetName::DEV_NET_CNF, XSetVar::MENU_LABEL, "_Protocol Handlers");
    xset_set_var(set, XSetVar::ICON, "gtk-preferences");

    // dev icons
    set = xset_set(XSetName::DEV_ICON, XSetVar::MENU_LABEL, "_Icon");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(
        set,
        XSetVar::DESC,
        "dev_icon_internal_mounted dev_icon_internal_unmounted separator dev_icon_remove_mounted "
        "dev_icon_remove_unmounted separator dev_icon_optical_mounted dev_icon_optical_media "
        "dev_icon_optical_nomedia dev_icon_audiocd separator dev_icon_floppy_mounted "
        "dev_icon_floppy_unmounted separator dev_icon_network dev_icon_file");

    set = xset_set(XSetName::DEV_ICON_AUDIOCD, XSetVar::MENU_LABEL, "Audio CD");
    set->menu_style = XSetMenu::ICON;
    xset_set_var(set, XSetVar::ICN, "gtk-cdrom");

    set = xset_set(XSetName::DEV_ICON_OPTICAL_MOUNTED, XSetVar::MENU_LABEL, "Optical Mounted");
    set->menu_style = XSetMenu::ICON;
    xset_set_var(set, XSetVar::ICN, "gtk-cdrom");

    set = xset_set(XSetName::DEV_ICON_OPTICAL_MEDIA, XSetVar::MENU_LABEL, "Optical Has Media");
    set->menu_style = XSetMenu::ICON;
    xset_set_var(set, XSetVar::ICN, "gtk-yes");

    set = xset_set(XSetName::DEV_ICON_OPTICAL_NOMEDIA, XSetVar::MENU_LABEL, "Optical No Media");
    set->menu_style = XSetMenu::ICON;
    xset_set_var(set, XSetVar::ICN, "gtk-close");

    set = xset_set(XSetName::DEV_ICON_FLOPPY_MOUNTED, XSetVar::MENU_LABEL, "Floppy Mounted");
    set->menu_style = XSetMenu::ICON;
    xset_set_var(set, XSetVar::ICN, "gtk-floppy");

    set = xset_set(XSetName::DEV_ICON_FLOPPY_UNMOUNTED, XSetVar::MENU_LABEL, "Floppy Unmounted");
    set->menu_style = XSetMenu::ICON;
    xset_set_var(set, XSetVar::ICN, "gtk-floppy");

    set = xset_set(XSetName::DEV_ICON_REMOVE_MOUNTED, XSetVar::MENU_LABEL, "Removable Mounted");
    set->menu_style = XSetMenu::ICON;
    xset_set_var(set, XSetVar::ICN, "gtk-add");

    set = xset_set(XSetName::DEV_ICON_REMOVE_UNMOUNTED, XSetVar::MENU_LABEL, "Removable Unmounted");
    set->menu_style = XSetMenu::ICON;
    xset_set_var(set, XSetVar::ICN, "gtk-remove");

    set = xset_set(XSetName::DEV_ICON_INTERNAL_MOUNTED, XSetVar::MENU_LABEL, "Internal Mounted");
    set->menu_style = XSetMenu::ICON;
    xset_set_var(set, XSetVar::ICN, "gtk-open");

    set =
        xset_set(XSetName::DEV_ICON_INTERNAL_UNMOUNTED, XSetVar::MENU_LABEL, "Internal Unmounted");
    set->menu_style = XSetMenu::ICON;
    xset_set_var(set, XSetVar::ICN, "gtk-harddisk");

    set = xset_set(XSetName::DEV_ICON_NETWORK, XSetVar::MENU_LABEL, "Mounted Network");
    set->menu_style = XSetMenu::ICON;
    xset_set_var(set, XSetVar::ICN, "gtk-network");

    set = xset_set(XSetName::DEV_ICON_FILE, XSetVar::MENU_LABEL, "Mounted Other");
    set->menu_style = XSetMenu::ICON;
    xset_set_var(set, XSetVar::ICN, "gtk-file");

    set = xset_set(XSetName::BOOK_OPEN, XSetVar::MENU_LABEL, "_Open");
    xset_set_var(set, XSetVar::ICN, "gtk-open");

    set = xset_set(XSetName::BOOK_SETTINGS, XSetVar::MENU_LABEL, "_Settings");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::ICN, "gtk-properties");

    set = xset_set(XSetName::BOOK_ICON, XSetVar::MENU_LABEL, "Bookmark _Icon");
    set->menu_style = XSetMenu::ICON;
    // do not set a default icon for book_icon

    set = xset_set(XSetName::BOOK_MENU_ICON, XSetVar::MENU_LABEL, "Sub_menu Icon");
    set->menu_style = XSetMenu::ICON;
    // do not set a default icon for book_menu_icon

    set = xset_set(XSetName::BOOK_ADD, XSetVar::MENU_LABEL, "New _Bookmark");
    xset_set_var(set, XSetVar::ICN, "gtk-jump-to");

    set = xset_set(XSetName::MAIN_BOOK, XSetVar::MENU_LABEL, "_Bookmarks");
    xset_set_var(set, XSetVar::ICN, "gtk-directory");
    set->menu_style = XSetMenu::SUBMENU;

    // Rename/Move Dialog
    set = xset_set(XSetName::MOVE_NAME, XSetVar::MENU_LABEL, "_Name");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::MOVE_FILENAME, XSetVar::MENU_LABEL, "F_ilename");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::MOVE_PARENT, XSetVar::MENU_LABEL, "_Parent");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::MOVE_PATH, XSetVar::MENU_LABEL, "P_ath");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::MOVE_TYPE, XSetVar::MENU_LABEL, "Typ_e");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::MOVE_TARGET, XSetVar::MENU_LABEL, "Ta_rget");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::MOVE_TEMPLATE, XSetVar::MENU_LABEL, "Te_mplate");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::MOVE_OPTION, XSetVar::MENU_LABEL, "_Option");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "move_copy move_link move_copyt move_linkt move_as_root");

    set = xset_set(XSetName::MOVE_COPY, XSetVar::MENU_LABEL, "_Copy");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::MOVE_LINK, XSetVar::MENU_LABEL, "_Link");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::MOVE_COPYT, XSetVar::MENU_LABEL, "Copy _Target");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::MOVE_LINKT, XSetVar::MENU_LABEL, "Lin_k Target");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::MOVE_AS_ROOT, XSetVar::MENU_LABEL, "_As Root");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::MOVE_DLG_HELP, XSetVar::MENU_LABEL, "_Help");
    xset_set_var(set, XSetVar::ICN, "gtk-help");

    set = xset_set(XSetName::MOVE_DLG_CONFIRM_CREATE, XSetVar::MENU_LABEL, "_Confirm Create");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    // status bar
    set = xset_set(XSetName::STATUS_MIDDLE, XSetVar::MENU_LABEL, "_Middle Click");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "status_name status_path status_info status_hide");

    set = xset_set(XSetName::STATUS_NAME, XSetVar::MENU_LABEL, "Copy _Name");
    set->menu_style = XSetMenu::RADIO;

    set = xset_set(XSetName::STATUS_PATH, XSetVar::MENU_LABEL, "Copy _Path");
    set->menu_style = XSetMenu::RADIO;

    set = xset_set(XSetName::STATUS_INFO, XSetVar::MENU_LABEL, "File _Info");
    set->menu_style = XSetMenu::RADIO;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::STATUS_HIDE, XSetVar::MENU_LABEL, "_Hide Panel");
    set->menu_style = XSetMenu::RADIO;

    // MAIN WINDOW MENUS

    // File
    set = xset_set(XSetName::MAIN_NEW_WINDOW, XSetVar::MENU_LABEL, "New _Window");
    xset_set_var(set, XSetVar::ICN, "spacefm");

    set = xset_set(XSetName::MAIN_ROOT_WINDOW, XSetVar::MENU_LABEL, "R_oot Window");
    xset_set_var(set, XSetVar::ICN, "gtk-dialog-warning");

    set = xset_set(XSetName::MAIN_SEARCH, XSetVar::MENU_LABEL, "_File Search");
    xset_set_var(set, XSetVar::ICN, "gtk-find");

    set = xset_set(XSetName::MAIN_TERMINAL, XSetVar::MENU_LABEL, "_Terminal");
    set->b = XSetB::XSET_B_UNSET; // discovery notification

    set = xset_set(XSetName::MAIN_ROOT_TERMINAL, XSetVar::MENU_LABEL, "_Root Terminal");
    xset_set_var(set, XSetVar::ICN, "gtk-dialog-warning");

    // was previously used for 'Save Session' < 0.9.4 as XSetMenu::NORMAL
    set = xset_set(XSetName::MAIN_SAVE_SESSION, XSetVar::MENU_LABEL, "Open _URL");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::ICN, "gtk-network");
    xset_set_var(set, XSetVar::TITLE, "Open URL");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter URL in the "
                 "format:\n\tPROTOCOL://USERNAME:PASSWORD@HOST:PORT/SHARE\n\nExamples:\n\tftp://"
                 "mirrors.kernel.org\n\tsmb://user:pass@10.0.0.1:50/docs\n\tssh://"
                 "user@sys.domain\n\tmtp://\n\nIncluding a password is unsafe.  To bookmark a "
                 "URL, right-click on the mounted network in Devices and select Bookmark.\n");
    set->line = nullptr;

    set = xset_set(XSetName::MAIN_SAVE_TABS, XSetVar::MENU_LABEL, "Save Ta_bs");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::MAIN_EXIT, XSetVar::MENU_LABEL, "E_xit");
    xset_set_var(set, XSetVar::ICN, "gtk-quit");

    // View
    set = xset_set(XSetName::PANEL1_SHOW, XSetVar::MENU_LABEL, "Panel _1");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::PANEL2_SHOW, XSetVar::MENU_LABEL, "Panel _2");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::PANEL3_SHOW, XSetVar::MENU_LABEL, "Panel _3");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::PANEL4_SHOW, XSetVar::MENU_LABEL, "Panel _4");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::MAIN_PBAR, XSetVar::MENU_LABEL, "Panel _Bar");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::MAIN_FOCUS_PANEL, XSetVar::MENU_LABEL, "F_ocus");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "panel_prev panel_next panel_hide panel_1 panel_2 panel_3 panel_4");
    xset_set_var(set, XSetVar::ICN, "gtk-go-forward");

    xset_set(XSetName::PANEL_PREV, XSetVar::MENU_LABEL, "_Prev");
    xset_set(XSetName::PANEL_NEXT, XSetVar::MENU_LABEL, "_Next");
    xset_set(XSetName::PANEL_HIDE, XSetVar::MENU_LABEL, "_Hide");
    xset_set(XSetName::PANEL_1, XSetVar::MENU_LABEL, "Panel _1");
    xset_set(XSetName::PANEL_2, XSetVar::MENU_LABEL, "Panel _2");
    xset_set(XSetName::PANEL_3, XSetVar::MENU_LABEL, "Panel _3");
    xset_set(XSetName::PANEL_4, XSetVar::MENU_LABEL, "Panel _4");

    set = xset_set(XSetName::MAIN_AUTO, XSetVar::MENU_LABEL, "_Event Manager");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "auto_inst auto_win auto_pnl auto_tab evt_device");
    xset_set_var(set, XSetVar::ICN, "gtk-execute");

    set = xset_set(XSetName::AUTO_INST, XSetVar::MENU_LABEL, "_Instance");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "evt_start evt_exit");

    set = xset_set(XSetName::EVT_START, XSetVar::MENU_LABEL, "_Startup");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Instance Startup Command");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or bash command line to be run automatically when a SpaceFM "
                 "instance starts:\n\nUse:\n\t%%e\tevent type  (evt_start)\n");

    set = xset_set(XSetName::EVT_EXIT, XSetVar::MENU_LABEL, "_Exit");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Instance Exit Command");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or bash command line to be run automatically when a SpaceFM "
                 "instance exits:\n\nUse:\n\t%%e\tevent type  (evt_exit)\n");

    set = xset_set(XSetName::AUTO_WIN, XSetVar::MENU_LABEL, "_Window");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "evt_win_new evt_win_focus evt_win_move evt_win_click evt_win_key evt_win_close");

    set = xset_set(XSetName::EVT_WIN_NEW, XSetVar::MENU_LABEL, "_New");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set New Window Command");
    xset_set_var(
        set,
        XSetVar::DESC,
        "Enter program or bash command line to be run automatically whenever a new SpaceFM "
        "window is opened:\n\nUse:\n\t%%e\tevent type  (evt_win_new)\n\t%%w\twindow id  (see "
        "spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg $fm_pwd, etc) "
        "can be used in this command.");

    set = xset_set(XSetName::EVT_WIN_FOCUS, XSetVar::MENU_LABEL, "_Focus");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Window Focus Command");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or bash command line to be run automatically whenever a SpaceFM "
                 "window gets focus:\n\nUse:\n\t%%e\tevent type  (evt_win_focus)\n\t%%w\twindow "
                 "id  (see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables "
                 "(eg $fm_pwd, etc) can be used in this command.");

    set = xset_set(XSetName::EVT_WIN_MOVE, XSetVar::MENU_LABEL, "_Move/Resize");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Window Move/Resize Command");
    xset_set_var(
        set,
        XSetVar::DESC,
        "Enter program or bash command line to be run automatically whenever a SpaceFM window is "
        "moved or resized:\n\nUse:\n\t%%e\tevent type  (evt_win_move)\n\t%%w\twindow id  (see "
        "spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg $fm_pwd, etc) "
        "can be used in this command.\n\nNote: This command may be run multiple times during "
        "resize.");

    set = xset_set(XSetName::EVT_WIN_CLICK, XSetVar::MENU_LABEL, "_Click");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Click Command");
    xset_set_var(
        set,
        XSetVar::DESC,
        "Enter program or bash command line to be run automatically whenever the mouse is "
        "clicked:\n\nUse:\n\t%%e\tevent type  (evt_win_click)\n\t%%w\twindow id  (see spacefm -s "
        "help)\n\t%%p\tpanel\n\t%%t\ttab\n\t%%b\tbutton  (mouse button pressed)\n\t%%m\tmodifier "
        " (modifier keys)\n\t%%f\tfocus  (element which received the click)\n\nExported bash "
        "variables (eg $fm_pwd, etc) can be used in this command when no asterisk prefix is "
        "used.\n\nPrefix your command with an asterisk (*) and conditionally return exit status "
        "0 to inhibit the default handler.  For example:\n*if [ \"%%b\" != \"2\" ];then exit 1; "
        "fi; spacefm -g --label \"\\nMiddle button was clicked in %%f\" --button ok &");

    set = xset_set(XSetName::EVT_WIN_KEY, XSetVar::MENU_LABEL, "_Keypress");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Window Keypress Command");
    xset_set_var(
        set,
        XSetVar::DESC,
        "Enter program or bash command line to be run automatically whenever a key is "
        "pressed:\n\nUse:\n\t%%e\tevent type  (evt_win_key)\n\t%%w\twindow id  (see spacefm -s "
        "help)\n\t%%p\tpanel\n\t%%t\ttab\n\t%%k\tkey code  (key pressed)\n\t%%m\tmodifier  "
        "(modifier keys)\n\nExported bash variables (eg $fm_pwd, etc) can be used in this "
        "command when no asterisk prefix is used.\n\nPrefix your command with an asterisk (*) "
        "and conditionally return exit status 0 to inhibit the default handler.  For "
        "example:\n*if [ \"%%k\" != \"0xffc5\" ];then exit 1; fi; spacefm -g --label \"\\nKey "
        "F8 was pressed.\" --button ok &");

    set = xset_set(XSetName::EVT_WIN_CLOSE, XSetVar::MENU_LABEL, "Cl_ose");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Window Close Command");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or bash command line to be run automatically whenever a SpaceFM "
                 "window is closed:\n\nUse:\n\t%%e\tevent type  (evt_win_close)\n\t%%w\twindow "
                 "id  (see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables "
                 "(eg $fm_pwd, etc) can be used in this command.");

    set = xset_set(XSetName::AUTO_PNL, XSetVar::MENU_LABEL, "_Panel");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "evt_pnl_focus evt_pnl_show evt_pnl_sel");

    set = xset_set(XSetName::EVT_PNL_FOCUS, XSetVar::MENU_LABEL, "_Focus");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Panel Focus Command");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or bash command line to be run automatically whenever a panel "
                 "gets focus:\n\nUse:\n\t%%e\tevent type  (evt_pnl_focus)\n\t%%w\twindow id  "
                 "(see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg "
                 "$fm_pwd, etc) can be used in this command.");

    set = xset_set(XSetName::EVT_PNL_SHOW, XSetVar::MENU_LABEL, "_Show");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Panel Show Command");
    xset_set_var(
        set,
        XSetVar::DESC,
        "Enter program or bash command line to be run automatically whenever a panel or panel "
        "element is shown or hidden:\n\nUse:\n\t%%e\tevent type  (evt_pnl_show)\n\t%%w\twindow "
        "id  (see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\t%%f\tfocus  (element shown or "
        "hidden)\n\t%%v\tvisible  (1 or 0)\n\nExported bash variables (eg $fm_pwd, etc) can be "
        "used in this command.");

    set = xset_set(XSetName::EVT_PNL_SEL, XSetVar::MENU_LABEL, "S_elect");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Panel Select Command");
    xset_set_var(
        set,
        XSetVar::DESC,
        "Enter program or bash command line to be run automatically whenever the file selection "
        "changes:\n\nUse:\n\t%%e\tevent type  (evt_pnl_sel)\n\t%%w\twindow id  (see spacefm -s "
        "help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg $fm_pwd, etc) can be "
        "used in this command.\n\nPrefix your command with an asterisk (*) and conditionally "
        "return exit status 0 to inhibit the default handler.");

    set = xset_set(XSetName::AUTO_TAB, XSetVar::MENU_LABEL, "_Tab");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "evt_tab_new evt_tab_chdir evt_tab_focus evt_tab_close");

    set = xset_set(XSetName::EVT_TAB_NEW, XSetVar::MENU_LABEL, "_New");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set New Tab Command");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or bash command line to be run automatically whenever a new tab "
                 "is opened:\n\nUse:\n\t%%e\tevent type  (evt_tab_new)\n\t%%w\twindow id  (see "
                 "spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg "
                 "$fm_pwd, etc) can be used in this command.");

    set = xset_set(XSetName::EVT_TAB_CHDIR, XSetVar::MENU_LABEL, "_Change Dir");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Tab Change Dir Command");
    xset_set_var(
        set,
        XSetVar::DESC,
        "Enter program or bash command line to be run automatically whenever a tab changes to a "
        "different directory:\n\nUse:\n\t%%e\tevent type  (evt_tab_chdir)\n\t%%w\twindow id  "
        "(see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\t%%d\tnew directory\n\nExported bash "
        "variables (eg $fm_pwd, etc) can be used in this command.");

    set = xset_set(XSetName::EVT_TAB_FOCUS, XSetVar::MENU_LABEL, "_Focus");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Tab Focus Command");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or bash command line to be run automatically whenever a tab gets "
                 "focus:\n\nUse:\n\t%%e\tevent type  (evt_tab_focus)\n\t%%w\twindow id  (see "
                 "spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg "
                 "$fm_pwd, etc) can be used in this command.");

    set = xset_set(XSetName::EVT_TAB_CLOSE, XSetVar::MENU_LABEL, "_Close");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Tab Close Command");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or bash command line to be run automatically whenever a tab is "
                 "closed:\n\nUse:\n\t%%e\tevent type  (evt_tab_close)\n\t%%w\twindow id  (see "
                 "spacefm -s help)\n\t%%p\tpanel\n\t%%t\tclosed tab");

    set = xset_set(XSetName::EVT_DEVICE, XSetVar::MENU_LABEL, "_Device");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Device Command");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or bash command line to be run automatically whenever a device "
                 "state changes:\n\nUse:\n\t%%e\tevent type  (evt_device)\n\t%%f\tdevice "
                 "file\n\t%%v\tchange  (added|removed|changed)\n");

    set = xset_set(XSetName::MAIN_TITLE, XSetVar::MENU_LABEL, "Wi_ndow Title");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Window Title Format");
    xset_set_var(
        set,
        XSetVar::DESC,
        "Set window title format:\n\nUse:\n\t%%n\tcurrent directory name (eg "
        "bin)\n\t%%d\tcurrent "
        "directory path (eg /usr/bin)\n\t%%p\tcurrent panel number (1-4)\n\t%%t\tcurrent tab "
        "number\n\t%%P\ttotal number of panels visible\n\t%%T\ttotal number of tabs in current "
        "panel\n\t*\tasterisk shown if tasks running in window");
    xset_set_var(set, XSetVar::S, "%d");
    xset_set_var(set, XSetVar::Z, "%d");

    set = xset_set(XSetName::MAIN_ICON, XSetVar::MENU_LABEL, "_Window Icon");
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

    set = xset_set(XSetName::MAIN_FULL, XSetVar::MENU_LABEL, "_Fullscreen");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::MAIN_DESIGN_MODE, XSetVar::MENU_LABEL, "_Design Mode");
    xset_set_var(set, XSetVar::ICN, "gtk-help");

    set = xset_set(XSetName::MAIN_PREFS, XSetVar::MENU_LABEL, "_Preferences");
    xset_set_var(set, XSetVar::ICN, "gtk-preferences");

    set = xset_set(XSetName::MAIN_TOOL, XSetVar::MENU_LABEL, "_Tool");
    set->menu_style = XSetMenu::SUBMENU;

    set = xset_get(XSetName::ROOT_BAR); // in Preferences
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::VIEW_THUMB,
                   XSetVar::MENU_LABEL,
                   "_Thumbnails (global)"); // in View|Panel View|Style
    set->menu_style = XSetMenu::CHECK;

    // Plugins
    set = xset_set(XSetName::PLUG_INSTALL, XSetVar::MENU_LABEL, "_Install");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "plug_ifile");
    xset_set_var(set, XSetVar::ICN, "gtk-add");

    set = xset_set(XSetName::PLUG_IFILE, XSetVar::MENU_LABEL, "_File");
    xset_set_var(set, XSetVar::ICN, "gtk-file");

    set = xset_set(XSetName::PLUG_COPY, XSetVar::MENU_LABEL, "_Import");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "plug_cfile separator plug_cverb");
    xset_set_var(set, XSetVar::ICN, "gtk-copy");

    set = xset_set(XSetName::PLUG_CFILE, XSetVar::MENU_LABEL, "_File");
    xset_set_var(set, XSetVar::ICN, "gtk-file");
    set = xset_set(XSetName::PLUG_CVERB, XSetVar::MENU_LABEL, "_Verbose");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    // Help
    set = xset_set(XSetName::MAIN_ABOUT, XSetVar::MENU_LABEL, "_About");
    xset_set_var(set, XSetVar::ICN, "gtk-about");

    set = xset_set(XSetName::MAIN_DEV, XSetVar::MENU_LABEL, "_Show Devices");
    xset_set_var(set, XSetVar::SHARED_KEY, "panel1_show_devmon");
    set->menu_style = XSetMenu::CHECK;

    // Tasks
    set = xset_set(XSetName::MAIN_TASKS, XSetVar::MENU_LABEL, "_Task Manager");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(
        set,
        XSetVar::DESC,
        "task_show_manager task_hide_manager separator task_columns task_popups task_errors "
        "task_queue");

    set = xset_set(XSetName::TASK_COL_STATUS, XSetVar::MENU_LABEL, "_Status");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("0");   // column position
    set->y = ztd::strdup("130"); // column width

    set = xset_set(XSetName::TASK_COL_COUNT, XSetVar::MENU_LABEL, "_Count");
    set->menu_style = XSetMenu::CHECK;
    set->x = ztd::strdup("1");

    set = xset_set(XSetName::TASK_COL_PATH, XSetVar::MENU_LABEL, "_Directory");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("2");

    set = xset_set(XSetName::TASK_COL_FILE, XSetVar::MENU_LABEL, "_Item");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("3");

    set = xset_set(XSetName::TASK_COL_TO, XSetVar::MENU_LABEL, "_To");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("4");

    set = xset_set(XSetName::TASK_COL_PROGRESS, XSetVar::MENU_LABEL, "_Progress");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("5");
    set->y = ztd::strdup("100");

    set = xset_set(XSetName::TASK_COL_TOTAL, XSetVar::MENU_LABEL, "T_otal");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("6");
    set->y = ztd::strdup("120");

    set = xset_set(XSetName::TASK_COL_STARTED, XSetVar::MENU_LABEL, "Sta_rted");
    set->menu_style = XSetMenu::CHECK;
    set->x = ztd::strdup("7");

    set = xset_set(XSetName::TASK_COL_ELAPSED, XSetVar::MENU_LABEL, "_Elapsed");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("8");
    set->y = ztd::strdup("70");

    set = xset_set(XSetName::TASK_COL_CURSPEED, XSetVar::MENU_LABEL, "C_urrent Speed");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("9");

    set = xset_set(XSetName::TASK_COL_CUREST, XSetVar::MENU_LABEL, "Current Re_main");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("10");

    set = xset_set(XSetName::TASK_COL_AVGSPEED, XSetVar::MENU_LABEL, "_Average Speed");
    set->menu_style = XSetMenu::CHECK;
    set->x = ztd::strdup("11");
    set->y = ztd::strdup("60");

    set = xset_set(XSetName::TASK_COL_AVGEST, XSetVar::MENU_LABEL, "A_verage Remain");
    set->menu_style = XSetMenu::CHECK;
    set->x = ztd::strdup("12");
    set->y = ztd::strdup("65");

    set = xset_set(XSetName::TASK_COL_REORDER, XSetVar::MENU_LABEL, "Reor_der");

    set = xset_set(XSetName::TASK_STOP, XSetVar::MENU_LABEL, "_Stop");
    xset_set_var(set, XSetVar::ICN, "gtk-stop");
    set = xset_set(XSetName::TASK_PAUSE, XSetVar::MENU_LABEL, "Pa_use");
    xset_set_var(set, XSetVar::ICN, "gtk-media-pause");
    set = xset_set(XSetName::TASK_QUE, XSetVar::MENU_LABEL, "_Queue");
    xset_set_var(set, XSetVar::ICN, "gtk-add");
    set = xset_set(XSetName::TASK_RESUME, XSetVar::MENU_LABEL, "_Resume");
    xset_set_var(set, XSetVar::ICN, "gtk-media-play");
    set = xset_set(XSetName::TASK_SHOWOUT, XSetVar::MENU_LABEL, "Sho_w Output");

    set = xset_set(XSetName::TASK_ALL, XSetVar::MENU_LABEL, "_All Tasks");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "task_stop_all task_pause_all task_que_all task_resume_all");

    set = xset_set(XSetName::TASK_STOP_ALL, XSetVar::MENU_LABEL, "_Stop");
    xset_set_var(set, XSetVar::ICN, "gtk-stop");
    set = xset_set(XSetName::TASK_PAUSE_ALL, XSetVar::MENU_LABEL, "Pa_use");
    xset_set_var(set, XSetVar::ICN, "gtk-media-pause");
    set = xset_set(XSetName::TASK_QUE_ALL, XSetVar::MENU_LABEL, "_Queue");
    xset_set_var(set, XSetVar::ICN, "gtk-add");
    set = xset_set(XSetName::TASK_RESUME_ALL, XSetVar::MENU_LABEL, "_Resume");
    xset_set_var(set, XSetVar::ICN, "gtk-media-play");

    set = xset_set(XSetName::TASK_SHOW_MANAGER, XSetVar::MENU_LABEL, "Show _Manager");
    set->menu_style = XSetMenu::RADIO;
    set->b = XSetB::XSET_B_FALSE;
    // set->x  used for task man height >=0.9.4

    set = xset_set(XSetName::TASK_HIDE_MANAGER, XSetVar::MENU_LABEL, "Auto-_Hide Manager");
    set->menu_style = XSetMenu::RADIO;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::TASK_COLUMNS, XSetVar::MENU_LABEL, "_Columns");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(
        set,
        XSetVar::DESC,
        "task_col_count task_col_path task_col_file task_col_to task_col_progress task_col_total "
        "task_col_started task_col_elapsed task_col_curspeed task_col_curest task_col_avgspeed "
        "task_col_avgest separator task_col_reorder");

    set = xset_set(XSetName::TASK_POPUPS, XSetVar::MENU_LABEL, "_Popups");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(
        set,
        XSetVar::DESC,
        "task_pop_all task_pop_top task_pop_above task_pop_stick separator task_pop_detail "
        "task_pop_over task_pop_err");

    set = xset_set(XSetName::TASK_POP_ALL, XSetVar::MENU_LABEL, "Popup _All Tasks");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_FALSE;

    set = xset_set(XSetName::TASK_POP_TOP, XSetVar::MENU_LABEL, "Stay On _Top");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_FALSE;

    set = xset_set(XSetName::TASK_POP_ABOVE, XSetVar::MENU_LABEL, "A_bove Others");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_FALSE;

    set = xset_set(XSetName::TASK_POP_STICK, XSetVar::MENU_LABEL, "All _Workspaces");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_FALSE;

    set = xset_set(XSetName::TASK_POP_DETAIL, XSetVar::MENU_LABEL, "_Detailed Stats");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_FALSE;

    set = xset_set(XSetName::TASK_POP_OVER, XSetVar::MENU_LABEL, "_Overwrite Option");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::TASK_POP_ERR, XSetVar::MENU_LABEL, "_Error Option");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::TASK_ERRORS, XSetVar::MENU_LABEL, "Err_ors");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "task_err_first task_err_any task_err_cont");

    set = xset_set(XSetName::TASK_ERR_FIRST, XSetVar::MENU_LABEL, "Stop If _First");
    set->menu_style = XSetMenu::RADIO;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::TASK_ERR_ANY, XSetVar::MENU_LABEL, "Stop On _Any");
    set->menu_style = XSetMenu::RADIO;
    set->b = XSetB::XSET_B_FALSE;

    set = xset_set(XSetName::TASK_ERR_CONT, XSetVar::MENU_LABEL, "_Continue");
    set->menu_style = XSetMenu::RADIO;
    set->b = XSetB::XSET_B_FALSE;

    set = xset_set(XSetName::TASK_QUEUE, XSetVar::MENU_LABEL, "Qu_eue");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "task_q_new task_q_smart task_q_pause");

    set = xset_set(XSetName::TASK_Q_NEW, XSetVar::MENU_LABEL, "_Queue New Tasks");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::TASK_Q_SMART, XSetVar::MENU_LABEL, "_Smart Queue");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::TASK_Q_PAUSE, XSetVar::MENU_LABEL, "_Pause On Error");
    set->menu_style = XSetMenu::CHECK;

    // PANELS COMMON
    xset_set(XSetName::DATE_FORMAT, XSetVar::S, "%Y-%m-%d %H:%M");

    set = xset_set(XSetName::CON_OPEN, XSetVar::MENU_LABEL, "_Open");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::ICN, "gtk-open");

    set = xset_set(XSetName::OPEN_EXECUTE, XSetVar::MENU_LABEL, "E_xecute");
    xset_set_var(set, XSetVar::ICN, "gtk-execute");

    set = xset_set(XSetName::OPEN_EDIT, XSetVar::MENU_LABEL, "Edi_t");
    xset_set_var(set, XSetVar::ICN, "gtk-edit");

    set = xset_set(XSetName::OPEN_EDIT_ROOT, XSetVar::MENU_LABEL, "Edit As _Root");
    xset_set_var(set, XSetVar::ICN, "gtk-dialog-warning");

    set = xset_set(XSetName::OPEN_OTHER, XSetVar::MENU_LABEL, "_Choose...");
    xset_set_var(set, XSetVar::ICN, "gtk-open");

    set = xset_set(XSetName::OPEN_HAND, XSetVar::MENU_LABEL, "File _Handlers...");
    xset_set_var(set, XSetVar::ICN, "gtk-preferences");

    set = xset_set(XSetName::OPEN_ALL, XSetVar::MENU_LABEL, "Open With _Default"); // virtual

    set = xset_set(XSetName::OPEN_IN_TAB, XSetVar::MENU_LABEL, "In _Tab");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "opentab_new opentab_prev opentab_next opentab_1 opentab_2 opentab_3 opentab_4 "
                 "opentab_5 opentab_6 opentab_7 opentab_8 opentab_9 opentab_10");

    xset_set(XSetName::OPENTAB_NEW, XSetVar::MENU_LABEL, "N_ew");
    xset_set(XSetName::OPENTAB_PREV, XSetVar::MENU_LABEL, "_Prev");
    xset_set(XSetName::OPENTAB_NEXT, XSetVar::MENU_LABEL, "_Next");
    xset_set(XSetName::OPENTAB_1, XSetVar::MENU_LABEL, "Tab _1");
    xset_set(XSetName::OPENTAB_2, XSetVar::MENU_LABEL, "Tab _2");
    xset_set(XSetName::OPENTAB_3, XSetVar::MENU_LABEL, "Tab _3");
    xset_set(XSetName::OPENTAB_4, XSetVar::MENU_LABEL, "Tab _4");
    xset_set(XSetName::OPENTAB_5, XSetVar::MENU_LABEL, "Tab _5");
    xset_set(XSetName::OPENTAB_6, XSetVar::MENU_LABEL, "Tab _6");
    xset_set(XSetName::OPENTAB_7, XSetVar::MENU_LABEL, "Tab _7");
    xset_set(XSetName::OPENTAB_8, XSetVar::MENU_LABEL, "Tab _8");
    xset_set(XSetName::OPENTAB_9, XSetVar::MENU_LABEL, "Tab _9");
    xset_set(XSetName::OPENTAB_10, XSetVar::MENU_LABEL, "Tab 1_0");

    set = xset_set(XSetName::OPEN_IN_PANEL, XSetVar::MENU_LABEL, "In _Panel");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "open_in_panelprev open_in_panelnext open_in_panel1 open_in_panel2 open_in_panel3 "
                 "open_in_panel4");

    xset_set(XSetName::OPEN_IN_PANELPREV, XSetVar::MENU_LABEL, "_Prev");
    xset_set(XSetName::OPEN_IN_PANELNEXT, XSetVar::MENU_LABEL, "_Next");
    xset_set(XSetName::OPEN_IN_PANEL1, XSetVar::MENU_LABEL, "Panel _1");
    xset_set(XSetName::OPEN_IN_PANEL2, XSetVar::MENU_LABEL, "Panel _2");
    xset_set(XSetName::OPEN_IN_PANEL3, XSetVar::MENU_LABEL, "Panel _3");
    xset_set(XSetName::OPEN_IN_PANEL4, XSetVar::MENU_LABEL, "Panel _4");

    set = xset_set(XSetName::ARC_EXTRACT, XSetVar::MENU_LABEL, "_Extract");
    xset_set_var(set, XSetVar::ICN, "gtk-convert");

    set = xset_set(XSetName::ARC_EXTRACTTO, XSetVar::MENU_LABEL, "Extract _To");
    xset_set_var(set, XSetVar::ICN, "gtk-convert");

    set = xset_set(XSetName::ARC_LIST, XSetVar::MENU_LABEL, "_List Contents");
    xset_set_var(set, XSetVar::ICN, "gtk-file");

    set = xset_set(XSetName::ARC_DEFAULT, XSetVar::MENU_LABEL, "_Archive Defaults");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "arc_conf2 separator arc_def_open arc_def_ex arc_def_exto arc_def_list separator "
                 "arc_def_parent arc_def_write");

    set = xset_set(XSetName::ARC_DEF_OPEN, XSetVar::MENU_LABEL, "_Open With App");
    set->menu_style = XSetMenu::RADIO;

    set = xset_set(XSetName::ARC_DEF_EX, XSetVar::MENU_LABEL, "_Extract");
    set->menu_style = XSetMenu::RADIO;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::ARC_DEF_EXTO, XSetVar::MENU_LABEL, "Extract _To");
    set->menu_style = XSetMenu::RADIO;

    set = xset_set(XSetName::ARC_DEF_LIST, XSetVar::MENU_LABEL, "_List Contents");
    set->menu_style = XSetMenu::RADIO;

    set = xset_set(XSetName::ARC_DEF_PARENT, XSetVar::MENU_LABEL, "_Create Subdirectory");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::ARC_DEF_WRITE, XSetVar::MENU_LABEL, "_Write Access");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::ARC_CONF2, XSetVar::MENU_LABEL, "Archive _Handlers");
    xset_set_var(set, XSetVar::ICON, "gtk-preferences");

    set = xset_set(XSetName::OPEN_NEW, XSetVar::MENU_LABEL, "_New");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(
        set,
        XSetVar::DESC,
        "new_file new_directory new_link new_archive separator tab_new tab_new_here new_bookmark");
    xset_set_var(set, XSetVar::ICN, "gtk-new");

    set = xset_set(XSetName::NEW_FILE, XSetVar::MENU_LABEL, "_File");
    xset_set_var(set, XSetVar::ICN, "gtk-file");

    set = xset_set(XSetName::NEW_DIRECTORY, XSetVar::MENU_LABEL, "Dir_ectory");
    xset_set_var(set, XSetVar::ICN, "gtk-directory");

    set = xset_set(XSetName::NEW_LINK, XSetVar::MENU_LABEL, "_Link");
    xset_set_var(set, XSetVar::ICN, "gtk-file");

    set = xset_set(XSetName::NEW_BOOKMARK, XSetVar::MENU_LABEL, "_Bookmark");
    xset_set_var(set, XSetVar::SHARED_KEY, "book_add");
    xset_set_var(set, XSetVar::ICN, "gtk-jump-to");

    set = xset_set(XSetName::NEW_ARCHIVE, XSetVar::MENU_LABEL, "_Archive");
    xset_set_var(set, XSetVar::ICN, "gtk-save-as");

    set = xset_get(XSetName::ARC_DLG);
    set->b = XSetB::XSET_B_TRUE; // Extract To - Create Subdirectory
    set->z = ztd::strdup("1");   // Extract To - Write Access

    set = xset_set(XSetName::TAB_NEW, XSetVar::MENU_LABEL, "_Tab");
    xset_set_var(set, XSetVar::ICN, "gtk-add");
    set = xset_set(XSetName::TAB_NEW_HERE, XSetVar::MENU_LABEL, "Tab _Here");
    xset_set_var(set, XSetVar::ICN, "gtk-add");

    set = xset_set(XSetName::NEW_APP, XSetVar::MENU_LABEL, "_Desktop Application");
    xset_set_var(set, XSetVar::ICN, "gtk-add");

    set = xset_set(XSetName::CON_GO, XSetVar::MENU_LABEL, "_Go");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "go_back go_forward go_up go_home go_default go_set_default edit_canon separator "
                 "go_tab go_focus");
    xset_set_var(set, XSetVar::ICN, "gtk-go-forward");

    set = xset_set(XSetName::GO_BACK, XSetVar::MENU_LABEL, "_Back");
    xset_set_var(set, XSetVar::ICN, "gtk-go-back");
    set = xset_set(XSetName::GO_FORWARD, XSetVar::MENU_LABEL, "_Forward");
    xset_set_var(set, XSetVar::ICN, "gtk-go-forward");
    set = xset_set(XSetName::GO_UP, XSetVar::MENU_LABEL, "_Up");
    xset_set_var(set, XSetVar::ICN, "gtk-go-up");
    set = xset_set(XSetName::GO_HOME, XSetVar::MENU_LABEL, "_Home");
    xset_set_var(set, XSetVar::ICN, "gtk-home");
    set = xset_set(XSetName::GO_DEFAULT, XSetVar::MENU_LABEL, "_Default");
    xset_set_var(set, XSetVar::ICN, "gtk-home");

    set = xset_set(XSetName::GO_SET_DEFAULT, XSetVar::MENU_LABEL, "_Set Default");
    xset_set_var(set, XSetVar::ICN, "gtk-save");

    set = xset_set(XSetName::EDIT_CANON, XSetVar::MENU_LABEL, "Re_al Path");

    set = xset_set(XSetName::GO_FOCUS, XSetVar::MENU_LABEL, "Fo_cus");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "focus_path_bar focus_filelist focus_dirtree focus_book focus_device");

    set = xset_set(XSetName::FOCUS_PATH_BAR, XSetVar::MENU_LABEL, "_Path Bar");
    xset_set_var(set, XSetVar::ICN, "gtk-dialog-question");
    set = xset_set(XSetName::FOCUS_FILELIST, XSetVar::MENU_LABEL, "_File List");
    xset_set_var(set, XSetVar::ICN, "gtk-file");
    set = xset_set(XSetName::FOCUS_DIRTREE, XSetVar::MENU_LABEL, "_Tree");
    xset_set_var(set, XSetVar::ICN, "gtk-directory");
    set = xset_set(XSetName::FOCUS_BOOK, XSetVar::MENU_LABEL, "_Bookmarks");
    xset_set_var(set, XSetVar::ICN, "gtk-jump-to");
    set = xset_set(XSetName::FOCUS_DEVICE, XSetVar::MENU_LABEL, "De_vices");
    xset_set_var(set, XSetVar::ICN, "gtk-harddisk");

    set = xset_set(XSetName::GO_TAB, XSetVar::MENU_LABEL, "_Tab");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "tab_prev tab_next tab_restore tab_close tab_1 tab_2 tab_3 tab_4 tab_5 tab_6 "
                 "tab_7 tab_8 tab_9 tab_10");

    xset_set(XSetName::TAB_PREV, XSetVar::MENU_LABEL, "_Prev");
    xset_set(XSetName::TAB_NEXT, XSetVar::MENU_LABEL, "_Next");
    xset_set(XSetName::TAB_RESTORE, XSetVar::MENU_LABEL, "_Restore");
    xset_set(XSetName::TAB_CLOSE, XSetVar::MENU_LABEL, "_Close");
    xset_set(XSetName::TAB_1, XSetVar::MENU_LABEL, "Tab _1");
    xset_set(XSetName::TAB_2, XSetVar::MENU_LABEL, "Tab _2");
    xset_set(XSetName::TAB_3, XSetVar::MENU_LABEL, "Tab _3");
    xset_set(XSetName::TAB_4, XSetVar::MENU_LABEL, "Tab _4");
    xset_set(XSetName::TAB_5, XSetVar::MENU_LABEL, "Tab _5");
    xset_set(XSetName::TAB_6, XSetVar::MENU_LABEL, "Tab _6");
    xset_set(XSetName::TAB_7, XSetVar::MENU_LABEL, "Tab _7");
    xset_set(XSetName::TAB_8, XSetVar::MENU_LABEL, "Tab _8");
    xset_set(XSetName::TAB_9, XSetVar::MENU_LABEL, "Tab _9");
    xset_set(XSetName::TAB_10, XSetVar::MENU_LABEL, "Tab 1_0");

    set = xset_set(XSetName::CON_VIEW, XSetVar::MENU_LABEL, "_View");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::ICN, "gtk-preferences");

    set = xset_set(XSetName::VIEW_LIST_STYLE, XSetVar::MENU_LABEL, "Styl_e");
    set->menu_style = XSetMenu::SUBMENU;

    set = xset_set(XSetName::VIEW_COLUMNS, XSetVar::MENU_LABEL, "C_olumns");
    set->menu_style = XSetMenu::SUBMENU;

    set = xset_set(XSetName::VIEW_REORDER_COL, XSetVar::MENU_LABEL, "_Reorder");

    set = xset_set(XSetName::RUBBERBAND, XSetVar::MENU_LABEL, "_Rubberband Select");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::VIEW_SORTBY, XSetVar::MENU_LABEL, "_Sort");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(
        set,
        XSetVar::DESC,
        "sortby_name sortby_size sortby_type sortby_perm sortby_owner sortby_date separator "
        "sortby_ascend sortby_descend separator sortx_alphanum sortx_case separator " // sortx_natural
        "sortx_directories sortx_files sortx_mix separator sortx_hidfirst sortx_hidlast");

    set = xset_set(XSetName::SORTBY_NAME, XSetVar::MENU_LABEL, "_Name");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set(XSetName::SORTBY_SIZE, XSetVar::MENU_LABEL, "_Size");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set(XSetName::SORTBY_TYPE, XSetVar::MENU_LABEL, "_Type");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set(XSetName::SORTBY_PERM, XSetVar::MENU_LABEL, "_Permission");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set(XSetName::SORTBY_OWNER, XSetVar::MENU_LABEL, "_Owner");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set(XSetName::SORTBY_DATE, XSetVar::MENU_LABEL, "_Modified");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set(XSetName::SORTBY_ASCEND, XSetVar::MENU_LABEL, "_Ascending");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set(XSetName::SORTBY_DESCEND, XSetVar::MENU_LABEL, "_Descending");
    set->menu_style = XSetMenu::RADIO;

    set = xset_set(XSetName::SORTX_ALPHANUM, XSetVar::MENU_LABEL, "Alphanumeric");
    set->menu_style = XSetMenu::CHECK;
#if 0
    set = xset_set(XSetName::SORTX_NATURAL, XSetVar::MENU_LABEL, "Nat_ural");
    set->menu_style = XSetMenu::CHECK;
#endif
    set = xset_set(XSetName::SORTX_CASE, XSetVar::MENU_LABEL, "_Case Sensitive");
    set->menu_style = XSetMenu::CHECK;
    set = xset_set(XSetName::SORTX_DIRECTORIES, XSetVar::MENU_LABEL, "Directories Fi_rst");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set(XSetName::SORTX_FILES, XSetVar::MENU_LABEL, "F_iles First");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set(XSetName::SORTX_MIX, XSetVar::MENU_LABEL, "Mi_xed");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set(XSetName::SORTX_HIDFIRST, XSetVar::MENU_LABEL, "_Hidden First");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set(XSetName::SORTX_HIDLAST, XSetVar::MENU_LABEL, "Hidden _Last");
    set->menu_style = XSetMenu::RADIO;

    set = xset_set(XSetName::VIEW_REFRESH, XSetVar::MENU_LABEL, "Re_fresh");
    xset_set_var(set, XSetVar::ICN, "gtk-refresh");

    set = xset_set(XSetName::PATH_SEEK, XSetVar::MENU_LABEL, "Auto See_k");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::PATH_HAND, XSetVar::MENU_LABEL, "_Protocol Handlers");
    xset_set_var(set, XSetVar::ICN, "gtk-preferences");
    xset_set_var(set, XSetVar::SHARED_KEY, "dev_net_cnf");
    // set->s was custom protocol handler in sfm<=0.9.3 - retained

    set = xset_set(XSetName::PATH_HELP, XSetVar::MENU_LABEL, "Path Bar _Help");
    xset_set_var(set, XSetVar::ICN, "gtk-help");

    // EDIT
    set = xset_set(XSetName::EDIT_CUT, XSetVar::MENU_LABEL, "Cu_t");
    xset_set_var(set, XSetVar::ICN, "gtk-cut");

    set = xset_set(XSetName::EDIT_COPY, XSetVar::MENU_LABEL, "_Copy");
    xset_set_var(set, XSetVar::ICN, "gtk-copy");

    set = xset_set(XSetName::EDIT_PASTE, XSetVar::MENU_LABEL, "_Paste");
    xset_set_var(set, XSetVar::ICN, "gtk-paste");

    set = xset_set(XSetName::EDIT_RENAME, XSetVar::MENU_LABEL, "_Rename");
    xset_set_var(set, XSetVar::ICN, "gtk-edit");

    set = xset_set(XSetName::EDIT_DELETE, XSetVar::MENU_LABEL, "_Delete");
    xset_set_var(set, XSetVar::ICN, "gtk-delete");

    set = xset_set(XSetName::EDIT_TRASH, XSetVar::MENU_LABEL, "_Trash");
    xset_set_var(set, XSetVar::ICN, "gtk-delete");

    set = xset_set(XSetName::EDIT_SUBMENU, XSetVar::MENU_LABEL, "_Actions");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(
        set,
        XSetVar::DESC,
        "copy_name copy_parent copy_path separator paste_link paste_target paste_as separator "
        "copy_to "
        "move_to edit_root edit_hide separator select_all select_patt select_invert select_un");
    xset_set_var(set, XSetVar::ICN, "gtk-edit");

    set = xset_set(XSetName::COPY_NAME, XSetVar::MENU_LABEL, "Copy _Name");
    xset_set_var(set, XSetVar::ICN, "gtk-copy");

    set = xset_set(XSetName::COPY_PATH, XSetVar::MENU_LABEL, "Copy _Path");
    xset_set_var(set, XSetVar::ICN, "gtk-copy");

    set = xset_set(XSetName::COPY_PARENT, XSetVar::MENU_LABEL, "Copy Pa_rent");
    xset_set_var(set, XSetVar::ICN, "gtk-copy");

    set = xset_set(XSetName::PASTE_LINK, XSetVar::MENU_LABEL, "Paste _Link");
    xset_set_var(set, XSetVar::ICN, "gtk-paste");

    set = xset_set(XSetName::PASTE_TARGET, XSetVar::MENU_LABEL, "Paste _Target");
    xset_set_var(set, XSetVar::ICN, "gtk-paste");

    set = xset_set(XSetName::PASTE_AS, XSetVar::MENU_LABEL, "Paste _As");
    xset_set_var(set, XSetVar::ICN, "gtk-paste");

    set = xset_set(XSetName::COPY_TO, XSetVar::MENU_LABEL, "_Copy To");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "copy_loc copy_loc_last separator copy_tab copy_panel");

    set = xset_set(XSetName::COPY_LOC, XSetVar::MENU_LABEL, "L_ocation");
    set = xset_set(XSetName::COPY_LOC_LAST, XSetVar::MENU_LABEL, "L_ast Location");
    xset_set_var(set, XSetVar::ICN, "gtk-redo");

    set = xset_set(XSetName::COPY_TAB, XSetVar::MENU_LABEL, "_Tab");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "copy_tab_prev copy_tab_next copy_tab_1 copy_tab_2 copy_tab_3 copy_tab_4 "
                 "copy_tab_5 copy_tab_6 copy_tab_7 copy_tab_8 copy_tab_9 copy_tab_10");

    xset_set(XSetName::COPY_TAB_PREV, XSetVar::MENU_LABEL, "_Prev");
    xset_set(XSetName::COPY_TAB_NEXT, XSetVar::MENU_LABEL, "_Next");
    xset_set(XSetName::COPY_TAB_1, XSetVar::MENU_LABEL, "Tab _1");
    xset_set(XSetName::COPY_TAB_2, XSetVar::MENU_LABEL, "Tab _2");
    xset_set(XSetName::COPY_TAB_3, XSetVar::MENU_LABEL, "Tab _3");
    xset_set(XSetName::COPY_TAB_4, XSetVar::MENU_LABEL, "Tab _4");
    xset_set(XSetName::COPY_TAB_5, XSetVar::MENU_LABEL, "Tab _5");
    xset_set(XSetName::COPY_TAB_6, XSetVar::MENU_LABEL, "Tab _6");
    xset_set(XSetName::COPY_TAB_7, XSetVar::MENU_LABEL, "Tab _7");
    xset_set(XSetName::COPY_TAB_8, XSetVar::MENU_LABEL, "Tab _8");
    xset_set(XSetName::COPY_TAB_9, XSetVar::MENU_LABEL, "Tab _9");
    xset_set(XSetName::COPY_TAB_10, XSetVar::MENU_LABEL, "Tab 1_0");

    set = xset_set(XSetName::COPY_PANEL, XSetVar::MENU_LABEL, "_Panel");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(
        set,
        XSetVar::DESC,
        "copy_panel_prev copy_panel_next copy_panel_1 copy_panel_2 copy_panel_3 copy_panel_4");

    xset_set(XSetName::COPY_PANEL_PREV, XSetVar::MENU_LABEL, "_Prev");
    xset_set(XSetName::COPY_PANEL_NEXT, XSetVar::MENU_LABEL, "_Next");
    xset_set(XSetName::COPY_PANEL_1, XSetVar::MENU_LABEL, "Panel _1");
    xset_set(XSetName::COPY_PANEL_2, XSetVar::MENU_LABEL, "Panel _2");
    xset_set(XSetName::COPY_PANEL_3, XSetVar::MENU_LABEL, "Panel _3");
    xset_set(XSetName::COPY_PANEL_4, XSetVar::MENU_LABEL, "Panel _4");

    set = xset_set(XSetName::MOVE_TO, XSetVar::MENU_LABEL, "_Move To");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "move_loc move_loc_last separator move_tab move_panel");

    set = xset_set(XSetName::MOVE_LOC, XSetVar::MENU_LABEL, "_Location");
    set = xset_set(XSetName::MOVE_LOC_LAST, XSetVar::MENU_LABEL, "L_ast Location");
    xset_set_var(set, XSetVar::ICN, "gtk-redo");
    set = xset_set(XSetName::MOVE_TAB, XSetVar::MENU_LABEL, "_Tab");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "move_tab_prev move_tab_next move_tab_1 move_tab_2 move_tab_3 move_tab_4 "
                 "move_tab_5 move_tab_6 move_tab_7 move_tab_8 move_tab_9 move_tab_10");

    xset_set(XSetName::MOVE_TAB_PREV, XSetVar::MENU_LABEL, "_Prev");
    xset_set(XSetName::MOVE_TAB_NEXT, XSetVar::MENU_LABEL, "_Next");
    xset_set(XSetName::MOVE_TAB_1, XSetVar::MENU_LABEL, "Tab _1");
    xset_set(XSetName::MOVE_TAB_2, XSetVar::MENU_LABEL, "Tab _2");
    xset_set(XSetName::MOVE_TAB_3, XSetVar::MENU_LABEL, "Tab _3");
    xset_set(XSetName::MOVE_TAB_4, XSetVar::MENU_LABEL, "Tab _4");
    xset_set(XSetName::MOVE_TAB_5, XSetVar::MENU_LABEL, "Tab _5");
    xset_set(XSetName::MOVE_TAB_6, XSetVar::MENU_LABEL, "Tab _6");
    xset_set(XSetName::MOVE_TAB_7, XSetVar::MENU_LABEL, "Tab _7");
    xset_set(XSetName::MOVE_TAB_8, XSetVar::MENU_LABEL, "Tab _8");
    xset_set(XSetName::MOVE_TAB_9, XSetVar::MENU_LABEL, "Tab _9");
    xset_set(XSetName::MOVE_TAB_10, XSetVar::MENU_LABEL, "Tab 1_0");

    set = xset_set(XSetName::MOVE_PANEL, XSetVar::MENU_LABEL, "_Panel");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(
        set,
        XSetVar::DESC,
        "move_panel_prev move_panel_next move_panel_1 move_panel_2 move_panel_3 move_panel_4");

    xset_set(XSetName::MOVE_PANEL_PREV, XSetVar::MENU_LABEL, "_Prev");
    xset_set(XSetName::MOVE_PANEL_NEXT, XSetVar::MENU_LABEL, "_Next");
    xset_set(XSetName::MOVE_PANEL_1, XSetVar::MENU_LABEL, "Panel _1");
    xset_set(XSetName::MOVE_PANEL_2, XSetVar::MENU_LABEL, "Panel _2");
    xset_set(XSetName::MOVE_PANEL_3, XSetVar::MENU_LABEL, "Panel _3");
    xset_set(XSetName::MOVE_PANEL_4, XSetVar::MENU_LABEL, "Panel _4");

    set = xset_set(XSetName::EDIT_HIDE, XSetVar::MENU_LABEL, "_Hide");

    set = xset_set(XSetName::SELECT_ALL, XSetVar::MENU_LABEL, "_Select All");
    xset_set_var(set, XSetVar::ICN, "gtk-select-all");

    set = xset_set(XSetName::SELECT_UN, XSetVar::MENU_LABEL, "_Unselect All");

    set = xset_set(XSetName::SELECT_INVERT, XSetVar::MENU_LABEL, "_Invert Selection");

    set = xset_set(XSetName::SELECT_PATT, XSetVar::MENU_LABEL, "S_elect By Pattern");

    set = xset_set(XSetName::EDIT_ROOT, XSetVar::MENU_LABEL, "R_oot");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "root_copy_loc root_move2 root_delete");
    xset_set_var(set, XSetVar::ICN, "gtk-dialog-warning");

    set = xset_set(XSetName::ROOT_COPY_LOC, XSetVar::MENU_LABEL, "_Copy To");
    set = xset_set(XSetName::ROOT_MOVE2, XSetVar::MENU_LABEL, "Move _To");
    set = xset_set(XSetName::ROOT_DELETE, XSetVar::MENU_LABEL, "_Delete");
    xset_set_var(set, XSetVar::ICN, "gtk-delete");

    // Properties
    set = xset_set(XSetName::CON_PROP, XSetVar::MENU_LABEL, "Propert_ies");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "");
    xset_set_var(set, XSetVar::ICN, "gtk-properties");

    set = xset_set(XSetName::PROP_INFO, XSetVar::MENU_LABEL, "_Info");
    xset_set_var(set, XSetVar::ICN, "gtk-dialog-info");

    set = xset_set(XSetName::PROP_PERM, XSetVar::MENU_LABEL, "_Permissions");
    xset_set_var(set, XSetVar::ICN, "dialog-password");

    set = xset_set(XSetName::PROP_QUICK, XSetVar::MENU_LABEL, "_Quick");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "perm_r perm_rw perm_rwx perm_r_r perm_rw_r perm_rw_rw perm_rwxr_x perm_rwxrwx "
                 "perm_r_r_r perm_rw_r_r perm_rw_rw_rw perm_rwxr_r perm_rwxr_xr_x perm_rwxrwxrwx "
                 "perm_rwxrwxrwt perm_unstick perm_stick perm_recurs");

    xset_set(XSetName::PERM_R, XSetVar::MENU_LABEL, "r--------");
    xset_set(XSetName::PERM_RW, XSetVar::MENU_LABEL, "rw-------");
    xset_set(XSetName::PERM_RWX, XSetVar::MENU_LABEL, "rwx------");
    xset_set(XSetName::PERM_R_R, XSetVar::MENU_LABEL, "r--r-----");
    xset_set(XSetName::PERM_RW_R, XSetVar::MENU_LABEL, "rw-r-----");
    xset_set(XSetName::PERM_RW_RW, XSetVar::MENU_LABEL, "rw-rw----");
    xset_set(XSetName::PERM_RWXR_X, XSetVar::MENU_LABEL, "rwxr-x---");
    xset_set(XSetName::PERM_RWXRWX, XSetVar::MENU_LABEL, "rwxrwx---");
    xset_set(XSetName::PERM_R_R_R, XSetVar::MENU_LABEL, "r--r--r--");
    xset_set(XSetName::PERM_RW_R_R, XSetVar::MENU_LABEL, "rw-r--r--");
    xset_set(XSetName::PERM_RW_RW_RW, XSetVar::MENU_LABEL, "rw-rw-rw-");
    xset_set(XSetName::PERM_RWXR_R, XSetVar::MENU_LABEL, "rwxr--r--");
    xset_set(XSetName::PERM_RWXR_XR_X, XSetVar::MENU_LABEL, "rwxr-xr-x");
    xset_set(XSetName::PERM_RWXRWXRWX, XSetVar::MENU_LABEL, "rwxrwxrwx");
    xset_set(XSetName::PERM_RWXRWXRWT, XSetVar::MENU_LABEL, "rwxrwxrwt");
    xset_set(XSetName::PERM_UNSTICK, XSetVar::MENU_LABEL, "-t");
    xset_set(XSetName::PERM_STICK, XSetVar::MENU_LABEL, "+t");

    set = xset_set(XSetName::PERM_RECURS, XSetVar::MENU_LABEL, "_Recursive");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "perm_go_w perm_go_rwx perm_ugo_w perm_ugo_rx perm_ugo_rwx");

    xset_set(XSetName::PERM_GO_W, XSetVar::MENU_LABEL, "go-w");
    xset_set(XSetName::PERM_GO_RWX, XSetVar::MENU_LABEL, "go-rwx");
    xset_set(XSetName::PERM_UGO_W, XSetVar::MENU_LABEL, "ugo+w");
    xset_set(XSetName::PERM_UGO_RX, XSetVar::MENU_LABEL, "ugo+rX");
    xset_set(XSetName::PERM_UGO_RWX, XSetVar::MENU_LABEL, "ugo+rwX");

    set = xset_set(XSetName::PROP_ROOT, XSetVar::MENU_LABEL, "_Root");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "rperm_rw rperm_rwx rperm_rw_r rperm_rw_rw rperm_rwxr_x rperm_rwxrwx rperm_rw_r_r "
                 "rperm_rw_rw_rw rperm_rwxr_r rperm_rwxr_xr_x rperm_rwxrwxrwx rperm_rwxrwxrwt "
                 "rperm_unstick rperm_stick rperm_recurs rperm_own");
    xset_set_var(set, XSetVar::ICN, "gtk-dialog-warning");

    xset_set(XSetName::RPERM_RW, XSetVar::MENU_LABEL, "rw-------");
    xset_set(XSetName::RPERM_RWX, XSetVar::MENU_LABEL, "rwx------");
    xset_set(XSetName::RPERM_RW_R, XSetVar::MENU_LABEL, "rw-r-----");
    xset_set(XSetName::RPERM_RW_RW, XSetVar::MENU_LABEL, "rw-rw----");
    xset_set(XSetName::RPERM_RWXR_X, XSetVar::MENU_LABEL, "rwxr-x---");
    xset_set(XSetName::RPERM_RWXRWX, XSetVar::MENU_LABEL, "rwxrwx---");
    xset_set(XSetName::RPERM_RW_R_R, XSetVar::MENU_LABEL, "rw-r--r--");
    xset_set(XSetName::RPERM_RW_RW_RW, XSetVar::MENU_LABEL, "rw-rw-rw-");
    xset_set(XSetName::RPERM_RWXR_R, XSetVar::MENU_LABEL, "rwxr--r--");
    xset_set(XSetName::RPERM_RWXR_XR_X, XSetVar::MENU_LABEL, "rwxr-xr-x");
    xset_set(XSetName::RPERM_RWXRWXRWX, XSetVar::MENU_LABEL, "rwxrwxrwx");
    xset_set(XSetName::RPERM_RWXRWXRWT, XSetVar::MENU_LABEL, "rwxrwxrwt");
    xset_set(XSetName::RPERM_UNSTICK, XSetVar::MENU_LABEL, "-t");
    xset_set(XSetName::RPERM_STICK, XSetVar::MENU_LABEL, "+t");

    set = xset_set(XSetName::RPERM_RECURS, XSetVar::MENU_LABEL, "_Recursive");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "rperm_go_w rperm_go_rwx rperm_ugo_w rperm_ugo_rx rperm_ugo_rwx");

    xset_set(XSetName::RPERM_GO_W, XSetVar::MENU_LABEL, "go-w");
    xset_set(XSetName::RPERM_GO_RWX, XSetVar::MENU_LABEL, "go-rwx");
    xset_set(XSetName::RPERM_UGO_W, XSetVar::MENU_LABEL, "ugo+w");
    xset_set(XSetName::RPERM_UGO_RX, XSetVar::MENU_LABEL, "ugo+rX");
    xset_set(XSetName::RPERM_UGO_RWX, XSetVar::MENU_LABEL, "ugo+rwX");

    set = xset_set(XSetName::RPERM_OWN, XSetVar::MENU_LABEL, "_Owner");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(
        set,
        XSetVar::DESC,
        "own_myuser own_myuser_users own_user1 own_user1_users own_user2 own_user2_users own_root "
        "own_root_users own_root_myuser own_root_user1 own_root_user2 own_recurs");

    xset_set(XSetName::OWN_MYUSER, XSetVar::MENU_LABEL, "myuser");
    xset_set(XSetName::OWN_MYUSER_USERS, XSetVar::MENU_LABEL, "myuser:users");
    xset_set(XSetName::OWN_USER1, XSetVar::MENU_LABEL, "user1");
    xset_set(XSetName::OWN_USER1_USERS, XSetVar::MENU_LABEL, "user1:users");
    xset_set(XSetName::OWN_USER2, XSetVar::MENU_LABEL, "user2");
    xset_set(XSetName::OWN_USER2_USERS, XSetVar::MENU_LABEL, "user2:users");
    xset_set(XSetName::OWN_ROOT, XSetVar::MENU_LABEL, "root");
    xset_set(XSetName::OWN_ROOT_USERS, XSetVar::MENU_LABEL, "root:users");
    xset_set(XSetName::OWN_ROOT_MYUSER, XSetVar::MENU_LABEL, "root:myuser");
    xset_set(XSetName::OWN_ROOT_USER1, XSetVar::MENU_LABEL, "root:user1");
    xset_set(XSetName::OWN_ROOT_USER2, XSetVar::MENU_LABEL, "root:user2");

    set = xset_set(XSetName::OWN_RECURS, XSetVar::MENU_LABEL, "_Recursive");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(
        set,
        XSetVar::DESC,
        "rown_myuser rown_myuser_users rown_user1 rown_user1_users rown_user2 rown_user2_users "
        "rown_root rown_root_users rown_root_myuser rown_root_user1 rown_root_user2");

    xset_set(XSetName::ROWN_MYUSER, XSetVar::MENU_LABEL, "myuser");
    xset_set(XSetName::ROWN_MYUSER_USERS, XSetVar::MENU_LABEL, "myuser:users");
    xset_set(XSetName::ROWN_USER1, XSetVar::MENU_LABEL, "user1");
    xset_set(XSetName::ROWN_USER1_USERS, XSetVar::MENU_LABEL, "user1:users");
    xset_set(XSetName::ROWN_USER2, XSetVar::MENU_LABEL, "user2");
    xset_set(XSetName::ROWN_USER2_USERS, XSetVar::MENU_LABEL, "user2:users");
    xset_set(XSetName::ROWN_ROOT, XSetVar::MENU_LABEL, "root");
    xset_set(XSetName::ROWN_ROOT_USERS, XSetVar::MENU_LABEL, "root:users");
    xset_set(XSetName::ROWN_ROOT_MYUSER, XSetVar::MENU_LABEL, "root:myuser");
    xset_set(XSetName::ROWN_ROOT_USER1, XSetVar::MENU_LABEL, "root:user1");
    xset_set(XSetName::ROWN_ROOT_USER2, XSetVar::MENU_LABEL, "root:user2");

    // PANELS
    for (panel_t p: PANELS)
    {
        set = xset_set_panel(p, XSetPanel::SHOW_TOOLBOX, XSetVar::MENU_LABEL, "_Toolbar");
        set->menu_style = XSetMenu::CHECK;
        set->b = XSetB::XSET_B_TRUE;
        if (p != 1)
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_show_toolbox");

        set = xset_set_panel(p, XSetPanel::SHOW_DEVMON, XSetVar::MENU_LABEL, "_Devices");
        set->menu_style = XSetMenu::CHECK;
        set->b = XSetB::XSET_B_UNSET;
        if (p != 1)
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_show_devmon");

        set = xset_set_panel(p, XSetPanel::SHOW_DIRTREE, XSetVar::MENU_LABEL, "T_ree");
        set->menu_style = XSetMenu::CHECK;
        set->b = XSetB::XSET_B_TRUE;
        if (p != 1)
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_show_dirtree");

        set = xset_set_panel(p, XSetPanel::SHOW_SIDEBAR, XSetVar::MENU_LABEL, "_Side Toolbar");
        set->menu_style = XSetMenu::CHECK;
        set->b = XSetB::XSET_B_UNSET;
        if (p != 1)
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_show_sidebar");

        set = xset_set_panel(p, XSetPanel::LIST_DETAILED, XSetVar::MENU_LABEL, "_Detailed");
        set->menu_style = XSetMenu::RADIO;
        set->b = XSetB::XSET_B_TRUE;
        if (p != 1)
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_list_detailed");

        set = xset_set_panel(p, XSetPanel::LIST_ICONS, XSetVar::MENU_LABEL, "_Icons");
        set->menu_style = XSetMenu::RADIO;
        if (p != 1)
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_list_icons");

        set = xset_set_panel(p, XSetPanel::LIST_COMPACT, XSetVar::MENU_LABEL, "_Compact");
        set->menu_style = XSetMenu::RADIO;
        if (p != 1)
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_list_compact");

        set = xset_set_panel(p, XSetPanel::LIST_LARGE, XSetVar::MENU_LABEL, "_Large Icons");
        set->menu_style = XSetMenu::CHECK;
        if (p != 1)
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_list_large");

        set = xset_set_panel(p, XSetPanel::SHOW_HIDDEN, XSetVar::MENU_LABEL, "_Hidden Files");
        set->menu_style = XSetMenu::CHECK;
        if (p != 1)
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_show_hidden");

        set = xset_set_panel(p, XSetPanel::ICON_TAB, XSetVar::MENU_LABEL, "_Icon");
        set->menu_style = XSetMenu::ICON;
        xset_set_var(set, XSetVar::ICN, "gtk-directory");

        set = xset_set_panel(p, XSetPanel::ICON_STATUS, XSetVar::MENU_LABEL, "_Icon");
        set->menu_style = XSetMenu::ICON;
        xset_set_var(set, XSetVar::ICN, "gtk-yes");
        if (p != 1)
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_icon_status");

        set = xset_set_panel(p, XSetPanel::DETCOL_NAME, XSetVar::MENU_LABEL, "_Name");
        set->menu_style = XSetMenu::CHECK;
        set->b = XSetB::XSET_B_TRUE; // visible
        set->x = ztd::strdup("0");   // position

        set = xset_set_panel(p, XSetPanel::DETCOL_SIZE, XSetVar::MENU_LABEL, "_Size");
        set->menu_style = XSetMenu::CHECK;
        set->b = XSetB::XSET_B_TRUE;
        set->x = ztd::strdup("1");
        if (p != 1)
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_detcol_size");

        set = xset_set_panel(p, XSetPanel::DETCOL_TYPE, XSetVar::MENU_LABEL, "_Type");
        set->menu_style = XSetMenu::CHECK;
        set->x = ztd::strdup("2");
        if (p != 1)
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_detcol_type");

        set = xset_set_panel(p, XSetPanel::DETCOL_PERM, XSetVar::MENU_LABEL, "_Permission");
        set->menu_style = XSetMenu::CHECK;
        set->x = ztd::strdup("3");
        if (p != 1)
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_detcol_perm");

        set = xset_set_panel(p, XSetPanel::DETCOL_OWNER, XSetVar::MENU_LABEL, "_Owner");
        set->menu_style = XSetMenu::CHECK;
        set->x = ztd::strdup("4");
        if (p != 1)
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_detcol_owner");

        set = xset_set_panel(p, XSetPanel::DETCOL_DATE, XSetVar::MENU_LABEL, "_Modified");
        set->menu_style = XSetMenu::CHECK;
        set->x = ztd::strdup("5");
        if (p != 1)
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_detcol_date");

        set = xset_get_panel(p, XSetPanel::SORT_EXTRA);
        set->b = XSetB::XSET_B_TRUE;               // sort_natural
        set->x = ztd::strdup(XSetB::XSET_B_FALSE); // sort_case
        set->y = ztd::strdup("1");                 // PTKFileListSortDir::PTK_LIST_SORT_DIR_FIRST
        set->z = ztd::strdup(XSetB::XSET_B_TRUE);  // sort_hidden_first

        set = xset_set_panel(p, XSetPanel::BOOK_FOL, XSetVar::MENU_LABEL, "Follow _Dir");
        set->menu_style = XSetMenu::CHECK;
        set->b = XSetB::XSET_B_TRUE;
        if (p != 1)
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_book_fol");
    }

    // speed
    set = xset_set(XSetName::BOOK_NEWTAB, XSetVar::MENU_LABEL, "_New Tab");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::BOOK_SINGLE, XSetVar::MENU_LABEL, "_Single Click");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::DEV_NEWTAB, XSetVar::MENU_LABEL, "_New Tab");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::DEV_SINGLE, XSetVar::MENU_LABEL, "_Single Click");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    // mark all labels and icons as default
    for (xset_t set2: xsets)
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
def_key(XSetName name, unsigned int key, unsigned int keymod)
{
    xset_t set = xset_get(name);

    // key already set or unset?
    if (set->key != 0 || key == 0)
        return;

    // key combo already in use?
    for (xset_t set2: keysets)
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
    for (xset_t set: xsets)
    {
        if (set->key)
            keysets.push_back(set);
    }

    def_key(XSetName::TAB_PREV, GDK_KEY_Tab, (GDK_SHIFT_MASK | GDK_CONTROL_MASK));
    def_key(XSetName::TAB_NEXT, GDK_KEY_Tab, GDK_CONTROL_MASK);
    def_key(XSetName::TAB_NEW, GDK_KEY_t, GDK_CONTROL_MASK);
    def_key(XSetName::TAB_RESTORE, GDK_KEY_T, (GDK_SHIFT_MASK | GDK_CONTROL_MASK));
    def_key(XSetName::TAB_CLOSE, GDK_KEY_w, GDK_CONTROL_MASK);
    def_key(XSetName::TAB_1, GDK_KEY_1, GDK_MOD1_MASK);
    def_key(XSetName::TAB_2, GDK_KEY_2, GDK_MOD1_MASK);
    def_key(XSetName::TAB_3, GDK_KEY_3, GDK_MOD1_MASK);
    def_key(XSetName::TAB_4, GDK_KEY_4, GDK_MOD1_MASK);
    def_key(XSetName::TAB_5, GDK_KEY_5, GDK_MOD1_MASK);
    def_key(XSetName::TAB_6, GDK_KEY_6, GDK_MOD1_MASK);
    def_key(XSetName::TAB_7, GDK_KEY_7, GDK_MOD1_MASK);
    def_key(XSetName::TAB_8, GDK_KEY_8, GDK_MOD1_MASK);
    def_key(XSetName::TAB_9, GDK_KEY_9, GDK_MOD1_MASK);
    def_key(XSetName::TAB_10, GDK_KEY_0, GDK_MOD1_MASK);
    def_key(XSetName::EDIT_CUT, GDK_KEY_x, GDK_CONTROL_MASK);
    def_key(XSetName::EDIT_COPY, GDK_KEY_c, GDK_CONTROL_MASK);
    def_key(XSetName::EDIT_PASTE, GDK_KEY_v, GDK_CONTROL_MASK);
    def_key(XSetName::EDIT_RENAME, GDK_KEY_F2, 0);
    def_key(XSetName::EDIT_DELETE, GDK_KEY_Delete, GDK_SHIFT_MASK);
    def_key(XSetName::EDIT_TRASH, GDK_KEY_Delete, 0);
    def_key(XSetName::COPY_NAME, GDK_KEY_C, (GDK_SHIFT_MASK | GDK_MOD1_MASK));
    def_key(XSetName::COPY_PATH, GDK_KEY_C, (GDK_SHIFT_MASK | GDK_CONTROL_MASK));
    def_key(XSetName::PASTE_LINK, GDK_KEY_V, (GDK_SHIFT_MASK | GDK_CONTROL_MASK));
    def_key(XSetName::PASTE_AS, GDK_KEY_A, (GDK_SHIFT_MASK | GDK_CONTROL_MASK));
    def_key(XSetName::SELECT_ALL, GDK_KEY_A, GDK_CONTROL_MASK);
    def_key(XSetName::MAIN_TERMINAL, GDK_KEY_F4, 0);
    def_key(XSetName::GO_DEFAULT, GDK_KEY_Escape, 0);
    def_key(XSetName::GO_BACK, GDK_KEY_Left, GDK_MOD1_MASK);
    def_key(XSetName::GO_FORWARD, GDK_KEY_Right, GDK_MOD1_MASK);
    def_key(XSetName::GO_UP, GDK_KEY_Up, GDK_MOD1_MASK);
    def_key(XSetName::FOCUS_PATH_BAR, GDK_KEY_l, GDK_CONTROL_MASK);
    def_key(XSetName::VIEW_REFRESH, GDK_KEY_F5, 0);
    def_key(XSetName::PROP_INFO, GDK_KEY_Return, GDK_MOD1_MASK);
    def_key(XSetName::PROP_PERM, GDK_KEY_p, GDK_CONTROL_MASK);
    def_key(XSetName::PANEL1_SHOW_HIDDEN, GDK_KEY_h, GDK_CONTROL_MASK);
    def_key(XSetName::BOOK_NEW, GDK_KEY_d, GDK_CONTROL_MASK);
    def_key(XSetName::NEW_FILE, GDK_KEY_F, (GDK_SHIFT_MASK | GDK_CONTROL_MASK));
    def_key(XSetName::NEW_DIRECTORY, GDK_KEY_N, (GDK_SHIFT_MASK | GDK_CONTROL_MASK));
    def_key(XSetName::NEW_LINK, GDK_KEY_L, (GDK_SHIFT_MASK | GDK_CONTROL_MASK));
    def_key(XSetName::MAIN_NEW_WINDOW, GDK_KEY_n, GDK_CONTROL_MASK);
    def_key(XSetName::OPEN_ALL, GDK_KEY_F6, 0);
    def_key(XSetName::MAIN_FULL, GDK_KEY_F11, 0);
    def_key(XSetName::PANEL1_SHOW, GDK_KEY_1, GDK_CONTROL_MASK);
    def_key(XSetName::PANEL2_SHOW, GDK_KEY_2, GDK_CONTROL_MASK);
    def_key(XSetName::PANEL3_SHOW, GDK_KEY_3, GDK_CONTROL_MASK);
    def_key(XSetName::PANEL4_SHOW, GDK_KEY_4, GDK_CONTROL_MASK);
    // def_key(XSetName::MAIN_HELP, GDK_KEY_F1, 0);
    def_key(XSetName::MAIN_EXIT, GDK_KEY_q, GDK_CONTROL_MASK);
    def_key(XSetName::MAIN_PREFS, GDK_KEY_F12, 0);
    def_key(XSetName::BOOK_ADD, GDK_KEY_d, GDK_CONTROL_MASK);
}
