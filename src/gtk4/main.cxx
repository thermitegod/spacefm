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

#include <print>

#include <cstdlib>

#include <glibmm.h>
#include <gtkmm.h>

#include <CLI/CLI.hpp>

#include "commandline/commandline.hxx"

#include "gui/main-window.hxx"

int
main(int argc, char* argv[])
{
    const auto opts = commandline::run(argc, argv);
    if (!opts)
    {
        std::println(stderr, "{}", opts.error());
        return EXIT_FAILURE;
    }

    auto app = Gtk::Application::create("org.thermitegod.experimental.spacefm");
    return app->make_window_and_run<gui::main_window>(0, nullptr, app);
}
