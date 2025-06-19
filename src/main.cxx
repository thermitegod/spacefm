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

#include <filesystem>
#include <format>
#include <memory>
#include <print>
#include <string>

#include <cassert>

#include <gdkmm.h>
#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "commandline/commandline.hxx"

#if defined(HAVE_SOCKET)
#include "socket/server.hxx"
#endif

#include "utils/shell-quote.hxx"

#include "settings/config.hxx"
#include "settings/settings.hxx"

#include "gui/main-window.hxx"

#include "gui/view/location.hxx"

#include "gui/dialog/app-chooser.hxx"
#include "gui/dialog/text.hxx"

#include "vfs/app-desktop.hxx"
#include "vfs/bookmarks.hxx"
#include "vfs/file.hxx"
#include "vfs/user-dirs.hxx"

#include "vfs/linux/self.hxx"

#include "autosave.hxx"
#include "logger.hxx"
#include "settings.hxx"
#include "single-instance.hxx"
#include "types.hxx"

// This is easier with gtkmm, can just pass shared_ptr
struct app_data : public std::enable_shared_from_this<app_data>
{
    commandline::opts opts;
    std::shared_ptr<config::settings> settings{nullptr};
};

static void
open_file(const std::filesystem::path& path) noexcept
{
    const auto file = vfs::file::create(path);
    const auto mime_type = file->mime_type();

    const auto app_name = mime_type->default_action();
    if (!app_name)
    {
        const auto app_name_alt = gui::dialog::app_chooser(mime_type, true, true, true, false);
        if (!app_name_alt)
        {
            logger::error("no application to open file: {}", path.string());
            return;
        }
    }

    const auto desktop = vfs::desktop::create(app_name.value());
    if (!desktop)
    {
        return;
    }

    const auto opened = desktop->open_file(Glib::get_current_dir(), path);
    if (!opened)
    {
        gui::dialog::error(
            "Error",
            std::format("Unable to use '{}' to open file:\n{}", app_name.value(), path.string()));
    }
}

static void
open_in_tab(MainWindow* main_window, const std::filesystem::path& path,
            commandline::opts& opts) noexcept
{
    // existing window
    bool tab_added = false;
    if (is_valid_panel(opts.panel))
    {
        // change to user-specified panel
        if (!gtk_notebook_get_n_pages(main_window->get_panel_notebook(opts.panel)))
        {
            // set panel to load path on panel load
            const auto set = xset::set::get(xset::panel::show, opts.panel);
            if (set->s)
            {
                set->s = std::format("{}{}{}",
                                     set->s.value(),
                                     config::disk_format::tab_delimiter,
                                     path.string());
            }
            else
            {
                set->s = std::format("{}", path.string());
            }
            tab_added = true;
            set->b = xset::set::enabled::yes;
            show_panels_all_windows(nullptr, main_window);
        }
        else if (!gtk_widget_get_visible(GTK_WIDGET(main_window->get_panel_notebook(opts.panel))))
        {
            // show panel
            const auto set = xset::set::get(xset::panel::show, opts.panel);
            set->b = xset::set::enabled::yes;
            show_panels_all_windows(nullptr, main_window);
        }
        main_window->curpanel = opts.panel;
        main_window->notebook = main_window->get_panel_notebook(opts.panel);
    }
    if (!tab_added)
    {
        if (opts.reuse_tab)
        {
            main_window->open_path_in_current_tab(path);
            opts.reuse_tab = false;
        }
        else
        {
            main_window->new_tab(path);
        }
    }
}

static void
tmp_clean() noexcept
{
    const auto tmp = vfs::program::tmp();
    if (std::filesystem::exists(tmp))
    {
        std::filesystem::remove_all(tmp);
        logger::info("Removed {}", tmp.string());
    }
}

static void
activate(GtkApplication* app, void* user_data) noexcept
{
    assert(GTK_IS_APPLICATION(app));

    const auto data = static_cast<app_data*>(user_data)->shared_from_this();

    auto& opts = data->opts;
    const auto settings = data->settings;

    settings->load_saved_tabs = !opts.no_tabs;

    MainWindow* main_window =
        MAIN_WINDOW(g_object_new(main_window_get_type(), "application", app, nullptr));
    gtk_window_set_application(GTK_WINDOW(main_window), app);
    assert(GTK_IS_APPLICATION_WINDOW(main_window));

    // logger::debug("main_window = {}  {} {}",
    //                    logger::utils::ptr(main_window),
    //                    opt->new_tab ? "new_tab" : "",
    //                    opt->reuse_tab ? "reuse_tab" : "");

    // open files passed in command line arguments
    for (const auto& file : opts.files)
    {
        const auto real_path = std::filesystem::absolute(file);

        if (std::filesystem::is_directory(real_path))
        {
            open_in_tab(main_window, real_path, opts);
        }
        else if (std::filesystem::exists(real_path))
        {
            const auto stat = ztd::stat::create(real_path);
            if (stat && stat->is_block_file())
            {
                // open block device eg /dev/sda1
                gui::view::location::open_block(real_path, true);
            }
            else
            {
                open_file(real_path);
            }
        }
        else if ((!file.string().starts_with('/') && file.string().contains(":/")) ||
                 file.string().starts_with("//"))
        {
            main_window->open_network(file.string(), true);
        }
        else
        {
            logger::warn("File does not exist: {}", real_path.string());
        }
    }

    if (is_valid_panel(opts.panel))
    {
        // user specified a panel with no file, let's show the panel
        if (!gtk_widget_get_visible(GTK_WIDGET(main_window->get_panel_notebook(opts.panel))))
        {
            // show panel
            const auto set = xset::set::get(xset::panel::show, opts.panel);
            set->b = xset::set::enabled::yes;
            show_panels_all_windows(nullptr, main_window);
        }
        main_window->focus_panel(opts.panel);
    }

    settings->load_saved_tabs = true;

    gtk_window_present(GTK_WINDOW(main_window));
}

int
main(int argc, char* argv[]) noexcept
{
    const auto opts = commandline::run(argc, argv);
    if (!opts)
    {
        std::println(stderr, "{}", opts.error());
        return EXIT_FAILURE;
    }

    // Gtk
    Glib::set_prgname(PACKAGE_NAME);

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
    auto* const stream = std::freopen("/dev/null", "w", stderr);
    if (stream == nullptr)
    {
        logger::error("Failed to freopen() stderr");
    }

    // ensure that there is only one instance of spacefm.
    // if there is an existing instance, only the FILES
    // command line argument will be passed to the existing instance,
    // and then the new instance will exit.
    const bool is_single_instance = single_instance_check();
    if (!is_single_instance)
    {
        // if another instance is running then open a tab in the
        // existing instance for each passed directory
        for (const auto& file : opts->files)
        {
            if (!std::filesystem::is_directory(file))
            {
                logger::error("Not a directory: '{}'", file.string());
                continue;
            }
            const auto command = std::format("{} socket set new-tab {}",
                                             vfs::linux::proc::self::exe().string(),
                                             ::utils::shell_quote(file.string()));
            Glib::spawn_command_line_sync(command);
        }

        std::exit(EXIT_SUCCESS);
    }
    // If we reach this point, we are the first instance.
    // Subsequent processes will exit and will not reach here.

#if defined(HAVE_SOCKET)
    // Start a thread to receive socket messages
    const std::jthread socket_server(spacefm::server::server_thread);
#endif

    // load config file
    const auto settings = std::make_shared<config::settings>();
    config::global::settings = settings; // settings used by legacy code
    assert(settings != nullptr);
    assert(config::global::settings != nullptr);
    load_settings(settings);

    // Initialize vfs system
    vfs::volume_init();

    // load user bookmarks
    vfs::bookmarks::load();

    // start autosave thread
    // autosave::create([&settings]() { save_settings(settings); }); // BUG
    autosave::create([]() { save_settings(); });

    std::atexit(tmp_clean);
    std::atexit(autosave::close);
    std::atexit(vfs::volume_finalize);

    const auto data = std::make_shared<app_data>();
    data->opts = opts.value();
    data->settings = settings;

    GtkApplication* app =
        gtk_application_new("com.thermitegod.spacefm", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), data.get());
    // Do not pass argc/argv, the CLI is not handled by GTK
    const auto status = g_application_run(G_APPLICATION(app), 0, nullptr);

    g_object_unref(app);

    std::exit(status);
}
