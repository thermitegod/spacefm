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

#include <format>

#include <filesystem>

#include <memory>

#include <chrono>

#include <CLI/CLI.hpp>

#include <fmt/format.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "types.hxx"

#include "main-window.hxx"

#include "vfs/vfs-app-desktop.hxx"
#include "vfs/vfs-user-dirs.hxx"
#include "vfs/vfs-thumbnail-loader.hxx"

#include "ptk/ptk-app-chooser.hxx"
#include "ptk/ptk-dialog.hxx"
#include "ptk/ptk-location-view.hxx"

#include "settings/app.hxx"

#include "single-instance.hxx"
#include "program-timer.hxx"
#include "autosave.hxx"

#include "ipc.hxx"
#include "settings.hxx"

#include "bookmarks.hxx"

#include "commandline/commandline.hxx"

static bool folder_initialized = false;

static void
init_folder()
{
    if (folder_initialized)
    {
        return;
    }

    vfs_volume_init();
    vfs_thumbnail_init();

    folder_initialized = true;
}

static void
open_file(const std::filesystem::path& path)
{
    vfs::file_info file = vfs_file_info_new(path);
    vfs::mime_type mime_type = file->mime_type();

    const auto check_app_name = mime_type->default_action();
    if (!check_app_name)
    {
        const auto app_name =
            ptk_choose_app_for_mime_type(nullptr, mime_type, true, true, true, false);
        if (!app_name)
        {
            ztd::logger::error("no application to open file: {}", path.string());
            return;
        }
    }

    const vfs::desktop desktop = vfs_get_desktop(check_app_name.value());

    try
    {
        desktop->open_file(vfs::user_dirs->current_dir(), path);
    }
    catch (const VFSAppDesktopException& e)
    {
        ptk_show_error(nullptr,
                       "Error",
                       std::format("Unable to open file:\n{}\n{}", path.string(), e.what()));
    }

    vfs_file_info_unref(file);
}

static void
open_in_tab(MainWindow** main_window, const std::filesystem::path& real_path,
            const commandline_opt_data_t& opt)
{
    xset_t set;
    panel_t panel;
    // create main window if needed
    if (!*main_window)
    {
        // initialize things required by folder view
        init_folder();

        // preload panel?
        if (opt->panel > 0 && opt->panel < 5)
        { // user specified panel
            panel = opt->panel;
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
        if (opt->panel > 0 && opt->panel < 5)
        {
            // change to user-specified panel
            if (!gtk_notebook_get_n_pages(GTK_NOTEBOOK((*main_window)->panel[opt->panel - 1])))
            {
                // set panel to load real_path on panel load
                set = xset_get_panel(opt->panel, xset::panel::show);
                set->ob1 = ztd::strdup(real_path);
                tab_added = true;
                set->b = xset::b::xtrue;
                show_panels_all_windows(nullptr, *main_window);
            }
            else if (!gtk_widget_get_visible((*main_window)->panel[opt->panel - 1]))
            {
                // show panel
                set = xset_get_panel(opt->panel, xset::panel::show);
                set->b = xset::b::xtrue;
                show_panels_all_windows(nullptr, *main_window);
            }
            (*main_window)->curpanel = opt->panel;
            (*main_window)->notebook = (*main_window)->panel[opt->panel - 1];
        }
        if (!tab_added)
        {
            if (opt->reuse_tab)
            {
                main_window_open_path_in_current_tab(*main_window, real_path);
                opt->reuse_tab = false;
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
handle_parsed_commandline_args(const commandline_opt_data_t& opt)
{
    app_settings.load_saved_tabs(!opt->no_tabs);

    // get the last active window on this desktop, if available
    MainWindow* main_window = nullptr;
    if (opt->new_tab || opt->reuse_tab)
    {
        main_window = main_window_get_on_current_desktop();
        // ztd::logger::debug("main_window_get_on_current_desktop = {}  {} {}",
        //                    fmt::ptr(main_window),
        //                    opt->new_tab ? "new_tab" : "",
        //                    opt->reuse_tab ? "reuse_tab" : "");
    }

    if (opt->files.empty())
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

        if (is_valid_panel(opt->panel))
        {
            // user specified a panel with no file, let's show the panel
            if (!gtk_widget_get_visible(main_window->panel[opt->panel - 1]))
            {
                // show panel
                xset_t set = xset_get_panel(opt->panel, xset::panel::show);
                set->b = xset::b::xtrue;
                show_panels_all_windows(nullptr, main_window);
            }
            focus_panel(main_window, opt->panel);
        }

        return true;
    }

    // open files passed in command line arguments
    bool ret = false;
    for (const auto& file : opt->files)
    {
        const auto real_path = std::filesystem::absolute(file);

        if (std::filesystem::is_directory(real_path))
        {
            open_in_tab(&main_window, real_path, opt);
            ret = true;
        }
        else if (std::filesystem::exists(real_path))
        {
            const auto file_stat = ztd::statx(real_path);
            if (file_stat && file_stat.is_block_file())
            {
                // open block device eg /dev/sda1
                if (!main_window)
                {
                    open_in_tab(&main_window, "/", opt);
                    ptk_location_view_open_block(real_path, false);
                }
                else
                {
                    ptk_location_view_open_block(real_path, true);
                }
                gtk_window_present(GTK_WINDOW(main_window));
                ret = true;
            }
            else
            {
                open_file(real_path);
            }
        }
        else if ((!ztd::startswith(file.string(), "/") && ztd::contains(file.string(), ":/")) ||
                 ztd::startswith(file.string(), "//"))
        {
            if (main_window)
            {
                main_window_open_network(main_window, file.string(), true);
            }
            else
            {
                open_in_tab(&main_window, "/", opt);
                main_window_open_network(main_window, file.string(), false);
            }
            gtk_window_present(GTK_WINDOW(main_window));
            ret = true;
        }
        else
        {
            ptk_show_error(nullptr,
                           "Error",
                           std::format("File does not exist:\n\n{}", real_path.string()));
        }
    }

    // ztd::logger::debug("handle_parsed_commandline_args mw = {}", fmt::ptr(main_window));

    return ret;
}

static void
tmp_clean()
{
    const auto& tmp = vfs::user_dirs->program_tmp_dir();
    if (std::filesystem::exists(tmp))
    {
        std::filesystem::remove_all(tmp);
        ztd::logger::info("Removed {}", tmp.string());
    }
}

int
main(int argc, char* argv[])
{
    // set locale to system default
    std::locale::global(std::locale(""));

    // logging init
    ztd::Logger->initialize();

    // CLI11
    CLI::App app{PACKAGE_NAME_FANCY, "A multi-panel tabbed file manager"};

    auto opt = std::make_shared<commandline_opt_data>();
    setup_commandline(app, opt);

    CLI11_PARSE(app, argc, argv);

    // start program timer
    program_timer::start();

    // Gtk
    g_set_prgname(PACKAGE_NAME);

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

    // initialize GTK+
    gtk_init(&argc, &argv);

    // ensure that there is only one instance of spacefm.
    // if there is an existing instance, only the FILES
    // command line argument will be passed to the existing instance,
    // and then the new instance will exit.
    const bool is_single_instance = single_instance_check();
    if (!is_single_instance)
    {
        // if another instance is running then open a tab in the
        // existing instance for each passed directory
        for (const auto& file : opt->files)
        {
            if (!std::filesystem::is_directory(file))
            {
                ztd::logger::error("Not a directory: '{}'", file.string());
                continue;
            }
            const auto command = std::format("{} socket set new-tab {}",
                                             ztd::program::exe().string(),
                                             ztd::shell::quote(file.string()));
            Glib::spawn_command_line_sync(command);
        }

        std::exit(EXIT_SUCCESS);
    }
    // If we reach this point, we are the first instance.
    // Subsequent processes will exit and will not reach here.

    // Start a thread to receive socket messages
    const std::jthread socket_server(socket_server_thread);

    // initialize the file alteration monitor
    if (!vfs_file_monitor_init())
    {
        ptk_show_error(nullptr,
                       "Error",
                       "Unable to initialize inotify file change monitor. Do you have an "
                       "inotify-capable kernel?");
        vfs_file_monitor_clean();
        std::exit(EXIT_FAILURE);
    }

    // Seed RNG
    // using the current time is a good enough seed
    const auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::srand(seed);

    // Initialize our mime-type system
    vfs_mime_type_init();

    // load config file
    load_settings();

    // load user bookmarks
    load_bookmarks();

    // start autosave thread
    autosave_init(save_settings);

    std::atexit(ztd::Logger->shutdown);
    std::atexit(free_settings);
    std::atexit(tmp_clean);
    std::atexit(autosave_terminate);
    std::atexit(vfs_file_monitor_clean);
    std::atexit(vfs_mime_type_finalize);
    std::atexit(vfs_volume_finalize);
    std::atexit(single_instance_finalize);
    std::atexit(save_bookmarks);

    // handle the parsed result of command line args
    if (handle_parsed_commandline_args(opt))
    {
        app_settings.load_saved_tabs(true);

        // run the main loop
        gtk_main();
    }

    std::exit(EXIT_SUCCESS);
}
