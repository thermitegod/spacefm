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

#include "message.hxx"

int
main(int argc, char* argv[])
{
    CLI::App capp{"Spacefm Dialog"};

    std::string title;
    capp.add_option("--title", title, "Dialog title")->required();

    std::string message;
    capp.add_option("--message", message, "Dialog message")->required();

    std::string secondary_message;
    capp.add_option("--secondary_message", secondary_message, "Dialog optional secondary message");

    bool button_ok = false;
    capp.add_flag("--button-ok", button_ok, "Dialog add button 'Ok'");

    bool button_close = false;
    capp.add_flag("--button-close", button_close, "Dialog add button 'Close'");

    bool button_cancel = false;
    capp.add_flag("--button-cancel", button_cancel, "Dialog add button 'Cancel'");

    bool button_yes_no = false;
    capp.add_flag("--button-yes-no", button_yes_no, "Dialog add buttons 'Yes', 'No");

    bool button_ok_cancel = false;
    capp.add_flag("--button-ok-cancel", button_ok_cancel, "Dialog add buttons 'Ok', 'Cancel'");

    CLI11_PARSE(capp, argc, argv);

    auto app = Gtk::Application::create("org.thermitegod.spacefm.message");

    return app->make_window_and_run<MessageDialog>(0,       // Gtk does not handle cli
                                                   nullptr, // Gtk does not handle cli
                                                   title,
                                                   message,
                                                   secondary_message,
                                                   button_ok,
                                                   button_cancel,
                                                   button_close,
                                                   button_yes_no,
                                                   button_ok_cancel);
}
