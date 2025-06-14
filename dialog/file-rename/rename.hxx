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

#include <filesystem>
#include <vector>

#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "vfs/vfs-file.hxx"

#include "datatypes.hxx"

class RenameDialog : public Gtk::ApplicationWindow
{
  public:
    RenameDialog(const std::string_view json_data);
    ~RenameDialog();

  private:
    datatype::rename::settings_data settings_;

    std::shared_ptr<vfs::file> file_;

    std::filesystem::path full_path_;
    std::filesystem::path old_path_;
    std::filesystem::path new_path_;
    std::string desc_;
    bool is_dir_{false};
    bool is_link_{false};
    bool clip_copy_{false};

    Gtk::Box box_;

    Gtk::Label label_type_;
    Gtk::Label label_mime_;
    Gtk::Box hbox_type_;
    std::string mime_type_;

    Gtk::Label label_target_;
    Gtk::Entry entry_target_;
    Gtk::Box hbox_target_;

    Gtk::Label label_full_name_;
    Gtk::ScrolledWindow scroll_full_name_;
    Gtk::TextView input_full_name_;
    Glib::RefPtr<Gtk::TextBuffer> buf_full_name_;

    Gtk::Label label_path_;
    Gtk::ScrolledWindow scroll_path_;
    Gtk::TextView input_path_;
    Glib::RefPtr<Gtk::TextBuffer> buf_path_;

    Gtk::Label label_full_path_;
    Gtk::ScrolledWindow scroll_full_path_;
    Gtk::TextView input_full_path_;
    Glib::RefPtr<Gtk::TextBuffer> buf_full_path_;

    Gtk::Box radio_button_box_;
    Gtk::CheckButton opt_move_;
    Gtk::CheckButton opt_copy_;
    Gtk::CheckButton opt_link_;
    Gtk::CheckButton opt_copy_target_;
    Gtk::CheckButton opt_link_target_;

    Gtk::Button button_options_;
    Gtk::Button button_revert_;
    Gtk::Button button_cancel_;
    Gtk::Button button_next_;
    Gtk::Box button_box_;

    bool full_path_exists_{false};
    bool full_path_exists_dir_{false};
    bool full_path_same_{false};
    bool path_missing_{false};
    bool path_exists_file_{false};
    bool mode_change_{false};
    bool is_move_{false};

    bool overwrite_{false};

    // Option button items
    Gtk::PopoverMenu context_menu_;
    Gtk::PopoverMenu context_menu_sub_;
    Glib::RefPtr<Gio::SimpleActionGroup> context_action_group_;
    Glib::RefPtr<Gio::SimpleAction> action_filename_;
    Glib::RefPtr<Gio::SimpleAction> action_parent_;
    Glib::RefPtr<Gio::SimpleAction> action_path_;
    Glib::RefPtr<Gio::SimpleAction> action_type_;
    Glib::RefPtr<Gio::SimpleAction> action_target_;
    // Option submenu
    Glib::RefPtr<Gio::SimpleAction> action_copy_;
    Glib::RefPtr<Gio::SimpleAction> action_link_;
    Glib::RefPtr<Gio::SimpleAction> action_copy_target_;
    Glib::RefPtr<Gio::SimpleAction> action_link_target_;

    Glib::RefPtr<Gio::SimpleAction> action_confirm_;

    // Signal handlers

    std::vector<sigc::connection> on_move_change_signals_;

    // Signal Handlers
    bool on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state);
    void on_button_ok_clicked();
    void on_button_cancel_clicked();
    void on_button_revert_clicked();
    void on_button_options_clicked();

    void on_opt_toggled();
    void on_toggled();

    void select_input() noexcept;
    void on_move_change(Glib::RefPtr<Gtk::TextBuffer>& widget);
};
