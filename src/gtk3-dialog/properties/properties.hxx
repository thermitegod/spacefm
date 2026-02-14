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

#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <string_view>
#include <thread>
#include <vector>

#include <gtkmm.h>

#include "vfs/file.hxx"

class PropertiesDialog : public Gtk::ApplicationWindow
{
  public:
    PropertiesDialog(const std::string_view json_data);

  protected:
    Gtk::Box box_;
    Gtk::Notebook notebook_;

    Gtk::Label total_size_label_;
    Gtk::Label size_on_disk_label_;
    Gtk::Label count_label_;

    Gtk::Box button_box_;
    Gtk::Button button_close_;

    // Signal Handlers
    bool on_key_press(std::uint32_t keyval, std::uint32_t keycode, Gdk::ModifierType state);
    void on_button_close_clicked();

  private:
    void calc_total_size_of_files(const std::filesystem::path& path) noexcept;
    void calc_size() noexcept;
    void on_update_labels() noexcept;

    void init_file_info_tab() noexcept;
    void init_media_info_tab() noexcept;
    void init_attributes_tab() noexcept;
    void init_permissions_tab() noexcept;

    std::vector<std::shared_ptr<vfs::file>> file_list_;
    std::filesystem::path cwd_;

    u64 total_size_{0};
    u64 size_on_disk_{0};
    u64 total_count_file_{0};
    u64 total_count_dir_{0};

    std::jthread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool abort_ = false;
};
