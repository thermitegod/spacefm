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

#include <span>

#include <atomic>

#include <chrono>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-mime-type.hxx"

#include <gtk/gtk.h>

#define VFS_FILE_INFO(obj) (static_cast<vfs::file_info>(obj))

namespace vfs
{
    enum file_info_flags
    { // For future use, not all supported now
        none = 0,
        home_dir = (1 << 0),    // Not implemented
        desktop_dir = (1 << 1), // Not implemented
        desktop_entry = (1 << 2),
        mount_point = (1 << 3), // Not implemented
        remote = (1 << 4),      // Not implemented
        // VIRTUAL = (1 << 5),     // Not implemented
    };
}

struct VFSFileInfo
{
  public:
    VFSFileInfo(const std::filesystem::path& file_path);
    ~VFSFileInfo();

    const std::string_view name() const noexcept;
    const std::string_view display_name() const noexcept;

    void update_display_name(const std::string_view new_display_name) noexcept;

    const std::filesystem::path& path() const noexcept;

    const std::string_view collate_key() const noexcept;
    const std::string_view collate_icase_key() const noexcept;

    u64 size() const noexcept;
    u64 size_on_disk() const noexcept;

    const std::string_view display_size() const noexcept;
    const std::string_view display_size_in_bytes() const noexcept;
    const std::string_view display_size_on_disk() const noexcept;

    u64 blocks() const noexcept;

    std::filesystem::perms permissions() const noexcept;

    vfs::mime_type mime_type() const noexcept;
    void reload_mime_type(const std::filesystem::path& full_path) noexcept;

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

    void load_thumbnail(const std::filesystem::path& full_path, bool big) noexcept;
    bool is_thumbnail_loaded(bool big) const noexcept;

    GdkPixbuf* big_icon() noexcept;
    GdkPixbuf* small_icon() noexcept;

    GdkPixbuf* big_thumbnail() const noexcept;
    GdkPixbuf* small_thumbnail() const noexcept;

    void unload_big_thumbnail() noexcept;
    void unload_small_thumbnail() noexcept;

    vfs::file_info_flags flags() const noexcept;

    void load_special_info(const std::filesystem::path& file_path) noexcept;

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
    bool is_desktop_entry() const noexcept;
    bool is_unknown_type() const noexcept;

    // Full path of the file is required by this function
    bool is_executable(const std::filesystem::path& file_path = "") const noexcept;

    // Full path of the file is required by this function
    bool is_text(const std::filesystem::path& file_path = "") const noexcept;

    // File attributes
    bool is_compressed() const noexcept; // file is compressed by the filesystem
    bool is_immutable() const noexcept;  // file cannot be modified
    bool is_append() const noexcept;     // file can only be opened in append mode for writing
    bool is_nodump() const noexcept;     // file is not a candidate for backup
    bool is_encrypted() const noexcept;  // file requires a key to be encrypted by the filesystem
    bool is_verity() const noexcept;     // file has fs-verity enabled
    bool is_dax() const noexcept;        // file is in the DAX (cpu direct access) state

    // update file info
    bool update(const std::filesystem::path& file_path) noexcept;

  private:
    ztd::statx file_stat_; // cached copy of struct statx()
    std::filesystem::file_status status_;

    std::filesystem::path path_{};     // real path on file system
    std::string name_{};               // real name on file system
    std::string display_name_{};       // displayed name (in UTF-8)
    std::string collate_key_{};        // sfm sort key
    std::string collate_icase_key_{};  // sfm case folded sort key
    std::string display_size_{};       // displayed human-readable file size
    std::string display_size_bytes_{}; // displayed file size in bytes
    std::string display_disk_size_{};  // displayed human-readable file size on disk
    std::string display_owner_{};      // displayed owner
    std::string display_group_{};      // displayed group
    std::string display_atime_{};      // displayed accessed time
    std::string display_btime_{};      // displayed created time
    std::string display_ctime_{};      // displayed last status change time
    std::string display_mtime_{};      // displayed modification time
    std::string display_perm_{};       // displayed permission in string form
    vfs::mime_type mime_type_{};       // mime type related information
    GdkPixbuf* big_thumbnail_{};       // thumbnail of the file
    GdkPixbuf* small_thumbnail_{};     // thumbnail of the file
    vfs::file_info_flags flags_{vfs::file_info_flags::none}; // if it is a special file

    bool is_hidden_{false}; // if the filename starts with '.'

  private:
    void load_thumbnail_small(const std::filesystem::path& full_path) noexcept;
    void load_thumbnail_big(const std::filesystem::path& full_path) noexcept;

  public: // TODO need to remove manual ref counting
    void ref_inc();
    void ref_dec();
    u32 ref_count();

  private:
    std::atomic<u32> n_ref{0};
};

namespace vfs
{
    using file_info = ztd::raw_ptr<VFSFileInfo>;
} // namespace vfs

vfs::file_info vfs_file_info_new(const std::filesystem::path& file_path);

vfs::file_info vfs_file_info_ref(vfs::file_info file);
void vfs_file_info_unref(vfs::file_info file);

void vfs_file_info_list_free(const std::span<const vfs::file_info> list);
