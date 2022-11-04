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

#include <atomic>
#include <chrono>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-mime-type.hxx"

#include <gtk/gtk.h>

#define VFS_FILE_INFO(obj)             (static_cast<vfs::file_info>(obj))
#define VFS_FILE_INFO_REINTERPRET(obj) (reinterpret_cast<vfs::file_info>(obj))

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
    // cached copy of struct stat file_stat
    // Only use some members of struct stat to reduce memory usage
    dev_t dev;         // st_dev - ID of device containing file
    mode_t mode;       // st_dev - File type and mode
    uid_t uid;         // st_uid - User ID of owner
    gid_t gid;         // st_gid - Group ID of owner
    off_t size;        // st_size - Total size, in bytes
    std::time_t atime; // st_atime - Time of the last access
    std::time_t mtime; // st_mtime - Time of last modification
    blksize_t blksize; // st_blksize - Block size for filesystem I/O
    blkcnt_t blocks;   // st_blocks - Number of 512B blocks allocated

    std::filesystem::file_status status;

    std::string name;              // real name on file system
    std::string disp_name;         // displayed name (in UTF-8)
    std::string collate_key;       // sfm sort key
    std::string collate_icase_key; // sfm case folded sort key
    std::string disp_size;         // displayed human-readable file size
    std::string disp_owner;        // displayed owner:group pair
    std::string disp_mtime;        // displayed last modification time
    std::string disp_perm;         // displayed permission in string form
    VFSMimeType* mime_type;        // mime type related information
    GdkPixbuf* big_thumbnail;      // thumbnail of the file
    GdkPixbuf* small_thumbnail;    // thumbnail of the file

    VFSFileInfoFlag flags; // if it is a special file

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
vfs::file_info vfs_file_info_ref(vfs::file_info fi);
void vfs_file_info_unref(vfs::file_info fi);

bool vfs_file_info_get(vfs::file_info fi, std::string_view file_path);

const char* vfs_file_info_get_name(vfs::file_info fi);
const char* vfs_file_info_get_disp_name(vfs::file_info fi);

void vfs_file_info_set_disp_name(vfs::file_info fi, const char* name);

off_t vfs_file_info_get_size(vfs::file_info fi);
const char* vfs_file_info_get_disp_size(vfs::file_info fi);

off_t vfs_file_info_get_blocks(vfs::file_info fi);

std::filesystem::perms vfs_file_info_get_mode(vfs::file_info fi);

VFSMimeType* vfs_file_info_get_mime_type(vfs::file_info fi);
void vfs_file_info_reload_mime_type(vfs::file_info fi, const char* full_path);

const char* vfs_file_info_get_mime_type_desc(vfs::file_info fi);

const char* vfs_file_info_get_disp_owner(vfs::file_info fi);
const char* vfs_file_info_get_disp_mtime(vfs::file_info fi);
const char* vfs_file_info_get_disp_perm(vfs::file_info fi);

time_t* vfs_file_info_get_mtime(vfs::file_info fi);
time_t* vfs_file_info_get_atime(vfs::file_info fi);

void vfs_file_info_set_thumbnail_size_big(i32 size);
void vfs_file_info_set_thumbnail_size_small(i32 size);

bool vfs_file_info_load_thumbnail(vfs::file_info fi, std::string_view full_path, bool big);
bool vfs_file_info_is_thumbnail_loaded(vfs::file_info fi, bool big);

GdkPixbuf* vfs_file_info_get_big_icon(vfs::file_info fi);
GdkPixbuf* vfs_file_info_get_small_icon(vfs::file_info fi);

GdkPixbuf* vfs_file_info_get_big_thumbnail(vfs::file_info fi);
GdkPixbuf* vfs_file_info_get_small_thumbnail(vfs::file_info fi);

bool vfs_file_info_is_dir(vfs::file_info fi);
bool vfs_file_info_is_regular_file(vfs::file_info fi);
bool vfs_file_info_is_symlink(vfs::file_info fi);
bool vfs_file_info_is_socket(vfs::file_info fi);
bool vfs_file_info_is_named_pipe(vfs::file_info fi);
bool vfs_file_info_is_block_device(vfs::file_info fi);
bool vfs_file_info_is_char_device(vfs::file_info fi);

bool vfs_file_info_is_image(vfs::file_info fi);
bool vfs_file_info_is_video(vfs::file_info fi);
bool vfs_file_info_is_desktop_entry(vfs::file_info fi);

/* Full path of the file is required by this function */
bool vfs_file_info_is_executable(vfs::file_info fi, std::string_view file_path = "");

/* Full path of the file is required by this function */
bool vfs_file_info_is_text(vfs::file_info fi, std::string_view file_path = "");

void vfs_file_info_load_special_info(vfs::file_info fi, std::string_view file_path = "");

void vfs_file_info_list_free(const std::vector<vfs::file_info>& list);
