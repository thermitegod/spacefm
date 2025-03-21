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

#include <print>
#include <string_view>

#include <gtkmm.h>

#include "datatypes.hxx"

class SetKeyDialog : public Gtk::ApplicationWindow
{
  public:
    SetKeyDialog(const std::string_view key_name, const std::string_view json_data);

  protected:
    Gtk::Box box_;

    Gtk::Label title_;
    Gtk::Label message_;
    Gtk::Label keybinding_;

    Gtk::Box button_box_;
    Gtk::Button button_set_;
    Gtk::Button button_unset_;
    Gtk::Button button_cancel_;

    // Signal Handlers
    bool on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state);
    void on_button_set_clicked();
    void on_button_unset_clicked();
    void on_button_cancel_clicked();

  private:
    datatype::keybinding::request_data keybinding_data_; // keybinding getting changed

    std::vector<datatype::keybinding::request_data> keybindings_data_;

    // New Keybindings data
    datatype::keybinding::response_data result;
};
