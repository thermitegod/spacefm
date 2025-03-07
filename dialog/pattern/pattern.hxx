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

#include <gtkmm.h>

class PatternDialog : public Gtk::ApplicationWindow
{
  public:
    PatternDialog();

  protected:
    Gtk::Box vbox_;

    Gtk::Expander expand_;
    Gtk::Label expand_data_;

    Gtk::TextView input_;
    Glib::RefPtr<Gtk::TextBuffer> buf_;
    Gtk::ScrolledWindow scroll_;

    Gtk::Box button_box_;
    Gtk::Button button_ok_;
    Gtk::Button button_cancel_;

    // Signal Handlers
    bool on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state);
    void on_button_ok_clicked();
    void on_button_cancel_clicked();
};
