/*
 *      vfs-async-task.hxx
 *
 *      Copyright 2008 PCMan <pcman.tw@gmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 3 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#pragma once

#include <glib.h>
#include <glib-object.h>

#define VFS_ASYNC_TASK_TYPE (vfs_async_task_get_type())
#define VFS_ASYNC_TASK(obj) (reinterpret_cast<VFSAsyncTask*>(obj))

struct VFSAsyncTask;
typedef void* (*VFSAsyncFunc)(VFSAsyncTask*, void*);

struct VFSAsyncTask
{
    GObject parent;
    VFSAsyncFunc func;
    void* user_data;
    void* ret_val;

    GThread* thread;
    GMutex* lock;

    unsigned int idle_id;
    bool cancel : 1;
    bool cancelled : 1;
    bool finished : 1;
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
