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

#pragma once

#include <string_view>

#include <gtkmm.h>

// TODO maybe convert to Gtk::AlertDialog when reintegrating
class ErrorDialog : public Gtk::ApplicationWindow
{
  public:
    ErrorDialog(const std::string_view json_data);

  protected:
    Gtk::Box vbox_;

    Gtk::Box hbox_;
    Gtk::Image icon_;
    Gtk::Label title_;

    Gtk::Label message_;

    Gtk::Box button_box_;
    Gtk::Button button_ok_;

    // Signal Handlers
    bool on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state);
    void on_button_ok_clicked();
};
