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

#include <CLI/CLI.hpp>

#include <gtkmm.h>
#include <glibmm.h>

#include "error.hxx"

int
main(int argc, char* argv[])
{
    CLI::App capp{"Spacefm Dialog"};

    std::string title;
    capp.add_option("--title", title, "Dialog title")->required();

    std::string message;
    capp.add_option("--message", message, "Dialog message")->required();

    CLI11_PARSE(capp, argc, argv);

    auto app = Gtk::Application::create("org.thermitegod.spacefm.error");

    return app->make_window_and_run<ErrorDialog>(0,       // Gtk does not handle cli
                                                 nullptr, // Gtk does not handle cli
                                                 title,
                                                 message);
}
