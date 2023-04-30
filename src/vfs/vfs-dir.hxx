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

#include <mutex>

#include <sigc++/sigc++.h>

#include <glib.h>

#include <ztd/ztd.hxx>

#include "vfs/vfs-file-monitor.hxx"
#include "vfs/vfs-file-info.hxx"
#include "vfs/vfs-async-task.hxx"

#include "signals.hxx"

#define VFS_DIR(obj) (static_cast<VFSDir*>(obj))

// forward declare types
struct VFSFileInfo;
struct PtkFileBrowser;
struct PtkFileList;
struct VFSDir;
struct VFSThumbnailLoader;

namespace vfs
{
    using dir = ztd::raw_ptr<VFSDir>;
    using thumbnail_loader = ztd::raw_ptr<VFSThumbnailLoader>;
} // namespace vfs

struct VFSDir
{
    GObject parent;

    std::string path{};
    std::vector<vfs::file_info> file_list{};

    /*<private>*/
    vfs::file_monitor monitor{nullptr};

    std::mutex mutex;

    vfs::async_task task{nullptr};

    bool file_listed{true};
    bool load_complete{true};
    bool cancel{true};
    bool show_hidden{true};
    bool avoid_changes{true};

    vfs::thumbnail_loader thumbnail_loader{nullptr};

    std::vector<vfs::file_info> changed_files{};
    std::vector<std::string> created_files{};

    i64 xhidden_count{0};

    // Signals
  public:
    // Signals function types
    using evt_file_created__run_first__t = void(vfs::file_info, PtkFileBrowser*);
    using evt_file_created__run_last__t = void(vfs::file_info, PtkFileList*);

    using evt_file_changed__run_first__t = void(vfs::file_info, PtkFileBrowser*);
    using evt_file_changed__run_last__t = void(vfs::file_info, PtkFileList*);

    using evt_file_deleted__run_first__t = void(vfs::file_info, PtkFileBrowser*);
    using evt_file_deleted__run_last__t = void(vfs::file_info, PtkFileList*);

    using evt_file_listed_t = void(PtkFileBrowser*, bool);

    using evt_file_thumbnail_loaded_t = void(vfs::file_info, PtkFileList*);

    using evt_mime_change_t = void();

    // Signals Add Event
    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_CREATED, sigc::connection>::type
    add_event(evt_file_created__run_first__t fun, PtkFileBrowser* browser)
    {
        // ztd::logger::trace("Signal Connect   : EventType::FILE_CREATED");
        this->evt_data_browser = browser;
        return this->evt_file_created__first.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_CREATED, sigc::connection>::type
    add_event(evt_file_created__run_last__t fun, PtkFileList* list)
    {
        // ztd::logger::trace("Signal Connect   : EventType::FILE_CREATED");
        this->evt_data_list = list;
        return this->evt_file_created__last.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_CREATED, sigc::connection>::type
    add_event(evt_mime_change_t fun)
    {
        // ztd::logger::trace("Signal Connect   : EventType::FILE_CREATED");
        return this->evt_mime_change.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_CHANGED, sigc::connection>::type
    add_event(evt_file_changed__run_first__t fun, PtkFileBrowser* browser)
    {
        // ztd::logger::trace("Signal Connect   : EventType::FILE_CHANGED");
        this->evt_data_browser = browser;
        return this->evt_file_changed__first.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_CHANGED, sigc::connection>::type
    add_event(evt_file_changed__run_last__t fun, PtkFileList* list)
    {
        // ztd::logger::trace("Signal Connect   : EventType::FILE_CHANGED");
        this->evt_data_list = list;
        return this->evt_file_changed__last.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_CHANGED, sigc::connection>::type
    add_event(evt_mime_change_t fun)
    {
        // ztd::logger::trace("Signal Connect   : EventType::FILE_CHANGED");
        return this->evt_mime_change.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_DELETED, sigc::connection>::type
    add_event(evt_file_deleted__run_first__t fun, PtkFileBrowser* browser)
    {
        // ztd::logger::trace("Signal Connect   : EventType::FILE_DELETED");
        this->evt_data_browser = browser;
        return this->evt_file_deleted__first.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_DELETED, sigc::connection>::type
    add_event(evt_file_deleted__run_last__t fun, PtkFileList* list)
    {
        // ztd::logger::trace("Signal Connect   : EventType::FILE_DELETED");
        this->evt_data_list = list;
        return this->evt_file_deleted__last.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_DELETED, sigc::connection>::type
    add_event(evt_mime_change_t fun)
    {
        // ztd::logger::trace("Signal Connect   : EventType::FILE_DELETED");
        return this->evt_mime_change.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_LISTED, sigc::connection>::type
    add_event(evt_file_listed_t fun, PtkFileBrowser* browser)
    {
        // ztd::logger::trace("Signal Connect   : EventType::FILE_LISTED");
        // this->evt_data_listed_browser = browser;
        this->evt_data_browser = browser;
        return this->evt_file_listed.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_LISTED, sigc::connection>::type
    add_event(evt_mime_change_t fun)
    {
        // ztd::logger::trace("Signal Connect   : EventType::FILE_LISTED");
        return this->evt_mime_change.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_THUMBNAIL_LOADED, sigc::connection>::type
    add_event(evt_file_thumbnail_loaded_t fun, PtkFileList* list)
    {
        // ztd::logger::trace("Signal Connect   : EventType::FILE_THUMBNAIL_LOADED");
        // this->evt_data_thumb_list = list;
        this->evt_data_list = list;
        return this->evt_file_thumbnail_loaded.connect(sigc::ptr_fun(fun));
    }

    // Signals Run Event
    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_CREATED, void>::type
    run_event(vfs::file_info file)
    {
        // ztd::logger::trace("Signal Execute   : EventType::FILE_CREATED");
        this->evt_mime_change.emit();
        this->evt_file_created__first.emit(file, this->evt_data_browser);
        this->evt_file_created__last.emit(file, this->evt_data_list);
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_CHANGED, void>::type
    run_event(vfs::file_info file)
    {
        // ztd::logger::trace("Signal Execute   : EventType::FILE_CHANGED");
        this->evt_mime_change.emit();
        this->evt_file_changed__first.emit(file, this->evt_data_browser);
        this->evt_file_changed__last.emit(file, this->evt_data_list);
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_DELETED, void>::type
    run_event(vfs::file_info file)
    {
        // ztd::logger::trace("Signal Execute   : EventType::FILE_DELETED");
        this->evt_mime_change.emit();
        this->evt_file_deleted__first.emit(file, this->evt_data_browser);
        this->evt_file_deleted__last.emit(file, this->evt_data_list);
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_LISTED, void>::type
    run_event(bool is_cancelled)
    {
        // ztd::logger::trace("Signal Execute   : EventType::FILE_LISTED");
        this->evt_mime_change.emit();
        this->evt_file_listed.emit(this->evt_data_browser, is_cancelled);
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::FILE_THUMBNAIL_LOADED, void>::type
    run_event(vfs::file_info file)
    {
        // ztd::logger::trace("Signal Execute   : EventType::FILE_THUMBNAIL_LOADED");
        this->evt_file_thumbnail_loaded.emit(file, this->evt_data_list);
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

    sigc::signal<evt_mime_change_t> evt_mime_change;

  private:
    // Signal data
    // TODO/FIXME has to be a better way to do this
    // PtkFileBrowser* evt_data_listed_browser{nullptr};
    PtkFileBrowser* evt_data_browser{nullptr};
    PtkFileList* evt_data_list{nullptr};
    // PtkFileList* evt_data_thumb_list{nullptr};

  public:
    // private:
    // Signals we connect to
    sigc::connection signal_task_load_dir;
};

using VFSDirForeachFunc = void (*)(vfs::dir dir, bool user_data);

GType vfs_dir_get_type();

vfs::dir vfs_dir_get_by_path(std::string_view path);
vfs::dir vfs_dir_get_by_path_soft(std::string_view path);

bool vfs_dir_is_file_listed(vfs::dir dir);

void vfs_dir_unload_thumbnails(vfs::dir dir, bool is_big);

/* emit signals */
void vfs_dir_emit_file_created(vfs::dir dir, std::string_view file_name, bool force);
void vfs_dir_emit_file_deleted(vfs::dir dir, std::string_view file_name, vfs::file_info file);
void vfs_dir_emit_file_changed(vfs::dir dir, std::string_view file_name, vfs::file_info file,
                               bool force);
void vfs_dir_emit_thumbnail_loaded(vfs::dir dir, vfs::file_info file);
void vfs_dir_flush_notify_cache();

bool vfs_dir_add_hidden(std::string_view path, std::string_view file_name);

/* call function "func" for every VFSDir instances */
void vfs_dir_foreach(VFSDirForeachFunc func, bool user_data);

void vfs_dir_monitor_mime();

void vfs_dir_mime_type_reload();
