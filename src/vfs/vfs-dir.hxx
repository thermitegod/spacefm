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

#include <vector>

#include <sigc++/sigc++.h>

#include <glib.h>

#include "vfs/vfs-file-monitor.hxx"
#include "vfs/vfs-file-info.hxx"
#include "vfs/vfs-async-task.hxx"

#include "signals.hxx"

#define VFS_DIR(obj)             (static_cast<VFSDir*>(obj))
#define VFS_DIR_REINTERPRET(obj) (reinterpret_cast<VFSDir*>(obj))

#define VFS_TYPE_DIR (vfs_dir_get_type())

// forward declare types
struct VFSFileInfo;
struct PtkFileBrowser;
struct PtkFileList;
struct VFSDir;

struct VFSDir
{
    GObject parent;

    char* path;
    char* disp_path;
    std::vector<VFSFileInfo*> file_list;

    /*<private>*/
    VFSFileMonitor* monitor;

    GMutex* mutex; /* Used to guard file_list */

    VFSAsyncTask* task;

    bool file_listed{true};
    bool load_complete{true};
    bool cancel{true};
    bool show_hidden{true};
    bool avoid_changes{true};

    struct VFSThumbnailLoader* thumbnail_loader;

    std::vector<VFSFileInfo*> changed_files;
    GSList* created_files; // MOD
    long xhidden_count;    // MOD

    // Signals
  public:
    // Signals function types
    using evt_file_created__run_first__t = void(VFSFileInfo*, PtkFileBrowser*);
    using evt_file_created__run_last__t = void(VFSFileInfo*, PtkFileList*);

    using evt_file_changed__run_first__t = void(VFSFileInfo*, PtkFileBrowser*);
    using evt_file_changed__run_last__t = void(VFSFileInfo*, PtkFileList*);

    using evt_file_deleted__run_first__t = void(VFSFileInfo*, PtkFileBrowser*);
    using evt_file_deleted__run_last__t = void(VFSFileInfo*, PtkFileList*);

    using evt_file_listed_t = void(PtkFileBrowser*, bool);

    using evt_file_thumbnail_loaded_t = void(VFSFileInfo*, PtkFileList*);

    // Signals Add Event
    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_CREATED, sigc::connection>::type
    add_event(evt_file_created__run_first__t fun, PtkFileBrowser* browser)
    {
        // LOG_TRACE("Signal Connect   : EventType::FILE_CREATED");
        this->evt_data_browser = browser;
        return this->evt_file_created__first.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_CREATED, sigc::connection>::type
    add_event(evt_file_created__run_last__t fun, PtkFileList* list)
    {
        // LOG_TRACE("Signal Connect   : EventType::FILE_CREATED");
        this->evt_data_list = list;
        return this->evt_file_created__last.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_CHANGED, sigc::connection>::type
    add_event(evt_file_changed__run_first__t fun, PtkFileBrowser* browser)
    {
        // LOG_TRACE("Signal Connect   : EventType::FILE_CHANGED");
        this->evt_data_browser = browser;
        return this->evt_file_changed__first.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_CHANGED, sigc::connection>::type
    add_event(evt_file_changed__run_last__t fun, PtkFileList* list)
    {
        // LOG_TRACE("Signal Connect   : EventType::FILE_CHANGED");
        this->evt_data_list = list;
        return this->evt_file_changed__last.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_DELETED, sigc::connection>::type
    add_event(evt_file_deleted__run_first__t fun, PtkFileBrowser* browser)
    {
        // LOG_TRACE("Signal Connect   : EventType::FILE_DELETED");
        this->evt_data_browser = browser;
        return this->evt_file_deleted__first.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_DELETED, sigc::connection>::type
    add_event(evt_file_deleted__run_last__t fun, PtkFileList* list)
    {
        // LOG_TRACE("Signal Connect   : EventType::FILE_DELETED");
        this->evt_data_list = list;
        return this->evt_file_deleted__last.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_LISTED, sigc::connection>::type
    add_event(evt_file_listed_t fun, PtkFileBrowser* browser)
    {
        // LOG_TRACE("Signal Connect   : EventType::FILE_LISTED");
        // this->evt_data_listed_browser = browser;
        this->evt_data_browser = browser;
        return this->evt_file_listed.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_THUMBNAIL_LOADED, sigc::connection>::type
    add_event(evt_file_thumbnail_loaded_t fun, PtkFileList* list)
    {
        // LOG_TRACE("Signal Connect   : EventType::FILE_THUMBNAIL_LOADED");
        // this->evt_data_thumb_list = list;
        this->evt_data_list = list;
        return this->evt_file_thumbnail_loaded.connect(sigc::ptr_fun(fun));
    }

    // Signals Run Event
    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_CREATED, void>::type
    run_event(VFSFileInfo* info)
    {
        // LOG_TRACE("Signal Execute   : EventType::FILE_CREATED");
        this->evt_file_created__first.emit(info, this->evt_data_browser);
        this->evt_file_created__last.emit(info, this->evt_data_list);
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_CHANGED, void>::type
    run_event(VFSFileInfo* info)
    {
        // LOG_TRACE("Signal Execute   : EventType::FILE_CHANGED");
        this->evt_file_changed__first.emit(info, this->evt_data_browser);
        this->evt_file_changed__last.emit(info, this->evt_data_list);
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_DELETED, void>::type
    run_event(VFSFileInfo* info)
    {
        // LOG_TRACE("Signal Execute   : EventType::FILE_DELETED");
        this->evt_file_deleted__first.emit(info, this->evt_data_browser);
        this->evt_file_deleted__last.emit(info, this->evt_data_list);
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_LISTED, void>::type
    run_event(bool is_cancelled)
    {
        // LOG_TRACE("Signal Execute   : EventType::FILE_LISTED");
        this->evt_file_listed.emit(this->evt_data_browser, is_cancelled);
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_THUMBNAIL_LOADED, void>::type
    run_event(VFSFileInfo* info)
    {
        // LOG_TRACE("Signal Execute   : EventType::FILE_THUMBNAIL_LOADED");
        this->evt_file_thumbnail_loaded.emit(info, this->evt_data_list);
    }

    // Signals
  private:
    // Signal types
    sigc::signal<evt_file_created__run_first__t> evt_file_created__first;
    sigc::signal<evt_file_created__run_last__t> evt_file_created__last;

    sigc::signal<evt_file_changed__run_first__t> evt_file_changed__first;
    sigc::signal<evt_file_changed__run_last__t> evt_file_changed__last;

    sigc::signal<evt_file_deleted__run_first__t> evt_file_deleted__first;
    sigc::signal<evt_file_deleted__run_last__t> evt_file_deleted__last;

    sigc::signal<evt_file_listed_t> evt_file_listed;

    sigc::signal<evt_file_thumbnail_loaded_t> evt_file_thumbnail_loaded;

  private:
    // Signal data
    // TODO/FIXME has to be a better way to do this
    // PtkFileBrowser* evt_data_listed_browser{nullptr};
    PtkFileBrowser* evt_data_browser{nullptr};
    PtkFileList* evt_data_list{nullptr};
    // PtkFileList* evt_data_thumb_list{nullptr};
};

using VFSDirForeachFunc = void (*)(const char* parh, VFSDir* dir, void* user_data);

void vfs_dir_lock(VFSDir* dir);
void vfs_dir_unlock(VFSDir* dir);

GType vfs_dir_get_type();

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

bool vfs_dir_add_hidden(std::string_view path, std::string_view file_name);

/* call function "func" for every VFSDir instances */
void vfs_dir_foreach(VFSDirForeachFunc func, void* user_data);

void vfs_dir_monitor_mime();
