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

#include <cstdint>

#include <glibmm.h>
#include <gtkmm.h>

#include "window.hxx"

int
main(std::int32_t argc, char** argv)
{
    (void)argc;
    (void)argv;

    auto app =
        Gtk::Application::create("org.thermitegod.template", Gio::Application::Flags::NON_UNIQUE);

    return app->make_window_and_run<gui::window>(0, nullptr);
}
