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
#include <sigc++/sigc++.h>

#include "window.hxx"

gui::window::window()
{
    set_size_request(600, 400);
    set_title("Template Window");
    set_resizable(false);
    set_visible(true);

    // Content //

    box_ = Gtk::Box(Gtk::Orientation::VERTICAL, 5);
    box_.set_margin(5);

    auto key_controller = Gtk::EventControllerKey::create();
    key_controller->signal_key_pressed().connect(sigc::mem_fun(*this, &window::on_key_press),
                                                 false);
    box_.add_controller(key_controller);

    // Buttons //

    button_box_ = Gtk::Box(Gtk::Orientation::HORIZONTAL, 5);
    button_ok_ = Gtk::Button("_Ok", true);
    button_cancel_ = Gtk::Button("_Close", true);

    box_.append(button_box_);
    button_box_.set_halign(Gtk::Align::END);
    button_box_.append(button_cancel_);
    button_box_.append(button_ok_);

    button_ok_.signal_clicked().connect(sigc::mem_fun(*this, &window::on_button_ok_clicked));
    button_cancel_.signal_clicked().connect(
        sigc::mem_fun(*this, &window::on_button_cancel_clicked));

    set_child(box_);
}

bool
gui::window::on_key_press(std::uint32_t keyval, std::uint32_t keycode,
                          Gdk::ModifierType state) noexcept
{
    (void)keycode;
    (void)state;
    if (keyval == GDK_KEY_Escape)
    {
        on_button_cancel_clicked();
    }
    return false;
}

void
gui::window::on_button_ok_clicked() noexcept
{
    close();
}

void
gui::window::on_button_cancel_clicked() noexcept
{
    close();
}
