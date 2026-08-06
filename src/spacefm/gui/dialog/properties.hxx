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

#include "settings/settings.hxx"

#include "gui/dialog/media/metadata.hxx"

#include "vfs/file.hxx"

namespace gui::dialog
{
class properties : public Gtk::ApplicationWindow
{
  public:
    enum class page : std::int32_t
    {
        info = 0,
        media,
        checksum,
        attributes,
        permissions,
    };

    properties(Gtk::ApplicationWindow& parent, properties::page page,
               const std::filesystem::path& cwd,
               const std::span<const std::shared_ptr<vfs::file>>& files,
               const std::shared_ptr<config::settings>& settings) noexcept;

  protected:
    std::shared_ptr<config::settings> settings_;

    Gtk::Box box_;
    Gtk::Notebook notebook_;

    Gtk::Label total_size_label_;
    Gtk::Label size_on_disk_label_;
    Gtk::Label count_label_;

    Gtk::Button* button_close_;

    // Signal Handlers
    bool on_key_press(std::uint32_t keyval, std::uint32_t keycode,
                      Gdk::ModifierType state) noexcept;
    void on_button_close_clicked() noexcept;

  private:
    void init_file_info_tab() noexcept;
    void init_media_info_tab() noexcept;
    void init_checksum_tab() noexcept;
    void init_attributes_tab() noexcept;
    void init_permissions_tab() noexcept;

    void on_size_update() noexcept;

    std::vector<std::shared_ptr<vfs::file>> files_;
    std::filesystem::path cwd_;

    struct calc_worker
    {
        calc_worker(std::vector<std::shared_ptr<vfs::file>> targets) : files(std::move(targets)) {}

        void calc_size(const std::stop_token& stoken) noexcept;
        void calc_total_size_of_files(const std::stop_token& stoken,
                                      const std::filesystem::path& path) noexcept;

        std::vector<std::shared_ptr<vfs::file>> files;

        std::atomic<std::uint64_t> total_size{0};
        std::atomic<std::uint64_t> size_on_disk{0};
        std::atomic<std::uint64_t> total_count_file{0};
        std::atomic<std::uint64_t> total_count_dir{0};

        Glib::Dispatcher dispatcher;
        std::jthread thread;
    };
    std::unique_ptr<calc_worker> calc_worker_;

#if defined(HAVE_MEDIA)
    struct metadata_worker
    {
        metadata_worker(const std::shared_ptr<vfs::file>& file) : file(std::move(file)) {}

        void extract_metadata(const std::stop_token& stoken) noexcept;

        std::shared_ptr<vfs::file> file;

        std::vector<metadata_data> result;

        Glib::Dispatcher dispatcher;
        std::jthread thread;
    };
    std::unique_ptr<metadata_worker> metadata_worker_;
#endif
};
} // namespace gui::dialog
