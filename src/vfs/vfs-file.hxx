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
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-mime-type.hxx"

// https://en.cppreference.com/w/cpp/memory/enable_shared_from_this

namespace vfs
{
    struct file : public std::enable_shared_from_this<file>
    {
      public:
        file(const std::filesystem::path& file_path);
        ~file();

        static const std::shared_ptr<vfs::file> create(const std::filesystem::path& path) noexcept;

        const std::string_view name() const noexcept;
        const std::string_view display_name() const noexcept;

        void update_display_name(const std::string_view new_display_name) noexcept;

        const std::filesystem::path& path() const noexcept;
        const std::string_view uri() const noexcept;

        u64 size() const noexcept;
        u64 size_on_disk() const noexcept;

        const std::string_view display_size() const noexcept;
        const std::string_view display_size_in_bytes() const noexcept;
        const std::string_view display_size_on_disk() const noexcept;

        u64 blocks() const noexcept;

        std::filesystem::perms permissions() const noexcept;

        const std::shared_ptr<vfs::mime_type>& mime_type() const noexcept;
        void reload_mime_type() noexcept;

        const std::string_view display_owner() const noexcept;
        const std::string_view display_group() const noexcept;
        const std::string_view display_atime() const noexcept;
        const std::string_view display_btime() const noexcept;
        const std::string_view display_ctime() const noexcept;
        const std::string_view display_mtime() const noexcept;
        const std::string_view display_permissions() noexcept;

        std::time_t atime() const noexcept;
        std::time_t btime() const noexcept;
        std::time_t ctime() const noexcept;
        std::time_t mtime() const noexcept;

        void load_thumbnail(bool big) noexcept;
        bool is_thumbnail_loaded(bool big) const noexcept;

        GdkPixbuf* big_icon() noexcept;
        GdkPixbuf* small_icon() noexcept;

        GdkPixbuf* big_thumbnail() const noexcept;
        GdkPixbuf* small_thumbnail() const noexcept;

        void unload_big_thumbnail() noexcept;
        void unload_small_thumbnail() noexcept;

        bool is_directory() const noexcept;
        bool is_regular_file() const noexcept;
        bool is_symlink() const noexcept;
        bool is_socket() const noexcept;
        bool is_fifo() const noexcept;
        bool is_block_file() const noexcept;
        bool is_character_file() const noexcept;
        bool is_other() const noexcept;

        bool is_hidden() const noexcept;

        bool is_image() const noexcept;
        bool is_video() const noexcept;
        bool is_archive() const noexcept;
        bool is_desktop_entry() const noexcept;
        bool is_unknown_type() const noexcept;

        bool is_executable() const noexcept;
        bool is_text() const noexcept;

        // File attributes
        bool is_compressed() const noexcept; // file is compressed by the filesystem
        bool is_immutable() const noexcept;  // file cannot be modified
        bool is_append() const noexcept;     // file can only be opened in append mode for writing
        bool is_nodump() const noexcept;     // file is not a candidate for backup
        bool is_encrypted() const noexcept; // file requires a key to be encrypted by the filesystem
        bool is_verity() const noexcept;    // file has fs-verity enabled
        bool is_dax() const noexcept;       // file is in the DAX (cpu direct access) state

        // update file info
        bool update() noexcept;

      private:
        ztd::statx file_stat_; // cached copy of struct statx()
        std::filesystem::file_status status_;

        std::filesystem::path path_{}; // real path on file system
        std::string uri_{};            // uri of the real path on file system

        std::string name_{};                          // real name on file system
        std::string display_name_{};                  // displayed name (in UTF-8)
        std::string display_size_{};                  // displayed human-readable file size
        std::string display_size_bytes_{};            // displayed file size in bytes
        std::string display_disk_size_{};             // displayed human-readable file size on disk
        std::string display_owner_{};                 // displayed owner
        std::string display_group_{};                 // displayed group
        std::string display_atime_{};                 // displayed accessed time
        std::string display_btime_{};                 // displayed created time
        std::string display_ctime_{};                 // displayed last status change time
        std::string display_mtime_{};                 // displayed modification time
        std::string display_perm_{};                  // displayed permission in string form
        std::shared_ptr<vfs::mime_type> mime_type_{}; // mime type related information
        GdkPixbuf* big_thumbnail_{};                  // thumbnail of the file
        GdkPixbuf* small_thumbnail_{};                // thumbnail of the file

        bool is_special_desktop_entry_{false}; // is a .desktop file

        bool is_hidden_{false}; // if the filename starts with '.'

      private:
        void load_thumbnail_small() noexcept;
        void load_thumbnail_big() noexcept;

        void load_special_info() noexcept;

        const std::string_view special_directory_get_icon_name() const noexcept;
    };
} // namespace vfs
