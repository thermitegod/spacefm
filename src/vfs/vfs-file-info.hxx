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

#include <atomic>
#include <chrono>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-mime-type.hxx"

#include <gtk/gtk.h>

#define VFS_FILE_INFO(obj)             (static_cast<VFSFileInfo*>(obj))
#define VFS_FILE_INFO_REINTERPRET(obj) (reinterpret_cast<VFSFileInfo*>(obj))

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
    unsigned int ref_count();

  private:
    std::atomic<unsigned int> n_ref{0};
};

VFSFileInfo* vfs_file_info_new();
VFSFileInfo* vfs_file_info_ref(VFSFileInfo* fi);
void vfs_file_info_unref(VFSFileInfo* fi);

bool vfs_file_info_get(VFSFileInfo* fi, const std::string& file_path);

const char* vfs_file_info_get_name(VFSFileInfo* fi);
const char* vfs_file_info_get_disp_name(VFSFileInfo* fi);

void vfs_file_info_set_disp_name(VFSFileInfo* fi, const char* name);

off_t vfs_file_info_get_size(VFSFileInfo* fi);
const char* vfs_file_info_get_disp_size(VFSFileInfo* fi);

off_t vfs_file_info_get_blocks(VFSFileInfo* fi);

mode_t vfs_file_info_get_mode(VFSFileInfo* fi);

VFSMimeType* vfs_file_info_get_mime_type(VFSFileInfo* fi);
void vfs_file_info_reload_mime_type(VFSFileInfo* fi, const char* full_path);

const char* vfs_file_info_get_mime_type_desc(VFSFileInfo* fi);

const char* vfs_file_info_get_disp_owner(VFSFileInfo* fi);
const char* vfs_file_info_get_disp_mtime(VFSFileInfo* fi);
const char* vfs_file_info_get_disp_perm(VFSFileInfo* fi);

time_t* vfs_file_info_get_mtime(VFSFileInfo* fi);
time_t* vfs_file_info_get_atime(VFSFileInfo* fi);

void vfs_file_info_set_thumbnail_size_big(int size);
void vfs_file_info_set_thumbnail_size_small(int size);

bool vfs_file_info_load_thumbnail(VFSFileInfo* fi, const std::string& full_path, bool big);
bool vfs_file_info_is_thumbnail_loaded(VFSFileInfo* fi, bool big);

GdkPixbuf* vfs_file_info_get_big_icon(VFSFileInfo* fi);
GdkPixbuf* vfs_file_info_get_small_icon(VFSFileInfo* fi);

GdkPixbuf* vfs_file_info_get_big_thumbnail(VFSFileInfo* fi);
GdkPixbuf* vfs_file_info_get_small_thumbnail(VFSFileInfo* fi);

bool vfs_file_info_is_dir(VFSFileInfo* fi);
bool vfs_file_info_is_regular_file(VFSFileInfo* fi);
bool vfs_file_info_is_symlink(VFSFileInfo* fi);
bool vfs_file_info_is_socket(VFSFileInfo* fi);
bool vfs_file_info_is_named_pipe(VFSFileInfo* fi);
bool vfs_file_info_is_block_device(VFSFileInfo* fi);
bool vfs_file_info_is_char_device(VFSFileInfo* fi);

bool vfs_file_info_is_image(VFSFileInfo* fi);
bool vfs_file_info_is_video(VFSFileInfo* fi);
bool vfs_file_info_is_desktop_entry(VFSFileInfo* fi);

/* Full path of the file is required by this function */
bool vfs_file_info_is_executable(VFSFileInfo* fi, const char* file_path);

/* Full path of the file is required by this function */
bool vfs_file_info_is_text(VFSFileInfo* fi, const char* file_path);

void vfs_file_info_load_special_info(VFSFileInfo* fi, const char* file_path);

void vfs_file_info_list_free(std::vector<VFSFileInfo*>& list);
