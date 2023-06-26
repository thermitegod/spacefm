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
#include <string_view>

#include <format>

#include <filesystem>

#include <span>

#include <array>
#include <vector>

#include <chrono>

#include <cstdio>

#include <fmt/format.h>

#include <glibmm.h>
#include <glibmm/convert.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "types.hxx"

#include "main-window.hxx"

#include "vfs/vfs-app-desktop.hxx"
#include "vfs/vfs-user-dirs.hxx"
#include "vfs/vfs-thumbnail-loader.hxx"

#include "ptk/ptk-app-chooser.hxx"
#include "ptk/ptk-error.hxx"
#include "ptk/ptk-location-view.hxx"

#include "settings/app.hxx"

#include "program-timer.hxx"
#include "autosave.hxx"

#include "find-files.hxx"
#include "pref-dialog.hxx"
#include "socket.hxx"
#include "settings.hxx"

#include "bookmarks.hxx"

static bool folder_initialized = false;

// clang-format off
static std::array<GOptionEntry, 13> opt_entries =
{
    GOptionEntry{"new-tab", 't', 0, G_OPTION_ARG_NONE, &cli_flags.new_tab, "Open directories in new tab of last window (default)", nullptr},
    GOptionEntry{"reuse-tab", 'r', 0, G_OPTION_ARG_NONE, &cli_flags.reuse_tab, "Open directory in current tab of last used window", nullptr},
    GOptionEntry{"no-saved-tabs", 'n', 0, G_OPTION_ARG_NONE, &cli_flags.no_tabs, "Don't load saved tabs", nullptr},
    GOptionEntry{"new-window", 'w', 0, G_OPTION_ARG_NONE, &cli_flags.new_window, "Open directories in new window", nullptr},
    GOptionEntry{"panel", 'p', 0, G_OPTION_ARG_INT, &cli_flags.panel, "Open directories in panel 'P' (1-4)", "P"},
    GOptionEntry{"config", 'c', 0, G_OPTION_ARG_STRING, &cli_flags.config_dir, "Use DIR as configuration directory", "DIR"},
    GOptionEntry{"disable-git", 'G', 0, G_OPTION_ARG_NONE, &cli_flags.disable_git_settings, "Don't use git to keep session history", nullptr},
    GOptionEntry{"find-files", 'f', 0, G_OPTION_ARG_NONE, &cli_flags.find_files, "Show File Search", nullptr},
    GOptionEntry{"socket-cmd", 's', 0, G_OPTION_ARG_NONE, &cli_flags.socket_cmd, "Send a socket command (See -s help)", nullptr},
    GOptionEntry{"version", 'v', 0, G_OPTION_ARG_NONE, &cli_flags.version_opt, "Show version information", nullptr},
    GOptionEntry{G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &cli_flags.files, nullptr, "[DIR | FILE | URL]..."},
    GOptionEntry{},
};
// clang-format on

static void
init_folder()
{
    if (folder_initialized)
    {
        return;
    }

    vfs_volume_init();
    vfs_thumbnail_init();

    vfs_mime_type_set_icon_size_big(app_settings.get_icon_size_big());
    vfs_mime_type_set_icon_size_small(app_settings.get_icon_size_small());

    vfs_file_info_set_thumbnail_size_big(app_settings.get_icon_size_big());
    vfs_file_info_set_thumbnail_size_small(app_settings.get_icon_size_small());

    folder_initialized = true;
}

static void
open_file(const std::filesystem::path& path)
{
    vfs::file_info file = vfs_file_info_new(path);
    vfs::mime_type mime_type = file->mime_type();

    std::string app_name;
    const auto check_app_name = mime_type->default_action();
    if (!check_app_name)
    {
        app_name = ptk_choose_app_for_mime_type(nullptr, mime_type, true, true, true, false);
        if (app_name.empty())
        {
            ztd::logger::error("no application to open file: {}", path.string());
            return;
        }
    }
    else
    {
        app_name = check_app_name.value();
    }

    const vfs::desktop desktop = vfs_get_desktop(app_name);

    const std::vector<std::filesystem::path> open_files{path};

    try
    {
        desktop->open_files(vfs::user_dirs->current_dir(), open_files);
    }
    catch (const VFSAppDesktopException& e)
    {
        const std::string msg =
            std::format("Unable to open file:\n{}\n{}", path.string(), e.what());
        ptk_show_error(nullptr, "Error", msg);
    }

    vfs_file_info_unref(file);
}

static void
open_in_tab(MainWindow** main_window, const char* real_path)
{
    xset_t set;
    panel_t panel;
    // create main window if needed
    if (!*main_window)
    {
        // initialize things required by folder view
        init_folder();

        // preload panel?
        if (cli_flags.panel > 0 && cli_flags.panel < 5)
        { // user specified panel
            panel = cli_flags.panel;
        }
        else
        {
            // use first visible panel
            for (const panel_t p : PANELS)
            {
                if (xset_get_b_panel(p, xset::panel::show))
                {
                    panel = p;
                    break;
                }
            }
        }

        // set panel to load real_path on window creation
        set = xset_get_panel(panel, xset::panel::show);
        set->ob1 = ztd::strdup(real_path);
        set->b = xset::b::xtrue;

        // create new window
        main_window_store_positions(nullptr);
        *main_window = MAIN_WINDOW_REINTERPRET(main_window_new());
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
                set = xset_get_panel(cli_flags.panel, xset::panel::show);
                set->ob1 = ztd::strdup(real_path);
                tab_added = true;
                set->b = xset::b::xtrue;
                show_panels_all_windows(nullptr, *main_window);
            }
            else if (!gtk_widget_get_visible((*main_window)->panel[cli_flags.panel - 1]))
            {
                // show panel
                set = xset_get_panel(cli_flags.panel, xset::panel::show);
                set->b = xset::b::xtrue;
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
            {
                main_window_add_new_tab(*main_window, real_path);
            }
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
    {
        cli_flags.files = default_files;
    }

    // get the last active window on this desktop, if available
    MainWindow* main_window = nullptr;
    if (cli_flags.new_tab || cli_flags.reuse_tab)
    {
        main_window = main_window_get_on_current_desktop();
        // ztd::logger::info("main_window_get_on_current_desktop = {:p}  {} {}",
        //          (void*)main_window,
        //          cli_flags.new_tab ? "new_tab" : "",
        //          cli_flags.reuse_tab ? "reuse_tab" : "");
    }

    if (cli_flags.find_files)
    {
        // find files
        init_folder();

        std::vector<std::filesystem::path> search_dirs;
        char** dir;
        for (dir = cli_flags.files; *dir; ++dir)
        {
            if (!*dir)
            {
                continue;
            }
            if (std::filesystem::is_directory(*dir))
            {
                search_dirs.emplace_back(*dir);
            }
        }

        find_files(search_dirs);
        cli_flags.find_files = false;
    }
    else if (cli_flags.files != default_files)
    {
        // open files passed in command line arguments
        ret = false;
        char** file;
        for (file = cli_flags.files; *file; ++file)
        {
            // skip empty string
            if (!**file)
            {
                continue;
            }

            const std::string real_path = std::filesystem::absolute(*file);

            if (std::filesystem::is_directory(real_path))
            {
                open_in_tab(&main_window, real_path.data());
                ret = true;
            }
            else if (std::filesystem::exists(real_path))
            {
                const auto file_stat = ztd::stat(real_path);
                if (file_stat.is_valid() && file_stat.is_block_file())
                {
                    // open block device eg /dev/sda1
                    if (!main_window)
                    {
                        open_in_tab(&main_window, "/");
                        ptk_location_view_open_block(real_path, false);
                    }
                    else
                    {
                        ptk_location_view_open_block(real_path, true);
                    }
                    ret = true;
                    gtk_window_present(GTK_WINDOW(main_window));
                }
                else
                {
                    open_file(real_path);
                }
            }
            else if ((*file[0] != '/' && ztd::contains(*file, ":/")) ||
                     ztd::startswith(*file, "//"))
            {
                if (main_window)
                {
                    main_window_open_network(main_window, *file, true);
                }
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
                const std::string err_msg = std::format("File does not exist:\n\n{}", real_path);
                ptk_show_error(nullptr, "Error", err_msg);
            }
        }
    }
    else
    {
        // no files specified, just create window with default tabs
        if (!main_window)
        {
            // initialize things required by folder view
            init_folder();

            main_window_store_positions(nullptr);
            main_window = MAIN_WINDOW_REINTERPRET(main_window_new());
        }
        gtk_window_present(GTK_WINDOW(main_window));

        if (cli_flags.panel > 0 && cli_flags.panel < 5)
        {
            // user specified a panel with no file, let's show the panel
            if (!gtk_widget_get_visible(main_window->panel[cli_flags.panel - 1]))
            {
                // show panel
                xset_t set;
                set = xset_get_panel(cli_flags.panel, xset::panel::show);
                set->b = xset::b::xtrue;
                show_panels_all_windows(nullptr, main_window);
            }
            focus_panel(nullptr, (void*)main_window, cli_flags.panel);
        }
    }

    // ztd::logger::info("    handle_parsed_commandline_args mw = {:p}", main_window);

    if (cli_flags.files != default_files)
    {
        g_strfreev(cli_flags.files);
    }
    cli_flags.files = nullptr;

    return ret;
}

static void
tmp_clean()
{
    const auto tmp = vfs::user_dirs->program_tmp_dir();
    std::filesystem::remove_all(tmp);
    ztd::logger::info("Removed {}", tmp.string());
}

int
main(int argc, char* argv[])
// main(const int argc, const char* const* const argv) // does not work with gtk_init_with_args()
{
    const std::vector<std::string_view> args(argv,
                                             std::next(argv, static_cast<std::ptrdiff_t>(argc)));

    // set locale to system default
    std::locale::global(std::locale(""));

    // logging init
    ztd::Logger->initialize();

    // Gtk4 porting
    g_set_prgname(PACKAGE_NAME);

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
    (void)freopen("/dev/null", "w", stderr);

    // separate instance options
    if (args.size() > 1)
    {
        // socket_command?
        if (ztd::same(args[1], "-s") || ztd::same(args[1], "--socket-cmd"))
        {
            const auto [ret, socket_reply] = send_socket_command(args);
            if (!socket_reply.empty())
            {
                fmt::print("{}\n", socket_reply);
            }
            std::exit(ret);
        }
    }

    // initialize GTK+ and parse the command line arguments
    GError* err = nullptr;
    if (!gtk_init_with_args(&argc, &argv, "", opt_entries.data(), nullptr, &err))
    {
        ztd::logger::info("{}", err->message);
        g_error_free(err);
        std::exit(EXIT_FAILURE);
    }

    // socket command with other options?
    if (cli_flags.socket_cmd)
    {
        ztd::logger::error("socket-cmd must be first option");
        std::exit(EXIT_FAILURE);
    }

    // --disable-git
    app_settings.set_git_backed_settings(!cli_flags.disable_git_settings);

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
    const auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::srand(seed);

    // Initialize our mime-type system
    vfs_mime_type_init();

    // Sets custom config dir
    if (cli_flags.config_dir)
    {
        vfs::user_dirs->program_config_dir(cli_flags.config_dir);
    }

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
    std::atexit(vfs_mime_type_finalize);
    std::atexit(vfs_volume_finalize);
    std::atexit(single_instance_finalize);
    std::atexit(save_bookmarks);

    main_window_event(nullptr, nullptr, xset::name::evt_start, 0, 0, nullptr, 0, 0, 0, false);

    // handle the parsed result of command line args
    if (handle_parsed_commandline_args())
    {
        app_settings.set_load_saved_tabs(true);

        // run the main loop
        gtk_main();
    }

    main_window_event(nullptr, nullptr, xset::name::evt_exit, 0, 0, nullptr, 0, 0, 0, false);

    std::exit(EXIT_SUCCESS);
}
