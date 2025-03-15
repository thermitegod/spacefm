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
    PatternDialog(const std::string_view json_data);
    ~PatternDialog();

  protected:
    Gtk::Box box_;

    Gtk::Expander expand_;
    Gtk::Label expand_data_;

    Gtk::TextView input_;
    Glib::RefPtr<Gtk::TextBuffer> buf_;
    Gtk::ScrolledWindow scroll_;

    Gtk::Box button_box_;
    Gtk::Button button_select_;
    Gtk::Button button_cancel_;
    Gtk::Button button_patterns_;

    // Option button items
    enum class patterns : std::uint8_t
    {
        jpg,
        png,
        gif,
        mp4,
        mkv,
        tar,
        szip,
        rar,
        zip,
    };
    Gtk::PopoverMenu context_menu_;
    Glib::RefPtr<Gio::SimpleActionGroup> context_action_group_;
    Glib::RefPtr<Gio::SimpleAction> action_filename_;
    Glib::RefPtr<Gio::SimpleAction> action_parent_;
    Glib::RefPtr<Gio::SimpleAction> action_path_;
    Glib::RefPtr<Gio::SimpleAction> action_confirm_;

    void on_context_menu_set_pattern(const patterns pattern);

    // Signal Handlers
    bool on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state);
    void on_button_select_clicked();
    void on_button_cancel_clicked();
    void on_button_patterns_clicked();
};
