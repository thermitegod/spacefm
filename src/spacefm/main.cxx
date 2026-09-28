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

#include <glibmm.h>
#include <gtkmm.h>

#include "commandline/commandline.hxx"

#include "settings/config.hxx"
#include "settings/settings.hxx"

#include "gui/main-window.hxx"

int
main(int argc, char* argv[])
{
    const auto opts = commandline::run(argc, argv);
    if (!opts)
    {
        return EXIT_FAILURE;
    }

    auto app = Gtk::Application::create("org.thermitegod.experimental.spacefm");

    auto settings = std::make_shared<config::settings>();
    config::manager config_manager(settings);
    config_manager.load();

    app->signal_startup().connect(
        [&]()
        {
            for (const auto& [window_id, _] : settings->windows)
            {
                auto* window = Gtk::make_managed<gui::main_window>(app, window_id, settings);
                window->present();
            }
        });

    const auto status = app->run(0, nullptr);

    config_manager.save();

    return status;
}
