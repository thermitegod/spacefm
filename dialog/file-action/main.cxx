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

#include "action.hxx"

int
main(int argc, char* argv[])
{
    CLI::App capp{"Spacefm Dialog"};

    std::string header;
    capp.add_option("--header", header, "Set header")->required();

    std::string json_data;
    capp.add_option("--json", json_data, "json data")->required();

    CLI11_PARSE(capp, argc, argv);

    auto app = Gtk::Application::create("org.thermitegod.spacefm.action");

    return app->make_window_and_run<ActionDialog>(0,       // Gtk does not handle cli
                                                  nullptr, // Gtk does not handle cli
                                                  header,
                                                  json_data);
}