/*
 *  C Interface: vfs-dir
 *
 * Description: Object used to present a directory
 *
 *
 * Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 */

#pragma once

#include <glib.h>

#include "vfs-file-monitor.hxx"
#include "vfs-file-info.hxx"
#include "vfs-async-task.hxx"

#define VFS_TYPE_DIR (vfs_dir_get_type())
#define VFS_DIR(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), VFS_TYPE_DIR, VFSDir))

typedef struct VFSDir
{
    GObject parent;

    char* path;
    char* disp_path;
    GList* file_list;
    int n_files;

    union
    {
        int flags;
        struct
        {
            bool is_home : 1;
            bool is_desktop : 1;
            bool is_mount_point : 1;
            bool is_remote : 1;
            bool is_virtual : 1;
        };
    };

    /*<private>*/
    VFSFileMonitor* monitor;

    GMutex* mutex; /* Used to guard file_list */

    VFSAsyncTask* task;
    bool file_listed : 1;
    bool load_complete : 1;
    bool cancel : 1;
    bool show_hidden : 1;
    bool avoid_changes : 1; // sfm

    struct VFSThumbnailLoader* thumbnail_loader;

    GSList* changed_files;
    GSList* created_files; // MOD
    long xhidden_count;    // MOD
} VFSDir;

typedef struct VFSDirClass
{
    GObjectClass parent;
    /* Default signal handlers */
    void (*file_created)(VFSDir* dir, VFSFileInfo* file);
    void (*file_deleted)(VFSDir* dir, VFSFileInfo* file);
    void (*file_changed)(VFSDir* dir, VFSFileInfo* file);
    void (*thumbnail_loaded)(VFSDir* dir, VFSFileInfo* file);
    void (*file_listed)(VFSDir* dir);
    void (*load_complete)(VFSDir* dir);
    /*  void (*need_reload) ( VFSDir* dir ); */
    /*  void (*update_mime) ( VFSDir* dir ); */
} VFSDirClass;

void vfs_dir_lock(VFSDir* dir);
void vfs_dir_unlock(VFSDir* dir);

GType vfs_dir_get_type(void);

VFSDir* vfs_dir_get_by_path(const char* path);
VFSDir* vfs_dir_get_by_path_soft(const char* path);

bool vfs_dir_is_file_listed(VFSDir* dir);

void vfs_dir_unload_thumbnails(VFSDir* dir, bool is_big);

/* emit signals */
void vfs_dir_emit_file_created(VFSDir* dir, const char* file_name, bool force);
void vfs_dir_emit_file_deleted(VFSDir* dir, const char* file_name, VFSFileInfo* file);
void vfs_dir_emit_file_changed(VFSDir* dir, const char* file_name, VFSFileInfo* file, bool force);
void vfs_dir_emit_thumbnail_loaded(VFSDir* dir, VFSFileInfo* file);
void vfs_dir_flush_notify_cache();

/* get the path of desktop dir */
const char* vfs_get_desktop_dir();

bool vfs_dir_add_hidden(const char* path, const char* file_name); // MOD added

/* call function "func" for every VFSDir instances */
void vfs_dir_foreach(GHFunc func, void* user_data);

void vfs_dir_monitor_mime();
