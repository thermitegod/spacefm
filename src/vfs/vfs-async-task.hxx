/**
 * Copyright 2008 PCMan <pcman.tw@gmail.com>
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

#include <gtk/gtk.h>

#include <glib.h>
#include <glib-object.h>

#include <sigc++/sigc++.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "signals.hxx"

#define VFS_ASYNC_TASK(obj)             (static_cast<VFSAsyncTask*>(obj))
#define VFS_ASYNC_TASK_REINTERPRET(obj) (reinterpret_cast<VFSAsyncTask*>(obj))

#define VFS_ASYNC_TASK_TYPE (vfs_async_task_get_type())

// forward declare
struct VFSDir;
struct VFSAsyncTask;
struct FindFile;

using VFSAsyncFunc = void* (*)(VFSAsyncTask*, void*);
struct VFSAsyncTask
{
    GObject parent;
    VFSAsyncFunc func;
    void* user_data;
    void* ret_val;

    GThread* thread;
    GMutex* lock;

    unsigned int idle_id;
    bool cancel{true};
    bool cancelled{true};
    bool finished{true};

    // Signals
  public:
    // Signals function types
    using evt_task_finished__find_file_t = void(FindFile*);
    using evt_task_finished__load_app_t = void(VFSAsyncTask*, bool, GtkWidget*);
    using evt_task_finished__load_dir_t = void(VFSDir*, bool);

    // Signals Add Event
    template<EventType evt>
    typename std::enable_if<evt == EventType::TASK_FINISH, sigc::connection>::type
    add_event(evt_task_finished__find_file_t fun, FindFile* file)
    {
        // LOG_TRACE("Signal Connect   : EventType::TASK_FINISH");
        this->evt_data_find_file = file;
        return this->evt_task_finished__find_file.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::TASK_FINISH, sigc::connection>::type
    add_event(evt_task_finished__load_app_t fun, GtkWidget* app)
    {
        // LOG_TRACE("Signal Connect   : EventType::TASK_FINISH");
        this->evt_data_load_app = app;
        return this->evt_task_finished__load_app.connect(sigc::ptr_fun(fun));
    }

    template<EventType evt>
    typename std::enable_if<evt == EventType::TASK_FINISH, sigc::connection>::type
    add_event(evt_task_finished__load_dir_t fun, VFSDir* dir)
    {
        // LOG_TRACE("Signal Connect   : EventType::TASK_FINISH");
        this->evt_data_load_dir = dir;
        return this->evt_task_finished__load_dir.connect(sigc::ptr_fun(fun));
    }

    // Signals Run Event
    template<EventType evt>
    typename std::enable_if<evt == EventType::TASK_FINISH, void>::type
    run_event(bool is_cancelled)
    {
        // LOG_TRACE("Signal Execute   : EventType::TASK_FINISH");
        this->evt_task_finished__find_file.emit(this->evt_data_find_file);
        this->evt_task_finished__load_app.emit(this, is_cancelled, this->evt_data_load_app);
        this->evt_task_finished__load_dir.emit(this->evt_data_load_dir, is_cancelled);
    }

    // Signals
  private:
    // Signal types
    sigc::signal<evt_task_finished__find_file_t> evt_task_finished__find_file;
    sigc::signal<evt_task_finished__load_app_t> evt_task_finished__load_app;
    sigc::signal<evt_task_finished__load_dir_t> evt_task_finished__load_dir;

  private:
    // Signal data
    // TODO/FIXME has to be a better way to do this
    FindFile* evt_data_find_file{nullptr};
    GtkWidget* evt_data_load_app{nullptr};
    VFSDir* evt_data_load_dir{nullptr};
};

struct VFSAsyncTaskClass
{
    GObjectClass parent_class;
    void (*finish)(VFSAsyncTask* task, bool is_cancelled);
};

GType vfs_async_task_get_type();
VFSAsyncTask* vfs_async_task_new(VFSAsyncFunc task_func, void* user_data);

void* vfs_async_task_get_data(VFSAsyncTask* task);

/* Execute the async task */
void vfs_async_task_execute(VFSAsyncTask* task);

bool vfs_async_task_is_finished(VFSAsyncTask* task);
bool vfs_async_task_is_cancelled(VFSAsyncTask* task);

/*
 * Cancel the async task running in another thread.
 * NOTE: Only can be called from main thread.
 */
void vfs_async_task_cancel(VFSAsyncTask* task);

void vfs_async_task_lock(VFSAsyncTask* task);
void vfs_async_task_unlock(VFSAsyncTask* task);
