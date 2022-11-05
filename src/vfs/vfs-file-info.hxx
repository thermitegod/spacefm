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

#include <vector>

#include <memory>

#include <atomic>
#include <chrono>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-mime-type.hxx"

#include <gtk/gtk.h>

#define VFS_FILE_INFO(obj) (static_cast<vfs::file_info>(obj))

// For future use, not all supported now
enum VFSFileInfoFlag
{
    VFS_FILE_INFO_NONE = 0,
    VFS_FILE_INFO_HOME_DIR = (1 << 0),    // Not implemented
    VFS_FILE_INFO_DESKTOP_DIR = (1 << 1), // Not implemented
    VFS_FILE_INFO_DESKTOP_ENTRY = (1 << 2),
    VFS_FILE_INFO_MOUNT_POINT = (1 << 3), // Not implemented
    VFS_FILE_INFO_REMOTE = (1 << 4),      // Not implemented
    VFS_FILE_INFO_VIRTUAL = (1 << 5)      // Not implemented
};

struct VFSFileInfo
{
  public:
    // cached copy of struct stat()
    // Only use some members of struct stat to reduce memory usage
    dev_t dev;         // st_dev     - ID of device containing file
    mode_t mode;       // st_dev     - File type and mode
    uid_t uid;         // st_uid     - User ID of owner
    gid_t gid;         // st_gid     - Group ID of owner
    off_t size;        // st_size    - Total size, in bytes
    std::time_t atime; // st_atime   - Time of the last access
    std::time_t mtime; // st_mtime   - Time of last modification
    blksize_t blksize; // st_blksize - Block size for filesystem I/O
    blkcnt_t blocks;   // st_blocks  - Number of 512B blocks allocated

    std::filesystem::file_status status;

    std::string name;              // real name on file system
    std::string disp_name;         // displayed name (in UTF-8)
    std::string collate_key;       // sfm sort key
    std::string collate_icase_key; // sfm case folded sort key
    std::string disp_size;         // displayed human-readable file size
    std::string disp_owner;        // displayed owner:group pair
    std::string disp_mtime;        // displayed last modification time
    std::string disp_perm;         // displayed permission in string form
    vfs::mime_type mime_type;      // mime type related information
    GdkPixbuf* big_thumbnail;      // thumbnail of the file
    GdkPixbuf* small_thumbnail;    // thumbnail of the file

    VFSFileInfoFlag flags; // if it is a special file

  public:
    const std::string& get_name() const noexcept;
    const std::string& get_disp_name() const noexcept;

    void set_disp_name(std::string_view new_disp_name) noexcept;

    off_t get_size() const noexcept;
    const std::string& get_disp_size() const noexcept;

    blkcnt_t get_blocks() const noexcept;

    std::filesystem::perms get_permissions() const noexcept;

    vfs::mime_type get_mime_type() const noexcept;
    void reload_mime_type(std::string_view full_path) noexcept;

    const std::string get_mime_type_desc() const noexcept;

    const std::string& get_disp_owner() noexcept;
    const std::string& get_disp_mtime() noexcept;
    const std::string& get_disp_perm() noexcept;

    std::time_t* get_mtime() noexcept;
    std::time_t* get_atime() noexcept;

    bool load_thumbnail(std::string_view full_path, bool big) noexcept;
    bool is_thumbnail_loaded(bool big) const noexcept;

    GdkPixbuf* get_big_icon() noexcept;
    GdkPixbuf* get_small_icon() noexcept;

    GdkPixbuf* get_big_thumbnail() const noexcept;
    GdkPixbuf* get_small_thumbnail() const noexcept;

    void load_special_info(std::string_view file_path) noexcept;

    bool is_directory() const noexcept;
    bool is_regular_file() const noexcept;
    bool is_symlink() const noexcept;
    bool is_socket() const noexcept;
    bool is_fifo() const noexcept;
    bool is_block_file() const noexcept;
    bool is_character_file() const noexcept;

    bool is_image() const noexcept;
    bool is_video() const noexcept;
    bool is_desktop_entry() const noexcept;
    bool is_unknown_type() const noexcept;

    // Full path of the file is required by this function
    bool is_executable(std::string_view file_path = "") const noexcept;

    // Full path of the file is required by this function
    bool is_text(std::string_view file_path = "") const noexcept;

  public:
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

vfs::file_info vfs_file_info_new();
vfs::file_info vfs_file_info_ref(vfs::file_info file);
void vfs_file_info_unref(vfs::file_info file);

bool vfs_file_info_get(vfs::file_info file, std::string_view file_path);

void vfs_file_info_list_free(const std::vector<vfs::file_info>& list);

void vfs_file_info_set_thumbnail_size_big(i32 size);
void vfs_file_info_set_thumbnail_size_small(i32 size);
