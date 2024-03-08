/**
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *
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

#include <string>
#include <string_view>

#include <filesystem>

#include <memory>

#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "vfs/vfs-mime-type.hxx"

// https://en.cppreference.com/w/cpp/memory/enable_shared_from_this

namespace vfs
{
struct file : public std::enable_shared_from_this<file>
{
  public:
    file() = delete;
    file(const std::filesystem::path& file_path);
    ~file();
    file(const file& other) = delete;
    file(file&& other) = delete;
    file& operator=(const file& other) = delete;
    file& operator=(file&& other) = delete;

    [[nodiscard]] static const std::shared_ptr<vfs::file>
    create(const std::filesystem::path& path) noexcept;

    [[nodiscard]] const std::string_view name() const noexcept;

    [[nodiscard]] const std::filesystem::path& path() const noexcept;
    [[nodiscard]] const std::string_view uri() const noexcept;

    [[nodiscard]] u64 size() const noexcept;
    [[nodiscard]] u64 size_on_disk() const noexcept;

    [[nodiscard]] const std::string_view display_size() const noexcept;
    [[nodiscard]] const std::string_view display_size_in_bytes() const noexcept;
    [[nodiscard]] const std::string_view display_size_on_disk() const noexcept;

    [[nodiscard]] u64 blocks() const noexcept;

    [[nodiscard]] std::filesystem::perms permissions() const noexcept;

    [[nodiscard]] const std::shared_ptr<vfs::mime_type>& mime_type() const noexcept;
    void reload_mime_type() noexcept;

    [[nodiscard]] const std::string_view display_owner() const noexcept;
    [[nodiscard]] const std::string_view display_group() const noexcept;
    [[nodiscard]] const std::string_view display_atime() const noexcept;
    [[nodiscard]] const std::string_view display_btime() const noexcept;
    [[nodiscard]] const std::string_view display_ctime() const noexcept;
    [[nodiscard]] const std::string_view display_mtime() const noexcept;
    [[nodiscard]] const std::string_view display_permissions() noexcept;

    [[nodiscard]] const std::chrono::system_clock::time_point atime() const noexcept;
    [[nodiscard]] const std::chrono::system_clock::time_point btime() const noexcept;
    [[nodiscard]] const std::chrono::system_clock::time_point ctime() const noexcept;
    [[nodiscard]] const std::chrono::system_clock::time_point mtime() const noexcept;

    enum class thumbnail_size
    {
        big,
        small,
    };
    GdkPixbuf* icon(const thumbnail_size size) noexcept;
    GdkPixbuf* thumbnail(const thumbnail_size size) const noexcept;
    void load_thumbnail(const thumbnail_size size) noexcept;
    void unload_thumbnail(const thumbnail_size size) noexcept;
    [[nodiscard]] bool is_thumbnail_loaded(const thumbnail_size size) const noexcept;

    [[nodiscard]] bool is_directory() const noexcept;
    [[nodiscard]] bool is_regular_file() const noexcept;
    [[nodiscard]] bool is_symlink() const noexcept;
    [[nodiscard]] bool is_socket() const noexcept;
    [[nodiscard]] bool is_fifo() const noexcept;
    [[nodiscard]] bool is_block_file() const noexcept;
    [[nodiscard]] bool is_character_file() const noexcept;
    [[nodiscard]] bool is_other() const noexcept;

    [[nodiscard]] bool is_hidden() const noexcept;

    [[nodiscard]] bool is_desktop_entry() const noexcept;

    // File attributes
    [[nodiscard]] bool is_compressed() const noexcept; // file is compressed by the filesystem
    [[nodiscard]] bool is_immutable() const noexcept;  // file cannot be modified
    [[nodiscard]] bool
    is_append() const noexcept; // file can only be opened in append mode for writing
    [[nodiscard]] bool is_nodump() const noexcept; // file is not a candidate for backup
    [[nodiscard]] bool
    is_encrypted() const noexcept; // file requires a key to be encrypted by the filesystem
    [[nodiscard]] bool is_automount() const noexcept;  // file is a automount trigger
    [[nodiscard]] bool is_mount_root() const noexcept; // file is the root of a mount
    [[nodiscard]] bool is_verity() const noexcept;     // file has fs-verity enabled
    [[nodiscard]] bool is_dax() const noexcept; // file is in the DAX (cpu direct access) state

    // update file info
    [[nodiscard]] bool update() noexcept;

  private:
    ztd::statx file_stat_; // cached copy of struct statx()
    std::filesystem::file_status status_;

    std::filesystem::path path_; // real path on file system
    std::string uri_;            // uri of the real path on file system

    std::string name_;                          // real name on file system
    std::string display_size_;                  // displayed human-readable file size
    std::string display_size_bytes_;            // displayed file size in bytes
    std::string display_disk_size_;             // displayed human-readable file size on disk
    std::string display_owner_;                 // displayed owner
    std::string display_group_;                 // displayed group
    std::string display_atime_;                 // displayed accessed time
    std::string display_btime_;                 // displayed created time
    std::string display_ctime_;                 // displayed last status change time
    std::string display_mtime_;                 // displayed modification time
    std::string display_perm_;                  // displayed permission in string form
    std::shared_ptr<vfs::mime_type> mime_type_; // mime type related information
    GdkPixbuf* big_thumbnail_{nullptr};         // thumbnail of the file
    GdkPixbuf* small_thumbnail_{nullptr};       // thumbnail of the file

    bool is_special_desktop_entry_{false}; // is a .desktop file

    bool is_hidden_{false}; // if the filename starts with '.'

  private:
    void load_special_info() noexcept;

    [[nodiscard]] const std::string create_file_perm_string() const noexcept;

    [[nodiscard]] const std::string_view
    special_directory_get_icon_name(const bool symbolic = false) const noexcept;
};
} // namespace vfs
