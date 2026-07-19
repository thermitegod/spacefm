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
#include <memory>

#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "gui/dialog/widgets/text-entry.hxx"

#include "vfs/task-manager.hxx"

namespace gui::dialog
{
class overwrite : public Gtk::ApplicationWindow
{
  public:
    overwrite(Gtk::ApplicationWindow& parent,
              const std::shared_ptr<vfs::task_collision>& collision);

  private:
    std::shared_ptr<vfs::task_collision> collision_;

    Gtk::Box box_;

    Gtk::Label label_message_;
    Gtk::Label label_from_;
    Gtk::Label label_to_;

    Gtk::Label label_file_info_;

    gui::widget::TextEntry* name_entry_;

    Gtk::Button* button_rename_;
    Gtk::Button* button_rename_auto_;
    Gtk::Button* button_rename_all_;
    Gtk::Button* button_overwrite_;
    Gtk::Button* button_overwrite_all_;
    Gtk::Button* button_pause_;
    Gtk::Button* button_skip_;
    Gtk::Button* button_skip_all_;
    Gtk::Button* button_cancel_;

    bool is_dir_ = false;
    bool is_copy_ = false;
    std::filesystem::path source_;
    std::filesystem::path destination_;

    // Signal Handlers

    void on_filename_update() noexcept;

    bool on_key_press(std::uint32_t keyval, std::uint32_t keycode,
                      Gdk::ModifierType state) noexcept;

    void on_button_rename_clicked() noexcept;
    void on_button_rename_auto_clicked() noexcept;
    void on_button_rename_all_clicked() noexcept;
    void on_button_overwrite_clicked() noexcept;
    void on_button_overwrite_all_clicked() noexcept;
    void on_button_pause_clicked() noexcept;
    void on_button_skip_clicked() noexcept;
    void on_button_skip_all_clicked() noexcept;
    void on_button_cancel_clicked() noexcept;
};
} // namespace gui::dialog
