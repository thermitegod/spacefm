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

#include <csignal>

#include <string>
#include <filesystem>

#include <vector>

#include <stdio.h>

#include <fmt/format.h>

#include <glibmm.h>
#include <glibmm/convert.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "types.hxx"

#include "main-window.hxx"
#include "window-reference.hxx"

#include "vfs/vfs-app-desktop.hxx"
#include "vfs/vfs-user-dir.hxx"
#include "vfs/vfs-thumbnail-loader.hxx"

#include "ptk/ptk-utils.hxx"
#include "ptk/ptk-app-chooser.hxx"
#include "ptk/ptk-location-view.hxx"

#include "settings/app.hxx"
#include "settings/etc.hxx"
#include "settings/load_etc.hxx"

#include "program-timer.hxx"
#include "autosave.hxx"

#include "find-files.hxx"
#include "pref-dialog.hxx"
#include "socket.hxx"
#include "utils.hxx"
#include "settings.hxx"

#include "bookmarks.hxx"

static bool folder_initialized = false;
static bool daemon_initialized = false;

// clang-format off
static GOptionEntry opt_entries[] =
{
    {"new-tab", 't', 0, G_OPTION_ARG_NONE, &cli_flags.new_tab, "Open directories in new tab of last window (default)", nullptr},
    {"reuse-tab", 'r', 0, G_OPTION_ARG_NONE, &cli_flags.reuse_tab, "Open directory in current tab of last used window", nullptr},
    {"no-saved-tabs", 'n', 0, G_OPTION_ARG_NONE, &cli_flags.no_tabs, "Don't load saved tabs", nullptr},
    {"new-window", 'w', 0, G_OPTION_ARG_NONE, &cli_flags.new_window, "Open directories in new window", nullptr},
    {"panel", 'p', 0, G_OPTION_ARG_INT, &cli_flags.panel, "Open directories in panel 'P' (1-4)", "P"},
    {"daemon-mode", 'd', 0, G_OPTION_ARG_NONE, &cli_flags.daemon_mode, "Run as a daemon", nullptr},
    {"config", 'c', 0, G_OPTION_ARG_STRING, &cli_flags.config_dir, "Use DIR as configuration directory", "DIR"},
    {"disable-git", 'G', 0, G_OPTION_ARG_NONE, &cli_flags.disable_git_settings, "Don't use git to keep session history", nullptr},
    {"find-files", 'f', 0, G_OPTION_ARG_NONE, &cli_flags.find_files, "Show File Search", nullptr},
    {"socket-cmd", 's', 0, G_OPTION_ARG_NONE, &cli_flags.socket_cmd, "Send a socket command (See -s help)", nullptr},
    {"version", 'v', 0, G_OPTION_ARG_NONE, &cli_flags.version_opt, "Show version information", nullptr},
    {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &cli_flags.files, nullptr, "[DIR | FILE | URL]..."},
    {GOptionEntry()}
};
// clang-format on

static bool handle_parsed_commandline_args();
static void open_file(const std::string& path);

static void
init_folder()
{
    if (folder_initialized)
        return;

    vfs_volume_init();
    vfs_thumbnail_init();

    vfs_mime_type_set_icon_size_big(app_settings.get_icon_size_big());
    vfs_mime_type_set_icon_size_small(app_settings.get_icon_size_small());

    vfs_file_info_set_thumbnail_size_big(app_settings.get_icon_size_big());
    vfs_file_info_set_thumbnail_size_small(app_settings.get_icon_size_small());

    folder_initialized = true;
}

[[noreturn]] static void
exit_from_signal(int sig)
{
    gtk_main_quit();
    std::exit(sig);
}

static void
init_daemon()
{
    if (daemon_initialized)
        return;

    init_folder();

    signal(SIGPIPE, SIG_IGN);
    signal(SIGHUP, exit_from_signal);
    signal(SIGINT, exit_from_signal);
    signal(SIGTERM, exit_from_signal);

    daemon_initialized = true;
}

static void
open_in_tab(FMMainWindow** main_window, const char* real_path)
{
    xset_t set;
    panel_t panel;
    // create main window if needed
    if (!*main_window)
    {
        // initialize things required by folder view
        if (!cli_flags.daemon_mode)
            init_folder();

        // preload panel?
        if (cli_flags.panel > 0 && cli_flags.panel < 5)
        { // user specified panel
            panel = cli_flags.panel;
        }
        else
        {
            // use first visible panel
            for (panel_t p: PANELS)
            {
                if (xset_get_b_panel(p, XSetPanel::SHOW))
                {
                    panel = p;
                    break;
                }
            }
        }

        // set panel to load real_path on window creation
        set = xset_get_panel(panel, XSetPanel::SHOW);
        set->ob1 = ztd::strdup(real_path);
        set->b = XSetB::XSET_B_TRUE;

        // create new window
        fm_main_window_store_positions(nullptr);
        *main_window = FM_MAIN_WINDOW_REINTERPRET(fm_main_window_new());
    }
    else
    {
        // existing window
        bool tab_added = false;
        if (cli_flags.panel > 0 && cli_flags.panel < 5)
        {
            // change to user-specified panel
            if (!gtk_notebook_get_n_pages(GTK_NOTEBOOK((*main_window)->panel[cli_flags.panel - 1])))
            {
                // set panel to load real_path on panel load
                set = xset_get_panel(cli_flags.panel, XSetPanel::SHOW);
                set->ob1 = ztd::strdup(real_path);
                tab_added = true;
                set->b = XSetB::XSET_B_TRUE;
                show_panels_all_windows(nullptr, *main_window);
            }
            else if (!gtk_widget_get_visible((*main_window)->panel[cli_flags.panel - 1]))
            {
                // show panel
                set = xset_get_panel(cli_flags.panel, XSetPanel::SHOW);
                set->b = XSetB::XSET_B_TRUE;
                show_panels_all_windows(nullptr, *main_window);
            }
            (*main_window)->curpanel = cli_flags.panel;
            (*main_window)->notebook = (*main_window)->panel[cli_flags.panel - 1];
        }
        if (!tab_added)
        {
            if (cli_flags.reuse_tab)
            {
                main_window_open_path_in_current_tab(*main_window, real_path);
                cli_flags.reuse_tab = false;
            }
            else
                fm_main_window_add_new_tab(*main_window, real_path);
        }
    }
    gtk_window_present(GTK_WINDOW(*main_window));
}

static bool
handle_parsed_commandline_args()
{
    bool ret = true;
    char* default_files[2] = {nullptr, nullptr};

    app_settings.set_load_saved_tabs(!cli_flags.no_tabs);

    // no files were specified from cli
    if (!cli_flags.files)
        cli_flags.files = default_files;

    // get the last active window on this desktop, if available
    FMMainWindow* main_window = nullptr;
    if (cli_flags.new_tab || cli_flags.reuse_tab)
    {
        main_window = fm_main_window_get_on_current_desktop();
        // LOG_INFO("fm_main_window_get_on_current_desktop = {:p}  {} {}",
        //          (void*)main_window,
        //          cli_flags.new_tab ? "new_tab" : "",
        //          cli_flags.reuse_tab ? "reuse_tab" : "");
    }

    if (cli_flags.find_files)
    {
        // find files
        init_folder();

        std::vector<const char*> search_dirs;
        char** dir;
        for (dir = cli_flags.files; *dir; ++dir)
        {
            if (!*dir)
                continue;
            if (std::filesystem::is_directory(*dir))
                search_dirs.push_back(*dir);
        }

        fm_find_files(search_dirs);
        cli_flags.find_files = false;
    }
    else if (cli_flags.daemon_mode)
    {
        // start daemon mode
        init_daemon();
    }
    else if (cli_flags.files != default_files)
    {
        // open files passed in command line arguments
        ret = false;
        char** file;
        for (file = cli_flags.files; *file; ++file)
        {
            std::string real_path;

            // skip empty string
            if (!**file)
                continue;

            real_path = std::filesystem::absolute(*file);

            if (std::filesystem::is_directory(real_path))
            {
                open_in_tab(&main_window, real_path.c_str());
                ret = true;
            }
            else if (std::filesystem::exists(real_path.c_str()))
            {
                struct stat statbuf;
                if (stat(real_path.c_str(), &statbuf) == 0 && S_ISBLK(statbuf.st_mode))
                {
                    // open block device eg /dev/sda1
                    if (!main_window)
                    {
                        open_in_tab(&main_window, "/");
                        ptk_location_view_open_block(real_path.c_str(), false);
                    }
                    else
                        ptk_location_view_open_block(real_path.c_str(), true);
                    ret = true;
                    gtk_window_present(GTK_WINDOW(main_window));
                }
                else
                {
                    open_file(real_path);
                }
            }
            else if ((*file[0] != '/' && strstr(*file, ":/")) || ztd::startswith(*file, "//"))
            {
                if (main_window)
                    main_window_open_network(main_window, *file, true);
                else
                {
                    open_in_tab(&main_window, "/");
                    main_window_open_network(main_window, *file, false);
                }
                ret = true;
                gtk_window_present(GTK_WINDOW(main_window));
            }
            else
            {
                const std::string err_msg = fmt::format("File does not exist:\n\n{}", real_path);
                ptk_show_error(nullptr, "Error", err_msg);
            }
        }
    }
    else if (!check_socket_daemon())
    {
        // no files specified, just create window with default tabs
        if (!main_window)
        {
            // initialize things required by folder view
            if (!cli_flags.daemon_mode)
                init_folder();
            fm_main_window_store_positions(nullptr);
            main_window = FM_MAIN_WINDOW_REINTERPRET(fm_main_window_new());
        }
        gtk_window_present(GTK_WINDOW(main_window));

        if (cli_flags.panel > 0 && cli_flags.panel < 5)
        {
            // user specified a panel with no file, let's show the panel
            if (!gtk_widget_get_visible(main_window->panel[cli_flags.panel - 1]))
            {
                // show panel
                xset_t set;
                set = xset_get_panel(cli_flags.panel, XSetPanel::SHOW);
                set->b = XSetB::XSET_B_TRUE;
                show_panels_all_windows(nullptr, main_window);
            }
            focus_panel(nullptr, (void*)main_window, cli_flags.panel);
        }
    }

    // LOG_INFO("    handle_parsed_commandline_args mw = {:p}", main_window);

    if (cli_flags.files != default_files)
        g_strfreev(cli_flags.files);
    cli_flags.files = nullptr;

    return ret;
}

static void
tmp_clean()
{
    const std::string tmp = xset_get_user_tmp_dir();
    std::filesystem::remove_all(tmp);
    LOG_INFO("Removed {}", tmp);
}

int
main(int argc, char* argv[])
{
    // logging init
    ztd::Logger->initialize();

    // start program timer
    program_timer::start();

    // FIXME - This directs all writes to stderr into /dev/null, should try
    // and only have writes from ffmpeg get redirected.
    //
    // This is only done because ffmpeg, through libffmpegthumbnailer,
    // will output its warnings/errors when files are having their thumbnails generated. Which
    // floods stderr with messages that the user can do nothing about, such as
    // 'deprecated pixel format used, make sure you did set range correctly'
    //
    // An alternative solution to this would be to use Glib::spawn_command_line_sync
    // and redirecting that output to /dev/null, but that would involve using the
    // libffmpegthumbnailer CLI program and not the C++ interface. Not a solution that I want to do.
    //
    // In closing stderr is not used by this program for output, and this should only affect ffmpeg.
    freopen("/dev/null", "w", stderr);

    // load /etc/spacefm.conf
    load_etc_conf();

    // separate instance options
    if (argc > 1)
    {
        // socket_command?
        if (ztd::same(argv[1], "-s") || ztd::same(argv[1], "--socket-cmd"))
        {
            if (argv[2] && (ztd::same(argv[2], "help") || ztd::same(argv[2], "--help")))
            {
                fmt::print("For help run, 'man spacefm-socket'\n");
                std::exit(EXIT_SUCCESS);
            }
            std::string sock_reply;
            int ret = send_socket_command(argc, argv, sock_reply);
            if (!sock_reply.empty())
                fmt::print("{}\n", sock_reply);
            std::exit(ret);
        }
    }

    // initialize GTK+ and parse the command line arguments
    GError* err = nullptr;
    if (!gtk_init_with_args(&argc, &argv, "", opt_entries, nullptr, &err))
    {
        LOG_INFO("{}", err->message);
        g_error_free(err);
        std::exit(EXIT_FAILURE);
    }

    // ref counter needs to know if in daemon_mode
    WindowReference::set_daemon(cli_flags.daemon_mode);

    // socket command with other options?
    if (cli_flags.socket_cmd)
    {
        LOG_ERROR("socket-cmd must be first option");
        std::exit(EXIT_FAILURE);
    }

    // --disable-git
    etc_settings.set_git_backed_settings(!cli_flags.disable_git_settings);

    // --version
    if (cli_flags.version_opt)
    {
        fmt::print("{} {}\n", PACKAGE_NAME_FANCY, PACKAGE_VERSION);
        std::exit(EXIT_SUCCESS);
    }

    // ensure that there is only one instance of spacefm.
    // if there is an existing instance, command line arguments
    // will be passed to the existing instance, and exit() will be called here.
    single_instance_check();
    // If we reach this point, we are the first instance.
    // Subsequent processes will exit() inside single_instance_check
    // and will not reach here.

    // initialize the file alteration monitor
    if (!vfs_file_monitor_init())
    {
        ptk_show_error(nullptr,
                       "Error",
                       "Error: Unable to initialize inotify file change monitor.\n\nDo you have "
                       "an inotify-capable kernel?");
        vfs_file_monitor_clean();
        std::exit(EXIT_FAILURE);
    }

    // Seed RNG
    // using the current time is a good enough seed
    std::srand(std::time(nullptr));

    // Initialize our mime-type system
    vfs_mime_type_init();

    // Set current config dir
    vfs_user_set_config_dir(cli_flags.config_dir);

    // load config file
    load_settings();

    // load user bookmarks
    load_bookmarks();

    // start autosave thread
    autosave_init(autosave_settings);

    std::atexit(ztd::Logger->shutdown);
    std::atexit(free_settings);
    std::atexit(tmp_clean);
    std::atexit(autosave_terminate);
    std::atexit(vfs_file_monitor_clean);
    std::atexit(vfs_mime_type_clean);
    std::atexit(vfs_volume_finalize);
    std::atexit(single_instance_finalize);
    std::atexit(save_bookmarks);

    main_window_event(nullptr, nullptr, XSetName::EVT_START, 0, 0, nullptr, 0, 0, 0, false);

    // handle the parsed result of command line args
    if (handle_parsed_commandline_args())
    {
        app_settings.set_load_saved_tabs(true);

        // run the main loop
        gtk_main();
    }

    main_window_event(nullptr, nullptr, XSetName::EVT_EXIT, 0, 0, nullptr, 0, 0, 0, false);

    std::exit(EXIT_SUCCESS);
}

static void
open_file(const std::string& path)
{
    VFSFileInfo* file = vfs_file_info_new();
    vfs_file_info_get(file, path);
    VFSMimeType* mime_type = vfs_file_info_get_mime_type(file);

    char* app_name = vfs_mime_type_get_default_action(mime_type);
    if (!app_name)
    {
        app_name = ptk_choose_app_for_mime_type(nullptr, mime_type, true, true, true, false);
        if (!app_name)
        {
            LOG_ERROR("no application to open file: {}", path);
            return;
        }
    }

    VFSAppDesktop desktop(app_name);

    std::string open_file = path;
    std::vector<std::string> open_files;
    open_files.push_back(open_file);

    try
    {
        desktop.open_files(vfs_current_dir(), open_files);
    }
    catch (const VFSAppDesktopException& e)
    {
        const std::string disp_path = Glib::filename_display_name(path);
        const std::string msg =
            fmt::format("Unable to open file:\n\"{}\"\n{}", disp_path, e.what());
        ptk_show_error(nullptr, "Error", msg);
    }

    free(app_name);
    vfs_mime_type_unref(mime_type);
    vfs_file_info_unref(file);
}
