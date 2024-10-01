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
class MessageDialog : public Gtk::Window
{
  public:
    MessageDialog(const std::string_view title, const std::string_view message,
                  const std::string_view secondary_message, const bool button_ok,
                  const bool button_cancel, const bool button_close, const bool button_yes_no,
                  const bool button_ok_cancel);

  protected:
    Gtk::Box vbox_;

    Gtk::Label title_;
    Gtk::Label message_;
    Gtk::Label secondary_message_;

    Gtk::Box button_box_;
    Gtk::Button button_ok_;
    Gtk::Button button_cancel_;
    Gtk::Button button_yes_;
    Gtk::Button button_no_;
    Gtk::Button button_close_;

    // Signal Handlers
    bool on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state);
    void on_button_ok_clicked();
    void on_button_cancel_clicked();
    void on_button_yes_clicked();
    void on_button_no_clicked();
    void on_button_close_clicked();
};
