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

#include <thread>
#include <condition_variable>
#include <mutex>
#include <future>

#include <glibmm.h>

#include "vfs/vfs-async-task.hxx"

struct VFSAsyncTaskClass
{
    GObjectClass parent_class;
    void (*finish)(vfs::async_task task, bool is_cancelled);
};

GType vfs_async_task_get_type();

#define VFS_ASYNC_TASK_REINTERPRET(obj) (reinterpret_cast<VFSAsyncTask*>(obj))

#define VFS_ASYNC_TASK_TYPE (vfs_async_task_get_type())

static void vfs_async_task_class_init(VFSAsyncTaskClass* klass);
static void vfs_async_task_init(vfs::async_task task);
static void vfs_async_task_finalize(GObject* object);
static void vfs_async_task_finish(vfs::async_task task, bool is_cancelled);

/* Local data */
static GObjectClass* parent_class = nullptr;

GType
vfs_async_task_get_type()
{
    static GType self_type = 0;
    if (!self_type)
    {
        static const GTypeInfo self_info = {
            sizeof(VFSAsyncTaskClass),
            nullptr, /* base_init */
            nullptr, /* base_finalize */
            (GClassInitFunc)vfs_async_task_class_init,
            nullptr, /* class_finalize */
            nullptr, /* class_data */
            sizeof(VFSAsyncTask),
            0,
            (GInstanceInitFunc)vfs_async_task_init,
            nullptr /* value_table */
        };

        self_type = g_type_register_static(G_TYPE_OBJECT,
                                           "VFSAsyncTask",
                                           &self_info,
                                           GTypeFlags::G_TYPE_FLAG_NONE);
    }

    return self_type;
}

static void
vfs_async_task_class_init(VFSAsyncTaskClass* klass)
{
    GObjectClass* g_object_class;
    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = vfs_async_task_finalize;
    parent_class = (GObjectClass*)g_type_class_peek(G_TYPE_OBJECT);

    klass->finish = vfs_async_task_finish;
}

static void
vfs_async_task_init(vfs::async_task task)
{
    (void)task;
}

vfs::async_task
vfs_async_task_new(VFSAsyncFunc task_func, void* user_data)
{
    vfs::async_task task = VFS_ASYNC_TASK(g_object_new(VFS_ASYNC_TASK_TYPE, nullptr));
    task->func = task_func;
    task->user_data_ = user_data;
    return VFS_ASYNC_TASK(task);
}

static void
vfs_async_task_finalize(GObject* object)
{
    vfs::async_task task;
    // FIXME: destroying the object without calling vfs_async_task_cancel
    // currently induces unknown errors.
    task = VFS_ASYNC_TASK_REINTERPRET(object);

    // finalize = true, inhibit the emission of signal
    task->real_cancel(true);
    task->cleanup(true);

    if (G_OBJECT_CLASS(parent_class)->finalize)
    {
        (*G_OBJECT_CLASS(parent_class)->finalize)(object);
    }
}

static bool
on_idle(void* _task)
{
    vfs::async_task task = VFS_ASYNC_TASK(_task);
    task->cleanup(false);
    return true; // the idle handler is removed in task->cleanup.
}

void*
vfs_async_task_thread(void* _task)
{
    vfs::async_task task = VFS_ASYNC_TASK(_task);
    void* ret = task->func(task, task->user_data_);

    const std::unique_lock<std::mutex> lock(task->mutex);

    task->idle_id = g_idle_add((GSourceFunc)on_idle, task); // runs in main loop thread
    task->thread_finished = true;

    return ret;
}

static void
vfs_async_task_finish(vfs::async_task task, bool is_cancelled)
{
    (void)task;
    (void)is_cancelled;
    // default handler of spacefm::signal::task_finish signal.
}

void*
VFSAsyncTask::user_data()
{
    return this->user_data_;
}

void
VFSAsyncTask::run()
{
    this->thread = g_thread_new("async_task", vfs_async_task_thread, this);
}

bool
VFSAsyncTask::is_finished()
{
    return this->thread_finished;
}

bool
VFSAsyncTask::is_canceled()
{
    return this->thread_cancel;
}

void
VFSAsyncTask::real_cancel(bool finalize)
{
    if (!this->thread)
    {
        return;
    }

    /*
     * NOTE: Well, this dirty hack is needed. Since the function is always
     * called from main thread, the GTK+ main loop may have this gdk lock locked
     * when this function gets called.  However, our task running in another thread
     * might need to use GTK+, too. If we do not release the gdk lock in main thread
     * temporarily, the task in another thread will be blocked due to waiting for
     * the gdk lock locked by our main thread, and hence cannot be finished.
     * Then we'll end up in endless waiting for that thread to finish, the so-called deadlock.
     *
     * The doc of GTK+ really sucks. GTK+ use this GTK_THREADS_ENTER everywhere internally,
     * but the behavior of the lock is not well-documented. So it is very difficult for use
     * to get things right.
     */

    this->thread_cancel = true;
    this->cleanup(finalize);
    this->thread_cancelled = true;
}

void
VFSAsyncTask::cancel()
{
    this->real_cancel(false);
}

void
VFSAsyncTask::cleanup(bool finalize)
{
    if (this->idle_id)
    {
        g_source_remove(this->idle_id);
        this->idle_id = 0;
    }
    if (this->thread)
    {
        g_thread_join(this->thread);
        this->thread = nullptr;
        this->thread_finished = true;

        // Only emit the signal when we are not finalizing.
        // Emitting signal on an object during destruction is not allowed.
        if (!finalize)
        {
            this->run_event<spacefm::signal::task_finish>(this->thread_cancelled);
        }
    }
}
