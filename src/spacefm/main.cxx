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
#include <memory>
#include <optional>
#include <print>
#include <ranges>
#include <string>
#include <vector>

#include <cstdlib>

#include <giomm.h>
#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "settings/config.hxx"
#include "settings/settings.hxx"

#include "gui/main-window.hxx"

#include "logger.hxx"

struct commandline_opts final
{
    std::vector<std::filesystem::path> files;
    bool no_saved_state{false};
    bool new_window{false};
    std::int32_t panel{1};
    bool version{false};
};

std::optional<commandline_opts>
parse_options(auto& argc, auto& argv) noexcept
{
    commandline_opts options{};
    try
    {
        Glib::OptionContext context("[FILES...]");
        context.set_summary("A multi-panel tabbed file manager");

        Glib::OptionGroup group("main", PACKAGE_NAME_FANCY, "Application options");

        Glib::OptionEntry no_saved_state;
        no_saved_state.set_long_name("no-saved-state");
        no_saved_state.set_description("Do not restore saved state");
        group.add_entry(no_saved_state, options.no_saved_state);

        Glib::OptionEntry new_window;
        new_window.set_long_name("new-window");
        new_window.set_description("Open a new window");
        group.add_entry(new_window, options.new_window);

        Glib::OptionEntry panel_entry;
        panel_entry.set_long_name("panel");
        panel_entry.set_description("Panel (1-4)");
        group.add_entry(panel_entry, options.panel);

        Glib::OptionEntry version_entry;
        version_entry.set_short_name('v');
        version_entry.set_long_name("version");
        version_entry.set_description("Show version information");
        group.add_entry(version_entry, options.version);

        context.set_main_group(group);
        context.parse(argc, argv);

        for (const auto i : std::views::iota(1, argc))
        {
            std::filesystem::path path{argv[i]};
            if (!std::filesystem::exists(path))
            {
                std::println(stderr, "CLI: does not exist '{}'", path);
                continue;
            }
            if (!std::filesystem::is_directory(path))
            {
                std::println(stderr, "CLI: not a directory '{}'", path);
                continue;
            }
            options.files.emplace_back(std::move(path));
        }
    }
    catch (const Glib::Error& ex)
    {
        std::println(stderr, "Error parsing command-line options: {}", ex.what());
        return std::nullopt;
    }

    if (options.version)
    {
        std::println("{} {}", PACKAGE_NAME_FANCY, PACKAGE_VERSION);
        return std::nullopt;
    }

    return options;
}

int
main(int argc, char* argv[])
{
    auto app = Gtk::Application::create("org.thermitegod.spacefm",
                                        Gio::Application::Flags::HANDLES_COMMAND_LINE);

    auto settings = std::make_shared<config::settings>();
    auto config_manager = std::make_shared<config::manager>(settings);

    bool initialized = false;

    app->signal_command_line().connect(
        [app, settings, config_manager, &initialized](
            const Glib::RefPtr<Gio::ApplicationCommandLine>& commandline)
        {
            int argc = 0;
            char** argv = commandline->get_arguments(argc);

            const auto options = parse_options(argc, argv);
            g_strfreev(argv);

            if (!options)
            {
                return EXIT_FAILURE;
            }

            if (!initialized)
            {
                logger::initialize();
                config_manager->load();
                initialized = true;

                if (options->no_saved_state)
                { // remove saved state
                    settings->clear_windows();
                }

                gui::main_window* window = nullptr;
                auto windows = settings->get_windows();
                if (windows.empty())
                { // No saved state
                    const auto window_id = *settings->create_window_state();

                    window = Gtk::make_managed<gui::main_window>(app, window_id, settings);
                    window->present();
                }
                else
                {
                    for (const auto& [window_id, _] : settings->get_windows())
                    {
                        window = Gtk::make_managed<gui::main_window>(app, window_id, settings);
                        window->present();
                    }
                }

                // open cli files in last window, this should be good enough for now
                window->open_files(static_cast<config::panel_id>(options->panel - 1),
                                   options->files);
            }
            else
            {
                if (options->new_window || options->files.empty())
                {
                    auto* window = dynamic_cast<gui::main_window*>(app->get_active_window());
                    if (window)
                    {
                        window->new_window();
                    }
                }

                // TODO? if opening a new window with a file list
                // dont open the default home tab

                auto* window = dynamic_cast<gui::main_window*>(app->get_active_window());
                if (window && !options->files.empty())
                {
                    window->open_files(static_cast<config::panel_id>(options->panel - 1),
                                       options->files);
                }
            }

            return EXIT_SUCCESS;
        },
        false);

    app->signal_shutdown().connect(
        [config_manager, &initialized]()
        {
            if (initialized)
            {
                config_manager->save();
            }
        });

    return app->run(argc, argv);
}
