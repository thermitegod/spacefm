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

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "vfs/vfs-mime-type.hxx"

namespace vfs
{
struct file : public std::enable_shared_from_this<file>
{
  public:
    file() = delete;
    explicit file(const std::filesystem::path& file_path) noexcept;
    ~file() noexcept;
    file(const file& other) = delete;
    file(file&& other) = delete;
    file& operator=(const file& other) = delete;
    file& operator=(file&& other) = delete;

    [[nodiscard]] static std::shared_ptr<vfs::file>
    create(const std::filesystem::path& path) noexcept;

    [[nodiscard]] std::string_view name() const noexcept;

    [[nodiscard]] const std::filesystem::path& path() const noexcept;

    [[nodiscard]] u64 size() const noexcept;
    [[nodiscard]] u64 size_on_disk() const noexcept;

    [[nodiscard]] std::string_view display_size() const noexcept;
    [[nodiscard]] std::string_view display_size_in_bytes() const noexcept;
    [[nodiscard]] std::string_view display_size_on_disk() const noexcept;

    [[nodiscard]] u64 blocks() const noexcept;

    [[nodiscard]] std::filesystem::perms permissions() const noexcept;

    [[nodiscard]] const std::shared_ptr<vfs::mime_type>& mime_type() const noexcept;

    [[nodiscard]] std::string_view display_owner() const noexcept;
    [[nodiscard]] std::string_view display_group() const noexcept;
    [[nodiscard]] std::string_view display_atime() const noexcept;
    [[nodiscard]] std::string_view display_btime() const noexcept;
    [[nodiscard]] std::string_view display_ctime() const noexcept;
    [[nodiscard]] std::string_view display_mtime() const noexcept;

    [[nodiscard]] std::chrono::system_clock::time_point atime() const noexcept;
    [[nodiscard]] std::chrono::system_clock::time_point btime() const noexcept;
    [[nodiscard]] std::chrono::system_clock::time_point ctime() const noexcept;
    [[nodiscard]] std::chrono::system_clock::time_point mtime() const noexcept;

    [[nodiscard]] bool is_directory() const noexcept;
    [[nodiscard]] bool is_regular_file() const noexcept;
    [[nodiscard]] bool is_symlink() const noexcept;
    [[nodiscard]] bool is_socket() const noexcept;
    [[nodiscard]] bool is_fifo() const noexcept;
    [[nodiscard]] bool is_block_file() const noexcept;
    [[nodiscard]] bool is_character_file() const noexcept;
    [[nodiscard]] bool is_other() const noexcept;

    [[nodiscard]] bool is_hidden() const noexcept;

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
    ztd::statx file_stat_;
    std::filesystem::file_status status_;

    std::filesystem::path path_; // real path on file system

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
    std::shared_ptr<vfs::mime_type> mime_type_; // mime type related information

    bool is_hidden_{false}; // if the filename starts with '.'
};
} // namespace vfs
