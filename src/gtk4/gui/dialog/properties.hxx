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
#include <stop_token>
#include <thread>
#include <vector>

#include <gtkmm.h>

#include "vfs/file.hxx"

namespace gui::dialog
{
class properties : public Gtk::ApplicationWindow
{
  public:
    properties(Gtk::ApplicationWindow& parent, std::int32_t page, const std::filesystem::path& cwd,
               const std::span<const std::shared_ptr<vfs::file>>& files);

  protected:
    Gtk::Box box_;
    Gtk::Notebook notebook_;

    Gtk::Label total_size_label_;
    Gtk::Label size_on_disk_label_;
    Gtk::Label count_label_;

    Gtk::Box button_box_;
    Gtk::Button button_close_;

    // Signal Handlers
    bool on_key_press(std::uint32_t keyval, std::uint32_t keycode,
                      Gdk::ModifierType state) noexcept;
    void on_button_close_clicked() noexcept;

  private:
    void calc_total_size_of_files(const std::filesystem::path& path,
                                  const std::stop_token& stoken) noexcept;
    void calc_size(const std::stop_token& stoken) noexcept;
    void on_update_labels() noexcept;

    void init_file_info_tab() noexcept;
    void init_media_info_tab() noexcept;
    void init_attributes_tab() noexcept;
    void init_permissions_tab() noexcept;

    std::vector<std::shared_ptr<vfs::file>> files_;
    std::filesystem::path cwd_;

    u64 total_size_{0};
    u64 size_on_disk_{0};
    u64 total_count_file_{0};
    u64 total_count_dir_{0};

    std::jthread thread_;
};
} // namespace gui::dialog
