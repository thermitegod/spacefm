/*
 *  C Interface: vfs-file-info
 *
 * Description: File information
 *
 *
 * Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 */

#pragma once

#include <atomic>
#include <ctime>

#include "vfs-mime-type.hxx"

#include <gtk/gtk.h>

enum VFSFileInfoFlag
{
    VFS_FILE_INFO_NONE = 0,
    VFS_FILE_INFO_HOME_DIR = (1 << 0),
    VFS_FILE_INFO_DESKTOP_DIR = (1 << 1),
    VFS_FILE_INFO_DESKTOP_ENTRY = (1 << 2),
    VFS_FILE_INFO_MOUNT_POINT = (1 << 3),
    VFS_FILE_INFO_REMOTE = (1 << 4),
    VFS_FILE_INFO_VIRTUAL = (1 << 5)
}; /* For future use, not all supported now */

struct VFSFileInfo
{
    /* struct stat file_stat; */
    /* Only use some members of struct stat to reduce memory usage */
    mode_t mode;
    dev_t dev;
    uid_t uid;
    gid_t gid;
    off_t size;
    std::time_t mtime;
    std::time_t atime;
    long blksize;
    blkcnt_t blocks;

    char* name;                 /* real name on file system */
    char* disp_name;            /* displayed name (in UTF-8) */
    char* collate_key;          // sfm sort key
    char* collate_icase_key;    // sfm case folded sort key
    char* disp_size;            /* displayed human-readable file size */
    char* disp_owner;           /* displayed owner:group pair */
    char* disp_mtime;           /* displayed last modification time */
    char disp_perm[12];         /* displayed permission in string form */
    VFSMimeType* mime_type;     /* mime type related information */
    GdkPixbuf* big_thumbnail;   /* thumbnail of the file */
    GdkPixbuf* small_thumbnail; /* thumbnail of the file */

    VFSFileInfoFlag flags; /* if it's a special file */

    void ref_inc();
    void ref_dec();
    unsigned int ref_count();

  private:
    std::atomic<unsigned int> n_ref{0};
};

void vfs_file_info_set_utf8_filename(bool is_utf8);

VFSFileInfo* vfs_file_info_new();
VFSFileInfo* vfs_file_info_ref(VFSFileInfo* fi);
void vfs_file_info_unref(VFSFileInfo* fi);

bool vfs_file_info_get(VFSFileInfo* fi, const char* file_path, const char* base_name);

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

void vfs_file_info_set_thumbnail_size(int big, int small);
bool vfs_file_info_load_thumbnail(VFSFileInfo* fi, const char* full_path, bool big);
bool vfs_file_info_is_thumbnail_loaded(VFSFileInfo* fi, bool big);

GdkPixbuf* vfs_file_info_get_big_icon(VFSFileInfo* fi);
GdkPixbuf* vfs_file_info_get_small_icon(VFSFileInfo* fi);

GdkPixbuf* vfs_file_info_get_big_thumbnail(VFSFileInfo* fi);
GdkPixbuf* vfs_file_info_get_small_thumbnail(VFSFileInfo* fi);

void vfs_file_size_to_string_format(char* buf, uint64_t size, bool decimal);

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

/*
 * Run default action of specified file.
 * Full path of the file is required by this function.
 */
bool vfs_file_info_open_file(VFSFileInfo* fi, const char* file_path, GError** err);

void vfs_file_info_load_special_info(VFSFileInfo* fi, const char* file_path);

void vfs_file_info_list_free(GList* list);

/* resolve file path name */
char* vfs_file_resolve_path(const char* cwd, const char* relative_path);
