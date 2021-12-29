/*
 * SpaceFM settings.c
 *
 * Copyright (C) 2015 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>
 *
 * License: See COPYING file
 *
 */

#include <string>
#include <filesystem>

#include <regex>

#include <iostream>
#include <fstream>

#include <unistd.h>

#include <sys/stat.h>

#include <gtk/gtk.h>

#include <gdk/gdkkeysyms.h>
#include <fcntl.h>

#include <gmodule.h>

#include <exo/exo.h>

#include "settings.hxx"
#include "main-window.hxx"
#include "item-prop.hxx"

#include "autosave.hxx"
#include "extern.hxx"
#include "utils.hxx"
#include "logger.hxx"

#include "vfs/vfs-app-desktop.hxx"
#include "vfs/vfs-utils.hxx"
#include "vfs/vfs-user-dir.hxx"

#include "ptk/ptk-utils.hxx"
#include "ptk/ptk-app-chooser.hxx"
#include "ptk/ptk-handler.hxx"
#include "ptk/ptk-file-menu.hxx"
#include "ptk/ptk-location-view.hxx"

#define CONFIG_VERSION "38" // 1.0.6

AppSettings app_settings = AppSettings();
ConfigSettings config_settings = ConfigSettings();

// MOD settings
static void xset_write(std::string& buf);
static void xset_parse(std::string& line);
static void read_root_settings();
static void xset_defaults();

GList* xsets = nullptr;
static GList* keysets = nullptr;
static XSet* set_clipboard = nullptr;
static bool clipboard_is_cut;
static XSet* set_last;

std::string settings_config_dir;
std::string settings_user_tmp_dir;

static XSetContext* xset_context = nullptr;
static XSet* book_icon_set_cached = nullptr;

EventHandler event_handler;

GList* xset_cmd_history = nullptr;

typedef void (*SettingsParseFunc)(std::string& line);

static void xset_free_all();
static void xset_default_keys();
static char* xset_color_dialog(GtkWidget* parent, char* title, char* defcolor);
static GtkWidget* xset_design_additem(GtkWidget* menu, const char* label, int job, XSet* set);
static bool xset_design_cb(GtkWidget* item, GdkEventButton* event, XSet* set);
static void xset_builtin_tool_activate(char tool_type, XSet* set, GdkEventButton* event);
static XSet* xset_new_builtin_toolitem(char tool_type);
static void xset_custom_insert_after(XSet* target, XSet* set);
static XSet* xset_custom_copy(XSet* set, bool copy_next, bool delete_set);
static XSet* xset_set_set_int(XSet* set, const char* var, const char* value);
static void xset_free(XSet* set);

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

static const char* builtin_tool_name[] = { // must match XSET_TOOL_ enum
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
    "Large Icons"};

static const char* builtin_tool_icon[] = { // must match XSET_TOOL_ enum
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
    "zoom-in"};

static const char* builtin_tool_shared_key[] = { // must match XSET_TOOL_ enum
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
    "panel1_list_large"};

static void
parse_general_settings(std::string& line)
{
    std::size_t sep = line.find("=");
    if (sep == std::string::npos)
        return;

    // remove leading spaces
    line = std::regex_replace(line, std::regex("^ +"), "");
    // remove tailing spaces
    line = std::regex_replace(line, std::regex(" +$"), "");

    if (line.at(0) == '#')
        return;

    std::string token = line.substr(0, sep);
    std::string value = line.substr(sep + 1, std::string::npos - 1);

    // remove any quotes
    std::size_t quotes = value.find('\"');
    if (quotes != std::string::npos)
        value.erase(std::remove(value.begin(), value.end(), '\"'), value.end());

    if (value.empty())
        return;

    if (!token.compare("show_thumbnail"))
        app_settings.show_thumbnail = std::stoi(value);
    else if (!token.compare("max_thumb_size"))
        app_settings.max_thumb_size = std::stoi(value) << 10;
    else if (!token.compare("big_icon_size"))
        app_settings.big_icon_size = std::stoi(value);
    else if (!token.compare("small_icon_size"))
        app_settings.small_icon_size = std::stoi(value);
    else if (!token.compare("tool_icon_size"))
        app_settings.tool_icon_size = std::stoi(value);
    else if (!token.compare("single_click"))
        app_settings.single_click = std::stoi(value);
    else if (!token.compare("no_single_hover"))
        app_settings.no_single_hover = std::stoi(value);
    else if (!token.compare("sort_order"))
        app_settings.sort_order = std::stoi(value);
    else if (!token.compare("sort_type"))
        app_settings.sort_type = std::stoi(value);
    else if (!token.compare("use_si_prefix"))
        app_settings.use_si_prefix = std::stoi(value);
    else if (!token.compare("no_execute"))
        app_settings.no_execute = std::stoi(value);
    else if (!token.compare("no_confirm"))
        app_settings.no_confirm = std::stoi(value);
    else if (!token.compare("no_confirm_trash"))
        app_settings.no_confirm_trash = std::stoi(value);
}

static void
parse_window_state(std::string& line)
{
    std::size_t sep = line.find("=");
    if (sep == std::string::npos)
        return;

    // remove leading spaces
    line = std::regex_replace(line, std::regex("^ +"), "");
    // remove tailing spaces
    line = std::regex_replace(line, std::regex(" +$"), "");

    if (line.at(0) == '#')
        return;

    std::string token = line.substr(0, sep);
    std::string value = line.substr(sep + 1, std::string::npos - 1);

    // remove any quotes
    std::size_t quotes = value.find('\"');
    if (quotes != std::string::npos)
        value.erase(std::remove(value.begin(), value.end(), '\"'), value.end());

    if (value.empty())
        return;

    if (!token.compare("width"))
        app_settings.width = std::stoi(value);
    else if (!token.compare("height"))
        app_settings.height = std::stoi(value);
    else if (!token.compare("maximized"))
        app_settings.maximized = std::stoi(value);
}

static void
parse_interface_settings(std::string& line)
{
    std::size_t sep = line.find("=");
    if (sep == std::string::npos)
        return;

    // remove leading spaces
    line = std::regex_replace(line, std::regex("^ +"), "");
    // remove tailing spaces
    line = std::regex_replace(line, std::regex(" +$"), "");

    if (line.at(0) == '#')
        return;

    std::string token = line.substr(0, sep);
    std::string value = line.substr(sep + 1, std::string::npos - 1);

    // remove any quotes
    std::size_t quotes = value.find('\"');
    if (quotes != std::string::npos)
        value.erase(std::remove(value.begin(), value.end(), '\"'), value.end());

    if (value.empty())
        return;

    if (!token.compare("always_show_tabs"))
        app_settings.always_show_tabs = std::stoi(value);
    else if (!token.compare("show_close_tab_buttons"))
        app_settings.show_close_tab_buttons = std::stoi(value);
}

static void
parse_conf(std::string& etc_path, std::string& line)
{
    std::size_t sep = line.find("=");
    if (sep == std::string::npos)
        return;

    // remove leading spaces
    line = std::regex_replace(line, std::regex("^ +"), "");
    // remove tailing spaces
    line = std::regex_replace(line, std::regex(" +$"), "");

    if (line.at(0) == '#')
        return;

    std::string token = line.substr(0, sep);
    std::string value = line.substr(sep + 1, std::string::npos - 1);

    // remove any quotes
    std::size_t quotes = value.find('\"');
    if (quotes != std::string::npos)
        value.erase(std::remove(value.begin(), value.end(), '\"'), value.end());

    if (value.empty())
        return;

    // LOG_INFO("token {}", token);
    // LOG_INFO("value {}", value);

    if (!token.compare("terminal_su") || !token.compare("graphical_su"))
    {
        if (value.at(0) != '/' || !std::filesystem::exists(value))
            LOG_WARN("{}: {} '{}' file not found", etc_path, token, value);
        else if (!token.compare("terminal_su"))
            config_settings.terminal_su = value.c_str();
    }
    else if (!token.compare("font_view_icon"))
        config_settings.font_view_icon = value.c_str();
    else if (!token.compare("font_view_compact"))
        config_settings.font_view_compact = value.c_str();
    else if (!token.compare("font_general"))
        config_settings.font_general = value.c_str();
}

void
load_conf()
{
    std::string default_font = "Monospace 9";

    /* Set default config values */
    config_settings.terminal_su = nullptr;
    config_settings.tmp_dir = vfs_user_cache_dir();
    config_settings.font_view_icon = default_font.c_str();
    config_settings.font_view_compact = default_font.c_str();
    config_settings.font_general = default_font.c_str();

    // load spacefm.conf
    std::string config_path =
        g_build_filename(vfs_user_config_dir(), "spacefm", "spacefm.conf", nullptr);
    if (!std::filesystem::exists(config_path))
        config_path = g_build_filename(SYSCONFDIR, "spacefm", "spacefm.conf", nullptr);

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
    xset_cmd_history = nullptr;
    app_settings.load_saved_tabs = true;

    if (config_dir)
        settings_config_dir = g_build_filename(config_dir, nullptr);
    else
        settings_config_dir = g_build_filename(vfs_user_config_dir(), "spacefm", nullptr);

    // MOD extra settings
    xset_defaults();

    const std::string session = g_build_filename(settings_config_dir.c_str(), "session", nullptr);

    std::string command;

    if (!std::filesystem::exists(settings_config_dir))
    {
        // copy /etc/xdg/spacefm
        std::string xdg_path =
            g_build_filename(settings_config_dir.c_str(), "xdg", "spacefm", nullptr);
        if (std::filesystem::is_directory(xdg_path))
        {
            command = fmt::format("cp -r {} '{}'", xdg_path, settings_config_dir);
            print_command(command);
            g_spawn_command_line_sync(command.c_str(), nullptr, nullptr, nullptr, nullptr);

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
        if (!g_find_program_in_path("git"))
        {
            LOG_ERROR("git backed settings enabled but git is not installed");
            config_settings.git_backed_settings = false;
        }
    }

    if (config_settings.git_backed_settings)
    {
        std::string git_path = g_build_filename(settings_config_dir.c_str(), ".git", nullptr);
        if (!std::filesystem::exists(git_path))
        {
            command = fmt::format("{} -c \"cd {} && git init && "
                                  "git config commit.gpgsign false\"",
                                  BASHPATH,
                                  settings_config_dir);
            g_spawn_command_line_sync(command.c_str(), nullptr, nullptr, nullptr, nullptr);
            LOG_INFO("Initialized git repo at: {}", git_path);
        }
        else if (G_LIKELY(std::filesystem::exists(session)))
        {
            command = fmt::format("{} -c \"cd {} && git add session && "
                                  "git commit -m 'Session File' 1>/dev/null\"",
                                  BASHPATH,
                                  settings_config_dir);
            g_spawn_command_line_sync(command.c_str(), nullptr, nullptr, nullptr, nullptr);
            LOG_INFO("Updated git copy of: {}", session);
        }
        else if (std::filesystem::exists(git_path))
        {
            command = fmt::format("{} -c \"cd {} && git checkout session\"",
                                  BASHPATH,
                                  settings_config_dir);
            print_command(command);
            g_spawn_command_line_sync(command.c_str(), nullptr, nullptr, nullptr, nullptr);
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
                    if (!line.compare("[General]"))
                        func = &parse_general_settings;
                    else if (!line.compare("[Window]"))
                        func = &parse_window_state;
                    else if (!line.compare("[Interface]"))
                        func = &parse_interface_settings;
                    else if (!line.compare("[MOD]"))
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
        xset_set("date_format", "s", app_settings.date_format.c_str());
    }

    // MOD su command discovery (sets default)
    char* set_su = get_valid_su();
    if (set_su)
        g_free(set_su);

    // MOD terminal discovery
    char* terminal = xset_get_s("main_terminal");
    if (!terminal || terminal[0] == '\0')
    {
        unsigned int i;
        for (i = 0; i < G_N_ELEMENTS(terminal_programs); i++)
        {
            char* term;
            if ((term = g_find_program_in_path(terminal_programs[i])))
            {
                xset_set("main_terminal", "s", terminal_programs[i]);
                xset_set_b("main_terminal", true); // discovery
                g_free(term);
                break;
            }
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
                VFSAppDesktop* app = vfs_app_desktop_new(app_name);
                if (app)
                {
                    if (app->exec)
                        xset_set("editor", "s", app->exec);
                    vfs_app_desktop_unref(app);
                }
            }
        }
    }

    // add default handlers
    ptk_handler_add_defaults(HANDLER_MODE_ARC, false, false);
    ptk_handler_add_defaults(HANDLER_MODE_FS, false, false);
    ptk_handler_add_defaults(HANDLER_MODE_NET, false, false);
    ptk_handler_add_defaults(HANDLER_MODE_FILE, false, false);

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
save_settings(void* main_window_ptr)
{
    XSet* set;
    FMMainWindow* main_window;
    // LOG_INFO("save_settings");

    xset_set("config_version", "s", CONFIG_VERSION);

    // save tabs
    bool save_tabs = xset_get_b("main_save_tabs");
    if (main_window_ptr)
        main_window = static_cast<FMMainWindow*>(main_window_ptr);
    else
        main_window = fm_main_window_get_last_active();

    if (GTK_IS_WIDGET(main_window))
    {
        int p;
        if (save_tabs)
        {
            for (p = 1; p < 5; p++)
            {
                set = xset_get_panel(p, "show");
                if (GTK_IS_NOTEBOOK(main_window->panel[p - 1]))
                {
                    int pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_window->panel[p - 1]));
                    if (pages) // panel was shown
                    {
                        if (set->s)
                        {
                            g_free(set->s);
                            set->s = nullptr;
                        }
                        char* tabs = g_strdup("");
                        int g;
                        for (g = 0; g < pages; g++)
                        {
                            PtkFileBrowser* file_browser = PTK_FILE_BROWSER(
                                gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_window->panel[p - 1]),
                                                          g));
                            char* old_tabs = tabs;
                            tabs = g_strdup_printf("%s///%s",
                                                   old_tabs,
                                                   ptk_file_browser_get_cwd(file_browser));
                            g_free(old_tabs);
                        }
                        if (tabs[0] != '\0')
                            set->s = tabs;
                        else
                            g_free(tabs);

                        // save current tab
                        if (set->x)
                            g_free(set->x);
                        set->x = g_strdup_printf(
                            "%d",
                            gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->panel[p - 1])));
                    }
                }
            }
        }
        else
        {
            // clear saved tabs
            for (p = 1; p < 5; p++)
            {
                set = xset_get_panel(p, "show");
                if (set->s)
                {
                    g_free(set->s);
                    set->s = nullptr;
                }
                if (set->x)
                {
                    g_free(set->x);
                    set->x = nullptr;
                }
            }
        }
    }

    /* save settings */
    if (G_UNLIKELY(!std::filesystem::exists(settings_config_dir)))
    {
        std::filesystem::create_directories(settings_config_dir);
        std::filesystem::permissions(settings_config_dir, std::filesystem::perms::owner_all);
    }

    std::string buf = "";

    // clang-format off
    buf.append("[General]\n");
    buf.append(fmt::format("show_thumbnail={:d}\n", app_settings.show_thumbnail));
    buf.append(fmt::format("max_thumb_size={}\n", app_settings.max_thumb_size >> 10));
    buf.append(fmt::format("big_icon_size={}\n", app_settings.big_icon_size));
    buf.append(fmt::format("small_icon_size={}\n", app_settings.small_icon_size));
    buf.append(fmt::format("tool_icon_size={}\n", app_settings.tool_icon_size));
    buf.append(fmt::format("single_click={:d}\n", app_settings.single_click));
    buf.append(fmt::format("no_single_hover={:d}\n", app_settings.no_single_hover));
    buf.append(fmt::format("sort_order={}\n", app_settings.sort_order));
    buf.append(fmt::format("sort_type={}\n", app_settings.sort_type));
    buf.append(fmt::format("use_si_prefix={:d}\n", app_settings.use_si_prefix));
    buf.append(fmt::format("no_execute={:d}\n", app_settings.no_execute));
    buf.append(fmt::format("no_confirm={:d}\n", app_settings.no_confirm));
    buf.append(fmt::format("no_confirm_trash={:d}\n", app_settings.no_confirm_trash));

    buf.append("\n[Window]\n");
    buf.append(fmt::format("width={}\n", app_settings.width));
    buf.append(fmt::format("height={}\n", app_settings.height));
    buf.append(fmt::format("maximized={:d}\n", app_settings.maximized));

    buf.append("\n[Interface]\n");
    buf.append(fmt::format("always_show_tabs={:d}\n", app_settings.always_show_tabs));
    buf.append(fmt::format("show_close_tab_buttons={:d}\n", app_settings.show_close_tab_buttons));

    buf.append("\n[MOD]\n");
    xset_write(buf);
    // clang-format on

    // move
    std::string path = g_build_filename(settings_config_dir.c_str(), "session", nullptr);
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
    if (xset_cmd_history)
    {
        g_list_foreach(xset_cmd_history, (GFunc)g_free, nullptr);
        g_list_free(xset_cmd_history);
        xset_cmd_history = nullptr;
    }

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

    settings_user_tmp_dir = g_build_filename(config_settings.tmp_dir, "spacefm", nullptr);
    std::filesystem::create_directories(settings_user_tmp_dir);
    std::filesystem::permissions(settings_user_tmp_dir, std::filesystem::perms::owner_all);

    return settings_user_tmp_dir.c_str();
}

static void
xset_free_all()
{
    XSet* set;
    GList* l;

    for (l = xsets; l; l = l->next)
    {
        set = XSET(l->data);
        if (set->ob2_data && g_str_has_prefix(set->name, "evt_"))
        {
            g_list_foreach((GList*)set->ob2_data, (GFunc)g_free, nullptr);
            g_list_free((GList*)set->ob2_data);
        }

        xset_free(set);

        g_slice_free(XSet, set);
    }
    g_list_free(xsets);
    xsets = nullptr;
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
        g_free(set->name);
    if (set->s)
        g_free(set->s);
    if (set->x)
        g_free(set->x);
    if (set->y)
        g_free(set->y);
    if (set->z)
        g_free(set->z);
    if (set->menu_label)
        g_free(set->menu_label);
    if (set->shared_key)
        g_free(set->shared_key);
    if (set->icon)
        g_free(set->icon);
    if (set->desc)
        g_free(set->desc);
    if (set->title)
        g_free(set->title);
    if (set->next)
        g_free(set->next);
    if (set->parent)
        g_free(set->parent);
    if (set->child)
        g_free(set->child);
    if (set->prev)
        g_free(set->prev);
    if (set->line)
        g_free(set->line);
    if (set->context)
        g_free(set->context);
    if (set->plugin)
    {
        if (set->plug_dir)
            g_free(set->plug_dir);
        if (set->plug_name)
            g_free(set->plug_name);
    }

    xsets = g_list_remove(xsets, set);
    g_slice_free(XSet, set);
    set_last = nullptr;
}

static XSet*
xset_new(const char* name)
{
    XSet* set = g_slice_new(XSet);
    set->name = g_strdup(name);

    set->b = XSET_B_UNSET;
    set->s = nullptr;
    set->x = nullptr;
    set->y = nullptr;
    set->z = nullptr;
    set->disable = false;
    set->menu_label = nullptr;
    set->menu_style = XSET_MENU_NORMAL;
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
    set->tool = XSET_TOOL_NOT;
    set->lock = true;
    set->plugin = false;

    // custom ( !lock )
    set->prev = nullptr;
    set->parent = nullptr;
    set->child = nullptr;
    set->line = nullptr;
    set->task = XSET_B_UNSET;
    set->task_pop = XSET_B_UNSET;
    set->task_err = XSET_B_UNSET;
    set->task_out = XSET_B_UNSET;
    set->in_terminal = XSET_B_UNSET;
    set->keep_terminal = XSET_B_UNSET;
    set->scroll_lock = XSET_B_UNSET;
    set->opener = 0;
    return set;
}

XSet*
xset_get(const char* name)
{
    if (!name)
        return nullptr;

    GList* l;
    for (l = xsets; l; l = l->next)
    {
        if (!strcmp(name, (XSET(l->data))->name))
        {
            // existing xset
            return XSET(l->data);
        }
    }

    // add new
    xsets = g_list_prepend(xsets, xset_new(name));
    return XSET(xsets->data);
}

XSet*
xset_get_panel(int panel, const char* name)
{
    char* fullname = g_strdup_printf("panel%d_%s", panel, name);
    XSet* set = xset_get(fullname);
    g_free(fullname);
    return set;
}

XSet*
xset_get_panel_mode(int panel, const char* name, char mode)
{
    char* fullname = g_strdup_printf("panel%d_%s%d", panel, name, mode);
    XSet* set = xset_get(fullname);
    g_free(fullname);
    return set;
}

char*
xset_get_s(const char* name)
{
    XSet* set = xset_get(name);
    if (set)
        return set->s;
    else
        return nullptr;
}

char*
xset_get_s_panel(int panel, const char* name)
{
    char* fullname = g_strdup_printf("panel%d_%s", panel, name);
    char* s = xset_get_s(fullname);
    g_free(fullname);
    return s;
}

bool
xset_get_b(const char* name)
{
    XSet* set = xset_get(name);
    return (set->b == XSET_B_TRUE);
}

bool
xset_get_b_panel(int panel, const char* name)
{
    char* fullname = g_strdup_printf("panel%d_%s", panel, name);
    bool b = xset_get_b(fullname);
    g_free(fullname);
    return b;
}

bool
xset_get_b_panel_mode(int panel, const char* name, char mode)
{
    char* fullname = g_strdup_printf("panel%d_%s%d", panel, name, mode);
    bool b = xset_get_b(fullname);
    g_free(fullname);
    return b;
}

static bool
xset_get_b_set(XSet* set)
{
    return (set->b == XSET_B_TRUE);
}

XSet*
xset_is(const char* name)
{
    if (!name)
        return nullptr;

    GList* l;
    for (l = xsets; l; l = l->next)
    {
        if (!strcmp(name, (XSET(l->data))->name))
        {
            // existing xset
            return XSET(l->data);
        }
    }
    return nullptr;
}

XSet*
xset_set_b(const char* name, bool bval)
{
    XSet* set = xset_get(name);

    if (bval)
        set->b = XSET_B_TRUE;
    else
        set->b = XSET_B_FALSE;
    return set;
}

XSet*
xset_set_b_panel(int panel, const char* name, bool bval)
{
    char* fullname = g_strdup_printf("panel%d_%s", panel, name);
    XSet* set = xset_set_b(fullname, bval);
    g_free(fullname);
    return set;
}

XSet*
xset_set_b_panel_mode(int panel, const char* name, char mode, bool bval)
{
    char* fullname = g_strdup_printf("panel%d_%s%d", panel, name, mode);
    XSet* set = xset_set_b(fullname, bval);
    g_free(fullname);
    return set;
}

static int
xset_get_int_set(XSet* set, const char* var)
{
    if (!set || !var)
        return -1;
    const char* varstring = nullptr;
    if (!strcmp(var, "x"))
        varstring = set->x;
    else if (!strcmp(var, "y"))
        varstring = set->y;
    else if (!strcmp(var, "z"))
        varstring = set->z;
    else if (!strcmp(var, "s"))
        varstring = set->s;
    else if (!strcmp(var, "key"))
        return set->key;
    else if (!strcmp(var, "keymod"))
        return set->keymod;
    if (!varstring)
        return 0;
    return strtol(varstring, nullptr, 10);
}

int
xset_get_int(const char* name, const char* var)
{
    XSet* set = xset_get(name);
    return xset_get_int_set(set, var);
}

int
xset_get_int_panel(int panel, const char* name, const char* var)
{
    char* fullname = g_strdup_printf("panel%d_%s", panel, name);
    int i = xset_get_int(fullname, var);
    g_free(fullname);
    return i;
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
        buf.append(fmt::format("{}-s={}\n", set->name, set->s));
    if (set->x)
        buf.append(fmt::format("{}-x={}\n", set->name, set->x));
    if (set->y)
        buf.append(fmt::format("{}-y={}\n", set->name, set->y));
    if (set->z)
        buf.append(fmt::format("{}-z={}\n", set->name, set->z));
    if (set->key)
        buf.append(fmt::format("{}-key={}\n", set->name, set->key));
    if (set->keymod)
        buf.append(fmt::format("{}-keymod={}\n", set->name, set->keymod));
    // menu label
    if (set->menu_label)
    {
        if (set->lock)
        {
            // built-in
            if (set->in_terminal == XSET_B_TRUE && set->menu_label && set->menu_label[0])
                // only save lbl if menu_label was customized
                buf.append(fmt::format("{}-lbl={}\n", set->name, set->menu_label));
        }
        else
            // custom
            buf.append(fmt::format("{}-label={}\n", set->name, set->menu_label));
    }
    // icon
    if (set->lock)
    {
        // built-in
        if (set->keep_terminal == XSET_B_TRUE)
            // only save icn if icon was customized
            buf.append(fmt::format("{}-icn={}\n", set->name, set->icon ? set->icon : ""));
    }
    else if (set->icon)
        // custom
        buf.append(fmt::format("{}-icon={}\n", set->name, set->icon));
    if (set->next)
        buf.append(fmt::format("{}-next={}\n", set->name, set->next));
    if (set->child)
        buf.append(fmt::format("{}-child={}\n", set->name, set->child));
    if (set->context)
        buf.append(fmt::format("{}-cxt={}\n", set->name, set->context));
    if (set->b != XSET_B_UNSET)
        buf.append(fmt::format("{}-b={}\n", set->name, set->b));
    if (set->tool != XSET_TOOL_NOT)
        buf.append(fmt::format("{}-tool={}\n", set->name, set->tool));
    if (!set->lock)
    {
        if (set->menu_style)
            buf.append(fmt::format("{}-style={}\n", set->name, set->menu_style));
        if (set->desc)
            buf.append(fmt::format("{}-desc={}\n", set->name, set->desc));
        if (set->title)
            buf.append(fmt::format("{}-title={}\n", set->name, set->title));
        if (set->prev)
            buf.append(fmt::format("{}-prev={}\n", set->name, set->prev));
        if (set->parent)
            buf.append(fmt::format("{}-parent={}\n", set->name, set->parent));
        if (set->line)
            buf.append(fmt::format("{}-line={}\n", set->name, set->line));
        if (set->task != XSET_B_UNSET)
            buf.append(fmt::format("{}-task={}\n", set->name, set->task));
        if (set->task_pop != XSET_B_UNSET)
            buf.append(fmt::format("{}-task_pop={}\n", set->name, set->task_pop));
        if (set->task_err != XSET_B_UNSET)
            buf.append(fmt::format("{}-task_err={}\n", set->name, set->task_err));
        if (set->task_out != XSET_B_UNSET)
            buf.append(fmt::format("{}-task_out={}\n", set->name, set->task_out));
        if (set->in_terminal != XSET_B_UNSET)
            buf.append(fmt::format("{}-term={}\n", set->name, set->in_terminal));
        if (set->keep_terminal != XSET_B_UNSET)
            buf.append(fmt::format("{}-keep={}\n", set->name, set->keep_terminal));
        if (set->scroll_lock != XSET_B_UNSET)
            buf.append(fmt::format("{}-scroll={}\n", set->name, set->scroll_lock));
        if (set->opener != 0)
            buf.append(fmt::format("{}-op={}\n", set->name, set->opener));
    }
}

static void
xset_write(std::string& buf)
{
    GList* l;

    for (l = g_list_last(xsets); l; l = l->prev)
    {
        // hack to not save default handlers - this allows default handlers
        // to be updated more easily
        if ((bool)(XSET(l->data))->disable && (char)(XSET(l->data))->name[0] == 'h' &&
            g_str_has_prefix((char*)(XSET(l->data))->name, "hand"))
            continue;
        xset_write_set(buf, XSET(l->data));
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

    // remove leading spaces
    line = std::regex_replace(line, std::regex("^ +"), "");
    // remove tailing spaces
    line = std::regex_replace(line, std::regex(" +$"), "");

    if (line.at(0) == '#')
        return;

    std::string token = line.substr(0, sep2);
    std::string value = line.substr(sep + 1, std::string::npos - 1);
    std::string token_var = line.substr(sep2 + 1, sep - sep2 - 1);

    // remove any quotes
    std::size_t quotes = value.find('\"');
    if (quotes != std::string::npos)
        value.erase(std::remove(value.begin(), value.end(), '\"'), value.end());

    if (value.empty())
        return;

    if (token.substr(0, 5) == "cstm_" || token.substr(0, 5) == "hand_")
    {
        // custom
        if (!token.compare(set_last->name))
            xset_set_set_int(set_last, token_var.c_str(), value.c_str());
        else
        {
            set_last = xset_get(token.c_str());
            if (set_last->lock)
                set_last->lock = false;
            xset_set_set_int(set_last, token_var.c_str(), value.c_str());
        }
    }
    else
    {
        // normal (lock)
        if (!token.compare(set_last->name))
            xset_set_set_int(set_last, token_var.c_str(), value.c_str());
        else
            set_last = xset_set(token.c_str(), token_var.c_str(), value.c_str());
    }
}

XSet*
xset_set_cb(const char* name, GFunc cb_func, void* cb_data)
{
    XSet* set = xset_get(name);
    set->cb_func = cb_func;
    set->cb_data = cb_data;
    return set;
}

XSet*
xset_set_cb_panel(int panel, const char* name, GFunc cb_func, void* cb_data)
{
    char* fullname = g_strdup_printf("panel%d_%s", panel, name);
    XSet* set = xset_set_cb(fullname, cb_func, cb_data);
    g_free(fullname);
    return set;
}

XSet*
xset_set_ob1_int(XSet* set, const char* ob1, int ob1_int)
{
    if (set->ob1)
        g_free(set->ob1);
    set->ob1 = g_strdup(ob1);
    set->ob1_data = GINT_TO_POINTER(ob1_int);
    return set;
}

XSet*
xset_set_ob1(XSet* set, const char* ob1, void* ob1_data)
{
    if (set->ob1)
        g_free(set->ob1);
    set->ob1 = g_strdup(ob1);
    set->ob1_data = ob1_data;
    return set;
}

XSet*
xset_set_ob2(XSet* set, const char* ob2, void* ob2_data)
{
    if (set->ob2)
        g_free(set->ob2);
    set->ob2 = g_strdup(ob2);
    set->ob2_data = ob2_data;
    return set;
}

static XSet*
xset_set_set_int(XSet* set, const char* var, const char* value)
{
    int tmp;
    if (!strcmp(var, "s"))
        tmp = XSET_SET_SET_S;
    else if (!strcmp(var, "b"))
        tmp = XSET_SET_SET_B;
    else if (!strcmp(var, "x"))
        tmp = XSET_SET_SET_X;
    else if (!strcmp(var, "y"))
        tmp = XSET_SET_SET_Y;
    else if (!strcmp(var, "z"))
        tmp = XSET_SET_SET_Z;
    else if (!strcmp(var, "key"))
        tmp = XSET_SET_SET_KEY;
    else if (!strcmp(var, "keymod"))
        tmp = XSET_SET_SET_KEYMOD;
    else if (!strcmp(var, "style"))
        tmp = XSET_SET_SET_STYLE;
    else if (!strcmp(var, "desc"))
        tmp = XSET_SET_SET_DESC;
    else if (!strcmp(var, "title"))
        tmp = XSET_SET_SET_TITLE;
    else if (!strcmp(var, "lbl"))
        tmp = XSET_SET_SET_LBL;
    else if (!strcmp(var, "icn"))
        tmp = XSET_SET_SET_ICN;
    else if (!strcmp(var, "label"))
        tmp = XSET_SET_SET_LABEL;
    else if (!strcmp(var, "icon"))
        tmp = XSET_SET_SET_ICON;
    else if (!strcmp(var, "shared_key"))
        tmp = XSET_SET_SET_SHARED_KEY;
    else if (!strcmp(var, "next"))
        tmp = XSET_SET_SET_NEXT;
    else if (!strcmp(var, "prev"))
        tmp = XSET_SET_SET_PREV;
    else if (!strcmp(var, "parent"))
        tmp = XSET_SET_SET_PARENT;
    else if (!strcmp(var, "child"))
        tmp = XSET_SET_SET_CHILD;
    else if (!strcmp(var, "cxt"))
        tmp = XSET_SET_SET_CXT;
    else if (!strcmp(var, "line"))
        tmp = XSET_SET_SET_LINE;
    else if (!strcmp(var, "tool"))
        tmp = XSET_SET_SET_TOOL;
    else if (!strcmp(var, "task"))
        tmp = XSET_SET_SET_TASK;
    else if (!strcmp(var, "task_pop"))
        tmp = XSET_SET_SET_TASK_POP;
    else if (!strcmp(var, "task_err"))
        tmp = XSET_SET_SET_TASK_ERR;
    else if (!strcmp(var, "task_out"))
        tmp = XSET_SET_SET_TASK_OUT;
    else if (!strcmp(var, "term"))
        tmp = XSET_SET_SET_TERM;
    else if (!strcmp(var, "keep"))
        tmp = XSET_SET_SET_KEEP;
    else if (!strcmp(var, "scroll"))
        tmp = XSET_SET_SET_SCROLL;
    else if (!strcmp(var, "disable"))
        tmp = XSET_SET_SET_DISABLE;
    else if (!strcmp(var, "op"))
        tmp = XSET_SET_SET_OP;
    else
        return nullptr;

    return xset_set_set(set, tmp, value);
}

XSet*
xset_set_set(XSet* set, int var, const char* value)
{
    if (!set)
        return nullptr;

    switch (var)
    {
        case XSET_SET_SET_S:
            if (set->s)
                g_free(set->s);
            set->s = g_strdup(value);
            break;
        case XSET_SET_SET_B:
            if (!strcmp(value, "1"))
                set->b = XSET_B_TRUE;
            else
                set->b = XSET_B_FALSE;
            break;
        case XSET_SET_SET_X:
            if (set->x)
                g_free(set->x);
            set->x = g_strdup(value);
            break;
        case XSET_SET_SET_Y:
            if (set->y)
                g_free(set->y);
            set->y = g_strdup(value);
            break;
        case XSET_SET_SET_Z:
            if (set->z)
                g_free(set->z);
            set->z = g_strdup(value);
            break;
        case XSET_SET_SET_KEY:
            set->key = strtol(value, nullptr, 10);
            break;
        case XSET_SET_SET_KEYMOD:
            set->keymod = strtol(value, nullptr, 10);
            break;
        case XSET_SET_SET_STYLE:
            set->menu_style = (XSetMenu)strtol(value, nullptr, 10);
            break;
        case XSET_SET_SET_DESC:
            if (set->desc)
                g_free(set->desc);
            set->desc = g_strdup(value);
            break;
        case XSET_SET_SET_TITLE:
            if (set->title)
                g_free(set->title);
            set->title = g_strdup(value);
            break;
        case XSET_SET_SET_LBL:
            // lbl is only used >= 0.9.0 for changed lock default menu_label
            if (set->menu_label)
                g_free(set->menu_label);
            set->menu_label = g_strdup(value);
            if (set->lock)
                // indicate that menu label is not default and should be saved
                set->in_terminal = XSET_B_TRUE;
            break;
        case XSET_SET_SET_ICN:
            // icn is only used >= 0.9.0 for changed lock default icon
            if (set->icon)
                g_free(set->icon);
            set->icon = g_strdup(value);
            if (set->lock)
                // indicate that icon is not default and should be saved
                set->keep_terminal = XSET_B_TRUE;
            break;
        case XSET_SET_SET_LABEL:
            // pre-0.9.0 menu_label or >= 0.9.0 custom item label
            // only save if custom or not default label
            if (!set->lock || g_strcmp0(set->menu_label, value))
            {
                if (set->menu_label)
                    g_free(set->menu_label);
                set->menu_label = g_strdup(value);
                if (set->lock)
                    // indicate that menu label is not default and should be saved
                    set->in_terminal = XSET_B_TRUE;
            }
            break;
        case XSET_SET_SET_ICON:
            // pre-0.9.0 icon or >= 0.9.0 custom item icon
            // only save if custom or not default icon
            // also check that stock name doesn't match
            break;
        case XSET_SET_SET_SHARED_KEY:
            if (set->shared_key)
                g_free(set->shared_key);
            set->shared_key = g_strdup(value);
            break;
        case XSET_SET_SET_NEXT:
            if (set->next)
                g_free(set->next);
            set->next = g_strdup(value);
            break;
        case XSET_SET_SET_PREV:
            if (set->prev)
                g_free(set->prev);
            set->prev = g_strdup(value);
            break;
        case XSET_SET_SET_PARENT:
            if (set->parent)
                g_free(set->parent);
            set->parent = g_strdup(value);
            break;
        case XSET_SET_SET_CHILD:
            if (set->child)
                g_free(set->child);
            set->child = g_strdup(value);
            break;
        case XSET_SET_SET_CXT:
            if (set->context)
                g_free(set->context);
            set->context = g_strdup(value);
            break;
        case XSET_SET_SET_LINE:
            if (set->line)
                g_free(set->line);
            set->line = g_strdup(value);
            break;
        case XSET_SET_SET_TOOL:
            set->tool = strtol(value, nullptr, 10);
            break;
        case XSET_SET_SET_TASK:
            if (strtol(value, nullptr, 10) == 1)
                set->task = XSET_B_TRUE;
            else
                set->task = XSET_B_UNSET;
            break;
        case XSET_SET_SET_TASK_POP:
            if (strtol(value, nullptr, 10) == 1)
                set->task_pop = XSET_B_TRUE;
            else
                set->task_pop = XSET_B_UNSET;
            break;
        case XSET_SET_SET_TASK_ERR:
            if (strtol(value, nullptr, 10) == 1)
                set->task_err = XSET_B_TRUE;
            else
                set->task_err = XSET_B_UNSET;
            break;
        case XSET_SET_SET_TASK_OUT:
            if (strtol(value, nullptr, 10) == 1)
                set->task_out = XSET_B_TRUE;
            else
                set->task_out = XSET_B_UNSET;
            break;
        case XSET_SET_SET_TERM:
            if (strtol(value, nullptr, 10) == 1)
                set->in_terminal = XSET_B_TRUE;
            else
                set->in_terminal = XSET_B_UNSET;
            break;
        case XSET_SET_SET_KEEP:
            if (strtol(value, nullptr, 10) == 1)
                set->keep_terminal = XSET_B_TRUE;
            else
                set->keep_terminal = XSET_B_UNSET;
            break;
        case XSET_SET_SET_SCROLL:
            if (strtol(value, nullptr, 10) == 1)
                set->scroll_lock = XSET_B_TRUE;
            else
                set->scroll_lock = XSET_B_UNSET;
            break;
        case XSET_SET_SET_DISABLE:
            if (!strcmp(value, "1"))
                set->disable = true;
            else
                set->disable = false;
            break;
        case XSET_SET_SET_OP:
            set->opener = strtol(value, nullptr, 10);
            break;
        default:
            break;
    }

    return set;
}

XSet*
xset_set(const char* name, const char* var, const char* value)
{
    XSet* set = xset_get(name);
    if (!set->lock || (strcmp(var, "style") && strcmp(var, "desc") && strcmp(var, "title") &&
                       strcmp(var, "shared_key")))
        return xset_set_set_int(set, var, value);
    else
        return set;
}

XSet*
xset_set_panel(int panel, const char* name, const char* var, const char* value)
{
    char* fullname = g_strdup_printf("panel%d_%s", panel, name);
    XSet* set = xset_set(fullname, var, value);
    g_free(fullname);
    return set;
}

XSet*
xset_find_custom(const char* search)
{
    // find a custom command or submenu by label or xset name
    GList* l;

    char* label = clean_label(search, true, false);
    for (l = xsets; l; l = l->next)
    {
        XSet* set = XSET(l->data);
        if (!set->lock && ((set->menu_style == XSET_MENU_SUBMENU && set->child) ||
                           (set->menu_style < XSET_MENU_SUBMENU &&
                            xset_get_int_set(set, "x") <= XSET_CMD_BOOKMARK)))
        {
            // custom submenu or custom command - label or name matches?
            char* str = clean_label(set->menu_label, true, false);
            if (!g_strcmp0(set->name, search) || !g_strcmp0(str, label))
            {
                // match
                g_free(str);
                g_free(label);
                return set;
            }
            g_free(str);
        }
    }
    g_free(label);
    return nullptr;
}

bool
xset_opener(PtkFileBrowser* file_browser, char job)
{ // find an opener for job
    XSet* set;
    XSet* mset;
    XSet* open_all_set;
    XSet* tset;
    XSet* open_all_tset;
    GList* l;
    GList* ll;
    XSetContext* context = nullptr;
    int context_action;
    bool found = false;
    char pinned;

    for (l = xsets; l; l = l->next)
    {
        if (!(XSET(l->data))->lock && (XSET(l->data))->opener == job && !(XSET(l->data))->tool &&
            (XSET(l->data))->menu_style != XSET_MENU_SUBMENU &&
            (XSET(l->data))->menu_style != XSET_MENU_SEP)
        {
            if ((XSET(l->data))->desc && !strcmp((XSET(l->data))->desc, "@plugin@mirror@"))
            {
                // is a plugin mirror
                mset = XSET(l->data);
                set = xset_is(mset->shared_key);
                if (!set)
                    continue;
            }
            else if ((XSET(l->data))->plugin && (XSET(l->data))->shared_key)
            {
                // plugin with mirror - ignore to use mirror's context only
                continue;
            }
            else
                set = mset = XSET(l->data);

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
                char* open_all_name = g_strdup(context->var[CONTEXT_MIME]);
                if (!open_all_name)
                    open_all_name = g_strdup("");
                char* str = open_all_name;
                open_all_name = replace_string(str, "-", "_", false);
                g_free(str);
                str = replace_string(open_all_name, " ", "", false);
                g_free(open_all_name);
                open_all_name = g_strdup_printf("open_all_type_%s", str);
                g_free(str);
                open_all_set = xset_is(open_all_name);
                g_free(open_all_name);
            }

            // test context
            if (mset->context)
            {
                context_action = xset_context_test(context, mset->context, false);
                if (context_action == CONTEXT_HIDE || context_action == CONTEXT_DISABLE)
                    continue;
            }

            // valid custom type?
            int cmd_type = xset_get_int_set(set, "x");
            if (cmd_type != XSET_CMD_APP && cmd_type != XSET_CMD_LINE &&
                cmd_type != XSET_CMD_SCRIPT)
                continue;

            // is set pinned to open_all_type for pre-context?
            pinned = 0;
            for (ll = xsets; ll && !pinned; ll = ll->next)
            {
                if ((XSET(ll->data))->next &&
                    g_str_has_prefix((XSET(ll->data))->name, "open_all_type_"))
                {
                    tset = open_all_tset = XSET(ll->data);
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
                        tset = xset_is(tset->next);
                    }
                }
            }
            if (pinned == 1)
                continue;

            // valid
            found = true;
            set->browser = file_browser;
            char* clean = clean_label(set->menu_label, false, false);
            LOG_INFO("Selected Menu Item '{}' As Handler", clean);
            g_free(clean);
            xset_menu_cb(nullptr, set); // also does custom activate
        }
    }
    return found;
}

static void
write_root_saver(std::string& buf, const char* path, const char* name, const char* var,
                 const char* value)
{
    if (!value)
        return;

    char* save = g_strdup_printf("%s-%s=%s", name, var, value);
    char* qsave = bash_quote(save);
    buf.append(fmt::format("echo {} >>| \"{}\"\n", qsave, path));
    g_free(save);
    g_free(qsave);
}

bool
write_root_settings(std::string& buf, const char* path)
{
    GList* l;
    XSet* set;

    buf.append(fmt::format("\n#save root settings\nmkdir -p {}/spacefm\n"
                           "echo -e '#SpaceFM As-Root Session File\\n\\' >| '{}'\n",
                           SYSCONFDIR,
                           path));

    for (l = xsets; l; l = l->next)
    {
        set = XSET(l->data);
        if (set)
        {
            if (!strcmp(set->name, "root_editor") || !strcmp(set->name, "dev_back_part") ||
                !strcmp(set->name, "dev_rest_file") || !strcmp(set->name, "dev_root_check") ||
                !strcmp(set->name, "dev_root_mount") || !strcmp(set->name, "dev_root_unmount") ||
                !strcmp(set->name, "main_terminal") || !strncmp(set->name, "dev_fmt_", 8) ||
                !strncmp(set->name, "label_cmd_", 8))
            {
                write_root_saver(buf, path, set->name, "s", set->s);
                write_root_saver(buf, path, set->name, "x", set->x);
                write_root_saver(buf, path, set->name, "y", set->y);
                if (set->b != XSET_B_UNSET)
                    buf.append(fmt::format("echo '{}-b={}' >>| \"{}\"\n", set->name, set->b, path));
            }
        }
    }

    buf.append(fmt::format("chmod -R go-w+rX {}/spacefm\n\n", SYSCONFDIR));
    return true;
}

static void
read_root_settings()
{
    GList* l;
    XSet* set;

    if (geteuid() == 0)
        return;

    std::string root_set_path = fmt::format("{}/spacefm/{}-as-root", SYSCONFDIR, g_get_user_name());
    if (!std::filesystem::exists(root_set_path))
        root_set_path = g_strdup_printf("%s/spacefm/%d-as-root", SYSCONFDIR, geteuid());

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
    for (l = xsets; l; l = l->next)
    {
        set = XSET(l->data);
        if (set)
        {
            if (!strcmp(set->name, "root_editor") || !strcmp(set->name, "dev_back_part") ||
                !strcmp(set->name, "dev_rest_file") || !strcmp(set->name, "dev_root_check") ||
                !strcmp(set->name, "dev_root_mount") || !strcmp(set->name, "dev_root_unmount") ||
                !strncmp(set->name, "dev_fmt_", 8) || !strncmp(set->name, "label_cmd_", 8))
            {
                if (set->s)
                {
                    g_free(set->s);
                    set->s = nullptr;
                }
                if (set->x)
                {
                    g_free(set->x);
                    set->x = nullptr;
                }
                if (set->y)
                {
                    g_free(set->y);
                    set->y = nullptr;
                }
                set->b = XSET_B_UNSET;
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
    unsigned int i;
    if (!xset_context)
    {
        xset_context = g_slice_new0(XSetContext);
        xset_context->valid = false;
        for (i = 0; i < G_N_ELEMENTS(xset_context->var); i++)
            xset_context->var[i] = nullptr;
    }
    else
    {
        xset_context->valid = false;
        for (i = 0; i < G_N_ELEMENTS(xset_context->var); i++)
        {
            if (xset_context->var[i])
                g_free(xset_context->var[i]);
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
              char* elements)
{
    if (!elements)
        return;

    while (elements[0] == ' ')
        elements++;

    while (elements && elements[0] != '\0')
    {
        char* space = strchr(elements, ' ');
        if (space)
            space[0] = '\0';
        XSet* set = xset_get(elements);
        if (space)
            space[0] = ' ';
        elements = space;
        xset_add_menuitem(file_browser, menu, accel_group, set);
        if (elements)
        {
            while (elements[0] == ' ')
                elements++;
        }
    }
}

static GtkWidget*
xset_new_menuitem(const char* label, const char* icon)
{
    GtkWidget* item;

    if (label && strstr(label, "\\_"))
    {
        // allow escape of underscore
        char* str = clean_label(label, false, false);
        item = gtk_menu_item_new_with_label(str);
        g_free(str);
    }
    else
        item = gtk_menu_item_new_with_mnemonic(label);
    if (!(icon && icon[0]))
        return item;
    GtkWidget* image = xset_get_image(icon, GTK_ICON_SIZE_MENU);

    return item;
}

char*
xset_custom_get_app_name_icon(XSet* set, GdkPixbuf** icon, int icon_size)
{
    char* menu_label = nullptr;
    VFSAppDesktop* app = nullptr;
    GdkPixbuf* icon_new = nullptr;
    GtkIconTheme* icon_theme = gtk_icon_theme_get_default();

    if (!set->lock && xset_get_int_set(set, "x") == XSET_CMD_APP)
    {
        if (set->z && g_str_has_suffix(set->z, ".desktop") && (app = vfs_app_desktop_new(set->z)))
        {
            if (!(set->menu_label && set->menu_label[0]))
                menu_label = g_strdup(vfs_app_desktop_get_disp_name(app));
            if (set->icon)
                icon_new = vfs_load_icon(icon_theme, set->icon, icon_size);
            if (!icon_new)
                icon_new = vfs_app_desktop_get_icon(app, icon_size, true);
            if (app)
                vfs_app_desktop_unref(app);
        }
        else
        {
            // not a desktop file - probably executable
            if (set->icon)
                icon_new = vfs_load_icon(icon_theme, set->icon, icon_size);
            if (!icon_new && set->z)
            {
                // guess icon name from executable name
                char* name = g_path_get_basename(set->z);
                if (name && name[0])
                    icon_new = vfs_load_icon(icon_theme, name, icon_size);
                g_free(name);
            }
        }

        if (!icon_new)
        {
            // fallback
            icon_new = vfs_load_icon(icon_theme, "gtk-execute", icon_size);
        }
    }
    else
        LOG_WARN("xset_custom_get_app_name_icon set is not XSET_CMD_APP");

    if (icon)
        *icon = icon_new;
    else if (icon_new)
        g_object_unref(icon_new);

    if (!menu_label)
    {
        menu_label =
            set->menu_label && set->menu_label[0] ? g_strdup(set->menu_label) : g_strdup(set->z);
        if (!menu_label)
            menu_label = g_strdup("Application");
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
    GtkIconTheme* icon_theme = gtk_icon_theme_get_default();

    if (!book_icon_set_cached)
        book_icon_set_cached = xset_get("book_icon");

    if (!set->lock && xset_get_int_set(set, "x") == XSET_CMD_BOOKMARK)
    {
        if (!set->icon && (set->z && (strstr(set->z, ":/") || g_str_has_prefix(set->z, "//"))))
        {
            // a bookmarked URL - show network icon
            XSet* set2 = xset_get("dev_icon_network");
            if (set2->icon)
                icon1 = g_strdup(set2->icon);
            else
                icon1 = g_strdup("gtk-network");
            icon2 = g_strdup("user-bookmarks");
            icon3 = g_strdup("gnome-fs-directory");
        }
        else if (set->z && (strstr(set->z, ":/") || g_str_has_prefix(set->z, "//")))
        {
            // a bookmarked URL - show custom or network icon
            icon1 = g_strdup(set->icon);
            XSet* set2 = xset_get("dev_icon_network");
            if (set2->icon)
                icon2 = g_strdup(set2->icon);
            else
                icon2 = g_strdup("gtk-network");
            icon3 = g_strdup("user-bookmarks");
        }
        else if (!set->icon && book_icon_set_cached->icon)
        {
            icon1 = g_strdup(book_icon_set_cached->icon);
            icon2 = g_strdup("user-bookmarks");
            icon3 = g_strdup("gnome-fs-directory");
        }
        else if (set->icon && book_icon_set_cached->icon)
        {
            icon1 = g_strdup(set->icon);
            icon2 = g_strdup(book_icon_set_cached->icon);
            icon3 = g_strdup("user-bookmarks");
        }
        else if (set->icon)
        {
            icon1 = g_strdup(set->icon);
            icon2 = g_strdup("user-bookmarks");
            icon3 = g_strdup("gnome-fs-directory");
        }
        else
        {
            icon1 = g_strdup("user-bookmarks");
            icon2 = g_strdup("gnome-fs-directory");
            icon3 = g_strdup("gtk-directory");
        }
        if (icon1)
            icon_new = vfs_load_icon(icon_theme, icon1, icon_size);
        if (!icon_new && icon2)
            icon_new = vfs_load_icon(icon_theme, icon2, icon_size);
        if (!icon_new && icon3)
            icon_new = vfs_load_icon(icon_theme, icon3, icon_size);
    }
    else
        LOG_WARN("xset_custom_get_bookmark_icon set is not XSET_CMD_BOOKMARK");
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
    int context_action = CONTEXT_SHOW;
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
            g_free(icon_file);
            icon_file = nullptr;
        }
        else
            icon_name = icon_file;
    }
    if (!context)
        context = set->context;

    // context?
    if (context && !set->tool && xset_context && xset_context->valid && !xset_get_b("context_dlg"))
        context_action = xset_context_test(xset_context, context, set->disable);

    if (context_action != CONTEXT_HIDE)
    {
        if (set->tool && set->menu_style != XSET_MENU_SUBMENU)
        {
            // item = xset_new_menuitem( set->menu_label, icon_name );
        }
        else if (set->menu_style)
        {
            XSet* set_radio;
            switch (set->menu_style)
            {
                case XSET_MENU_CHECK:
                    if (!(!set->lock && (xset_get_int_set(set, "x") > XSET_CMD_SCRIPT)))
                    {
                        item = gtk_check_menu_item_new_with_mnemonic(set->menu_label);
                        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),
                                                       mset->b == XSET_B_TRUE);
                    }
                    break;
                case XSET_MENU_RADIO:
                    if (set->ob2_data)
                        set_radio = XSET(set->ob2_data);
                    else
                        set_radio = set;
                    item = gtk_radio_menu_item_new_with_mnemonic((GSList*)set_radio->ob2_data,
                                                                 set->menu_label);
                    set_radio->ob2_data =
                        (void*)gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
                    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),
                                                   mset->b == XSET_B_TRUE);
                    break;
                case XSET_MENU_SUBMENU:
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
                            goto _next_item;
                        }
                    }
                    break;
                case XSET_MENU_SEP:
                    item = gtk_separator_menu_item_new();
                    break;
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
            int cmd_type = xset_get_int_set(set, "x");
            if (!set->lock && cmd_type == XSET_CMD_APP)
            {
                // Application
                char* menu_label = xset_custom_get_app_name_icon(set, &app_icon, icon_size);
                item = xset_new_menuitem(menu_label, nullptr);
                g_free(menu_label);
            }
            else if (!set->lock && cmd_type == XSET_CMD_BOOKMARK)
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
        if (set->menu_style != XSET_MENU_RADIO && set->ob2)
            g_object_set_data(G_OBJECT(item), set->ob2, set->ob2_data);

        if (set->menu_style < XSET_MENU_SUBMENU)
        {
            // activate callback
            if (!set->cb_func || set->menu_style)
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

        gtk_widget_set_sensitive(item, context_action != CONTEXT_DISABLE && !set->disable);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }

_next_item:
    if (icon_file)
        g_free(icon_file);

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
        g_free(path);
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

static char*
xset_custom_new_name()
{
    char* setname;

    do
    {
        char* hex8 = nullptr;
        if (hex8)
            g_free(hex8);
        hex8 = randhex8();
        setname = g_strdup_printf("cstm_%s", hex8);
        if (xset_is(setname))
        {
            g_free(setname);
            setname = nullptr;
        }
        else
        {
            char* path1 = g_build_filename(xset_get_config_dir(), "scripts", setname, nullptr);
            char* path2 = g_build_filename(xset_get_config_dir(), "plugin-data", setname, nullptr);
            if (std::filesystem::exists(path1) || std::filesystem::exists(path2))
            {
                g_free(setname);
                setname = nullptr;
            }
            g_free(path1);
            g_free(path2);
        }
    } while (!setname);
    return setname;
}

static void
xset_custom_copy_files(XSet* src, XSet* dest)
{
    std::string path_src;
    std::string path_dest;
    std::string command;

    char* stdout = nullptr;
    char* stderr = nullptr;
    std::string msg;
    bool ret;
    int exit_status;

    // LOG_INFO("xset_custom_copy_files( {}, {} )", src->name, dest->name);

    // copy command dir

    if (src->plugin)
        path_src = g_build_filename(src->plug_dir, src->plug_name, nullptr);
    else
        path_src = g_build_filename(xset_get_config_dir(), "scripts", src->name, nullptr);
    // LOG_INFO("    path_src={}", path_src);

    // LOG_INFO("    path_src EXISTS");
    path_dest = g_build_filename(xset_get_config_dir(), "scripts", nullptr);
    std::filesystem::create_directories(path_dest);
    std::filesystem::permissions(path_dest, std::filesystem::perms::owner_all);
    path_dest = g_build_filename(xset_get_config_dir(), "scripts", dest->name, nullptr);
    command = fmt::format("cp -a {} {}", path_src, path_dest);

    // LOG_INFO("    path_dest={}", path_dest );
    print_command(command);
    ret = g_spawn_command_line_sync(command.c_str(), &stdout, &stderr, &exit_status, nullptr);
    LOG_INFO("{}{}", stdout, stderr);

    if (!ret || (exit_status && WIFEXITED(exit_status)))
    {
        msg = fmt::format("An error occured copying command files\n\n{}", stderr ? stderr : "");
        xset_msg_dialog(nullptr, GTK_MESSAGE_ERROR, "Copy Command Error", 0, msg.c_str(), nullptr);
    }
    if (stderr)
        g_free(stderr);
    if (stdout)
        g_free(stdout);
    stderr = stdout = nullptr;
    command = fmt::format("chmod -R go-rwx {}", path_dest);
    print_command(command);
    g_spawn_command_line_sync(command.c_str(), nullptr, nullptr, nullptr, nullptr);

    // copy data dir
    XSet* mset = xset_get_plugin_mirror(src);
    path_src = g_build_filename(xset_get_config_dir(), "plugin-data", mset->name, nullptr);
    if (std::filesystem::is_directory(path_src))
    {
        path_dest = g_build_filename(xset_get_config_dir(), "plugin-data", dest->name, nullptr);
        command = fmt::format("cp -a {} {}", path_src, path_dest);
        stderr = stdout = nullptr;
        print_command(command);
        ret = g_spawn_command_line_sync(command.c_str(), &stdout, &stderr, &exit_status, nullptr);
        LOG_INFO("{}{}", stdout, stderr);
        if (!ret || (exit_status && WIFEXITED(exit_status)))
        {
            msg = fmt::format("An error occured copying command data files\n\n{}",
                              stderr ? stderr : "");
            xset_msg_dialog(nullptr,
                            GTK_MESSAGE_ERROR,
                            "Copy Command Error",
                            0,
                            msg.c_str(),
                            nullptr);
        }
        if (stderr)
            g_free(stderr);
        if (stdout)
            g_free(stdout);
        stderr = stdout = nullptr;
        command = fmt::format("chmod -R go-rwx {}", path_dest);
        print_command(command);
        g_spawn_command_line_sync(command.c_str(), nullptr, nullptr, nullptr, nullptr);
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
    newset->menu_label = g_strdup(set->menu_label);
    newset->s = g_strdup(set->s);
    newset->x = g_strdup(set->x);
    newset->y = g_strdup(set->y);
    newset->z = g_strdup(set->z);
    newset->desc = g_strdup(set->desc);
    newset->title = g_strdup(set->title);
    newset->b = set->b;
    newset->menu_style = set->menu_style;
    newset->context = g_strdup(mset->context);
    newset->line = g_strdup(set->line);

    newset->task = mset->task;
    newset->task_pop = mset->task_pop;
    newset->task_err = mset->task_err;
    newset->task_out = mset->task_out;
    newset->in_terminal = mset->in_terminal;
    newset->keep_terminal = mset->keep_terminal;
    newset->scroll_lock = mset->scroll_lock;

    if (!mset->icon && set->plugin)
        newset->icon = g_strdup(set->icon);
    else
        newset->icon = g_strdup(mset->icon);

    xset_custom_copy_files(set, newset);
    newset->tool = set->tool;

    if (set->menu_style == XSET_MENU_SUBMENU && set->child)
    {
        XSet* set_child = xset_get(set->child);
        // LOG_INFO("    copy submenu {}", set_child->name);
        XSet* newchild = xset_custom_copy(set_child, true, delete_set);
        newset->child = g_strdup(newchild->name);
        newchild->parent = g_strdup(newset->name);
    }

    if (copy_next && set->next)
    {
        XSet* set_next = xset_get(set->next);
        // LOG_INFO("    copy next {}", set_next->name);
        XSet* newnext = xset_custom_copy(set_next, true, delete_set);
        newnext->prev = g_strdup(newset->name);
        newset->next = g_strdup(newnext->name);
    }

    // when copying imported plugin file, discard mirror xset
    if (delete_set)
        xset_custom_delete(set, false);

    return newset;
}

void
clean_plugin_mirrors()
{ // remove plugin mirrors for non-existent plugins
    GList* l;
    XSet* set;
    bool redo = true;

    while (redo)
    {
        redo = false;
        for (l = xsets; l; l = l->next)
        {
            if ((XSET(l->data))->desc && !strcmp((XSET(l->data))->desc, "@plugin@mirror@"))
            {
                set = XSET(l->data);
                if (!set->shared_key || !xset_is(set->shared_key))
                {
                    xset_free(set);
                    redo = true;
                    break;
                }
            }
        }
    }

    // remove plugin-data for non-existent xsets
    const char* name;
    char* command;
    char* stdout;
    char* stderr;
    GDir* dir;
    char* path = g_build_filename(xset_get_config_dir(), "plugin-data", nullptr);
_redo:
    dir = g_dir_open(path, 0, nullptr);
    if (dir)
    {
        while ((name = g_dir_read_name(dir)))
        {
            if (strlen(name) == 13 && g_str_has_prefix(name, "cstm_") && !xset_is(name))
            {
                g_dir_close(dir);
                // FIXME - this and all the of other 'rm -rf' look like a bad idea
                std::string command = fmt::format("rm -rf {}/{}", path, name);
                stderr = stdout = nullptr;
                print_command(command);
                g_spawn_command_line_sync(command.c_str(), nullptr, nullptr, nullptr, nullptr);
                if (stderr)
                    g_free(stderr);
                if (stdout)
                    g_free(stdout);
                goto _redo;
            }
        }
        g_dir_close(dir);
    }
    g_free(path);
}

static void
xset_set_plugin_mirror(XSet* pset)
{
    XSet* set;
    GList* l;

    for (l = xsets; l; l = l->next)
    {
        if ((XSET(l->data))->desc && !strcmp((XSET(l->data))->desc, "@plugin@mirror@"))
        {
            set = XSET(l->data);
            if (set->parent && set->child)
            {
                if (!strcmp(set->child, pset->plug_name) && !strcmp(set->parent, pset->plug_dir))
                {
                    if (set->shared_key)
                        g_free(set->shared_key);
                    set->shared_key = g_strdup(pset->name);
                    if (pset->shared_key)
                        g_free(pset->shared_key);
                    pset->shared_key = g_strdup(set->name);
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
    newset->desc = g_strdup("@plugin@mirror@");
    newset->parent = g_strdup(set->plug_dir);
    newset->child = g_strdup(set->plug_name);
    newset->shared_key = g_strdup(set->name); // this will not be saved
    newset->task = set->task;
    newset->task_pop = set->task_pop;
    newset->task_err = set->task_err;
    newset->task_out = set->task_out;
    newset->in_terminal = set->in_terminal;
    newset->keep_terminal = set->keep_terminal;
    newset->scroll_lock = set->scroll_lock;
    newset->context = g_strdup(set->context);
    newset->opener = set->opener;
    newset->b = set->b;
    newset->s = g_strdup(set->s);
    set->shared_key = g_strdup(newset->name);
    return newset;
}

static int
compare_plugin_sets(XSet* a, XSet* b)
{
    return g_utf8_collate(a->menu_label, b->menu_label);
}

GList*
xset_get_plugins(bool included)
{ // return list of plugin sets (included or not ) sorted by menu_label
    GList* l;
    GList* plugins = nullptr;
    XSet* set;

    for (l = xsets; l; l = l->next)
    {
        if ((XSET(l->data))->plugin && (XSET(l->data))->plugin_top && (XSET(l->data))->plug_dir)
        {
            set = XSET(l->data);
            if (strstr(set->plug_dir, "/included/"))
            {
                if (included)
                    plugins = g_list_prepend(plugins, l->data);
            }
            else if (!included)
                plugins = g_list_prepend(plugins, l->data);
        }
    }
    plugins = g_list_sort(plugins, (GCompareFunc)compare_plugin_sets);
    return plugins;
}

static XSet*
xset_get_by_plug_name(const char* plug_dir, const char* plug_name)
{
    GList* l;
    if (!plug_name)
        return nullptr;

    for (l = xsets; l; l = l->next)
    {
        if ((XSET(l->data))->plugin && !strcmp(plug_name, (XSET(l->data))->plug_name) &&
            !strcmp(plug_dir, (XSET(l->data))->plug_dir))
            return XSET(l->data);
    }

    // add new
    XSet* set = xset_new(xset_custom_new_name());
    set->plug_dir = g_strdup(plug_dir);
    set->plug_name = g_strdup(plug_name);
    set->plugin = true;
    set->lock = false;
    xsets = g_list_append(xsets, set);
    return set;
}

static void
xset_parse_plugin(const char* plug_dir, const char* line, int use)
{
    char* sep = strchr(const_cast<char*>(line), '=');
    const char* name;
    char* value;
    XSet* set;
    XSet* set2;
    const char* prefix;
    const char* handler_prefix[] = {"hand_arc_", "hand_fs_", "hand_net_", "hand_f_"};

    if (!sep)
        return;
    name = line;
    value = sep + 1;
    *sep = '\0';
    sep = strchr(const_cast<char*>(name), '-');
    if (!sep)
        return;
    char* var = sep + 1;
    *sep = '\0';

    if (use < PLUGIN_USE_BOOKMARKS)
    {
        // handler
        prefix = handler_prefix[use];
    }
    else
        prefix = g_strdup("cstm_");

    if (g_str_has_prefix(name, prefix))
    {
        set = xset_get_by_plug_name(plug_dir, name);
        xset_set_set_int(set, var, value);

        if (use >= PLUGIN_USE_BOOKMARKS)
        {
            // map plug names to new set names (does not apply to handlers)
            if (set->prev && !strcmp(var, "prev"))
            {
                if (!strncmp(set->prev, "cstm_", 5))
                {
                    set2 = xset_get_by_plug_name(plug_dir, set->prev);
                    g_free(set->prev);
                    set->prev = g_strdup(set2->name);
                }
                else
                {
                    g_free(set->prev);
                    set->prev = nullptr;
                }
            }
            else if (set->next && !strcmp(var, "next"))
            {
                if (!strncmp(set->next, "cstm_", 5))
                {
                    set2 = xset_get_by_plug_name(plug_dir, set->next);
                    g_free(set->next);
                    set->next = g_strdup(set2->name);
                }
                else
                {
                    g_free(set->next);
                    set->next = nullptr;
                }
            }
            else if (set->parent && !strcmp(var, "parent"))
            {
                if (!strncmp(set->parent, "cstm_", 5))
                {
                    set2 = xset_get_by_plug_name(plug_dir, set->parent);
                    g_free(set->parent);
                    set->parent = g_strdup(set2->name);
                }
                else
                {
                    g_free(set->parent);
                    set->parent = nullptr;
                }
            }
            else if (set->child && !strcmp(var, "child"))
            {
                if (!strncmp(set->child, "cstm_", 5))
                {
                    set2 = xset_get_by_plug_name(plug_dir, set->child);
                    g_free(set->child);
                    set->child = g_strdup(set2->name);
                }
                else
                {
                    g_free(set->child);
                    set->child = nullptr;
                }
            }
        }
    }
}

XSet*
xset_import_plugin(const char* plug_dir, int* use)
{
    bool func;
    GList* l;
    XSet* set;

    if (use)
        *use = PLUGIN_USE_NORMAL;

    // clear all existing plugin sets with this plug_dir
    // ( keep the mirrors to retain user prefs )
    bool redo = true;
    while (redo)
    {
        redo = false;
        for (l = xsets; l; l = l->next)
        {
            if ((XSET(l->data))->plugin && !strcmp(plug_dir, (XSET(l->data))->plug_dir))
            {
                xset_free(XSET(l->data));
                redo = true; // search list from start again due to changed list
                break;
            }
        }
    }

    // read plugin file into xsets
    bool plugin_good = false;
    std::string plugin = g_build_filename(plug_dir, "plugin", nullptr);

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
                if (!line.compare("[Plugin]"))
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

                if (use && *use == PLUGIN_USE_NORMAL)
                {
                    if (!token.compare("main_book-child"))
                    {
                        // This plugin is an export of all bookmarks
                        *use = PLUGIN_USE_BOOKMARKS;
                    }
                    else if (token.substr(0, 5) == "hand_")
                    {
                        if (token.substr(0, 8) == "hand_fs_")
                            *use = PLUGIN_USE_HAND_FS;
                        else if (token.substr(0, 9) == "hand_arc_")
                            *use = PLUGIN_USE_HAND_ARC;
                        else if (token.substr(0, 9) == "hand_net_")
                            *use = PLUGIN_USE_HAND_NET;
                        else if (token.substr(0, 7) == "hand_f_")
                            *use = PLUGIN_USE_HAND_FILE;
                    }
                }
                xset_parse_plugin(plug_dir, token.c_str(), use ? *use : PLUGIN_USE_NORMAL);
                if (!plugin_good)
                    plugin_good = true;
            }
        }
    }

    file.close();

    // clean plugin sets, set type
    bool top = true;
    XSet* rset = nullptr;
    for (l = xsets; l; l = l->next)
    {
        if ((XSET(l->data))->plugin && !strcmp(plug_dir, (XSET(l->data))->plug_dir))
        {
            set = XSET(l->data);
            set->key = set->keymod = set->tool = set->opener = 0;
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
    int job;
};

static void
on_install_plugin_cb(VFSFileTask* task, PluginData* plugin_data)
{
    XSet* set;
    char* msg;
    // LOG_INFO("on_install_plugin_cb");
    if (plugin_data->job == PLUGIN_JOB_REMOVE) // uninstall
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
            int use = PLUGIN_USE_NORMAL;
            set = xset_import_plugin(plugin_data->plug_dir, &use);
            if (!set)
            {
                msg = g_strdup_printf(
                    "The imported plugin directory does not contain a valid plugin.\n\n(%s/)",
                    plugin_data->plug_dir);
                xset_msg_dialog(GTK_WIDGET(plugin_data->main_window),
                                GTK_MESSAGE_ERROR,
                                "Invalid Plugin",
                                0,
                                msg,
                                nullptr);
                g_free(msg);
            }
            // TODO - switch
            else if (use == PLUGIN_USE_BOOKMARKS)
            {
                // bookmarks
                if (plugin_data->job != PLUGIN_JOB_COPY || !plugin_data->set)
                {
                    // This dialog should never be seen - failsafe
                    xset_msg_dialog(
                        GTK_WIDGET(plugin_data->main_window),
                        GTK_MESSAGE_ERROR,
                        "Bookmarks",
                        0,
                        "This plugin file contains exported bookmarks which cannot be installed or "
                        "imported to the design clipboard.\n\nYou can import these directly into a "
                        "menu (select New|Import from the Design Menu).",
                        nullptr);
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
                        if (plugin_data->set->tool)
                            set->tool = XSET_TOOL_CUSTOM;
                        else
                            set->tool = XSET_TOOL_NOT;
                        if (!set->next)
                            break;
                    } while ((set = xset_get(set->next)));
                    // set now points to last bookmark
                    newset->prev = g_strdup(plugin_data->set->name);
                    set->next = plugin_data->set->next; // steal
                    if (plugin_data->set->next)
                    {
                        XSet* set_next = xset_get(plugin_data->set->next);
                        g_free(set_next->prev);
                        set_next->prev = g_strdup(set->name); // last bookmark
                    }
                    plugin_data->set->next = g_strdup(newset->name);
                    // find parent
                    set = newset;
                    while (set->prev)
                        set = xset_get(set->prev);
                    if (set->parent)
                        main_window_bookmark_changed(set->parent);
                }
            }
            else if (use < PLUGIN_USE_BOOKMARKS)
            {
                // handler
                set->plugin_top = false; // prevent being added to Plugins menu
                if (plugin_data->job == PLUGIN_JOB_INSTALL)
                {
                    // This dialog should never be seen - failsafe
                    xset_msg_dialog(plugin_data->main_window ? GTK_WIDGET(plugin_data->main_window)
                                                             : nullptr,
                                    GTK_MESSAGE_ERROR,
                                    "Handler Plugin",
                                    0,
                                    "This file contains a handler plugin which cannot be installed "
                                    "as a plugin.\n\nYou can import handlers from a handler "
                                    "configuration window, or use Plugins|Import.",
                                    nullptr);
                }
                else
                    ptk_handler_import(use, plugin_data->handler_dlg, set);
            }
            else if (plugin_data->job == PLUGIN_JOB_COPY)
            {
                // copy
                set->plugin_top = false; // don't show tmp plugin in Plugins menu
                if (plugin_data->set)
                {
                    // paste after insert_set (plugin_data->set)
                    XSet* newset = xset_custom_copy(set, false, true);
                    newset->prev = g_strdup(plugin_data->set->name);
                    newset->next = plugin_data->set->next; // steal
                    if (plugin_data->set->next)
                    {
                        XSet* set_next = xset_get(plugin_data->set->next);
                        g_free(set_next->prev);
                        set_next->prev = g_strdup(newset->name);
                    }
                    plugin_data->set->next = g_strdup(newset->name);
                    if (plugin_data->set->tool)
                        newset->tool = XSET_TOOL_CUSTOM;
                    else
                        newset->tool = XSET_TOOL_NOT;
                    main_window_bookmark_changed(newset->name);
                }
                else
                {
                    // place on design clipboard
                    set_clipboard = set;
                    clipboard_is_cut = false;
                    if (xset_get_b("plug_cverb") || plugin_data->handler_dlg)
                    {
                        char* label = clean_label(set->menu_label, false, false);
                        if (geteuid() == 0)
                            msg = g_strdup_printf(
                                "The '%s' plugin has been copied to the design clipboard.  Use "
                                "View|Design Mode to paste it into a menu.\n\nBecause it has not "
                                "been installed, this plugin will not appear in the Plugins "
                                "menu.",
                                label);
                        else
                            msg = g_strdup_printf(
                                "The '%s' plugin has been copied to the design clipboard.  Use "
                                "View|Design Mode to paste it into a menu.\n\nBecause it has not "
                                "been installed, this plugin will not appear in the Plugins "
                                "menu, and its contents are not protected by root (once pasted "
                                "it will be saved with normal ownership).\n\nIf this plugin "
                                "contains su commands or will be run as root, installing it to "
                                "and running it only from the Plugins menu is recommended to "
                                "improve your system security.",
                                label);
                        g_free(label);
                        xset_msg_dialog(GTK_WIDGET(plugin_data->main_window),
                                        0,
                                        "Copy Plugin",
                                        0,
                                        msg,
                                        nullptr);
                        g_free(msg);
                    }
                }
            }
            clean_plugin_mirrors();
        }
        g_free(plugin);
    }
    g_free(plugin_data->plug_dir);
    g_slice_free(PluginData, plugin_data);
}

static void
xset_remove_plugin(GtkWidget* parent, PtkFileBrowser* file_browser, XSet* set)
{
    char* msg;

    if (!file_browser || !set || !set->plugin_top || !set->plug_dir)
        return;

    if (strstr(set->plug_dir, "/included/"))
        return; // failsafe - don't allow removal of included

    if (!app_settings.no_confirm)
    {
        char* label = clean_label(set->menu_label, false, false);
        msg = g_strdup_printf("Uninstall the '%s' plugin?\n\n( %s )", label, set->plug_dir);
        g_free(label);
        if (xset_msg_dialog(parent,
                            GTK_MESSAGE_WARNING,
                            "Uninstall Plugin",
                            GTK_BUTTONS_YES_NO,
                            msg,
                            nullptr) != GTK_RESPONSE_YES)
        {
            g_free(msg);
            return;
        }
        g_free(msg);
    }
    PtkFileTask* task =
        ptk_file_exec_new("Uninstall Plugin", nullptr, parent, file_browser->task_view);

    char* plug_dir_q = bash_quote(set->plug_dir);

    task->task->exec_command = g_strdup_printf("rm -rf %s", plug_dir_q);
    g_free(plug_dir_q);
    task->task->exec_sync = true;
    task->task->exec_popup = false;
    task->task->exec_show_output = false;
    task->task->exec_show_error = true;
    task->task->exec_export = false;
    task->task->exec_as_user = g_strdup("root");

    PluginData* plugin_data = g_slice_new0(PluginData);
    plugin_data->main_window = nullptr;
    plugin_data->plug_dir = g_strdup(set->plug_dir);
    plugin_data->set = set;
    plugin_data->job = PLUGIN_JOB_REMOVE;
    task->complete_notify = (GFunc)on_install_plugin_cb;
    task->user_data = plugin_data;

    ptk_file_task_run(task);
}

void
install_plugin_file(void* main_win, GtkWidget* handler_dlg, const char* path, const char* plug_dir,
                    int job, XSet* insert_set)
{
    char* file_path;
    char* file_path_q;
    char* own;
    char* rem = g_strdup("");
    const char* compression = g_strdup("z");

    FMMainWindow* main_window = static_cast<FMMainWindow*>(main_win);
    // task
    PtkFileTask* task = ptk_file_exec_new("Install Plugin",
                                          nullptr,
                                          main_win ? GTK_WIDGET(main_window) : nullptr,
                                          main_win ? main_window->task_view : nullptr);

    char* plug_dir_q = bash_quote(plug_dir);
    file_path_q = bash_quote(path);

    switch (job)
    {
        case PLUGIN_JOB_INSTALL:
            // install
            own = g_strdup_printf("chown -R root:root %s && chmod -R go+rX-w %s",
                                  plug_dir_q,
                                  plug_dir_q);
            task->task->exec_as_user = g_strdup("root");
            break;
        case PLUGIN_JOB_COPY:
            // copy to clipboard or import to menu
            own = g_strdup_printf("chmod -R go+rX-w %s", plug_dir_q);
            break;
        default:
            break;
    }

    const char* book = g_strdup("");
    if (insert_set && !strcmp(insert_set->name, "main_book"))
    {
        // import bookmarks to end
        XSet* set = xset_get("main_book");
        set = xset_is(set->child);
        while (set && set->next)
            set = xset_is(set->next);
        if (set)
            insert_set = set;
        else
            insert_set = nullptr; // failsafe
    }
    if (job == PLUGIN_JOB_INSTALL || !insert_set)
    {
        // prevent install of exported bookmarks or handler as plugin or design clipboard
        if (job == PLUGIN_JOB_INSTALL)
            book = g_strdup(" || [ -e main_book ] || [ -d hand_* ]");
        else
            book = g_strdup(" || [ -e main_book ]");
    }

    task->task->exec_command = g_strdup_printf(
        "rm -rf %s ; mkdir -p %s && cd %s && tar --exclude='/*' --keep-old-files -xf %s ; "
        "err=$?; if [ $err -ne 0 ] || [ ! -e plugin ]%s;then rm -rf %s ; echo 'Error installing "
        "plugin (invalid plugin file?)'; exit 1 ; fi ; %s %s",
        plug_dir_q,
        plug_dir_q,
        plug_dir_q,
        file_path_q,
        book,
        plug_dir_q,
        own,
        rem);
    g_free(plug_dir_q);
    g_free(file_path_q);
    g_free(own);
    g_free(rem);
    task->task->exec_sync = true;
    task->task->exec_popup = false;
    task->task->exec_show_output = false;
    task->task->exec_show_error = true;
    task->task->exec_export = false;

    PluginData* plugin_data = g_slice_new0(PluginData);
    plugin_data->main_window = main_window;
    plugin_data->handler_dlg = handler_dlg;
    plugin_data->plug_dir = g_strdup(plug_dir);
    plugin_data->job = job;
    plugin_data->set = insert_set;
    task->complete_notify = (GFunc)on_install_plugin_cb;
    task->user_data = plugin_data;

    ptk_file_task_run(task);
}

static bool
xset_custom_export_files(XSet* set, char* plug_dir)
{
    char* path_src;
    char* path_dest;
    char* stdout = nullptr;
    char* stderr = nullptr;

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
                g_free(path_src);
                g_free(path_dest);
                return false;
            }
        }
        // skip empty or missing dirs
        g_free(path_src);
        g_free(path_dest);
        return true;
    }

    std::string command = fmt::format("cp -a {} {}", path_src, path_dest);
    g_free(path_src);
    g_free(path_dest);
    print_command(command);
    bool ret = g_spawn_command_line_sync(command.c_str(), nullptr, nullptr, nullptr, nullptr);
    if (stderr)
        g_free(stderr);
    if (stdout)
        g_free(stdout);

    return ret;
}

static bool
xset_custom_export_write(std::string& buf, XSet* set, char* plug_dir)
{ // recursively write set, submenu sets, and next sets
    xset_write_set(buf, set);
    if (!xset_custom_export_files(set, plug_dir))
        return false;
    if (set->menu_style == XSET_MENU_SUBMENU && set->child)
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
    char* deffile;

    // get new plugin filename
    XSet* save = xset_get("plug_cfile");
    if (save->s) //&& std::filesystem::is_directory(save->s)
        deffolder = save->s;
    else
    {
        if (!(deffolder = xset_get_s("go_set_default")))
            deffolder = g_strdup("/");
    }

    if (!set->plugin)
    {
        char* s1 = clean_label(set->menu_label, true, false);
        char* s2 = plain_ascii_name(s1);
        if (s2[0] == '\0')
        {
            g_free(s2);
            s2 = g_strdup("Plugin");
        }
        char* type;
        if (!strcmp(set->name, "main_book"))
            type = g_strdup_printf("bookmarks");
        else if (g_str_has_prefix(set->name, "hand_arc_"))
            type = g_strdup_printf("archive-handler");
        else if (g_str_has_prefix(set->name, "hand_fs_"))
            type = g_strdup_printf("device-handler");
        else if (g_str_has_prefix(set->name, "hand_net_"))
            type = g_strdup_printf("protocol-handler");
        else if (g_str_has_prefix(set->name, "hand_f_"))
            type = g_strdup_printf("file-handler");
        else
            type = g_strdup_printf("plugin");

        deffile = g_strdup_printf("%s.spacefm-%s.tar.xz", s2, type);

        g_free(s1);
        g_free(s2);
        g_free(type);
    }
    else
    {
        char* s1 = g_path_get_basename(set->plug_dir);
        deffile = g_strdup_printf("%s.spacefm-plugin.tar.xz", s1);
        g_free(s1);
    }

    char* path = xset_file_dialog(parent,
                                  GTK_FILE_CHOOSER_ACTION_SAVE,
                                  "Save As Plugin File",
                                  deffolder,
                                  deffile);
    g_free(deffile);
    if (!path)
        return;
    if (save->s)
        g_free(save->s);
    save->s = g_path_get_dirname(path);

    // get or create tmp plugin dir
    char* plug_dir = nullptr;
    if (!set->plugin)
    {
        char* s1 = (char*)xset_get_user_tmp_dir();
        if (!s1)
            goto _export_error;
        while (!plug_dir || std::filesystem::exists(plug_dir))
        {
            char* hex8 = randhex8();
            if (plug_dir)
                g_free(plug_dir);
            plug_dir = g_build_filename(s1, hex8, nullptr);
            g_free(hex8);
        }
        std::filesystem::create_directories(plug_dir);
        std::filesystem::permissions(plug_dir, std::filesystem::perms::owner_all);

        // Create plugin file
        char* plugin_path = g_build_filename(plug_dir, "plugin", nullptr);

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

        if (!xset_custom_export_files(set, plug_dir))
            goto _rmtmp_error;
        if (set->menu_style == XSET_MENU_SUBMENU && set->child)
        {
            if (!xset_custom_export_write(buf, xset_get(set->child), plug_dir))
                goto _rmtmp_error;
        }
        buf.append("\n");

        std::ofstream file(path);
        if (file.is_open())
            file << buf;
        file.close();

        g_free(plugin_path);
    }
    else
        plug_dir = g_strdup(set->plug_dir);

    // tar and delete tmp files
    // task
    PtkFileTask* task;
    task = ptk_file_exec_new("Export Plugin",
                             plug_dir,
                             parent,
                             file_browser ? file_browser->task_view : nullptr);
    char* plug_dir_q;
    char* path_q;
    plug_dir_q = bash_quote(plug_dir);
    path_q = bash_quote(path);
    if (!set->plugin)
        task->task->exec_command =
            g_strdup_printf("tar --numeric-owner -cJf %s * ; err=$? ; rm -rf %s ; if [ $err -ne 0 "
                            "];then rm -f %s; fi; exit $err",
                            path_q,
                            plug_dir_q,
                            path_q);
    else
        task->task->exec_command = g_strdup_printf("tar --numeric-owner -cJf %s * ; err=$? ; if [ "
                                                   "$err -ne 0 ];then rm -f %s; fi; exit $err",
                                                   path_q,
                                                   path_q);
    g_free(plug_dir_q);
    g_free(path_q);
    task->task->exec_sync = true;
    task->task->exec_popup = false;
    task->task->exec_show_output = false;
    task->task->exec_show_error = true;
    task->task->exec_export = false;
    task->task->exec_browser = file_browser;
    ptk_file_task_run(task);

    g_free(path);
    g_free(plug_dir);
    return;

_rmtmp_error:
    if (!set->plugin)
    {
        std::string command = fmt::format("rm -rf {}", bash_quote(plug_dir));
        print_command(command);
        g_spawn_command_line_sync(command.c_str(), nullptr, nullptr, nullptr, nullptr);
    }
_export_error:
    g_free(plug_dir);
    g_free(path);
    xset_msg_dialog(parent,
                    GTK_MESSAGE_ERROR,
                    "Export Error",
                    0,
                    "Unable to create temporary files",
                    nullptr);
}

static void
open_spec(PtkFileBrowser* file_browser, const char* url, bool in_new_tab)
{
    char* tilde_url = nullptr;
    const char* use_url;

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
    if (g_str_has_prefix(url, "~/") || !g_strcmp0(url, "~"))
        use_url = tilde_url = g_strdup_printf("%s%s", vfs_user_home_dir(), url + 1);
    else
        use_url = url;

    if ((use_url[0] != '/' && strstr(use_url, ":/")) || g_str_has_prefix(use_url, "//"))
    {
        // network
        if (file_browser)
            ptk_location_view_mount_network(file_browser, use_url, new_tab, false);
        else
            open_in_prog(use_url);
    }
    else if (std::filesystem::is_directory(use_url))
    {
        // dir
        if (file_browser)
        {
            if (new_tab || g_strcmp0(ptk_file_browser_get_cwd(file_browser), use_url))
                ptk_file_browser_emit_open(file_browser,
                                           use_url,
                                           new_tab ? PTK_OPEN_NEW_TAB : PTK_OPEN_DIR);
            gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view));
        }
        else
            open_in_prog(use_url);
    }
    else if (std::filesystem::exists(use_url))
    {
        // file - open dir and select file
        char* dir = g_path_get_dirname(use_url);
        if (dir && std::filesystem::is_directory(dir))
        {
            if (file_browser)
            {
                if (!new_tab && !strcmp(dir, ptk_file_browser_get_cwd(file_browser)))
                {
                    ptk_file_browser_select_file(file_browser, use_url);
                    gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view));
                }
                else
                {
                    file_browser->select_path = g_strdup(use_url);
                    ptk_file_browser_emit_open(file_browser,
                                               dir,
                                               new_tab ? PTK_OPEN_NEW_TAB : PTK_OPEN_DIR);
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
                            file_browser->select_path = g_strdup(use_url);
                            // usually this is not ready but try anyway
                            ptk_file_browser_select_file(file_browser, use_url);
                        }
                    }
                }
            }
            else
                open_in_prog(dir);
        }
        g_free(dir);
    }
    else
    {
        char* msg = g_strdup_printf("Bookmark target '%s' is missing or invalid.", use_url);
        xset_msg_dialog(file_browser ? GTK_WIDGET(file_browser) : nullptr,
                        GTK_MESSAGE_ERROR,
                        "Invalid Bookmark Target",
                        0,
                        msg,
                        nullptr);
        g_free(msg);
    }
    g_free(tilde_url);
}

static void
xset_custom_activate(GtkWidget* item, XSet* set)
{
    GtkWidget* parent;
    GtkWidget* task_view = nullptr;
    const char* cwd;
    char* command;
    char* value = nullptr;
    XSet* mset;

    // builtin toolitem?
    if (set->tool > XSET_TOOL_CUSTOM)
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
    if (!set->plugin &&
        !(!set->lock && xset_get_int_set(set, "x") > XSET_CMD_SCRIPT /*app or bookmark*/))
    {
        if (!(set->menu_label && set->menu_label[0]) ||
            (set->menu_label && !strcmp(set->menu_label, "New _Command")))
        {
            if (!xset_text_dialog(parent,
                                  "Change Item Name",
                                  false,
                                  enter_menu_name_new,
                                  nullptr,
                                  set->menu_label,
                                  &set->menu_label,
                                  nullptr,
                                  false))
                return;
        }
    }

    // variable value
    switch (set->menu_style)
    {
        case XSET_MENU_CHECK:
            value = g_strdup_printf("%d", mset->b == XSET_B_TRUE ? 1 : 0);
            break;
        case XSET_MENU_STRING:
            value = g_strdup(mset->s);
            break;
        default:
            value = g_strdup(set->menu_label);
            break;
    }

    // is not activatable command?
    if (!(!set->lock && set->menu_style < XSET_MENU_SUBMENU))
    {
        xset_item_prop_dlg(xset_context, set, 0);
        return;
    }

    // command
    bool app_no_sync = false;
    int cmd_type = xset_get_int_set(set, "x");
    switch (cmd_type)
    {
        case XSET_CMD_LINE:
            // line
            if (!set->line || set->line[0] == '\0')
            {
                xset_item_prop_dlg(xset_context, set, 2);
                return;
            }
            command = replace_line_subs(set->line);
            char* str;
            str = replace_string(command, "\\n", "\n", false);
            g_free(command);
            command = replace_string(str, "\\t", "\t", false);
            g_free(str);
            break;
        case XSET_CMD_SCRIPT:
            // script
            command = xset_custom_get_script(set, false);
            if (!command)
                return;
            break;
        case XSET_CMD_APP:
            // app or executable
            if (!(set->z && set->z[0]))
            {
                xset_item_prop_dlg(xset_context, set, 0);
                return;
            }
            else if (g_str_has_suffix(set->z, ".desktop"))
            {
                VFSAppDesktop* app = vfs_app_desktop_new(set->z);
                if (app && app->exec && app->exec[0] != '\0')
                {
                    // get file list
                    GList* sel_files;
                    GdkScreen* screen;
                    if (set->browser)
                    {
                        sel_files = ptk_file_browser_get_selected_files(set->browser);
                        screen = gtk_widget_get_screen(GTK_WIDGET(set->browser));
                    }
                    else
                    {
                        sel_files = nullptr;
                        cwd = g_strdup("/");
                        screen = gdk_screen_get_default();
                    }
                    GList* file_paths = nullptr;
                    GList* l;
                    for (l = sel_files; l; l = l->next)
                    {
                        file_paths =
                            g_list_prepend(file_paths,
                                           g_build_filename(cwd,
                                                            vfs_file_info_get_name(
                                                                static_cast<VFSFileInfo*>(l->data)),
                                                            nullptr));
                    }
                    file_paths = g_list_reverse(file_paths);

                    // open in app
                    GError* err = nullptr;
                    if (!vfs_app_desktop_open_files(screen, cwd, app, file_paths, &err))
                    {
                        ptk_show_error(parent ? GTK_WINDOW(parent) : nullptr,
                                       "Error",
                                       err->message);
                        g_error_free(err);
                    }
                    if (sel_files)
                    {
                        g_list_foreach(sel_files, (GFunc)vfs_file_info_unref, nullptr);
                        g_list_free(sel_files);
                    }
                    if (file_paths)
                    {
                        g_list_foreach(file_paths, (GFunc)g_free, nullptr);
                        g_list_free(file_paths);
                    }
                }
                if (app)
                    vfs_app_desktop_unref(app);
                return;
            }
            else
            {
                command = bash_quote(set->z);
                app_no_sync = true;
            }
            break;
        case XSET_CMD_BOOKMARK:
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
                    url = g_strdup(specs);
                    url = g_strstrip(url);
                    if (url[0])
                        open_spec(set->browser, url, true);
                    g_free(url);
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
        default:
            return;
    }

    // task
    char* task_name = clean_label(set->menu_label, false, false);
    PtkFileTask* task = ptk_file_exec_new(task_name, cwd, parent, task_view);
    g_free(task_name);
    // don't free cwd!
    task->task->exec_browser = set->browser;
    task->task->exec_command = command;
    task->task->exec_set = set;

    if (set->y && set->y[0] != '\0')
        task->task->exec_as_user = g_strdup(set->y);

    if (set->plugin && set->shared_key && mset->icon)
        task->task->exec_icon = g_strdup(mset->icon);
    if (!task->task->exec_icon && set->icon)
        task->task->exec_icon = g_strdup(set->icon);

    task->task->current_dest = value; // temp storage
    task->task->exec_terminal = (mset->in_terminal == XSET_B_TRUE);
    task->task->exec_keep_terminal = (mset->keep_terminal == XSET_B_TRUE);
    task->task->exec_sync = !app_no_sync && (mset->task == XSET_B_TRUE);
    task->task->exec_popup = (mset->task_pop == XSET_B_TRUE);
    task->task->exec_show_output = (mset->task_out == XSET_B_TRUE);
    task->task->exec_show_error = (mset->task_err == XSET_B_TRUE);
    task->task->exec_scroll_lock = (mset->scroll_lock == XSET_B_TRUE);
    task->task->exec_checksum = set->plugin;
    task->task->exec_export = true;
    // task->task->exec_keep_tmp = true;

    ptk_file_task_run(task);
}

void
xset_custom_delete(XSet* set, bool delete_next)
{
    std::string command;

    if (set->menu_style == XSET_MENU_SUBMENU && set->child)
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

    char* path1 = g_build_filename(xset_get_config_dir(), "scripts", set->name, nullptr);
    char* path2 = g_build_filename(xset_get_config_dir(), "plugin-data", set->name, nullptr);
    if (std::filesystem::exists(path1) || std::filesystem::exists(path2))
        command = fmt::format("rm -rf {} {}", path1, path2);

    g_free(path1);
    g_free(path2);
    if (!command.empty())
    {
        print_command(command);
        g_spawn_command_line_sync(command.c_str(), nullptr, nullptr, nullptr, nullptr);
    }
    xset_free(set);
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
            g_free(set_prev->next);
        if (set->next)
            set_prev->next = g_strdup(set->next);
        else
            set_prev->next = nullptr;
    }
    if (set->next)
    {
        set_next = xset_get(set->next);
        if (set_next->prev)
            g_free(set_next->prev);
        if (set->prev)
            set_next->prev = g_strdup(set->prev);
        else
        {
            set_next->prev = nullptr;
            if (set->parent)
            {
                set_parent = xset_get(set->parent);
                if (set_parent->child)
                    g_free(set_parent->child);
                set_parent->child = g_strdup(set_next->name);
                set_next->parent = g_strdup(set->parent);
            }
        }
    }
    if (!set->prev && !set->next && set->parent)
    {
        set_parent = xset_get(set->parent);
        if (set->tool)
            set_child = xset_new_builtin_toolitem(XSET_TOOL_HOME);
        else
        {
            set_child = xset_custom_new();
            set_child->menu_label = g_strdup("New _Command");
        }
        if (set_parent->child)
            g_free(set_parent->child);
        set_parent->child = g_strdup(set_child->name);
        set_child->parent = g_strdup(set->parent);
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
        g_free(set->parent);
        set->parent = nullptr;
    }

    g_free(set->prev);
    g_free(set->next);
    set->prev = g_strdup(target->name);
    set->next = target->next; // steal string
    if (target->next)
    {
        target_next = xset_get(target->next);
        if (target_next->prev)
            g_free(target_next->prev);
        target_next->prev = g_strdup(set->name);
    }
    target->next = g_strdup(set->name);
    if (target->tool)
    {
        if (set->tool < XSET_TOOL_CUSTOM)
            set->tool = XSET_TOOL_CUSTOM;
    }
    else
    {
        if (set->tool > XSET_TOOL_CUSTOM)
            LOG_WARN("xset_custom_insert_after builtin tool inserted after non-tool");
        set->tool = XSET_TOOL_NOT;
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
    char* setname;
    XSet* set;

    setname = xset_custom_new_name();

    // create set
    set = xset_get(setname);
    g_free(setname);
    set->lock = false;
    set->keep_terminal = XSET_B_TRUE;
    set->task = XSET_B_TRUE;
    set->task_err = XSET_B_TRUE;
    set->task_out = XSET_B_TRUE;
    return set;
}

void
xset_edit(GtkWidget* parent, const char* path, bool force_root, bool no_root)
{
    bool as_root = false;
    bool terminal;
    char* editor;
    char* quoted_path;
    GtkWidget* dlgparent = nullptr;
    if (!path)
        return;
    if (force_root && no_root)
        return;

    if (parent)
        dlgparent = gtk_widget_get_toplevel(GTK_WIDGET(parent));

    if (geteuid() != 0 && !force_root && (no_root || have_rw_access(path)))
    {
        editor = xset_get_s("editor");
        if (!editor || editor[0] == '\0')
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
        if (!editor || editor[0] == '\0')
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
    quoted_path = bash_quote(path);
    if (strstr(editor, "%f"))
        editor = replace_string(editor, "%f", quoted_path, false);
    else if (strstr(editor, "%F"))
        editor = replace_string(editor, "%F", quoted_path, false);
    else if (strstr(editor, "%u"))
        editor = replace_string(editor, "%u", quoted_path, false);
    else if (strstr(editor, "%U"))
        editor = replace_string(editor, "%U", quoted_path, false);
    else
        editor = g_strdup_printf("%s %s", editor, quoted_path);
    g_free(quoted_path);

    // task
    char* task_name = g_strdup_printf("Edit %s", path);
    char* cwd = g_path_get_dirname(path);
    PtkFileTask* task = ptk_file_exec_new(task_name, cwd, dlgparent, nullptr);
    g_free(task_name);
    g_free(cwd);
    task->task->exec_command = editor;
    task->task->exec_sync = false;
    task->task->exec_terminal = terminal;
    if (as_root)
        task->task->exec_as_user = g_strdup_printf("root");
    ptk_file_task_run(task);
}

char*
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
        return g_strdup("( none )");
    char* mod = g_strdup(gdk_keyval_name(keyval));
    if (mod && mod[0] && !mod[1] && g_ascii_isalpha(mod[0]))
        mod[0] = g_ascii_toupper(mod[0]);
    else if (!mod)
        mod = g_strdup("NA");
    char* str;
    if (keymod)
    {
        if (keymod & GDK_SUPER_MASK)
        {
            str = mod;
            mod = g_strdup_printf("Super+%s", str);
            g_free(str);
        }
        if (keymod & GDK_HYPER_MASK)
        {
            str = mod;
            mod = g_strdup_printf("Hyper+%s", str);
            g_free(str);
        }
        if (keymod & GDK_META_MASK)
        {
            str = mod;
            mod = g_strdup_printf("Meta+%s", str);
            g_free(str);
        }
        if (keymod & GDK_MOD1_MASK)
        {
            str = mod;
            mod = g_strdup_printf("Alt+%s", str);
            g_free(str);
        }
        if (keymod & GDK_CONTROL_MASK)
        {
            str = mod;
            mod = g_strdup_printf("Ctrl+%s", str);
            g_free(str);
        }
        if (keymod & GDK_SHIFT_MASK)
        {
            str = mod;
            mod = g_strdup_printf("Shift+%s", str);
            g_free(str);
        }
    }
    return mod;
}

static bool
on_set_key_keypress(GtkWidget* widget, GdkEventKey* event, GtkWidget* dlg)
{
    GList* l;
    int* newkey = (int*)g_object_get_data(G_OBJECT(dlg), "newkey");
    int* newkeymod = (int*)g_object_get_data(G_OBJECT(dlg), "newkeymod");
    GtkWidget* btn = GTK_WIDGET(g_object_get_data(G_OBJECT(dlg), "btn"));
    XSet* set = XSET(g_object_get_data(G_OBJECT(dlg), "set"));
    XSet* set2;
    XSet* keyset = nullptr;
    char* keyname;

    unsigned int keymod = (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK |
                                           GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK));

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

    for (l = xsets; l; l = l->next)
    {
        set2 = XSET(l->data);
        if (set2 && set2 != set && set2->key > 0 && set2->key == event->keyval &&
            set2->keymod == keymod && set2 != keyset)
        {
            char* name;
            if (set2->desc && !strcmp(set2->desc, "@plugin@mirror@") && set2->shared_key)
            {
                // set2 is plugin mirror
                XSet* rset = xset_get(set2->shared_key);
                if (rset->menu_label)
                    name = clean_label(rset->menu_label, false, false);
                else
                    name = g_strdup("( no name )");
            }
            else if (set2->menu_label)
                name = clean_label(set2->menu_label, false, false);
            else
                name = g_strdup("( no name )");

            keyname = xset_get_keyname(nullptr, event->keyval, keymod);
#ifdef HAVE_NONLATIN
            if (nonlatin_key == 0)
#endif
                gtk_message_dialog_format_secondary_text(
                    GTK_MESSAGE_DIALOG(dlg),
                    "\t%s\n\tKeycode: %#4x  Modifier: %#x\n\n%s is already assigned to "
                    "'%s'.\n\nPress a different key or click Set to replace the current key "
                    "assignment.",
                    keyname,
                    event->keyval,
                    keymod,
                    keyname,
                    name);
#ifdef HAVE_NONLATIN
            else
                gtk_message_dialog_format_secondary_text(
                    GTK_MESSAGE_DIALOG(dlg),
                    "\t%s\n\tKeycode: %#4x [%#4x]  Modifier: %#x\n\n%s is already assigned to "
                    "'%s'.\n\nPress a different key or click Set to replace the current key "
                    "assignment.",
                    keyname,
                    event->keyval,
                    nonlatin_key,
                    keymod,
                    keyname,
                    name);
#endif
            g_free(name);
            g_free(keyname);
            *newkey = event->keyval;
            *newkeymod = keymod;
            return true;
        }
    }
    keyname = xset_get_keyname(nullptr, event->keyval, keymod);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg),
                                             "\t%s\n\tKeycode: %#4x  Modifier: %#x",
                                             keyname,
                                             event->keyval,
                                             keymod);
    g_free(keyname);
    *newkey = event->keyval;
    *newkeymod = keymod;
    return true;
}

void
xset_set_key(GtkWidget* parent, XSet* set)
{
    char* name;
    char* keymsg;
    XSet* keyset;
    unsigned int newkey = 0;
    unsigned int newkeymod = 0;
    GtkWidget* dlgparent = nullptr;

    if (set->menu_label)
        name = clean_label(set->menu_label, false, true);
    else if (set->tool > XSET_TOOL_CUSTOM)
        name = g_strdup(xset_get_builtin_toolitem_label(set->tool));
    else if (g_str_has_prefix(set->name, "open_all_type_"))
    {
        keyset = xset_get("open_all");
        name = clean_label(keyset->menu_label, false, true);
        if (set->shared_key)
            g_free(set->shared_key);
        set->shared_key = g_strdup("open_all");
    }
    else
        name = g_strdup("( no name )");
    keymsg = g_strdup_printf("Press your key combination for item '%s' then click Set.  To "
                             "remove the current key assignment, click Unset.",
                             name);
    g_free(name);
    if (parent)
        dlgparent = gtk_widget_get_toplevel(parent);
    GtkWidget* dlg = gtk_message_dialog_new_with_markup(dlgparent ? GTK_WINDOW(dlgparent) : nullptr,
                                                        GTK_DIALOG_MODAL,
                                                        GTK_MESSAGE_QUESTION,
                                                        GTK_BUTTONS_NONE,
                                                        keymsg,
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
            GList* l;
            XSet* set2;
            for (l = xsets; l; l = l->next)
            {
                set2 = XSET(l->data);
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
    char* msg;
    int response;
    char* folder;
    char* file = nullptr;
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

    int job = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "job"));
    int cmd_type = xset_get_int_set(set, "x");

    // LOG_INFO("activate job {} {}", job, set->name);
    switch (job)
    {
        case XSET_JOB_KEY:
            xset_set_key(parent, set);
            break;
        case XSET_JOB_ICON:
            mset = xset_get_plugin_mirror(set);
            char* old_icon;
            old_icon = g_strdup(mset->icon);
            // Note: xset_text_dialog uses the title passed to know this is an
            // icon chooser, so it adds a Choose button.  If you change the title,
            // change xset_text_dialog.
            xset_text_dialog(parent,
                             "Set Icon",
                             false,
                             icon_desc,
                             nullptr,
                             mset->icon,
                             &mset->icon,
                             nullptr,
                             false);
            if (set->lock && set->keep_terminal == XSET_B_UNSET && g_strcmp0(old_icon, mset->icon))
            {
                // built-in icon has been changed from default, save it
                set->keep_terminal = XSET_B_TRUE;
            }
            g_free(old_icon);
            break;
        case XSET_JOB_LABEL:
            break;
        case XSET_JOB_EDIT:
            if (cmd_type == XSET_CMD_SCRIPT)
            {
                // script
                cscript = xset_custom_get_script(set, !set->plugin);
                if (!cscript)
                    break;
                xset_edit(parent, cscript, false, true);
                g_free(cscript);
            }
            break;
        case XSET_JOB_EDIT_ROOT:
            if (cmd_type == XSET_CMD_SCRIPT)
            {
                // script
                cscript = xset_custom_get_script(set, !set->plugin);
                if (!cscript)
                    break;
                xset_edit(parent, cscript, true, false);
                g_free(cscript);
            }
            break;
        case XSET_JOB_COPYNAME:
            clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
            if (cmd_type == XSET_CMD_LINE)
            {
                // line
                gtk_clipboard_set_text(clip, set->line, -1);
            }
            else if (cmd_type == XSET_CMD_SCRIPT)
            {
                // script
                cscript = xset_custom_get_script(set, true);
                if (!cscript)
                    break;
                gtk_clipboard_set_text(clip, cscript, -1);
                g_free(cscript);
            }
            else if (cmd_type == XSET_CMD_APP)
            {
                // custom
                gtk_clipboard_set_text(clip, set->z, -1);
            }
            break;
        case XSET_JOB_LINE:
            if (xset_text_dialog(parent,
                                 "Edit Command Line",
                                 true,
                                 enter_command_line,
                                 nullptr,
                                 set->line,
                                 &set->line,
                                 nullptr,
                                 false))
                xset_set_set(set, XSET_SET_SET_X, "0");
            break;
        case XSET_JOB_SCRIPT:
            xset_set_set(set, XSET_SET_SET_X, "1");
            cscript = xset_custom_get_script(set, true);
            if (!cscript)
                break;
            xset_edit(parent, cscript, false, false);
            g_free(cscript);
            break;
        case XSET_JOB_CUSTOM:
            if (set->z && set->z[0] != '\0')
            {
                folder = g_path_get_dirname(set->z);
                file = g_path_get_basename(set->z);
            }
            else
            {
                folder = g_strdup_printf("/usr/bin");
                file = nullptr;
            }
            if ((custom_file = xset_file_dialog(parent,
                                                GTK_FILE_CHOOSER_ACTION_OPEN,
                                                "Choose Custom Executable",
                                                folder,
                                                file)))
            {
                xset_set_set(set, XSET_SET_SET_X, "2");
                xset_set_set(set, XSET_SET_SET_Z, custom_file);
                g_free(custom_file);
            }
            g_free(file);
            g_free(folder);
            break;
        case XSET_JOB_USER:
            if (!set->plugin)
                xset_text_dialog(
                    parent,
                    "Run As User",
                    false,
                    "Run this command as username:\n\n( Leave blank for current user )",
                    nullptr,
                    set->y,
                    &set->y,
                    nullptr,
                    false);
            break;
        case XSET_JOB_BOOKMARK:
        case XSET_JOB_APP:
        case XSET_JOB_COMMAND:
            if (g_str_has_prefix(set->name, "open_all_type_"))
            {
                name = set->name + 14;
                msg = g_strdup_printf(
                    "You are adding a custom command to the Default menu item.  This item will "
                    "automatically have a pre-context - it will only appear when the MIME type "
                    "of "
                    "the first selected file matches the current type '%s'.\n\nAdd commands or "
                    "menus "
                    "here which you only want to appear for this one MIME type.",
                    name[0] == '\0' ? "(none)" : name);
                if (xset_msg_dialog(parent,
                                    0,
                                    "New Context Command",
                                    GTK_BUTTONS_OK_CANCEL,
                                    msg,
                                    nullptr) != GTK_RESPONSE_OK)
                {
                    g_free(msg);
                    break;
                }
                g_free(msg);
            }
            switch (job)
            {
                case XSET_JOB_COMMAND:
                    name = g_strdup_printf("New _Command");
                    if (!xset_text_dialog(parent,
                                          "Set Item Name",
                                          false,
                                          enter_menu_name_new,
                                          nullptr,
                                          name,
                                          &name,
                                          nullptr,
                                          false))
                    {
                        g_free(name);
                        break;
                    }
                    file = nullptr;
                    break;
                case XSET_JOB_APP:
                {
                    VFSMimeType* mime_type;
                    mime_type = vfs_mime_type_get_from_type(
                        xset_context && xset_context->var[CONTEXT_MIME] &&
                                xset_context->var[CONTEXT_MIME][0]
                            ? xset_context->var[CONTEXT_MIME]
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
                        g_free(file);
                        break;
                    }
                    name = nullptr;
                    break;
                }
                case XSET_JOB_BOOKMARK:
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
                        g_free(file);
                        break;
                    }
                    name = g_path_get_basename(file);
                    break;
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
                case XSET_JOB_COMMAND:
                    xset_item_prop_dlg(xset_context, newset, 2);
                    break;
                case XSET_JOB_APP:
                    g_free(newset->x);
                    newset->x = g_strdup("2"); // XSET_CMD_APP
                    // unset these to save session space
                    newset->task = newset->task_err = newset->task_out = newset->keep_terminal =
                        XSET_B_UNSET;
                    break;
                case XSET_JOB_BOOKMARK:
                    g_free(newset->x);
                    newset->x = g_strdup("3"); // XSET_CMD_BOOKMARK
                    // unset these to save session space
                    newset->task = newset->task_err = newset->task_out = newset->keep_terminal =
                        XSET_B_UNSET;
                    break;
                default:
                    break;
            }
            main_window_bookmark_changed(newset->name);
            break;
        case XSET_JOB_SUBMENU:
        case XSET_JOB_SUBMENU_BOOK:
            if (g_str_has_prefix(set->name, "open_all_type_"))
            {
                name = set->name + 14;
                msg = g_strdup_printf(
                    "You are adding a custom submenu to the Default menu item.  This item will "
                    "automatically have a pre-context - it will only appear when the MIME type "
                    "of "
                    "the first selected file matches the current type '%s'.\n\nAdd commands or "
                    "menus "
                    "here which you only want to appear for this one MIME type.",
                    name[0] == '\0' ? "(none)" : name);
                if (xset_msg_dialog(parent,
                                    0,
                                    "New Context Submenu",
                                    GTK_BUTTONS_OK_CANCEL,
                                    msg,
                                    nullptr) != GTK_RESPONSE_OK)
                {
                    g_free(msg);
                    break;
                }
                g_free(msg);
            }
            name = nullptr;
            if (!xset_text_dialog(
                    parent,
                    "Set Submenu Name",
                    false,
                    "Enter submenu name:\n\nPrecede a character with an underscore (_) "
                    "to underline that character as a shortcut key if desired.",
                    nullptr,
                    "New _Submenu",
                    &name,
                    nullptr,
                    false) ||
                !name)
                break;

            // add new submenu
            newset = xset_custom_new();
            newset->menu_label = name;
            newset->menu_style = XSET_MENU_SUBMENU;
            xset_custom_insert_after(set, newset);

            // add submenu child
            childset = xset_custom_new();
            newset->child = g_strdup(childset->name);
            childset->parent = g_strdup(newset->name);
            if (job == XSET_JOB_SUBMENU_BOOK || xset_is_main_bookmark(set))
            {
                // adding new submenu from a bookmark - fill with bookmark
                folder = set->browser ? (char*)ptk_file_browser_get_cwd(set->browser)
                                      : (char*)vfs_user_desktop_dir();
                childset->menu_label = g_path_get_basename(folder);
                childset->z = g_strdup(folder);
                childset->x = g_strdup_printf("%d", XSET_CMD_BOOKMARK);
                childset->task = childset->task_err = childset->task_out = childset->keep_terminal =
                    XSET_B_UNSET;
            }
            else
                childset->menu_label = g_strdup_printf("New _Command");
            main_window_bookmark_changed(newset->name);
            break;
        case XSET_JOB_SEP:
            newset = xset_custom_new();
            newset->menu_style = XSET_MENU_SEP;
            xset_custom_insert_after(set, newset);
            main_window_bookmark_changed(newset->name);
            break;
        case XSET_JOB_ADD_TOOL:
            job = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "tool_type"));
            if (job < XSET_TOOL_DEVICES || job >= XSET_TOOL_INVALID || !set->tool)
                break;
            newset = xset_new_builtin_toolitem(job);
            if (newset)
                xset_custom_insert_after(set, newset);
            break;
        case XSET_JOB_IMPORT_FILE:
            // get file path
            XSet* save;
            save = xset_get("plug_ifile");
            if (save->s) //&& std::filesystem::is_directory(save->s)
                folder = save->s;
            else
            {
                if (!(folder = xset_get_s("go_set_default")))
                    folder = g_strdup("/");
            }
            file = xset_file_dialog(GTK_WIDGET(parent),
                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                    "Choose Plugin File",
                                    folder,
                                    nullptr);
            if (!file)
                break;
            if (save->s)
                g_free(save->s);
            save->s = g_path_get_dirname(file);

            // Make Plugin Dir
            const char* user_tmp;
            user_tmp = xset_get_user_tmp_dir();
            if (!user_tmp)
            {
                xset_msg_dialog(GTK_WIDGET(parent),
                                GTK_MESSAGE_ERROR,
                                "Error Creating Temp Directory",
                                0,
                                "Unable to create temporary directory",
                                nullptr);
                g_free(file);
                break;
            }
            char* hex8;
            folder = nullptr;
            while (!folder || std::filesystem::exists(folder))
            {
                hex8 = randhex8();
                if (folder)
                    g_free(folder);
                folder = g_build_filename(user_tmp, hex8, nullptr);
                g_free(hex8);
            }
            install_plugin_file(set->browser ? set->browser->main_window : nullptr,
                                nullptr,
                                file,
                                folder,
                                PLUGIN_JOB_COPY,
                                set);
            g_free(file);
            g_free(folder);
            break;
        case XSET_JOB_IMPORT_GTK:
            // both GTK2 and GTK3 now use new location?
            file = g_build_filename(vfs_user_config_dir(), "gtk-3.0", "bookmarks", nullptr);
            if (!(file && std::filesystem::exists(file)))
                file = g_build_filename(vfs_user_home_dir(), ".gtk-bookmarks", nullptr);
            msg =
                g_strdup_printf("GTK bookmarks (%s) will be imported into the current or selected "
                                "submenu.  Note that importing large numbers of bookmarks (eg more "
                                "than 500) may impact performance.",
                                file);
            if (xset_msg_dialog(parent,
                                GTK_MESSAGE_QUESTION,
                                "Import GTK Bookmarks",
                                GTK_BUTTONS_OK_CANCEL,
                                msg,
                                nullptr) != GTK_RESPONSE_OK)
            {
                g_free(msg);
                break;
            }
            g_free(msg);
            ptk_bookmark_view_import_gtk(file, set);
            g_free(file);
            break;
        case XSET_JOB_CUT:
            set_clipboard = set;
            clipboard_is_cut = true;
            break;
        case XSET_JOB_COPY:
            set_clipboard = set;
            clipboard_is_cut = false;

            // if copy bookmark, put target on real clipboard
            if (!set->lock && set->z && set->menu_style < XSET_MENU_SUBMENU &&
                xset_get_int_set(set, "x") == XSET_CMD_BOOKMARK)
            {
                clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
                gtk_clipboard_set_text(clip, set->z, -1);
                clip = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
                gtk_clipboard_set_text(clip, set->z, -1);
            }
            break;
        case XSET_JOB_PASTE:
            if (!set_clipboard)
                break;
            if (set_clipboard->tool > XSET_TOOL_CUSTOM && !set->tool)
                // failsafe - disallow pasting a builtin tool to a menu
                break;
            if (clipboard_is_cut)
            {
                update_toolbars = set_clipboard->tool != XSET_TOOL_NOT;
                if (!update_toolbars && set_clipboard->parent)
                {
                    newset = xset_get(set_clipboard->parent);
                    if (newset->tool)
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
        case XSET_JOB_REMOVE:
        case XSET_JOB_REMOVE_BOOK:
            if (set->plugin)
            {
                xset_remove_plugin(parent, set->browser, set);
                break;
            }
            if (set->menu_label && set->menu_label[0])
                name = clean_label(set->menu_label, false, false);
            else
            {
                if (!set->lock && set->z && set->menu_style < XSET_MENU_SUBMENU &&
                    (cmd_type == XSET_CMD_BOOKMARK || cmd_type == XSET_CMD_APP))
                    name = g_strdup(set->z);
                else
                    name = g_strdup("( no name )");
            }
            if (set->child && set->menu_style == XSET_MENU_SUBMENU)
            {
                msg = g_strdup_printf(
                    "Permanently remove the '%s' SUBMENU AND ALL ITEMS WITHIN IT?\n\nThis action "
                    "will delete all settings and files associated with these items.",
                    name);
                buttons = GTK_BUTTONS_YES_NO;
            }
            else
            {
                msg =
                    g_strdup_printf("Permanently remove the '%s' item?\n\nThis action will delete "
                                    "all settings and files associated with this item.",
                                    name);
                buttons = GTK_BUTTONS_OK_CANCEL;
            }
            g_free(name);
            bool is_bookmark_or_app;
            is_bookmark_or_app = !set->lock && set->menu_style < XSET_MENU_SUBMENU &&
                                 (cmd_type == XSET_CMD_BOOKMARK || cmd_type == XSET_CMD_APP) &&
                                 set->tool <= XSET_TOOL_CUSTOM;
            if (set->menu_style != XSET_MENU_SEP && !app_settings.no_confirm &&
                !is_bookmark_or_app && set->tool <= XSET_TOOL_CUSTOM)
            {
                if (parent)
                    dlgparent = gtk_widget_get_toplevel(parent);
                dlg = gtk_message_dialog_new(GTK_WINDOW(dlgparent),
                                             GTK_DIALOG_MODAL,
                                             GTK_MESSAGE_WARNING,
                                             (GtkButtonsType)buttons,
                                             msg,
                                             nullptr);
                xset_set_window_icon(GTK_WINDOW(dlg));
                gtk_window_set_title(GTK_WINDOW(dlg), "Confirm Remove");
                gtk_widget_show_all(dlg);
                response = gtk_dialog_run(GTK_DIALOG(dlg));
                gtk_widget_destroy(dlg);
                if (response != GTK_RESPONSE_OK && response != GTK_RESPONSE_YES)
                    break;
            }
            g_free(msg);

            // remove
            name = g_strdup(set->name);
            prog = g_strdup(set->parent);

            if (job == XSET_JOB_REMOVE && set->parent /* maybe only item in sub */
                && xset_is_main_bookmark(set))
                job = XSET_JOB_REMOVE_BOOK;

            if (set->parent && (set_next = xset_is(set->parent)) &&
                set_next->tool == XSET_TOOL_CUSTOM && set_next->menu_style == XSET_MENU_SUBMENU)
                // this set is first item in custom toolbar submenu
                update_toolbars = true;

            childset = xset_custom_remove(set);

            if (childset && job == XSET_JOB_REMOVE_BOOK)
            {
                // added a new default item to submenu from a bookmark
                folder = set->browser ? (char*)ptk_file_browser_get_cwd(set->browser)
                                      : (char*)vfs_user_desktop_dir();
                g_free(childset->menu_label);
                childset->menu_label = g_path_get_basename(folder);
                childset->z = g_strdup(folder);
                childset->x = g_strdup_printf("%d", XSET_CMD_BOOKMARK);
                childset->task = childset->task_err = childset->task_out = childset->keep_terminal =
                    XSET_B_UNSET;
            }
            else if (set->tool)
            {
                update_toolbars = true;
                g_free(name);
                g_free(prog);
                name = prog = nullptr;
            }
            else
            {
                g_free(prog);
                prog = nullptr;
            }

            xset_custom_delete(set, false);
            set = nullptr;

            if (prog)
                main_window_bookmark_changed(prog);
            else if (name)
                main_window_bookmark_changed(name);
            g_free(name);
            g_free(prog);
            break;
        case XSET_JOB_EXPORT:
            if ((!set->lock || !g_strcmp0(set->name, "main_book")) && set->tool <= XSET_TOOL_CUSTOM)
                xset_custom_export(parent, set->browser, set);
            break;
        case XSET_JOB_NORMAL:
            set->menu_style = XSET_MENU_NORMAL;
            break;
        case XSET_JOB_CHECK:
            set->menu_style = XSET_MENU_CHECK;
            break;
        case XSET_JOB_CONFIRM:
            if (!set->desc)
                set->desc = g_strdup("Are you sure?");
            if (xset_text_dialog(parent,
                                 "Dialog Message",
                                 true,
                                 "Enter the message to be displayed in this "
                                 "dialog:\n\nUse:\n\t\\n\tnewline\n\t\\t\ttab",
                                 nullptr,
                                 set->desc,
                                 &set->desc,
                                 nullptr,
                                 false))
                set->menu_style = XSET_MENU_CONFIRM;
            break;
        case XSET_JOB_DIALOG:
            if (xset_text_dialog(parent,
                                 "Dialog Message",
                                 true,
                                 "Enter the message to be displayed in this "
                                 "dialog:\n\nUse:\n\t\\n\tnewline\n\t\\t\ttab",
                                 nullptr,
                                 set->desc,
                                 &set->desc,
                                 nullptr,
                                 false))
                set->menu_style = XSET_MENU_STRING;
            break;
        case XSET_JOB_MESSAGE:
            xset_text_dialog(parent,
                             "Dialog Message",
                             true,
                             "Enter the message to be displayed in this "
                             "dialog:\n\nUse:\n\t\\n\tnewline\n\t\\t\ttab",
                             nullptr,
                             set->desc,
                             &set->desc,
                             nullptr,
                             false);
            break;
        case XSET_JOB_PROP:
            xset_item_prop_dlg(xset_context, set, 0);
            break;
        case XSET_JOB_PROP_CMD:
            xset_item_prop_dlg(xset_context, set, 2);
            break;
        case XSET_JOB_IGNORE_CONTEXT:
            xset_set_b("context_dlg", !xset_get_b("context_dlg"));
            break;
        case XSET_JOB_BROWSE_FILES:
            if (set->tool > XSET_TOOL_CUSTOM)
                break;
            if (set->plugin)
            {
                folder = g_build_filename(set->plug_dir, "files", nullptr);
                if (!std::filesystem::exists(folder))
                {
                    g_free(folder);
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
                ptk_file_browser_emit_open(set->browser, folder, PTK_OPEN_DIR);
            }
            break;
        case XSET_JOB_BROWSE_DATA:
            if (set->tool > XSET_TOOL_CUSTOM)
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
                ptk_file_browser_emit_open(set->browser, folder, PTK_OPEN_DIR);
            }
            break;
        case XSET_JOB_BROWSE_PLUGIN:
            if (set->plugin && set->plug_dir)
            {
                if (set->browser)
                {
                    ptk_file_browser_emit_open(set->browser, set->plug_dir, PTK_OPEN_DIR);
                }
            }
            break;
        case XSET_JOB_TERM:
            mset = xset_get_plugin_mirror(set);
            switch (mset->in_terminal)
            {
                case XSET_B_TRUE:
                    mset->in_terminal = XSET_B_UNSET;
                    break;
                default:
                    mset->in_terminal = XSET_B_TRUE;
                    mset->task = XSET_B_FALSE;
                    break;
            }
            break;
        case XSET_JOB_KEEP:
            mset = xset_get_plugin_mirror(set);
            switch (mset->keep_terminal)
            {
                case XSET_B_TRUE:
                    mset->keep_terminal = XSET_B_UNSET;
                    break;
                default:
                    mset->keep_terminal = XSET_B_TRUE;
                    break;
            }
            break;
        case XSET_JOB_TASK:
            mset = xset_get_plugin_mirror(set);
            switch (mset->task)
            {
                case XSET_B_TRUE:
                    mset->task = XSET_B_UNSET;
                    break;
                default:
                    mset->task = XSET_B_TRUE;
                    break;
            }
            break;
        case XSET_JOB_POP:
            mset = xset_get_plugin_mirror(set);
            switch (mset->task_pop)
            {
                case XSET_B_TRUE:
                    mset->task_pop = XSET_B_UNSET;
                    break;
                default:
                    mset->task_pop = XSET_B_TRUE;
                    break;
            }
            break;
        case XSET_JOB_ERR:
            mset = xset_get_plugin_mirror(set);
            switch (mset->task_err)
            {
                case XSET_B_TRUE:
                    mset->task_err = XSET_B_UNSET;
                    break;
                default:
                    mset->task_err = XSET_B_TRUE;
                    break;
            }
            break;
        case XSET_JOB_OUT:
            mset = xset_get_plugin_mirror(set);
            switch (mset->task_out)
            {
                case XSET_B_TRUE:
                    mset->task_out = XSET_B_UNSET;
                    break;
                default:
                    mset->task_out = XSET_B_TRUE;
                    break;
            }
            break;
        case XSET_JOB_SCROLL:
            mset = xset_get_plugin_mirror(set);
            switch (mset->scroll_lock)
            {
                case XSET_B_TRUE:
                    mset->scroll_lock = XSET_B_UNSET;
                    break;
                default:
                    mset->scroll_lock = XSET_B_TRUE;
                    break;
            }
            break;
        case XSET_JOB_TOOLTIPS:
            set_next = xset_get_panel(1, "tool_l");
            set_next->b = set_next->b == XSET_B_TRUE ? XSET_B_UNSET : XSET_B_TRUE;
            break;
        default:
            break;
    }

    if (set && (!set->lock || !strcmp(set->name, "main_book")))
    {
        main_window_bookmark_changed(set->name);
        if (set->parent && (set_next = xset_is(set->parent)) &&
            set_next->tool == XSET_TOOL_CUSTOM && set_next->menu_style == XSET_MENU_SUBMENU)
            // this set is first item in custom toolbar submenu
            update_toolbars = true;
    }

    if ((set && !set->lock && set->tool) || update_toolbars)
        main_window_rebuild_all_toolbars(set ? set->browser : nullptr);

    // autosave
    autosave_request();
}

static bool
xset_job_is_valid(XSet* set, int job)
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
        if (!set->plugin_top || strstr(set->plug_dir, "/included/"))
            no_remove = true;
    }

    // control open_all item
    if (g_str_has_prefix(set->name, "open_all_type_"))
        open_all = true;

    switch (job)
    {
        case XSET_JOB_KEY:
            return set->menu_style < XSET_MENU_SUBMENU;
        case XSET_JOB_ICON:
            return ((set->menu_style == XSET_MENU_NORMAL || set->menu_style == XSET_MENU_STRING ||
                     set->menu_style == XSET_MENU_FONTDLG ||
                     set->menu_style == XSET_MENU_COLORDLG ||
                     set->menu_style == XSET_MENU_SUBMENU || set->tool) &&
                    !open_all);
        case XSET_JOB_EDIT:
            return !set->lock && set->menu_style < XSET_MENU_SUBMENU;
        case XSET_JOB_COMMAND:
            return !set->plugin;
        case XSET_JOB_CUT:
            return (!set->lock && !set->plugin);
        case XSET_JOB_COPY:
            return !set->lock;
        case XSET_JOB_PASTE:
            if (!set_clipboard)
                no_paste = true;
            else if (set->plugin)
                no_paste = true;
            else if (set == set_clipboard && clipboard_is_cut)
                // don't allow cut paste to self
                no_paste = true;
            else if (set_clipboard->tool > XSET_TOOL_CUSTOM && !set->tool)
                // don't allow paste of builtin tool item to menu
                no_paste = true;
            else if (set_clipboard->menu_style == XSET_MENU_SUBMENU)
                // don't allow paste of submenu to self or below
                no_paste = xset_clipboard_in_set(set);
            return !no_paste;
        case XSET_JOB_REMOVE:
            return (!set->lock && !no_remove);
        // case XSET_JOB_CONTEXT:
        //    return ( xset_context && xset_context->valid && !open_all );
        case XSET_JOB_PROP:
        case XSET_JOB_PROP_CMD:
            return true;
        default:
            break;
    }
    return false;
}

static bool
xset_design_menu_keypress(GtkWidget* widget, GdkEventKey* event, XSet* set)
{
    int job = -1;

    GtkWidget* item = gtk_menu_shell_get_selected_item(GTK_MENU_SHELL(widget));
    if (!item)
        return false;

    unsigned int keymod = (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK |
                                           GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK));

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
                    job = XSET_JOB_PROP;
                    break;
                case GDK_KEY_F4:
                    if (xset_get_int_set(set, "x") == XSET_CMD_SCRIPT)
                        job = XSET_JOB_EDIT;
                    else
                        job = XSET_JOB_PROP_CMD;
                    break;
                case GDK_KEY_Delete:
                    job = XSET_JOB_REMOVE;
                    break;
                case GDK_KEY_Insert:
                    job = XSET_JOB_COMMAND;
                    break;
                default:
                    break;
            }
            break;
        case GDK_CONTROL_MASK:
            switch (event->keyval)
            {
                case GDK_KEY_c:
                    job = XSET_JOB_COPY;
                    break;
                case GDK_KEY_x:
                    job = XSET_JOB_CUT;
                    break;
                case GDK_KEY_v:
                    job = XSET_JOB_PASTE;
                    break;
                case GDK_KEY_e:
                    if (set->lock)
                    {
                        return false;
                    }
                    else
                        job = XSET_JOB_EDIT;
                    break;
                case GDK_KEY_k:
                    job = XSET_JOB_KEY;
                    break;
                case GDK_KEY_i:
                    job = XSET_JOB_ICON;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    if (job != -1)
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
xset_design_additem(GtkWidget* menu, const char* label, int job, XSet* set)
{
    GtkWidget* item;
    item = gtk_menu_item_new_with_mnemonic(label);

    g_object_set_data(G_OBJECT(item), "job", GINT_TO_POINTER(job));
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
    bool show_keys = !is_bookmark && !set->tool;

    // if (set->plugin && set->shared_key)
    //     mset = xset_get_plugin_mirror(set);
    // else
    //     mset = set;

    if (set->plugin)
    {
        if (set->plug_dir)
        {
            if (!set->plugin_top || strstr(set->plug_dir, "/included/"))
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
        // don't allow cut paste to self
        no_paste = true;
    else if (set_clipboard->tool > XSET_TOOL_CUSTOM && !insert_set->tool)
        // don't allow paste of builtin tool item to menu
        no_paste = true;
    else if (set_clipboard->menu_style == XSET_MENU_SUBMENU)
        // don't allow paste of submenu to self or below
        no_paste = xset_clipboard_in_set(insert_set);

    // control open_all item
    // if (g_str_has_prefix(set->name, "open_all_type_"))
    //    open_all = true;

    GtkWidget* design_menu = gtk_menu_new();
    GtkAccelGroup* accel_group = gtk_accel_group_new();

    // Cut
    newitem = xset_design_additem(design_menu, "Cu_t", XSET_JOB_CUT, set);
    gtk_widget_set_sensitive(newitem, !set->lock && !set->plugin);
    if (show_keys)
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_x,
                                   GDK_CONTROL_MASK,
                                   GTK_ACCEL_VISIBLE);

    // Copy
    newitem = xset_design_additem(design_menu, "_Copy", XSET_JOB_COPY, set);
    gtk_widget_set_sensitive(newitem, !set->lock);
    if (show_keys)
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_c,
                                   GDK_CONTROL_MASK,
                                   GTK_ACCEL_VISIBLE);

    // Paste
    newitem = xset_design_additem(design_menu, "_Paste", XSET_JOB_PASTE, insert_set);
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
                                  is_bookmark ? XSET_JOB_REMOVE_BOOK : XSET_JOB_REMOVE,
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
    newitem = xset_design_additem(design_menu, "E_xport", XSET_JOB_EXPORT, set);
    gtk_widget_set_sensitive(
        newitem,
        (!set->lock && set->menu_style < XSET_MENU_SEP && set->tool <= XSET_TOOL_CUSTOM) ||
            !g_strcmp0(set->name, "main_book"));

    //// New submenu
    newitem = gtk_menu_item_new_with_mnemonic("_New");
    submenu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(newitem), submenu);
    gtk_container_add(GTK_CONTAINER(design_menu), newitem);
    gtk_widget_set_sensitive(newitem, !set->plugin);
    g_object_set_data(G_OBJECT(newitem), "job", GINT_TO_POINTER(XSET_JOB_HELP_NEW));
    g_signal_connect(submenu, "key_press_event", G_CALLBACK(xset_design_menu_keypress), set);

    // New > Bookmark
    newitem = xset_design_additem(submenu, "_Bookmark", XSET_JOB_BOOKMARK, insert_set);

    // New > Application
    newitem = xset_design_additem(submenu, "_Application", XSET_JOB_APP, insert_set);

    // New > Command
    newitem = xset_design_additem(submenu, "_Command", XSET_JOB_COMMAND, insert_set);
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
                                  is_bookmark ? XSET_JOB_SUBMENU_BOOK : XSET_JOB_SUBMENU,
                                  insert_set);

    // New > Separator
    newitem = xset_design_additem(submenu, "S_eparator", XSET_JOB_SEP, insert_set);

    // New > Import >
    newitem = gtk_menu_item_new_with_mnemonic("_Import");
    submenu2 = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(newitem), submenu2);
    gtk_container_add(GTK_CONTAINER(submenu), newitem);
    gtk_widget_set_sensitive(newitem, !insert_set->plugin);
    g_object_set_data(G_OBJECT(newitem), "job", GINT_TO_POINTER(XSET_JOB_IMPORT_FILE));
    g_signal_connect(submenu2,
                     "key_press_event",
                     G_CALLBACK(xset_design_menu_keypress),
                     insert_set);

    newitem = xset_design_additem(submenu2, "_File", XSET_JOB_IMPORT_FILE, insert_set);
    if (is_bookmark)
        newitem = xset_design_additem(submenu2, "_GTK Bookmarks", XSET_JOB_IMPORT_GTK, set);

    if (insert_set->tool)
    {
        // "Add" submenu for builtin tool items
        newitem = gtk_menu_item_new_with_mnemonic("_Add");
        submenu = gtk_menu_new();
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(newitem), submenu);
        gtk_container_add(GTK_CONTAINER(design_menu), newitem);
        g_object_set_data(G_OBJECT(newitem), "job", GINT_TO_POINTER(XSET_JOB_HELP_ADD));
        g_signal_connect(submenu, "key_press_event", G_CALLBACK(xset_design_menu_keypress), set);

        for (unsigned int i = XSET_TOOL_DEVICES; i < G_N_ELEMENTS(builtin_tool_name); i++)
        {
            newitem =
                xset_design_additem(submenu, builtin_tool_name[i], XSET_JOB_ADD_TOOL, insert_set);
            g_object_set_data(G_OBJECT(newitem), "tool_type", GINT_TO_POINTER(i));
        }
    }

    // Separator
    gtk_container_add(GTK_CONTAINER(design_menu), gtk_separator_menu_item_new());

    // Help
    newitem = xset_design_additem(design_menu,
                                  "_Help",
                                  is_bookmark ? XSET_JOB_HELP_BOOK : XSET_JOB_HELP,
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
    if (set->tool)
    {
        newitem = xset_design_additem(design_menu, "T_ooltips", XSET_JOB_TOOLTIPS, set);
        if (!xset_get_b_panel(1, "tool_l"))
            set_check_menu_item_block(newitem);
    }

    // Key
    newitem = xset_design_additem(design_menu, "_Key Shortcut", XSET_JOB_KEY, set);
    gtk_widget_set_sensitive(newitem, (set->menu_style < XSET_MENU_SUBMENU));
    if (show_keys)
        gtk_widget_add_accelerator(newitem,
                                   "activate",
                                   accel_group,
                                   GDK_KEY_k,
                                   GDK_CONTROL_MASK,
                                   GTK_ACCEL_VISIBLE);

    // Edit (script)
    if (!set->lock && set->menu_style < XSET_MENU_SUBMENU && set->tool <= XSET_TOOL_CUSTOM)
    {
        if (xset_get_int_set(set, "x") == XSET_CMD_SCRIPT)
        {
            char* script = xset_custom_get_script(set, false);
            if (script)
            {
                if (geteuid() != 0 && have_rw_access(script))
                {
                    // edit as user
                    newitem = xset_design_additem(design_menu, "_Edit Script", XSET_JOB_EDIT, set);
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
                        xset_design_additem(design_menu, "E_dit As Root", XSET_JOB_EDIT_ROOT, set);
                    if (geteuid() == 0 && show_keys)
                        gtk_widget_add_accelerator(newitem,
                                                   "activate",
                                                   accel_group,
                                                   GDK_KEY_F4,
                                                   (GdkModifierType)0,
                                                   GTK_ACCEL_VISIBLE);
                }
                g_free(script);
            }
        }
        else if (xset_get_int_set(set, "x") == XSET_CMD_LINE)
        {
            // edit command line
            newitem = xset_design_additem(design_menu, "_Edit Command", XSET_JOB_PROP_CMD, set);
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
    newitem = xset_design_additem(design_menu, "_Properties", XSET_JOB_PROP, set);
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
    int job = -1;

    GtkWidget* menu = item ? GTK_WIDGET(g_object_get_data(G_OBJECT(item), "menu")) : nullptr;
    unsigned int keymod = (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK |
                                           GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK));

    if (event->type == GDK_BUTTON_RELEASE)
    {
        if (event->button == 1 && keymod == 0)
        {
            // user released left button - due to an apparent gtk bug, activate
            // doesn't always fire on this event so handle it ourselves
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
                    else if (event->button == 1 && set->tool && !set->lock)
                    {
                        // activate
                        if (set->tool == XSET_TOOL_CUSTOM)
                            xset_menu_cb(nullptr, set);
                        else
                            xset_builtin_tool_activate(set->tool, set, event);
                        return true;
                    }
                    break;
                case GDK_CONTROL_MASK:
                    // ctrl
                    job = XSET_JOB_COPY;
                    break;
                case GDK_MOD1_MASK:
                    // alt
                    job = XSET_JOB_CUT;
                    break;
                case GDK_SHIFT_MASK:
                    // shift
                    job = XSET_JOB_PASTE;
                    break;
                case (GDK_CONTROL_MASK | GDK_SHIFT_MASK):
                    // ctrl + shift
                    job = XSET_JOB_COMMAND;
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
                        if (xset_get_int_set(set, "x") == XSET_CMD_SCRIPT)
                            job = XSET_JOB_EDIT;
                        else
                            job = XSET_JOB_PROP_CMD;
                    }
                    break;
                case GDK_CONTROL_MASK:
                    // ctrl
                    job = XSET_JOB_KEY;
                    break;
                case GDK_MOD1_MASK:
                    // alt
                    job = XSET_JOB_HELP;
                    break;
                case GDK_SHIFT_MASK:
                    // shift
                    job = XSET_JOB_ICON;
                    break;
                case (GDK_CONTROL_MASK | GDK_SHIFT_MASK):
                    // ctrl + shift
                    job = XSET_JOB_REMOVE;
                    break;
                case (GDK_CONTROL_MASK | GDK_MOD1_MASK):
                    // ctrl + alt
                    job = XSET_JOB_PROP;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    if (job != -1)
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
    return false; // true won't stop activate on button-press (will on release)
}

bool
xset_menu_keypress(GtkWidget* widget, GdkEventKey* event, void* user_data)
{
    int job = -1;
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

    unsigned int keymod = (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK |
                                           GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK));

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
                    job = XSET_JOB_PROP;
                    break;
                case GDK_KEY_F4:
                    if (xset_get_int_set(set, "x") == XSET_CMD_SCRIPT)
                        job = XSET_JOB_EDIT;
                    else
                        job = XSET_JOB_PROP_CMD;
                    break;
                case GDK_KEY_Delete:
                    job = XSET_JOB_REMOVE;
                    break;
                case GDK_KEY_Insert:
                    job = XSET_JOB_COMMAND;
                    break;
                default:
                    break;
            }
            break;
        case GDK_CONTROL_MASK:
            switch (event->keyval)
            {
                case GDK_KEY_c:
                    job = XSET_JOB_COPY;
                    break;
                case GDK_KEY_x:
                    job = XSET_JOB_CUT;
                    break;
                case GDK_KEY_v:
                    job = XSET_JOB_PASTE;
                    break;
                case GDK_KEY_e:
                    if (set->lock)
                    {
                        xset_design_show_menu(widget, set, nullptr, 0, event->time);
                        return true;
                    }
                    else
                    {
                        if (xset_get_int_set(set, "x") == XSET_CMD_SCRIPT)
                            job = XSET_JOB_EDIT;
                        else
                            job = XSET_JOB_PROP_CMD;
                    }
                    break;
                case GDK_KEY_k:
                    job = XSET_JOB_KEY;
                    break;
                case GDK_KEY_i:
                    job = XSET_JOB_ICON;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }

    if (job != -1)
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
    char* title;
    XSet* mset; // mirror set or set
    XSet* rset; // real set

    if (item)
    {
        if (set->lock && set->menu_style == XSET_MENU_RADIO && GTK_IS_CHECK_MENU_ITEM(item) &&
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

    if (!rset->menu_style)
    {
        if (cb_func)
            cb_func(item, cb_data);
        else if (!rset->lock)
            xset_custom_activate(item, rset);
    }
    else
    {
        switch (rset->menu_style)
        {
            case XSET_MENU_SEP:
                break;
            case XSET_MENU_CHECK:
                if (mset->b == XSET_B_TRUE)
                    mset->b = XSET_B_FALSE;
                else
                    mset->b = XSET_B_TRUE;
                if (cb_func)
                    cb_func(item, cb_data);
                else if (!rset->lock)
                    xset_custom_activate(item, rset);
                if (set->tool == XSET_TOOL_CUSTOM)
                    ptk_file_browser_update_toolbar_widgets(set->browser, set, -1);
                break;
            case XSET_MENU_STRING:
            case XSET_MENU_CONFIRM:
            {
                char* msg;
                char* default_str = nullptr;
                if (rset->title && rset->lock)
                    title = g_strdup(rset->title);
                else
                    title = clean_label(rset->menu_label, false, false);
                if (rset->lock)
                {
                    msg = rset->desc;
                    default_str = rset->z;
                }
                else
                {
                    char* newline = g_strdup_printf("\n");
                    char* tab = g_strdup_printf("\t");
                    char* msg1 = replace_string(rset->desc, "\\n", newline, false);
                    msg = replace_string(msg1, "\\t", tab, false);
                    g_free(msg1);
                    g_free(newline);
                    g_free(tab);
                }
                if (rset->menu_style == XSET_MENU_CONFIRM)
                {
                    if (xset_msg_dialog(parent,
                                        GTK_MESSAGE_QUESTION,
                                        title,
                                        GTK_BUTTONS_OK_CANCEL,
                                        msg,
                                        nullptr) == GTK_RESPONSE_OK)
                    {
                        if (cb_func)
                            cb_func(item, cb_data);
                        else if (!set->lock)
                            xset_custom_activate(item, rset);
                    }
                }
                else if (xset_text_dialog(parent,
                                          title,
                                          true,
                                          msg,
                                          nullptr,
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
                if (!rset->lock)
                    g_free(msg);
                g_free(title);
            }
            break;
            case XSET_MENU_RADIO:
                if (mset->b != XSET_B_TRUE)
                    mset->b = XSET_B_TRUE;
                if (cb_func)
                    cb_func(item, cb_data);
                else if (!rset->lock)
                    xset_custom_activate(item, rset);
                break;
            case XSET_MENU_FONTDLG:
                break;
            case XSET_MENU_FILEDLG:
                // test purpose only
                {
                    char* file;
                    file = xset_file_dialog(parent,
                                            GTK_FILE_CHOOSER_ACTION_SAVE,
                                            rset->title,
                                            rset->s,
                                            "foobar.xyz");
                    // LOG_INFO("file={}", file);
                    g_free(file);
                }
                break;
            case XSET_MENU_ICON:
                // Note: xset_text_dialog uses the title passed to know this is an
                // icon chooser, so it adds a Choose button.  If you change the title,
                // change xset_text_dialog.
                if (xset_text_dialog(parent,
                                     rset->title ? rset->title : "Set Icon",
                                     false,
                                     rset->desc ? rset->desc : icon_desc,
                                     nullptr,
                                     rset->icon,
                                     &rset->icon,
                                     nullptr,
                                     false))
                {
                    if (rset->lock)
                        rset->keep_terminal = XSET_B_TRUE; // trigger save of changed icon
                    if (cb_func)
                        cb_func(item, cb_data);
                }
                break;
            case XSET_MENU_COLORDLG:
            {
                char* scolor;
                scolor = xset_color_dialog(parent, rset->title, rset->s);
                if (rset->s)
                    g_free(rset->s);
                rset->s = scolor;
                if (cb_func)
                    cb_func(item, cb_data);
            }
            break;
            default:
                if (cb_func)
                    cb_func(item, cb_data);
                else if (!set->lock)
                    xset_custom_activate(item, rset);
                break;
        }
    }

    if (rset->menu_style)
        autosave_request();
}

int
xset_msg_dialog(GtkWidget* parent, int action, const char* title, int buttons, const char* msg1,
                const char* msg2)
{
    /* action=
    GTK_MESSAGE_INFO,
    GTK_MESSAGE_WARNING,
    GTK_MESSAGE_QUESTION,
    GTK_MESSAGE_ERROR
    */
    GtkWidget* dlgparent = nullptr;

    if (parent)
        dlgparent = gtk_widget_get_toplevel(parent);

    if (!buttons)
        buttons = GTK_BUTTONS_OK;
    if (action == 0)
        action = GTK_MESSAGE_INFO;

    GtkWidget* dlg =
        gtk_message_dialog_new(GTK_WINDOW(dlgparent),
                               GtkDialogFlags(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                               (GtkMessageType)action,
                               (GtkButtonsType)buttons,
                               msg1,
                               nullptr);
    if (action == GTK_MESSAGE_INFO)
        xset_set_window_icon(GTK_WINDOW(dlg));
    gtk_window_set_role(GTK_WINDOW(dlg), "msg_dialog");

    if (msg2)
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), msg2, nullptr);
    if (title)
        gtk_window_set_title(GTK_WINDOW(dlg), title);

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
        g_free(all);
        return;
    }
    g_free(all);

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

    g_free(a);
    g_free(b);
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
        g_free(ret);
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
    int width = xset_get_int("main_icon", "x");
    int height = xset_get_int("main_icon", "y");
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
        char* str = g_strdup_printf("%d", allocation.width);
        xset_set("main_icon", "x", str);
        g_free(str);
        str = g_strdup_printf("%d", allocation.height);
        xset_set("main_icon", "y", str);
        g_free(str);
    }
    gtk_widget_destroy(icon_chooser);

    // remove busy cursor
    gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(parent)), nullptr);

    return icon;
}

bool
xset_text_dialog(GtkWidget* parent, const char* title, bool large, const char* msg1,
                 const char* msg2, const char* defstring, char** answer, const char* defreset,
                 bool edit_care)
{
    GtkTextIter iter;
    GtkTextIter siter;
    GtkAllocation allocation;
    int width;
    int height;
    GtkWidget* dlgparent = nullptr;

    if (parent)
        dlgparent = gtk_widget_get_toplevel(parent);
    GtkWidget* dlg = gtk_message_dialog_new(dlgparent ? GTK_WINDOW(dlgparent) : nullptr,
                                            GTK_DIALOG_MODAL,
                                            GTK_MESSAGE_QUESTION,
                                            GTK_BUTTONS_NONE,
                                            msg1,
                                            nullptr);
    xset_set_window_icon(GTK_WINDOW(dlg));
    gtk_window_set_role(GTK_WINDOW(dlg), "text_dialog");

    if (large)
    {
        width = xset_get_int("text_dlg", "s");
        height = xset_get_int("text_dlg", "z");
        if (width && height)
            gtk_window_set_default_size(GTK_WINDOW(dlg), width, height);
        else
            gtk_window_set_default_size(GTK_WINDOW(dlg), 600, 400);
        // gtk_widget_set_size_request( GTK_WIDGET( dlg ), 600, 400 );
    }
    else
    {
        width = xset_get_int("text_dlg", "x");
        height = xset_get_int("text_dlg", "y");
        if (width && height)
            gtk_window_set_default_size(GTK_WINDOW(dlg), width, -1);
        else
            gtk_window_set_default_size(GTK_WINDOW(dlg), 500, -1);
        // gtk_widget_set_size_request( GTK_WIDGET( dlg ), 500, 300 );
    }

    gtk_window_set_resizable(GTK_WINDOW(dlg), true);

    if (msg2)
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), msg2, nullptr);

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
    if (!g_strcmp0(title, "Set Icon") || !g_strcmp0(title, "Set Window Icon"))
    {
        btn_icon_choose = gtk_button_new_with_mnemonic("C_hoose");
        gtk_dialog_add_action_widget(GTK_DIALOG(dlg), btn_icon_choose, GTK_RESPONSE_ACCEPT);
        gtk_widget_set_focus_on_click(GTK_WIDGET(btn_icon_choose), false);
    }

    if (defreset)
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

    if (title)
        gtk_window_set_title(GTK_WINDOW(dlg), title);
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
                        g_free(*answer);
                    trim_ans = g_strdup(ans);
                    trim_ans = g_strstrip(trim_ans);
                    if (ans && trim_ans[0] != '\0')
                        *answer = g_filename_from_utf8(trim_ans, -1, nullptr, nullptr, nullptr);
                    else
                        *answer = nullptr;
                    if (ans)
                    {
                        g_free(trim_ans);
                        g_free(ans);
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
                g_free(icon);
                if (new_icon)
                {
                    gtk_text_buffer_set_text(buf, new_icon, -1);
                    g_free(new_icon);
                }
                exit_loop = true;
                break;
            case GTK_RESPONSE_NO:
                // btn_default clicked
                gtk_text_buffer_set_text(buf, defreset, -1);
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
        char* str = g_strdup_printf("%d", width);
        if (large)
            xset_set("text_dlg", "s", str);
        else
            xset_set("text_dlg", "x", str);
        g_free(str);
        str = g_strdup_printf("%d", height);
        if (large)
            xset_set("text_dlg", "z", str);
        else
            xset_set("text_dlg", "y", str);
        g_free(str);
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
            path = (char*)vfs_user_home_dir();
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
            g_free(path);
        }
    }

    int width = xset_get_int("file_dlg", "x");
    int height = xset_get_int("file_dlg", "y");
    if (width && height)
    {
        // filechooser won't honor default size or size request ?
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
        char* str = g_strdup_printf("%d", width);
        xset_set("file_dlg", "x", str);
        g_free(str);
        str = g_strdup_printf("%d", height);
        xset_set("file_dlg", "y", str);
        g_free(str);
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
            scolor = g_strdup(defcolor);
            break;
    }

    gtk_widget_destroy(dlg);
    return scolor;
}

static void
xset_builtin_tool_activate(char tool_type, XSet* set, GdkEventButton* event)
{
    XSet* set2;
    int p;
    char mode;
    PtkFileBrowser* file_browser = nullptr;
    FMMainWindow* main_window = fm_main_window_get_last_active();

    // set may be a submenu that doesn't match tool_type
    if (!(set && !set->lock && tool_type > XSET_TOOL_CUSTOM))
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
        case XSET_TOOL_DEVICES:
            set2 = xset_get_panel_mode(p, "show_devmon", mode);
            set2->b = set2->b == XSET_B_TRUE ? XSET_B_UNSET : XSET_B_TRUE;
            update_views_all_windows(nullptr, file_browser);
            break;
        case XSET_TOOL_BOOKMARKS:
            set2 = xset_get_panel_mode(p, "show_book", mode);
            set2->b = set2->b == XSET_B_TRUE ? XSET_B_UNSET : XSET_B_TRUE;
            update_views_all_windows(nullptr, file_browser);
            if (file_browser->side_book)
            {
                ptk_bookmark_view_chdir(GTK_TREE_VIEW(file_browser->side_book), file_browser, true);
                gtk_widget_grab_focus(GTK_WIDGET(file_browser->side_book));
            }
            break;
        case XSET_TOOL_TREE:
            set2 = xset_get_panel_mode(p, "show_dirtree", mode);
            set2->b = set2->b == XSET_B_TRUE ? XSET_B_UNSET : XSET_B_TRUE;
            update_views_all_windows(nullptr, file_browser);
            break;
        case XSET_TOOL_HOME:
            ptk_file_browser_go_home(nullptr, file_browser);
            break;
        case XSET_TOOL_DEFAULT:
            ptk_file_browser_go_default(nullptr, file_browser);
            break;
        case XSET_TOOL_UP:
            ptk_file_browser_go_up(nullptr, file_browser);
            break;
        case XSET_TOOL_BACK:
            ptk_file_browser_go_back(nullptr, file_browser);
            break;
        case XSET_TOOL_BACK_MENU:
            ptk_file_browser_show_history_menu(file_browser, true, event);
            break;
        case XSET_TOOL_FWD:
            ptk_file_browser_go_forward(nullptr, file_browser);
            break;
        case XSET_TOOL_FWD_MENU:
            ptk_file_browser_show_history_menu(file_browser, false, event);
            break;
        case XSET_TOOL_REFRESH:
            ptk_file_browser_refresh(nullptr, file_browser);
            break;
        case XSET_TOOL_NEW_TAB:
            ptk_file_browser_new_tab(nullptr, file_browser);
            break;
        case XSET_TOOL_NEW_TAB_HERE:
            ptk_file_browser_new_tab_here(nullptr, file_browser);
            break;
        case XSET_TOOL_SHOW_HIDDEN:
            set2 = xset_get_panel(p, "show_hidden");
            set2->b = set2->b == XSET_B_TRUE ? XSET_B_UNSET : XSET_B_TRUE;
            ptk_file_browser_show_hidden_files(file_browser, set2->b);
            break;
        case XSET_TOOL_SHOW_THUMB:
            main_window_toggle_thumbnails_all_windows();
            break;
        case XSET_TOOL_LARGE_ICONS:
            if (file_browser->view_mode != PTK_FB_ICON_VIEW)
            {
                xset_set_b_panel(p, "list_large", !file_browser->large_icons);
                on_popup_list_large(nullptr, file_browser);
            }
            break;
        default:
            LOG_WARN("xset_builtin_tool_activate invalid tool_type");
    }
}

const char*
xset_get_builtin_toolitem_label(unsigned char tool_type)
{
    if (tool_type < XSET_TOOL_DEVICES || tool_type >= XSET_TOOL_INVALID)
        return nullptr;
    return builtin_tool_name[tool_type];
}

static XSet*
xset_new_builtin_toolitem(char tool_type)
{
    if (tool_type < XSET_TOOL_DEVICES || tool_type >= XSET_TOOL_INVALID)
        return nullptr;

    XSet* set = xset_custom_new();
    set->tool = tool_type;
    set->task = XSET_B_UNSET;
    set->task_err = XSET_B_UNSET;
    set->task_out = XSET_B_UNSET;
    set->keep_terminal = XSET_B_UNSET;

    return set;
}

static bool
on_tool_icon_button_press(GtkWidget* widget, GdkEventButton* event, XSet* set)
{
    int job = -1;

    // LOG_INFO("on_tool_icon_button_press  {}   button = {}", set->menu_label, event->button);
    if (event->type != GDK_BUTTON_PRESS)
        return false;
    unsigned int keymod = (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK |
                                           GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK));

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
                        if (set->tool == XSET_TOOL_CUSTOM && set->menu_style == XSET_MENU_SUBMENU)
                        {
                            XSet* set_child = xset_is(set->child);
                            if (set_child)
                            {
                                // activate first item in custom submenu
                                xset_menu_cb(nullptr, set_child);
                            }
                        }
                        else if (set->tool == XSET_TOOL_CUSTOM)
                        {
                            // activate
                            xset_menu_cb(nullptr, set);
                        }
                        else if (set->tool == XSET_TOOL_BACK_MENU)
                            xset_builtin_tool_activate(XSET_TOOL_BACK, set, event);
                        else if (set->tool == XSET_TOOL_FWD_MENU)
                            xset_builtin_tool_activate(XSET_TOOL_FWD, set, event);
                        else if (set->tool)
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
                    job = XSET_JOB_COPY;
                    break;
                case GDK_MOD1_MASK:
                    // alt
                    job = XSET_JOB_CUT;
                    break;
                case GDK_SHIFT_MASK:
                    // shift
                    job = XSET_JOB_PASTE;
                    break;
                case (GDK_CONTROL_MASK | GDK_SHIFT_MASK):
                    // ctrl + shift
                    job = XSET_JOB_COMMAND;
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
                    if (set->tool == XSET_TOOL_CUSTOM &&
                        xset_get_int_set(set, "x") == XSET_CMD_SCRIPT)
                        job = XSET_JOB_EDIT;
                    else
                        job = XSET_JOB_PROP_CMD;
                    break;
                case GDK_CONTROL_MASK:
                    // ctrl
                    job = XSET_JOB_KEY;
                    break;
                case GDK_MOD1_MASK:
                    // alt
                    break;
                case GDK_SHIFT_MASK:
                    // shift
                    job = XSET_JOB_ICON;
                    break;
                case (GDK_CONTROL_MASK | GDK_SHIFT_MASK):
                    // ctrl + shift
                    job = XSET_JOB_REMOVE;
                    break;
                case (GDK_CONTROL_MASK | GDK_MOD1_MASK):
                    // ctrl + alt
                    job = XSET_JOB_PROP;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }

    if (job != -1)
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
    unsigned int keymod = (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK |
                                           GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK));
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
        if (set->tool == XSET_TOOL_CUSTOM)
        {
            // show custom submenu
            XSet* set_child;
            if (!(set && !set->lock && set->child && set->menu_style == XSET_MENU_SUBMENU &&
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
    char* str = g_strdup_printf("GtkWidget { padding-left: %dpx; padding-right: %dpx; "
                                "padding-top: %dpx; padding-bottom: %dpx; }",
                                left_right,
                                left_right,
                                top_bottom,
                                top_bottom);

    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(provider), str, -1, nullptr);
    GtkStyleContext* context = gtk_widget_get_style_context(widget);
    gtk_style_context_add_provider(context,
                                   GTK_STYLE_PROVIDER(provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_free(str);
}

static GtkWidget*
xset_add_toolitem(GtkWidget* parent, PtkFileBrowser* file_browser, GtkWidget* toolbar,
                  int icon_size, XSet* set, bool show_tooltips)
{
    GtkWidget* image = nullptr;
    GtkWidget* item = nullptr;
    GtkWidget* btn;
    XSet* set_next;
    char* new_menu_label = nullptr;
    GdkPixbuf* pixbuf = nullptr;
    char* icon_file = nullptr;
    int cmd_type;
    char* str;

    if (set->lock)
        return nullptr;

    if (set->tool == XSET_TOOL_NOT)
    {
        LOG_WARN("xset_add_toolitem set->tool == XSET_TOOL_NOT");
        set->tool = XSET_TOOL_CUSTOM;
    }

    // get real icon size from gtk icon size
    int icon_w, icon_h;
    gtk_icon_size_lookup((GtkIconSize)icon_size, &icon_w, &icon_h);
    int real_icon_size = icon_w > icon_h ? icon_w : icon_h;

    set->browser = file_browser;

    // builtin toolitems set shared_key on build
    if (set->tool >= XSET_TOOL_INVALID)
    {
        // looks like an unknown built-in toolitem from a future version - skip
        goto _next_toolitem;
    }
    if (set->tool > XSET_TOOL_CUSTOM && set->tool < XSET_TOOL_INVALID && !set->shared_key)
        set->shared_key = g_strdup(builtin_tool_shared_key[set->tool]);

    // builtin toolitems don't have menu_style set
    int menu_style;
    switch (set->tool)
    {
        case XSET_TOOL_DEVICES:
        case XSET_TOOL_BOOKMARKS:
        case XSET_TOOL_TREE:
        case XSET_TOOL_SHOW_HIDDEN:
        case XSET_TOOL_SHOW_THUMB:
        case XSET_TOOL_LARGE_ICONS:
            menu_style = XSET_MENU_CHECK;
            break;
        case XSET_TOOL_BACK_MENU:
        case XSET_TOOL_FWD_MENU:
            menu_style = XSET_MENU_SUBMENU;
            break;
        default:
            menu_style = set->menu_style;
    }

    const char* icon_name;
    icon_name = set->icon;
    if (!icon_name && set->tool == XSET_TOOL_CUSTOM)
    {
        // custom 'icon' file?
        icon_file = g_build_filename(xset_get_config_dir(), "scripts", set->name, "icon", nullptr);
        if (!std::filesystem::exists(icon_file))
        {
            g_free(icon_file);
            icon_file = nullptr;
        }
        else
            icon_name = icon_file;
    }

    char* menu_label;
    menu_label = set->menu_label;
    if (!menu_label && set->tool > XSET_TOOL_CUSTOM)
        menu_label = (char*)xset_get_builtin_toolitem_label(set->tool);

    if (!menu_style)
        menu_style = XSET_MENU_STRING;

    GtkWidget* ebox;
    GtkWidget* hbox;
    XSet* set_child;

    switch (menu_style)
    {
        case XSET_MENU_STRING:
            // normal item
            cmd_type = xset_get_int_set(set, "x");
            if (set->tool > XSET_TOOL_CUSTOM)
            {
                // builtin tool item
                if (icon_name)
                    image = xset_get_image(icon_name, (GtkIconSize)icon_size);
                else if (set->tool > XSET_TOOL_CUSTOM && set->tool < XSET_TOOL_INVALID)
                    image = xset_get_image(builtin_tool_icon[set->tool], (GtkIconSize)icon_size);
            }
            else if (!set->lock && cmd_type == XSET_CMD_APP)
            {
                // Application
                new_menu_label = xset_custom_get_app_name_icon(set, &pixbuf, real_icon_size);
            }
            else if (!set->lock && cmd_type == XSET_CMD_BOOKMARK)
            {
                // Bookmark
                pixbuf = xset_custom_get_bookmark_icon(set, real_icon_size);
                if (!(set->menu_label && set->menu_label[0]))
                    new_menu_label = g_strdup(set->z);
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
                new_menu_label = g_strdup(menu_label);

            // can't use gtk_tool_button_new because icon doesn't obey size
            // btn = GTK_WIDGET( gtk_tool_button_new( image, new_menu_label ) );
            btn = GTK_WIDGET(gtk_button_new());
            gtk_widget_show(image);
            gtk_button_set_image(GTK_BUTTON(btn), image);
            gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
            // These don't seem to do anything
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
                gtk_widget_set_tooltip_text(ebox, str);
                g_free(str);
            }
            g_free(new_menu_label);
            break;
        case XSET_MENU_CHECK:
            if (!icon_name && set->tool > XSET_TOOL_CUSTOM && set->tool < XSET_TOOL_INVALID)
                // builtin tool item
                image = xset_get_image(builtin_tool_icon[set->tool], (GtkIconSize)icon_size);
            else
                image =
                    xset_get_image(icon_name ? icon_name : "gtk-execute", (GtkIconSize)icon_size);

            // can't use gtk_tool_button_new because icon doesn't obey size
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
                gtk_widget_set_tooltip_text(ebox, str);
                g_free(str);
            }
            break;
        case XSET_MENU_SUBMENU:
            menu_label = nullptr;
            // create a tool button
            set_child = nullptr;
            if (set->tool == XSET_TOOL_CUSTOM)
                set_child = xset_is(set->child);

            if (!icon_name && set_child && set_child->icon)
                // take the user icon from the first item in the submenu
                icon_name = set_child->icon;
            else if (!icon_name && set->tool > XSET_TOOL_CUSTOM && set->tool < XSET_TOOL_INVALID)
                icon_name = builtin_tool_icon[set->tool];
            else if (!icon_name && set_child && set->tool == XSET_TOOL_CUSTOM)
            {
                // take the auto icon from the first item in the submenu
                cmd_type = xset_get_int_set(set_child, "x");
                switch (cmd_type)
                {
                    case XSET_CMD_APP:
                        // Application
                        new_menu_label = menu_label =
                            xset_custom_get_app_name_icon(set_child, &pixbuf, real_icon_size);
                        break;
                    case XSET_CMD_BOOKMARK:
                        // Bookmark
                        pixbuf = xset_custom_get_bookmark_icon(set_child, real_icon_size);
                        if (!(set_child->menu_label && set_child->menu_label[0]))
                            menu_label = set_child->z;
                        break;
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
                    case XSET_TOOL_BACK_MENU:
                        menu_label = (char*)builtin_tool_name[XSET_TOOL_BACK];
                        break;
                    case XSET_TOOL_FWD_MENU:
                        menu_label = (char*)builtin_tool_name[XSET_TOOL_FWD];
                        break;
                    case XSET_TOOL_CUSTOM:
                        if (set_child)
                            menu_label = set_child->menu_label;
                        break;
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

            // can't use gtk_tool_button_new because icon doesn't obey size
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
                gtk_widget_set_tooltip_text(ebox, str);
                g_free(str);
            }
            g_free(new_menu_label);

            // reset menu_label for below
            menu_label = set->menu_label;
            if (!menu_label && set->tool > XSET_TOOL_CUSTOM)
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
                gtk_widget_set_tooltip_text(ebox, str);
                g_free(str);
            }
            break;
        case XSET_MENU_SEP:
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
        default:
            return nullptr;
    }

    g_free(icon_file);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(item), -1);

// LOG_INFO("    set={}   set->next={}", set->name, set->next);
// next toolitem
_next_toolitem:
    if ((set_next = xset_is(set->next)))
    {
        // LOG_INFO("    NEXT {}", set_next->name);
        xset_add_toolitem(parent, file_browser, toolbar, icon_size, set_next, show_tooltips);
    }

    return item;
}

void
xset_fill_toolbar(GtkWidget* parent, PtkFileBrowser* file_browser, GtkWidget* toolbar,
                  XSet* set_parent, bool show_tooltips)
{
    const char default_tools[] = {XSET_TOOL_BOOKMARKS,
                                  XSET_TOOL_TREE,
                                  XSET_TOOL_NEW_TAB_HERE,
                                  XSET_TOOL_BACK_MENU,
                                  XSET_TOOL_FWD_MENU,
                                  XSET_TOOL_UP,
                                  XSET_TOOL_DEFAULT};
    int i;
    int stop_b4;
    XSet* set;
    XSet* set_target;

    // LOG_INFO("xset_fill_toolbar {}", set_parent->name);
    if (!(file_browser && toolbar && set_parent))
        return;

    set_parent->lock = true;
    set_parent->menu_style = XSET_MENU_SUBMENU;

    GtkIconSize icon_size = gtk_toolbar_get_icon_size(GTK_TOOLBAR(toolbar));

    XSet* set_child = nullptr;
    if (set_parent->child)
        set_child = xset_is(set_parent->child);
    if (!set_child)
    {
        // toolbar is empty - add default items
        set_child = xset_new_builtin_toolitem(
            strstr(set_parent->name, "tool_r") ? XSET_TOOL_REFRESH : XSET_TOOL_DEVICES);
        set_parent->child = g_strdup(set_child->name);
        set_child->parent = g_strdup(set_parent->name);
        if (!strstr(set_parent->name, "tool_r"))
        {
            if (strstr(set_parent->name, "tool_s"))
                stop_b4 = 2;
            else
                stop_b4 = G_N_ELEMENTS(default_tools);
            set_target = set_child;
            for (i = 0; i < stop_b4; i++)
            {
                set = xset_new_builtin_toolitem(default_tools[i]);
                xset_custom_insert_after(set_target, set);
                set_target = set;
            }
        }
    }

    xset_add_toolitem(parent, file_browser, toolbar, icon_size, set_child, show_tooltips);

    // These don't seem to do anything
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
    GtkIconTheme* theme = gtk_icon_theme_get_default();
    if (!theme)
        return;
    GError* error = nullptr;
    GdkPixbuf* icon = gtk_icon_theme_load_icon(theme, name, 48, (GtkIconLookupFlags)0, &error);
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

    mrand48();

    // set_last must be set (to anything)
    set_last = xset_get("separator");
    set_last->menu_style = XSET_MENU_SEP;

    // separator
    set = xset_get("separator");
    set->menu_style = XSET_MENU_SEP;

    // dev menu
    set = xset_set("dev_menu_remove", "lbl", "Remo_ve / Eject");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-disconnect");

    set = xset_set("dev_menu_unmount", "lbl", "_Unmount");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-remove");

    set = xset_set("dev_menu_reload", "lbl", "Re_load");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-disconnect");

    set = xset_set("dev_menu_sync", "lbl", "_Sync");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-save");

    set = xset_set("dev_menu_open", "lbl", "_Open");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-open");

    set = xset_set("dev_menu_tab", "lbl", "Open In _Tab");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-add");

    set = xset_set("dev_menu_mount", "lbl", "_Mount");
    xset_set_set(set, XSET_SET_SET_ICN, "drive-removable-media");

    set = xset_set("dev_menu_remount", "lbl", "Re_/mount");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-redo");

    set = xset_set("dev_menu_mark", "lbl", "_Bookmark");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-add");

    set = xset_set("dev_menu_root", "lbl", "_Root");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "dev_root_unmount dev_root_mount separator separator dev_root_check "
                 "dev_menu_format dev_menu_backup dev_menu_restore separator dev_root_fstab "
                 "dev_root_udevil");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-dialog-warning");

    set = xset_set("dev_root_mount", "lbl", "_Mount");
    xset_set_set(set, XSET_SET_SET_ICN, "drive-removable-media");
    xset_set_set(set, XSET_SET_SET_Z, "/usr/bin/udevil mount -o %o %v");

    set = xset_set("dev_root_unmount", "lbl", "_Unmount");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-remove");
    xset_set_set(set, XSET_SET_SET_Z, "/usr/bin/udevil umount %v");

    set = xset_set("dev_root_fstab", "lbl", "_Edit fstab");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-edit");

    set = xset_set("dev_root_udevil", "lbl", "Edit u_devil.conf");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-edit");

    set = xset_set("dev_prop", "lbl", "_Properties");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-properties");

    set = xset_set("dev_menu_settings", "lbl", "Setti_ngs");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-properties");
    set->menu_style = XSET_MENU_SUBMENU;

    // dev settings
    set = xset_set("dev_show", "lbl", "S_how");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "dev_show_internal_drives dev_show_empty dev_show_partition_tables dev_show_net "
                 "dev_show_file dev_ignore_udisks_hide dev_show_hide_volumes dev_dispname");

    set = xset_set("dev_show_internal_drives", "lbl", "_Internal Drives");
    set->menu_style = XSET_MENU_CHECK;
    set->b = geteuid() == 0 ? XSET_B_TRUE : XSET_B_FALSE;

    set = xset_set("dev_show_empty", "lbl", "_Empty Drives");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE; // geteuid() == 0 ? XSET_B_TRUE : XSET_B_UNSET;

    set = xset_set("dev_show_partition_tables", "lbl", "_Partition Tables");
    set->menu_style = XSET_MENU_CHECK;

    set = xset_set("dev_show_net", "lbl", "Mounted _Networks");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set("dev_show_file", "lbl", "Mounted _Other");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set("dev_show_hide_volumes", "lbl", "_Volumes...");
    xset_set_set(set, XSET_SET_SET_TITLE, "Show/Hide Volumes");
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "To force showing or hiding of some volumes, overriding other settings, you can specify "
        "the devices, volume labels, or device IDs in the space-separated list "
        "below.\n\nExample:  +/dev/sdd1 -Label With Space +ata-OCZ-part4\nThis would cause "
        "/dev/sdd1 and the OCZ device to be shown, and the volume with label \"Label With "
        "Space\" to be hidden.\n\nThere must be a space between entries and a plus or minus sign "
        "directly before each item.  This list is case-sensitive.\n\n");

    set = xset_set("dev_ignore_udisks_hide", "lbl", "Ignore _Hide Policy");
    set->menu_style = XSET_MENU_CHECK;

    set = xset_set("dev_dispname", "lbl", "_Display Name");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Set Display Name Format");
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "Enter device display name format:\n\nUse:\n\t%%v\tdevice filename (eg "
                 "sdd1)\n\t%%s\ttotal size (eg 800G)\n\t%%t\tfstype (eg ext4)\n\t%%l\tvolume "
                 "label (eg Label or [no media])\n\t%%m\tmount point if mounted, or "
                 "---\n\t%%i\tdevice ID\n\t%%n\tmajor:minor device numbers (eg 15:3)\n");
    xset_set_set(set, XSET_SET_SET_S, "%v %s %l %m");
    xset_set_set(set, XSET_SET_SET_Z, "%v %s %l %m");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-edit");

    set = xset_set("dev_menu_auto", "lbl", "_Auto Mount");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "dev_automount_optical dev_automount_removable dev_ignore_udisks_nopolicy "
                 "dev_automount_volumes dev_automount_dirs dev_auto_open dev_unmount_quit");

    set = xset_set("dev_automount_optical", "lbl", "Mount _Optical");
    set->b = geteuid() == 0 ? XSET_B_FALSE : XSET_B_TRUE;
    set->menu_style = XSET_MENU_CHECK;

    set = xset_set("dev_automount_removable", "lbl", "_Mount Removable");
    set->b = geteuid() == 0 ? XSET_B_FALSE : XSET_B_TRUE;
    set->menu_style = XSET_MENU_CHECK;

    set = xset_set("dev_automount_volumes", "lbl", "Mount _Volumes...");
    xset_set_set(set, XSET_SET_SET_TITLE, "Auto-Mount Volumes");
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "To force or prevent automounting of some volumes, overriding other settings, you can "
        "specify the devices, volume labels, or device IDs in the space-separated list "
        "below.\n\nExample:  +/dev/sdd1 -Label With Space +ata-OCZ-part4\nThis would cause "
        "/dev/sdd1 and the OCZ device to be auto-mounted when detected, and the volume with "
        "label \"Label With Space\" to be ignored.\n\nThere must be a space between entries and "
        "a plus or minus sign directly before each item.  This list is case-sensitive.\n\n");

    set = xset_set("dev_automount_dirs", "lbl", "Mount _Dirs...");
    xset_set_set(set, XSET_SET_SET_TITLE, "Automatic Mount Point Dirs");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "Enter the directory where SpaceFM should automatically create mount point directories "
        "for fuse and similar filesystems (%%a in handler commands).  This directory must be "
        "user-writable (do NOT use /media), and empty subdirectories will be removed.  If left "
        "blank, ~/.cache/spacefm/ (or $XDG_CACHE_HOME/spacefm/) is used.  The following "
        "variables are recognized: $USER $UID $HOME $XDG_RUNTIME_DIR $XDG_CACHE_HOME\n\nNote "
        "that some handlers or mount programs may not obey this setting.\n");

    set = xset_set("dev_auto_open", "lbl", "Open _Tab");
    set->b = XSET_B_TRUE;
    set->menu_style = XSET_MENU_CHECK;

    set = xset_set("dev_unmount_quit", "lbl", "_Unmount On Exit");
    set->b = XSET_B_UNSET;
    set->menu_style = XSET_MENU_CHECK;

    set = xset_set("dev_exec", "lbl", "Auto _Run");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "dev_exec_fs dev_exec_audio dev_exec_video separator dev_exec_insert "
                 "dev_exec_unmount dev_exec_remove");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-execute");

    set = xset_set("dev_exec_fs", "lbl", "On _Mount");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Auto Run On Mount");
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "Enter program or bash command line to be run automatically after a removable "
                 "drive or data disc is auto-mounted:\n\nUse:\n\t%%v\tdevice (eg "
                 "/dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_set("dev_exec_audio", "lbl", "On _Audio CD");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Auto Run On Audio CD");
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "Enter program or bash command line to be run automatically when an audio CD is "
                 "inserted in a qualified device:\n\nUse:\n\t%%v\tdevice (eg "
                 "/dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_set("dev_exec_video", "lbl", "On _Video DVD");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Auto Run On Video DVD");
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "Enter program or bash command line to be run automatically when a video DVD is "
                 "auto-mounted:\n\nUse:\n\t%%v\tdevice (eg /dev/sda1)\n\t%%l\tdevice "
                 "label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_set("dev_exec_insert", "lbl", "On _Insert");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Auto Run On Insert");
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "Enter program or bash command line to be run automatically when any device is "
                 "inserted:\n\nUse:\n\t%%v\tdevice added (eg /dev/sda1)\n\t%%l\tdevice "
                 "label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_set("dev_exec_unmount", "lbl", "On _Unmount");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Auto Run On Unmount");
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "Enter program or bash command line to be run automatically when any device is "
                 "unmounted by any means:\n\nUse:\n\t%%v\tdevice unmounted (eg "
                 "/dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_set("dev_exec_remove", "lbl", "On _Remove");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Auto Run On Remove");
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "Enter program or bash command line to be run automatically when any device is removed "
        "(ejection of media does not qualify):\n\nUse:\n\t%%v\tdevice removed (eg "
        "/dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_set("dev_ignore_udisks_nopolicy", "lbl", "Ignore _No Policy");
    set->menu_style = XSET_MENU_CHECK;

    set = xset_set("dev_mount_options", "lbl", "_Mount Options");
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "Enter your comma- or space-separated list of default mount options below (%%o in "
        "handlers).\n\nIn addition to regular options, you can also specify options to be added "
        "or removed for a specific filesystem type by using the form OPTION+FSTYPE or "
        "OPTION-FSTYPE.\n\nExample:  nosuid, sync+vfat, sync+ntfs, noatime, noatime-ext4\nThis "
        "will add nosuid and noatime for all filesystem types, add sync for vfat and ntfs only, "
        "and remove noatime for ext4.\n\nNote: Some options, such as nosuid, may be added by the "
        "mount program even if you don't include them.  Options in fstab take precedence.  "
        "pmount and some handlers may ignore options set here.");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Default Mount Options");
    xset_set_set(set, XSET_SET_SET_S, "noexec, nosuid, noatime");
    xset_set_set(set, XSET_SET_SET_Z, "noexec, nosuid, noatime");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-edit");

    set = xset_set("dev_remount_options", "z", "noexec, nosuid, noatime");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Re/mount With Options");
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "Device will be (re)mounted using the options below.\n\nIn addition to regular options, "
        "you can also specify options to be added or removed for a specific filesystem type by "
        "using the form OPTION+FSTYPE or OPTION-FSTYPE.\n\nExample:  nosuid, sync+vfat, "
        "sync+ntfs, noatime, noatime-ext4\nThis will add nosuid and noatime for all filesystem "
        "types, add sync for vfat and ntfs only, and remove noatime for ext4.\n\nNote: Some "
        "options, such as nosuid, may be added by the mount program even if you don't include "
        "them.  Options in fstab take precedence.  pmount ignores options set here.");
    xset_set_set(set, XSET_SET_SET_S, "noexec, nosuid, noatime");

    set = xset_set("dev_change", "lbl", "_Change Detection");
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "Enter your comma- or space-separated list of filesystems which should NOT be monitored "
        "for file changes.  This setting only affects non-block devices (such as nfs or fuse), "
        "and is usually used to prevent SpaceFM becoming unresponsive with network filesystems.  "
        "Loading of thumbnails and subdirectory sizes will also be disabled.");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Change Detection Blacklist");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-edit");
    set->s = g_strdup("cifs curlftpfs ftpfs fuse.sshfs nfs smbfs");
    set->z = g_strdup(set->s);

    set = xset_set("dev_fs_cnf", "label", "_Device Handlers");
    xset_set_set(set, XSET_SET_SET_ICON, "gtk-preferences");

    set = xset_set("dev_net_cnf", "label", "_Protocol Handlers");
    xset_set_set(set, XSET_SET_SET_ICON, "gtk-preferences");

    // dev icons
    set = xset_set("dev_icon", "lbl", "_Icon");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "dev_icon_internal_mounted dev_icon_internal_unmounted separator dev_icon_remove_mounted "
        "dev_icon_remove_unmounted separator dev_icon_optical_mounted dev_icon_optical_media "
        "dev_icon_optical_nomedia dev_icon_audiocd separator dev_icon_floppy_mounted "
        "dev_icon_floppy_unmounted separator dev_icon_network dev_icon_file");

    set = xset_set("dev_icon_audiocd", "lbl", "Audio CD");
    set->menu_style = XSET_MENU_ICON;
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-cdrom");

    set = xset_set("dev_icon_optical_mounted", "lbl", "Optical Mounted");
    set->menu_style = XSET_MENU_ICON;
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-cdrom");

    set = xset_set("dev_icon_optical_media", "lbl", "Optical Has Media");
    set->menu_style = XSET_MENU_ICON;
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-yes");

    set = xset_set("dev_icon_optical_nomedia", "lbl", "Optical No Media");
    set->menu_style = XSET_MENU_ICON;
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-close");

    set = xset_set("dev_icon_floppy_mounted", "lbl", "Floppy Mounted");
    set->menu_style = XSET_MENU_ICON;
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-floppy");

    set = xset_set("dev_icon_floppy_unmounted", "lbl", "Floppy Unmounted");
    set->menu_style = XSET_MENU_ICON;
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-floppy");

    set = xset_set("dev_icon_remove_mounted", "lbl", "Removable Mounted");
    set->menu_style = XSET_MENU_ICON;
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-add");

    set = xset_set("dev_icon_remove_unmounted", "lbl", "Removable Unmounted");
    set->menu_style = XSET_MENU_ICON;
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-remove");

    set = xset_set("dev_icon_internal_mounted", "lbl", "Internal Mounted");
    set->menu_style = XSET_MENU_ICON;
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-open");

    set = xset_set("dev_icon_internal_unmounted", "lbl", "Internal Unmounted");
    set->menu_style = XSET_MENU_ICON;
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-harddisk");

    set = xset_set("dev_icon_network", "lbl", "Mounted Network");
    set->menu_style = XSET_MENU_ICON;
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-network");

    set = xset_set("dev_icon_file", "lbl", "Mounted Other");
    set->menu_style = XSET_MENU_ICON;
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-file");

    set = xset_set("book_open", "lbl", "_Open");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-open");

    set = xset_set("book_settings", "lbl", "_Settings");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-properties");

    set = xset_set("book_icon", "lbl", "Bookmark _Icon");
    set->menu_style = XSET_MENU_ICON;
    // do not set a default icon for book_icon

    set = xset_set("book_menu_icon", "lbl", "Sub_menu Icon");
    set->menu_style = XSET_MENU_ICON;
    // do not set a default icon for book_menu_icon

    set = xset_set("book_show", "lbl", "_Show Bookmarks");
    set->menu_style = XSET_MENU_CHECK;
    xset_set_set(set, XSET_SET_SET_SHARED_KEY, "panel1_show_book");

    set = xset_set("book_add", "lbl", "New _Bookmark");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-jump-to");

    set = xset_set("main_book", "lbl", "_Bookmarks");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-directory");
    set->menu_style = XSET_MENU_SUBMENU;

    // Rename/Move Dialog
    set = xset_set("move_name", "lbl", "_Name");
    set->menu_style = XSET_MENU_CHECK;

    set = xset_set("move_filename", "lbl", "F_ilename");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set("move_parent", "lbl", "_Parent");
    set->menu_style = XSET_MENU_CHECK;

    set = xset_set("move_path", "lbl", "P_ath");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set("move_type", "lbl", "Typ_e");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set("move_target", "lbl", "Ta_rget");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set("move_template", "lbl", "Te_mplate");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set("move_option", "lbl", "_Option");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set, XSET_SET_SET_DESC, "move_copy move_link move_copyt move_linkt move_as_root");

    set = xset_set("move_copy", "lbl", "_Copy");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set("move_link", "lbl", "_Link");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set("move_copyt", "lbl", "Copy _Target");
    set->menu_style = XSET_MENU_CHECK;

    set = xset_set("move_linkt", "lbl", "Lin_k Target");
    set->menu_style = XSET_MENU_CHECK;

    set = xset_set("move_as_root", "lbl", "_As Root");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set("move_dlg_help", "lbl", "_Help");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-help");

    set = xset_set("move_dlg_confirm_create", "lbl", "_Confirm Create");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    // status bar
    set = xset_set("status_middle", "lbl", "_Middle Click");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set, XSET_SET_SET_DESC, "status_name status_path status_info status_hide");

    set = xset_set("status_name", "lbl", "Copy _Name");
    set->menu_style = XSET_MENU_RADIO;

    set = xset_set("status_path", "lbl", "Copy _Path");
    set->menu_style = XSET_MENU_RADIO;

    set = xset_set("status_info", "lbl", "File _Info");
    set->menu_style = XSET_MENU_RADIO;
    set->b = XSET_B_TRUE;

    set = xset_set("status_hide", "lbl", "_Hide Panel");
    set->menu_style = XSET_MENU_RADIO;

    // MAIN WINDOW MENUS

    // File
    set = xset_set("main_new_window", "lbl", "New _Window");
    xset_set_set(set, XSET_SET_SET_ICN, "spacefm");

    set = xset_set("main_root_window", "lbl", "R_oot Window");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-dialog-warning");

    set = xset_set("main_search", "lbl", "_File Search");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-find");

    set = xset_set("main_terminal", "lbl", "_Terminal");
    set->b = XSET_B_UNSET; // discovery notification

    set = xset_set("main_root_terminal", "lbl", "_Root Terminal");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-dialog-warning");

    // was previously used for 'Save Session' < 0.9.4 as XSET_MENU_NORMAL
    set = xset_set("main_save_session", "lbl", "Open _URL");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-network");
    xset_set_set(set, XSET_SET_SET_TITLE, "Open URL");
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "Enter URL in the "
                 "format:\n\tPROTOCOL://USERNAME:PASSWORD@HOST:PORT/SHARE\n\nExamples:\n\tftp://"
                 "mirrors.kernel.org\n\tsmb://user:pass@10.0.0.1:50/docs\n\tssh://"
                 "user@sys.domain\n\tmtp://\n\nIncluding a password is unsafe.  To bookmark a "
                 "URL, right-click on the mounted network in Devices and select Bookmark.\n");
    set->line = nullptr;

    set = xset_set("main_save_tabs", "lbl", "Save Ta_bs");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set("main_exit", "lbl", "E_xit");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-quit");

    // View
    set = xset_set("panel1_show", "lbl", "Panel _1");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set("panel2_show", "lbl", "Panel _2");
    set->menu_style = XSET_MENU_CHECK;

    set = xset_set("panel3_show", "lbl", "Panel _3");
    set->menu_style = XSET_MENU_CHECK;

    set = xset_set("panel4_show", "lbl", "Panel _4");
    set->menu_style = XSET_MENU_CHECK;

    set = xset_set("main_pbar", "lbl", "Panel _Bar");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set("main_focus_panel", "lbl", "F_ocus");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "panel_prev panel_next panel_hide panel_1 panel_2 panel_3 panel_4");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-go-forward");

    set = xset_set("panel_prev", "lbl", "_Prev");
    xset_set("panel_next", "lbl", "_Next");
    xset_set("panel_hide", "lbl", "_Hide");
    set = xset_set("panel_1", "lbl", "Panel _1");
    xset_set("panel_2", "lbl", "Panel _2");
    xset_set("panel_3", "lbl", "Panel _3");
    xset_set("panel_4", "lbl", "Panel _4");

    set = xset_set("main_auto", "lbl", "_Event Manager");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set, XSET_SET_SET_DESC, "auto_inst auto_win auto_pnl auto_tab evt_device");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-execute");

    set = xset_set("auto_inst", "lbl", "_Instance");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set, XSET_SET_SET_DESC, "evt_start evt_exit");

    set = xset_set("evt_start", "lbl", "_Startup");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Set Instance Startup Command");
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "Enter program or bash command line to be run automatically when a SpaceFM "
                 "instance starts:\n\nUse:\n\t%%e\tevent type  (evt_start)\n");

    set = xset_set("evt_exit", "lbl", "_Exit");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Set Instance Exit Command");
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "Enter program or bash command line to be run automatically when a SpaceFM "
                 "instance exits:\n\nUse:\n\t%%e\tevent type  (evt_exit)\n");

    set = xset_set("auto_win", "lbl", "_Window");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "evt_win_new evt_win_focus evt_win_move evt_win_click evt_win_key evt_win_close");

    set = xset_set("evt_win_new", "lbl", "_New");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Set New Window Command");
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "Enter program or bash command line to be run automatically whenever a new SpaceFM "
        "window is opened:\n\nUse:\n\t%%e\tevent type  (evt_win_new)\n\t%%w\twindow id  (see "
        "spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg $fm_pwd, etc) "
        "can be used in this command.");

    set = xset_set("evt_win_focus", "lbl", "_Focus");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Set Window Focus Command");
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "Enter program or bash command line to be run automatically whenever a SpaceFM "
                 "window gets focus:\n\nUse:\n\t%%e\tevent type  (evt_win_focus)\n\t%%w\twindow "
                 "id  (see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables "
                 "(eg $fm_pwd, etc) can be used in this command.");

    set = xset_set("evt_win_move", "lbl", "_Move/Resize");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Set Window Move/Resize Command");
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "Enter program or bash command line to be run automatically whenever a SpaceFM window is "
        "moved or resized:\n\nUse:\n\t%%e\tevent type  (evt_win_move)\n\t%%w\twindow id  (see "
        "spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg $fm_pwd, etc) "
        "can be used in this command.\n\nNote: This command may be run multiple times during "
        "resize.");

    set = xset_set("evt_win_click", "lbl", "_Click");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Set Click Command");
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "Enter program or bash command line to be run automatically whenever the mouse is "
        "clicked:\n\nUse:\n\t%%e\tevent type  (evt_win_click)\n\t%%w\twindow id  (see spacefm -s "
        "help)\n\t%%p\tpanel\n\t%%t\ttab\n\t%%b\tbutton  (mouse button pressed)\n\t%%m\tmodifier "
        " (modifier keys)\n\t%%f\tfocus  (element which received the click)\n\nExported bash "
        "variables (eg $fm_pwd, etc) can be used in this command when no asterisk prefix is "
        "used.\n\nPrefix your command with an asterisk (*) and conditionally return exit status "
        "0 to inhibit the default handler.  For example:\n*if [ \"%%b\" != \"2\" ];then exit 1; "
        "fi; spacefm -g --label \"\\nMiddle button was clicked in %%f\" --button ok &");

    set = xset_set("evt_win_key", "lbl", "_Keypress");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Set Window Keypress Command");
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "Enter program or bash command line to be run automatically whenever a key is "
        "pressed:\n\nUse:\n\t%%e\tevent type  (evt_win_key)\n\t%%w\twindow id  (see spacefm -s "
        "help)\n\t%%p\tpanel\n\t%%t\ttab\n\t%%k\tkey code  (key pressed)\n\t%%m\tmodifier  "
        "(modifier keys)\n\nExported bash variables (eg $fm_pwd, etc) can be used in this "
        "command when no asterisk prefix is used.\n\nPrefix your command with an asterisk (*) "
        "and conditionally return exit status 0 to inhibit the default handler.  For "
        "example:\n*if [ \"%%k\" != \"0xffc5\" ];then exit 1; fi; spacefm -g --label \"\\nKey "
        "F8 was pressed.\" --button ok &");

    set = xset_set("evt_win_close", "lbl", "Cl_ose");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Set Window Close Command");
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "Enter program or bash command line to be run automatically whenever a SpaceFM "
                 "window is closed:\n\nUse:\n\t%%e\tevent type  (evt_win_close)\n\t%%w\twindow "
                 "id  (see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables "
                 "(eg $fm_pwd, etc) can be used in this command.");

    set = xset_set("auto_pnl", "lbl", "_Panel");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set, XSET_SET_SET_DESC, "evt_pnl_focus evt_pnl_show evt_pnl_sel");

    set = xset_set("evt_pnl_focus", "lbl", "_Focus");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Set Panel Focus Command");
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "Enter program or bash command line to be run automatically whenever a panel "
                 "gets focus:\n\nUse:\n\t%%e\tevent type  (evt_pnl_focus)\n\t%%w\twindow id  "
                 "(see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg "
                 "$fm_pwd, etc) can be used in this command.");

    set = xset_set("evt_pnl_show", "lbl", "_Show");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Set Panel Show Command");
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "Enter program or bash command line to be run automatically whenever a panel or panel "
        "element is shown or hidden:\n\nUse:\n\t%%e\tevent type  (evt_pnl_show)\n\t%%w\twindow "
        "id  (see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\t%%f\tfocus  (element shown or "
        "hidden)\n\t%%v\tvisible  (1 or 0)\n\nExported bash variables (eg $fm_pwd, etc) can be "
        "used in this command.");

    set = xset_set("evt_pnl_sel", "lbl", "S_elect");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Set Panel Select Command");
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "Enter program or bash command line to be run automatically whenever the file selection "
        "changes:\n\nUse:\n\t%%e\tevent type  (evt_pnl_sel)\n\t%%w\twindow id  (see spacefm -s "
        "help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg $fm_pwd, etc) can be "
        "used in this command.\n\nPrefix your command with an asterisk (*) and conditionally "
        "return exit status 0 to inhibit the default handler.");

    set = xset_set("auto_tab", "lbl", "_Tab");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set, XSET_SET_SET_DESC, "evt_tab_new evt_tab_chdir evt_tab_focus evt_tab_close");

    set = xset_set("evt_tab_new", "lbl", "_New");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Set New Tab Command");
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "Enter program or bash command line to be run automatically whenever a new tab "
                 "is opened:\n\nUse:\n\t%%e\tevent type  (evt_tab_new)\n\t%%w\twindow id  (see "
                 "spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg "
                 "$fm_pwd, etc) can be used in this command.");

    set = xset_set("evt_tab_chdir", "lbl", "_Change Dir");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Set Tab Change Dir Command");
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "Enter program or bash command line to be run automatically whenever a tab changes to a "
        "different directory:\n\nUse:\n\t%%e\tevent type  (evt_tab_chdir)\n\t%%w\twindow id  "
        "(see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\t%%d\tnew directory\n\nExported bash "
        "variables (eg $fm_pwd, etc) can be used in this command.");

    set = xset_set("evt_tab_focus", "lbl", "_Focus");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Set Tab Focus Command");
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "Enter program or bash command line to be run automatically whenever a tab gets "
                 "focus:\n\nUse:\n\t%%e\tevent type  (evt_tab_focus)\n\t%%w\twindow id  (see "
                 "spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg "
                 "$fm_pwd, etc) can be used in this command.");

    set = xset_set("evt_tab_close", "lbl", "_Close");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Set Tab Close Command");
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "Enter program or bash command line to be run automatically whenever a tab is "
                 "closed:\n\nUse:\n\t%%e\tevent type  (evt_tab_close)\n\t%%w\twindow id  (see "
                 "spacefm -s help)\n\t%%p\tpanel\n\t%%t\tclosed tab");

    set = xset_set("evt_device", "lbl", "_Device");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Set Device Command");
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "Enter program or bash command line to be run automatically whenever a device "
                 "state changes:\n\nUse:\n\t%%e\tevent type  (evt_device)\n\t%%f\tdevice "
                 "file\n\t%%v\tchange  (added|removed|changed)\n");

    set = xset_set("main_title", "lbl", "Wi_ndow Title");
    set->menu_style = XSET_MENU_STRING;
    xset_set_set(set, XSET_SET_SET_TITLE, "Set Window Title Format");
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "Set window title format:\n\nUse:\n\t%%n\tcurrent directory name (eg "
        "bin)\n\t%%d\tcurrent "
        "directory path (eg /usr/bin)\n\t%%p\tcurrent panel number (1-4)\n\t%%t\tcurrent tab "
        "number\n\t%%P\ttotal number of panels visible\n\t%%T\ttotal number of tabs in current "
        "panel\n\t*\tasterisk shown if tasks running in window");
    xset_set_set(set, XSET_SET_SET_S, "%d");
    xset_set_set(set, XSET_SET_SET_Z, "%d");

    set = xset_set("main_icon", "lbl", "_Window Icon");
    set->menu_style = XSET_MENU_ICON;
    // Note: xset_text_dialog uses the title passed to know this is an
    // icon chooser, so it adds a Choose button.  If you change the title,
    // change xset_text_dialog.
    set->title = g_strdup("Set Window Icon");
    set->desc = g_strdup("Enter an icon name, icon file path, or stock item name:\n\nOr click "
                         "Choose to select an icon.  Not all icons may work properly due to "
                         "various issues.\n\nProvided alternate SpaceFM "
                         "icons:\n\tspacefm-[48|128]-[cube|pyramid]-[blue|green|red]\n\tspacefm-"
                         "48-folder-[blue|red]\n\nFor example: spacefm-48-pyramid-green");
    // x and y store global icon chooser dialog size

    set = xset_set("main_full", "lbl", "_Fullscreen");
    set->menu_style = XSET_MENU_CHECK;

    set = xset_set("main_design_mode", "lbl", "_Design Mode");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-help");

    set = xset_set("main_prefs", "lbl", "_Preferences");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-preferences");

    set = xset_set("main_tool", "lbl", "_Tool");
    set->menu_style = XSET_MENU_SUBMENU;

    set = xset_get("root_bar"); // in Preferences
    set->b = XSET_B_TRUE;

    set = xset_set("view_thumb", "lbl", "_Thumbnails (global)"); // in View|Panel View|Style
    set->menu_style = XSET_MENU_CHECK;

    // Plugins
    set = xset_set("plug_install", "lbl", "_Install");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set, XSET_SET_SET_DESC, "plug_ifile");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-add");

    set = xset_set("plug_ifile", "lbl", "_File");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-file");

    set = xset_set("plug_copy", "lbl", "_Import");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set, XSET_SET_SET_DESC, "plug_cfile separator plug_cverb");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-copy");

    set = xset_set("plug_cfile", "lbl", "_File");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-file");
    set = xset_set("plug_cverb", "lbl", "_Verbose");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set("plug_browse", "lbl", "_Browse");

    set = xset_set("plug_inc", "lbl", "In_cluded");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-media-play");

    // Help
    set = xset_set("main_about", "lbl", "_About");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-about");

    set = xset_set("main_dev", "lbl", "_Show Devices");
    xset_set_set(set, XSET_SET_SET_SHARED_KEY, "panel1_show_devmon");
    set->menu_style = XSET_MENU_CHECK;

    // Tasks
    set = xset_set("main_tasks", "lbl", "_Task Manager");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "task_show_manager task_hide_manager sep_t1 task_columns task_popups task_errors "
                 "task_queue");

    set = xset_set("task_col_status", "lbl", "_Status");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;
    set->x = g_strdup("0");   // column position
    set->y = g_strdup("130"); // column width

    set = xset_set("task_col_count", "lbl", "_Count");
    set->menu_style = XSET_MENU_CHECK;
    set->x = g_strdup_printf("%d", 1);

    set = xset_set("task_col_path", "lbl", "_Directory");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;
    set->x = g_strdup_printf("%d", 2);

    set = xset_set("task_col_file", "lbl", "_Item");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;
    set->x = g_strdup_printf("%d", 3);

    set = xset_set("task_col_to", "lbl", "_To");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;
    set->x = g_strdup_printf("%d", 4);

    set = xset_set("task_col_progress", "lbl", "_Progress");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;
    set->x = g_strdup_printf("%d", 5);
    set->y = g_strdup("100");

    set = xset_set("task_col_total", "lbl", "T_otal");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;
    set->x = g_strdup_printf("%d", 6);
    set->y = g_strdup("120");

    set = xset_set("task_col_started", "lbl", "Sta_rted");
    set->menu_style = XSET_MENU_CHECK;
    set->x = g_strdup_printf("%d", 7);

    set = xset_set("task_col_elapsed", "lbl", "_Elapsed");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;
    set->x = g_strdup_printf("%d", 8);
    set->y = g_strdup("70");

    set = xset_set("task_col_curspeed", "lbl", "C_urrent Speed");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;
    set->x = g_strdup_printf("%d", 9);

    set = xset_set("task_col_curest", "lbl", "Current Re_main");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;
    set->x = g_strdup_printf("%d", 10);

    set = xset_set("task_col_avgspeed", "lbl", "_Average Speed");
    set->menu_style = XSET_MENU_CHECK;
    set->x = g_strdup_printf("%d", 11);
    set->y = g_strdup("60");

    set = xset_set("task_col_avgest", "lbl", "A_verage Remain");
    set->menu_style = XSET_MENU_CHECK;
    set->x = g_strdup_printf("%d", 12);
    set->y = g_strdup("65");

    set = xset_set("task_col_reorder", "lbl", "Reor_der");

    set = xset_set("task_stop", "lbl", "_Stop");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-stop");
    set = xset_set("task_pause", "lbl", "Pa_use");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-media-pause");
    set = xset_set("task_que", "lbl", "_Queue");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-add");
    set = xset_set("task_resume", "lbl", "_Resume");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-media-play");
    set = xset_set("task_showout", "lbl", "Sho_w Output");

    set = xset_set("task_all", "lbl", "_All Tasks");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "task_stop_all task_pause_all task_que_all task_resume_all");

    set = xset_set("task_stop_all", "lbl", "_Stop");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-stop");
    set = xset_set("task_pause_all", "lbl", "Pa_use");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-media-pause");
    set = xset_set("task_que_all", "lbl", "_Queue");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-add");
    set = xset_set("task_resume_all", "lbl", "_Resume");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-media-play");

    set = xset_set("task_show_manager", "lbl", "Show _Manager");
    set->menu_style = XSET_MENU_RADIO;
    set->b = XSET_B_FALSE;
    // set->x  used for task man height >=0.9.4

    set = xset_set("task_hide_manager", "lbl", "Auto-_Hide Manager");
    set->menu_style = XSET_MENU_RADIO;
    set->b = XSET_B_TRUE;

    set = xset_set("task_columns", "lbl", "_Columns");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "task_col_count task_col_path task_col_file task_col_to task_col_progress task_col_total "
        "task_col_started task_col_elapsed task_col_curspeed task_col_curest task_col_avgspeed "
        "task_col_avgest sep_t2 task_col_reorder");

    set = xset_set("task_popups", "lbl", "_Popups");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "task_pop_all task_pop_top task_pop_above task_pop_stick sep_t6 task_pop_detail "
                 "task_pop_over task_pop_err");

    set = xset_set("task_pop_all", "lbl", "Popup _All Tasks");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_FALSE;

    set = xset_set("task_pop_top", "lbl", "Stay On _Top");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_FALSE;

    set = xset_set("task_pop_above", "lbl", "A_bove Others");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_FALSE;

    set = xset_set("task_pop_stick", "lbl", "All _Workspaces");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_FALSE;

    set = xset_set("task_pop_detail", "lbl", "_Detailed Stats");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_FALSE;

    set = xset_set("task_pop_over", "lbl", "_Overwrite Option");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set("task_pop_err", "lbl", "_Error Option");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set("task_errors", "lbl", "Err_ors");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set, XSET_SET_SET_DESC, "task_err_first task_err_any task_err_cont");

    set = xset_set("task_err_first", "lbl", "Stop If _First");
    set->menu_style = XSET_MENU_RADIO;
    set->b = XSET_B_TRUE;

    set = xset_set("task_err_any", "lbl", "Stop On _Any");
    set->menu_style = XSET_MENU_RADIO;
    set->b = XSET_B_FALSE;

    set = xset_set("task_err_cont", "lbl", "_Continue");
    set->menu_style = XSET_MENU_RADIO;
    set->b = XSET_B_FALSE;

    set = xset_set("task_queue", "lbl", "Qu_eue");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set, XSET_SET_SET_DESC, "task_q_new task_q_smart task_q_pause");

    set = xset_set("task_q_new", "lbl", "_Queue New Tasks");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set("task_q_smart", "lbl", "_Smart Queue");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set("task_q_pause", "lbl", "_Pause On Error");
    set->menu_style = XSET_MENU_CHECK;

    // PANELS COMMON
    xset_set("date_format", "s", "%Y-%m-%d %H:%M");

    set = xset_set("con_open", "lbl", "_Open");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-open");

    set = xset_set("open_execute", "lbl", "E_xecute");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-execute");

    set = xset_set("open_edit", "lbl", "Edi_t");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-edit");

    set = xset_set("open_edit_root", "lbl", "Edit As _Root");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-dialog-warning");

    set = xset_set("open_other", "lbl", "_Choose...");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-open");

    set = xset_set("open_hand", "lbl", "File _Handlers...");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-preferences");

    set = xset_set("open_all", "lbl", "Open With _Default"); // virtual

    set = xset_set("open_in_tab", "lbl", "In _Tab");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "opentab_new opentab_prev opentab_next opentab_1 opentab_2 opentab_3 opentab_4 "
                 "opentab_5 opentab_6 opentab_7 opentab_8 opentab_9 opentab_10");

    xset_set("opentab_new", "lbl", "N_ew");
    xset_set("opentab_prev", "lbl", "_Prev");
    xset_set("opentab_next", "lbl", "_Next");
    xset_set("opentab_1", "lbl", "Tab _1");
    xset_set("opentab_2", "lbl", "Tab _2");
    xset_set("opentab_3", "lbl", "Tab _3");
    xset_set("opentab_4", "lbl", "Tab _4");
    xset_set("opentab_5", "lbl", "Tab _5");
    xset_set("opentab_6", "lbl", "Tab _6");
    xset_set("opentab_7", "lbl", "Tab _7");
    xset_set("opentab_8", "lbl", "Tab _8");
    xset_set("opentab_9", "lbl", "Tab _9");
    xset_set("opentab_10", "lbl", "Tab 1_0");

    set = xset_set("open_in_panel", "lbl", "In _Panel");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "open_in_panelprev open_in_panelnext open_in_panel1 open_in_panel2 open_in_panel3 "
                 "open_in_panel4");

    xset_set("open_in_panelprev", "lbl", "_Prev");
    xset_set("open_in_panelnext", "lbl", "_Next");
    xset_set("open_in_panel1", "lbl", "Panel _1");
    xset_set("open_in_panel2", "lbl", "Panel _2");
    xset_set("open_in_panel3", "lbl", "Panel _3");
    xset_set("open_in_panel4", "lbl", "Panel _4");

    set = xset_set("arc_extract", "lbl", "_Extract");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-convert");

    set = xset_set("arc_extractto", "lbl", "Extract _To");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-convert");

    set = xset_set("arc_list", "lbl", "_List Contents");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-file");

    set = xset_set("arc_default", "lbl", "_Archive Defaults");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "arc_conf2 separator arc_def_open arc_def_ex arc_def_exto arc_def_list separator "
                 "arc_def_parent arc_def_write");

    set = xset_set("arc_def_open", "lbl", "_Open With App");
    set->menu_style = XSET_MENU_RADIO;

    set = xset_set("arc_def_ex", "lbl", "_Extract");
    set->menu_style = XSET_MENU_RADIO;
    set->b = XSET_B_TRUE;

    set = xset_set("arc_def_exto", "lbl", "Extract _To");
    set->menu_style = XSET_MENU_RADIO;

    set = xset_set("arc_def_list", "lbl", "_List Contents");
    set->menu_style = XSET_MENU_RADIO;

    set = xset_set("arc_def_parent", "lbl", "_Create Subdirectory");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set("arc_def_write", "lbl", "_Write Access");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set("arc_conf2", "label", "Archive _Handlers");
    xset_set_set(set, XSET_SET_SET_ICON, "gtk-preferences");

    set = xset_set("open_new", "lbl", "_New");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "new_file new_directory new_link new_archive separator tab_new tab_new_here new_bookmark");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-new");

    set = xset_set("new_file", "lbl", "_File");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-file");

    set = xset_set("new_directory", "lbl", "Dir_ectory");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-directory");

    set = xset_set("new_link", "lbl", "_Link");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-file");

    set = xset_set("new_bookmark", "lbl", "_Bookmark");
    xset_set_set(set, XSET_SET_SET_SHARED_KEY, "book_add");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-jump-to");

    set = xset_set("new_archive", "lbl", "_Archive");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-save-as");

    set = xset_get("arc_dlg");
    set->b = XSET_B_TRUE;   // Extract To - Create Subdirectory
    set->z = g_strdup("1"); // Extract To - Write Access

    set = xset_set("tab_new", "lbl", "_Tab");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-add");
    set = xset_set("tab_new_here", "lbl", "Tab _Here");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-add");

    set = xset_set("new_app", "lbl", "_Desktop Application");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-add");

    set = xset_set("con_go", "lbl", "_Go");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "go_back go_forward go_up go_home go_default go_set_default edit_canon separator "
                 "go_tab go_focus");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-go-forward");

    set = xset_set("go_back", "lbl", "_Back");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-go-back");
    set = xset_set("go_forward", "lbl", "_Forward");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-go-forward");
    set = xset_set("go_up", "lbl", "_Up");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-go-up");
    set = xset_set("go_home", "lbl", "_Home");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-home");
    set = xset_set("go_default", "lbl", "_Default");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-home");

    set = xset_set("go_set_default", "lbl", "_Set Default");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-save");

    set = xset_set("edit_canon", "lbl", "Re_al Path");

    set = xset_set("go_focus", "lbl", "Fo_cus");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "focus_path_bar focus_filelist focus_dirtree focus_book focus_device");

    set = xset_set("focus_path_bar", "lbl", "_Path Bar");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-dialog-question");
    set = xset_set("focus_filelist", "lbl", "_File List");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-file");
    set = xset_set("focus_dirtree", "lbl", "_Tree");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-directory");
    set = xset_set("focus_book", "lbl", "_Bookmarks");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-jump-to");
    set = xset_set("focus_device", "lbl", "De_vices");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-harddisk");

    set = xset_set("go_tab", "lbl", "_Tab");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "tab_prev tab_next tab_close tab_1 tab_2 tab_3 tab_4 tab_5 tab_6 tab_7 tab_8 tab_9 tab_10");

    xset_set("tab_prev", "lbl", "_Prev");
    xset_set("tab_next", "lbl", "_Next");
    set = xset_set("tab_close", "lbl", "_Close");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-close");
    xset_set("tab_1", "lbl", "Tab _1");
    xset_set("tab_2", "lbl", "Tab _2");
    xset_set("tab_3", "lbl", "Tab _3");
    xset_set("tab_4", "lbl", "Tab _4");
    xset_set("tab_5", "lbl", "Tab _5");
    xset_set("tab_6", "lbl", "Tab _6");
    xset_set("tab_7", "lbl", "Tab _7");
    xset_set("tab_8", "lbl", "Tab _8");
    xset_set("tab_9", "lbl", "Tab _9");
    xset_set("tab_10", "lbl", "Tab 1_0");

    set = xset_set("con_view", "lbl", "_View");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-preferences");

    set = xset_set("view_list_style", "lbl", "Styl_e");
    set->menu_style = XSET_MENU_SUBMENU;

    set = xset_set("view_columns", "lbl", "C_olumns");
    set->menu_style = XSET_MENU_SUBMENU;

    set = xset_set("view_reorder_col", "lbl", "_Reorder");

    set = xset_set("rubberband", "lbl", "_Rubberband Select");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set("view_sortby", "lbl", "_Sort");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "sortby_name sortby_size sortby_type sortby_perm sortby_owner sortby_date separator "
        "sortby_ascend sortby_descend separator sortx_natural sortx_case separator "
        "sortx_directories sortx_files sortx_mix separator sortx_hidfirst sortx_hidlast");

    set = xset_set("sortby_name", "lbl", "_Name");
    set->menu_style = XSET_MENU_RADIO;
    set = xset_set("sortby_size", "lbl", "_Size");
    set->menu_style = XSET_MENU_RADIO;
    set = xset_set("sortby_type", "lbl", "_Type");
    set->menu_style = XSET_MENU_RADIO;
    set = xset_set("sortby_perm", "lbl", "_Permission");
    set->menu_style = XSET_MENU_RADIO;
    set = xset_set("sortby_owner", "lbl", "_Owner");
    set->menu_style = XSET_MENU_RADIO;
    set = xset_set("sortby_date", "lbl", "_Modified");
    set->menu_style = XSET_MENU_RADIO;
    set = xset_set("sortby_ascend", "lbl", "_Ascending");
    set->menu_style = XSET_MENU_RADIO;
    set = xset_set("sortby_descend", "lbl", "_Descending");
    set->menu_style = XSET_MENU_RADIO;

    set = xset_set("sortx_natural", "lbl", "Nat_ural");
    set->menu_style = XSET_MENU_CHECK;
    set = xset_set("sortx_case", "lbl", "_Case Sensitive");
    set->menu_style = XSET_MENU_CHECK;
    set = xset_set("sortx_directories", "lbl", "Directories Fi_rst");
    set->menu_style = XSET_MENU_RADIO;
    set = xset_set("sortx_files", "lbl", "F_iles First");
    set->menu_style = XSET_MENU_RADIO;
    set = xset_set("sortx_mix", "lbl", "Mi_xed");
    set->menu_style = XSET_MENU_RADIO;
    set = xset_set("sortx_hidfirst", "lbl", "_Hidden First");
    set->menu_style = XSET_MENU_RADIO;
    set = xset_set("sortx_hidlast", "lbl", "Hidden _Last");
    set->menu_style = XSET_MENU_RADIO;

    set = xset_set("view_refresh", "lbl", "Re_fresh");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-refresh");

    set = xset_set("path_seek", "lbl", "Auto See_k");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set("path_hand", "lbl", "_Protocol Handlers");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-preferences");
    xset_set_set(set, XSET_SET_SET_SHARED_KEY, "dev_net_cnf");
    // set->s was custom protocol handler in sfm<=0.9.3 - retained

    set = xset_set("path_help", "lbl", "Path Bar _Help");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-help");

    // EDIT
    set = xset_set("edit_cut", "lbl", "Cu_t");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-cut");

    set = xset_set("edit_copy", "lbl", "_Copy");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-copy");

    set = xset_set("edit_paste", "lbl", "_Paste");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-paste");

    set = xset_set("edit_rename", "lbl", "_Rename");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-edit");

    set = xset_set("edit_delete", "lbl", "_Delete");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-delete");

    set = xset_set("edit_trash", "lbl", "_Trash");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-delete");

    set = xset_set("edit_submenu", "lbl", "_Actions");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "copy_name copy_parent copy_path separator paste_link paste_target paste_as separator "
        "copy_to "
        "move_to edit_root edit_hide separator select_all select_patt select_invert select_un");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-edit");

    set = xset_set("copy_name", "lbl", "Copy _Name");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-copy");

    set = xset_set("copy_path", "lbl", "Copy _Path");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-copy");

    set = xset_set("copy_parent", "lbl", "Copy Pa_rent");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-copy");

    set = xset_set("paste_link", "lbl", "Paste _Link");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-paste");

    set = xset_set("paste_target", "lbl", "Paste _Target");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-paste");

    set = xset_set("paste_as", "lbl", "Paste _As");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-paste");

    set = xset_set("copy_to", "lbl", "_Copy To");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set, XSET_SET_SET_DESC, "copy_loc copy_loc_last separator copy_tab copy_panel");

    set = xset_set("copy_loc", "lbl", "L_ocation");
    set = xset_set("copy_loc_last", "lbl", "L_ast Location");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-redo");

    set = xset_set("copy_tab", "lbl", "_Tab");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "copy_tab_prev copy_tab_next copy_tab_1 copy_tab_2 copy_tab_3 copy_tab_4 "
                 "copy_tab_5 copy_tab_6 copy_tab_7 copy_tab_8 copy_tab_9 copy_tab_10");

    xset_set("copy_tab_prev", "lbl", "_Prev");
    xset_set("copy_tab_next", "lbl", "_Next");
    xset_set("copy_tab_1", "lbl", "Tab _1");
    xset_set("copy_tab_2", "lbl", "Tab _2");
    xset_set("copy_tab_3", "lbl", "Tab _3");
    xset_set("copy_tab_4", "lbl", "Tab _4");
    xset_set("copy_tab_5", "lbl", "Tab _5");
    xset_set("copy_tab_6", "lbl", "Tab _6");
    xset_set("copy_tab_7", "lbl", "Tab _7");
    xset_set("copy_tab_8", "lbl", "Tab _8");
    xset_set("copy_tab_9", "lbl", "Tab _9");
    xset_set("copy_tab_10", "lbl", "Tab 1_0");

    set = xset_set("copy_panel", "lbl", "_Panel");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "copy_panel_prev copy_panel_next copy_panel_1 copy_panel_2 copy_panel_3 copy_panel_4");

    xset_set("copy_panel_prev", "lbl", "_Prev");
    xset_set("copy_panel_next", "lbl", "_Next");
    xset_set("copy_panel_1", "lbl", "Panel _1");
    xset_set("copy_panel_2", "lbl", "Panel _2");
    xset_set("copy_panel_3", "lbl", "Panel _3");
    xset_set("copy_panel_4", "lbl", "Panel _4");

    set = xset_set("move_to", "lbl", "_Move To");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set, XSET_SET_SET_DESC, "move_loc move_loc_last separator move_tab move_panel");

    set = xset_set("move_loc", "lbl", "_Location");
    set = xset_set("move_loc_last", "lbl", "L_ast Location");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-redo");
    set = xset_set("move_tab", "lbl", "_Tab");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "move_tab_prev move_tab_next move_tab_1 move_tab_2 move_tab_3 move_tab_4 "
                 "move_tab_5 move_tab_6 move_tab_7 move_tab_8 move_tab_9 move_tab_10");

    xset_set("move_tab_prev", "lbl", "_Prev");
    xset_set("move_tab_next", "lbl", "_Next");
    xset_set("move_tab_1", "lbl", "Tab _1");
    xset_set("move_tab_2", "lbl", "Tab _2");
    xset_set("move_tab_3", "lbl", "Tab _3");
    xset_set("move_tab_4", "lbl", "Tab _4");
    xset_set("move_tab_5", "lbl", "Tab _5");
    xset_set("move_tab_6", "lbl", "Tab _6");
    xset_set("move_tab_7", "lbl", "Tab _7");
    xset_set("move_tab_8", "lbl", "Tab _8");
    xset_set("move_tab_9", "lbl", "Tab _9");
    xset_set("move_tab_10", "lbl", "Tab 1_0");

    set = xset_set("move_panel", "lbl", "_Panel");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "move_panel_prev move_panel_next move_panel_1 move_panel_2 move_panel_3 move_panel_4");

    xset_set("move_panel_prev", "lbl", "_Prev");
    xset_set("move_panel_next", "lbl", "_Next");
    xset_set("move_panel_1", "lbl", "Panel _1");
    xset_set("move_panel_2", "lbl", "Panel _2");
    xset_set("move_panel_3", "lbl", "Panel _3");
    xset_set("move_panel_4", "lbl", "Panel _4");

    set = xset_set("edit_hide", "lbl", "_Hide");

    set = xset_set("select_all", "lbl", "_Select All");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-select-all");

    set = xset_set("select_un", "lbl", "_Unselect All");

    set = xset_set("select_invert", "lbl", "_Invert Selection");

    set = xset_set("select_patt", "lbl", "S_elect By Pattern");

    set = xset_set("edit_root", "lbl", "R_oot");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set, XSET_SET_SET_DESC, "root_copy_loc root_move2 root_delete");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-dialog-warning");

    set = xset_set("root_copy_loc", "lbl", "_Copy To");
    set = xset_set("root_move2", "lbl", "Move _To");
    set = xset_set("root_delete", "lbl", "_Delete");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-delete");

    // Properties
    set = xset_set("con_prop", "lbl", "Propert_ies");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set, XSET_SET_SET_DESC, "");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-properties");

    set = xset_set("prop_info", "lbl", "_Info");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-dialog-info");

    set = xset_set("prop_perm", "lbl", "_Permissions");
    xset_set_set(set, XSET_SET_SET_ICN, "dialog-password");

    set = xset_set("prop_quick", "lbl", "_Quick");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "perm_r perm_rw perm_rwx perm_r_r perm_rw_r perm_rw_rw perm_rwxr_x perm_rwxrwx "
                 "perm_r_r_r perm_rw_r_r perm_rw_rw_rw perm_rwxr_r perm_rwxr_xr_x perm_rwxrwxrwx "
                 "perm_rwxrwxrwt perm_unstick perm_stick perm_recurs");

    xset_set("perm_r", "lbl", "r--------");
    xset_set("perm_rw", "lbl", "rw-------");
    xset_set("perm_rwx", "lbl", "rwx------");
    xset_set("perm_r_r", "lbl", "r--r-----");
    xset_set("perm_rw_r", "lbl", "rw-r-----");
    xset_set("perm_rw_rw", "lbl", "rw-rw----");
    xset_set("perm_rwxr_x", "lbl", "rwxr-x---");
    xset_set("perm_rwxrwx", "lbl", "rwxrwx---");
    xset_set("perm_r_r_r", "lbl", "r--r--r--");
    xset_set("perm_rw_r_r", "lbl", "rw-r--r--");
    xset_set("perm_rw_rw_rw", "lbl", "rw-rw-rw-");
    xset_set("perm_rwxr_r", "lbl", "rwxr--r--");
    xset_set("perm_rwxr_xr_x", "lbl", "rwxr-xr-x");
    xset_set("perm_rwxrwxrwx", "lbl", "rwxrwxrwx");
    xset_set("perm_rwxrwxrwt", "lbl", "rwxrwxrwt");
    xset_set("perm_unstick", "lbl", "-t");
    xset_set("perm_stick", "lbl", "+t");

    set = xset_set("perm_recurs", "lbl", "_Recursive");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "perm_go_w perm_go_rwx perm_ugo_w perm_ugo_rx perm_ugo_rwx");

    xset_set("perm_go_w", "lbl", "go-w");
    xset_set("perm_go_rwx", "lbl", "go-rwx");
    xset_set("perm_ugo_w", "lbl", "ugo+w");
    xset_set("perm_ugo_rx", "lbl", "ugo+rX");
    xset_set("perm_ugo_rwx", "lbl", "ugo+rwX");

    set = xset_set("prop_root", "lbl", "_Root");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "rperm_rw rperm_rwx rperm_rw_r rperm_rw_rw rperm_rwxr_x rperm_rwxrwx rperm_rw_r_r "
                 "rperm_rw_rw_rw rperm_rwxr_r rperm_rwxr_xr_x rperm_rwxrwxrwx rperm_rwxrwxrwt "
                 "rperm_unstick rperm_stick rperm_recurs rperm_own");
    xset_set_set(set, XSET_SET_SET_ICN, "gtk-dialog-warning");

    xset_set("rperm_rw", "lbl", "rw-------");
    xset_set("rperm_rwx", "lbl", "rwx------");
    xset_set("rperm_rw_r", "lbl", "rw-r-----");
    xset_set("rperm_rw_rw", "lbl", "rw-rw----");
    xset_set("rperm_rwxr_x", "lbl", "rwxr-x---");
    xset_set("rperm_rwxrwx", "lbl", "rwxrwx---");
    xset_set("rperm_rw_r_r", "lbl", "rw-r--r--");
    xset_set("rperm_rw_rw_rw", "lbl", "rw-rw-rw-");
    xset_set("rperm_rwxr_r", "lbl", "rwxr--r--");
    xset_set("rperm_rwxr_xr_x", "lbl", "rwxr-xr-x");
    xset_set("rperm_rwxrwxrwx", "lbl", "rwxrwxrwx");
    xset_set("rperm_rwxrwxrwt", "lbl", "rwxrwxrwt");
    xset_set("rperm_unstick", "lbl", "-t");
    xset_set("rperm_stick", "lbl", "+t");

    set = xset_set("rperm_recurs", "lbl", "_Recursive");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(set,
                 XSET_SET_SET_DESC,
                 "rperm_go_w rperm_go_rwx rperm_ugo_w rperm_ugo_rx rperm_ugo_rwx");

    xset_set("rperm_go_w", "lbl", "go-w");
    xset_set("rperm_go_rwx", "lbl", "go-rwx");
    xset_set("rperm_ugo_w", "lbl", "ugo+w");
    xset_set("rperm_ugo_rx", "lbl", "ugo+rX");
    xset_set("rperm_ugo_rwx", "lbl", "ugo+rwX");

    set = xset_set("rperm_own", "lbl", "_Owner");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "own_myuser own_myuser_users own_user1 own_user1_users own_user2 own_user2_users own_root "
        "own_root_users own_root_myuser own_root_user1 own_root_user2 own_recurs");

    xset_set("own_myuser", "lbl", "myuser");
    xset_set("own_myuser_users", "lbl", "myuser:users");
    xset_set("own_user1", "lbl", "user1");
    xset_set("own_user1_users", "lbl", "user1:users");
    xset_set("own_user2", "lbl", "user2");
    xset_set("own_user2_users", "lbl", "user2:users");
    xset_set("own_root", "lbl", "root");
    xset_set("own_root_users", "lbl", "root:users");
    xset_set("own_root_myuser", "lbl", "root:myuser");
    xset_set("own_root_user1", "lbl", "root:user1");
    xset_set("own_root_user2", "lbl", "root:user2");

    set = xset_set("own_recurs", "lbl", "_Recursive");
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set(
        set,
        XSET_SET_SET_DESC,
        "rown_myuser rown_myuser_users rown_user1 rown_user1_users rown_user2 rown_user2_users "
        "rown_root rown_root_users rown_root_myuser rown_root_user1 rown_root_user2");

    xset_set("rown_myuser", "lbl", "myuser");
    xset_set("rown_myuser_users", "lbl", "myuser:users");
    xset_set("rown_user1", "lbl", "user1");
    xset_set("rown_user1_users", "lbl", "user1:users");
    xset_set("rown_user2", "lbl", "user2");
    xset_set("rown_user2_users", "lbl", "user2:users");
    xset_set("rown_root", "lbl", "root");
    xset_set("rown_root_users", "lbl", "root:users");
    xset_set("rown_root_myuser", "lbl", "root:myuser");
    xset_set("rown_root_user1", "lbl", "root:user1");
    xset_set("rown_root_user2", "lbl", "root:user2");

    // PANELS
    int p;
    for (p = 1; p < 5; p++)
    {
        set = xset_set_panel(p, "show_toolbox", "lbl", "_Toolbar");
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_TRUE;
        if (p != 1)
            xset_set_set(set, XSET_SET_SET_SHARED_KEY, "panel1_show_toolbox");

        set = xset_set_panel(p, "show_devmon", "lbl", "_Devices");
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_UNSET;
        if (p != 1)
            xset_set_set(set, XSET_SET_SET_SHARED_KEY, "panel1_show_devmon");

        set = xset_set_panel(p, "show_dirtree", "lbl", "T_ree");
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_TRUE;
        if (p != 1)
            xset_set_set(set, XSET_SET_SET_SHARED_KEY, "panel1_show_dirtree");

        set = xset_set_panel(p, "show_book", "lbl", "_Bookmarks");
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_UNSET;
        if (p != 1)
            xset_set_set(set, XSET_SET_SET_SHARED_KEY, "panel1_show_book");

        set = xset_set_panel(p, "show_sidebar", "lbl", "_Side Toolbar");
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_UNSET;
        if (p != 1)
            xset_set_set(set, XSET_SET_SET_SHARED_KEY, "panel1_show_sidebar");

        set = xset_set_panel(p, "list_detailed", "lbl", "_Detailed");
        set->menu_style = XSET_MENU_RADIO;
        set->b = XSET_B_TRUE;
        if (p != 1)
            xset_set_set(set, XSET_SET_SET_SHARED_KEY, "panel1_list_detailed");

        set = xset_set_panel(p, "list_icons", "lbl", "_Icons");
        set->menu_style = XSET_MENU_RADIO;
        if (p != 1)
            xset_set_set(set, XSET_SET_SET_SHARED_KEY, "panel1_list_icons");

        set = xset_set_panel(p, "list_compact", "lbl", "_Compact");
        set->menu_style = XSET_MENU_RADIO;
        if (p != 1)
            xset_set_set(set, XSET_SET_SET_SHARED_KEY, "panel1_list_compact");

        set = xset_set_panel(p, "list_large", "lbl", "_Large Icons");
        set->menu_style = XSET_MENU_CHECK;
        if (p != 1)
            xset_set_set(set, XSET_SET_SET_SHARED_KEY, "panel1_list_large");

        set = xset_set_panel(p, "show_hidden", "lbl", "_Hidden Files");
        set->menu_style = XSET_MENU_CHECK;
        if (p != 1)
            xset_set_set(set, XSET_SET_SET_SHARED_KEY, "panel1_show_hidden");

        set = xset_set_panel(p, "icon_tab", "lbl", "_Icon");
        set->menu_style = XSET_MENU_ICON;
        xset_set_set(set, XSET_SET_SET_ICN, "gtk-directory");

        set = xset_set_panel(p, "icon_status", "lbl", "_Icon");
        set->menu_style = XSET_MENU_ICON;
        xset_set_set(set, XSET_SET_SET_ICN, "gtk-yes");
        if (p != 1)
            xset_set_set(set, XSET_SET_SET_SHARED_KEY, "panel1_icon_status");

        set = xset_set_panel(p, "detcol_name", "lbl", "_Name");
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_TRUE;              // visible
        set->x = g_strdup_printf("%d", 0); // position

        set = xset_set_panel(p, "detcol_size", "lbl", "_Size");
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_TRUE;
        set->x = g_strdup_printf("%d", 1);
        if (p != 1)
            xset_set_set(set, XSET_SET_SET_SHARED_KEY, "panel1_detcol_size");

        set = xset_set_panel(p, "detcol_type", "lbl", "_Type");
        set->menu_style = XSET_MENU_CHECK;
        set->x = g_strdup_printf("%d", 2);
        if (p != 1)
            xset_set_set(set, XSET_SET_SET_SHARED_KEY, "panel1_detcol_type");

        set = xset_set_panel(p, "detcol_perm", "lbl", "_Permission");
        set->menu_style = XSET_MENU_CHECK;
        set->x = g_strdup_printf("%d", 3);
        if (p != 1)
            xset_set_set(set, XSET_SET_SET_SHARED_KEY, "panel1_detcol_perm");

        set = xset_set_panel(p, "detcol_owner", "lbl", "_Owner");
        set->menu_style = XSET_MENU_CHECK;
        set->x = g_strdup_printf("%d", 4);
        if (p != 1)
            xset_set_set(set, XSET_SET_SET_SHARED_KEY, "panel1_detcol_owner");

        set = xset_set_panel(p, "detcol_date", "lbl", "_Modified");
        set->menu_style = XSET_MENU_CHECK;
        set->x = g_strdup_printf("%d", 5);
        if (p != 1)
            xset_set_set(set, XSET_SET_SET_SHARED_KEY, "panel1_detcol_date");

        set = xset_get_panel(p, "sort_extra");
        set->b = XSET_B_TRUE;                         // sort_natural
        set->x = g_strdup_printf("%d", XSET_B_FALSE); // sort_case
        set->y = g_strdup("1"); // PTK_LIST_SORT_DIR_FIRST from ptk-file-list.hxx
        set->z = g_strdup_printf("%d", XSET_B_TRUE); // sort_hidden_first

        set = xset_set_panel(p, "book_fol", "lbl", "Follow _Dir");
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_TRUE;
        if (p != 1)
            xset_set_set(set, XSET_SET_SET_SHARED_KEY, "panel1_book_fol");
    }

    // speed
    set = xset_set("book_newtab", "lbl", "_New Tab");
    set->menu_style = XSET_MENU_CHECK;

    set = xset_set("book_single", "lbl", "_Single Click");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set("dev_newtab", "lbl", "_New Tab");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set("dev_single", "lbl", "_Single Click");
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    // mark all labels and icons as default
    GList* l;
    for (l = xsets; l; l = l->next)
    {
        if ((XSET(l->data))->lock)
        {
            if ((XSET(l->data))->in_terminal == XSET_B_TRUE)
                (XSET(l->data))->in_terminal = XSET_B_UNSET;
            if ((XSET(l->data))->keep_terminal == XSET_B_TRUE)
                (XSET(l->data))->keep_terminal = XSET_B_UNSET;
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
    GList* l;
    for (l = keysets; l; l = l->next)
    {
        if ((XSET(l->data))->key == key && (XSET(l->data))->keymod == keymod)
            return;
    }
    set->key = key;
    set->keymod = keymod;
}

static void
xset_default_keys()
{
    GList* l;

    // read all currently set or unset keys
    keysets = nullptr;
    for (l = xsets; l; l = l->next)
    {
        if ((XSET(l->data))->key)
            keysets = g_list_prepend(keysets, XSET(l->data));
    }

    def_key("tab_prev", GDK_KEY_Tab, (GDK_SHIFT_MASK | GDK_CONTROL_MASK));
    def_key("tab_next", GDK_KEY_Tab, GDK_CONTROL_MASK);
    def_key("tab_close", GDK_KEY_w, GDK_CONTROL_MASK);
    def_key("tab_new", GDK_KEY_t, GDK_CONTROL_MASK);
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

    if (keysets)
        g_list_free(keysets);
}
