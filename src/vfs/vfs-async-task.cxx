/*
 *      vfs-async-task.c
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

#include "vfs/vfs-async-task.hxx"

static void vfs_async_task_class_init(VFSAsyncTaskClass* klass);
static void vfs_async_task_init(VFSAsyncTask* task);
static void vfs_async_task_finalize(GObject* object);

static void vfs_async_task_finish(VFSAsyncTask* task, bool is_cancelled);
static void vfs_async_thread_cleanup(VFSAsyncTask* task, bool finalize);

void vfs_async_task_real_cancel(VFSAsyncTask* task, bool finalize);

/* Local data */
static GObjectClass* parent_class = nullptr;

enum VFSAsyncSignal
{
    FINISH_SIGNAL,
    N_SIGNALS
};

static unsigned int signals[N_SIGNALS] = {0};

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

        self_type =
            g_type_register_static(G_TYPE_OBJECT, "VFSAsyncTask", &self_info, (GTypeFlags)0);
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

    signals[FINISH_SIGNAL] = g_signal_new("finish",
                                          G_TYPE_FROM_CLASS(klass),
                                          G_SIGNAL_RUN_FIRST,
                                          G_STRUCT_OFFSET(VFSAsyncTaskClass, finish),
                                          nullptr,
                                          nullptr,
                                          g_cclosure_marshal_VOID__BOOLEAN,
                                          G_TYPE_NONE,
                                          1,
                                          G_TYPE_BOOLEAN);
}

static void
vfs_async_task_init(VFSAsyncTask* task)
{
    task->lock = (GMutex*)g_malloc(sizeof(GMutex));
    g_mutex_init(task->lock);
}

void
vfs_async_task_lock(VFSAsyncTask* task)
{
    g_mutex_lock(task->lock);
}

void
vfs_async_task_unlock(VFSAsyncTask* task)
{
    g_mutex_unlock(task->lock);
}

VFSAsyncTask*
vfs_async_task_new(VFSAsyncFunc task_func, void* user_data)
{
    VFSAsyncTask* task = static_cast<VFSAsyncTask*>(g_object_new(VFS_ASYNC_TASK_TYPE, nullptr));
    task->func = task_func;
    task->user_data = user_data;
    return static_cast<VFSAsyncTask*>(task);
}

void*
vfs_async_task_get_data(VFSAsyncTask* task)
{
    return task->user_data;
}

static void
vfs_async_task_finalize(GObject* object)
{
    VFSAsyncTask* task;
    /* FIXME: destroying the object without calling vfs_async_task_cancel
     currently induces unknown errors. */
    task = VFS_ASYNC_TASK(object);

    /* finalize = true, inhibit the emission of signals */
    vfs_async_task_real_cancel(task, true);
    vfs_async_thread_cleanup(task, true);

    task->lock = nullptr;

    if (G_OBJECT_CLASS(parent_class)->finalize)
        (*G_OBJECT_CLASS(parent_class)->finalize)(object);
}

static bool
on_idle(void* _task)
{
    VFSAsyncTask* task = static_cast<VFSAsyncTask*>(_task);
    vfs_async_thread_cleanup(task, false);
    return true; /* the idle handler is removed in vfs_async_thread_cleanup. */
}

static void*
vfs_async_task_thread(void* _task)
{
    VFSAsyncTask* task = static_cast<VFSAsyncTask*>(_task);
    void* ret = task->func(task, task->user_data);

    vfs_async_task_lock(task);
    task->idle_id = g_idle_add((GSourceFunc)on_idle, task); // runs in main loop thread
    task->ret_val = ret;
    task->finished = true;
    vfs_async_task_unlock(task);

    return ret;
}

void
vfs_async_task_execute(VFSAsyncTask* task)
{
    task->thread = g_thread_new("async_task", vfs_async_task_thread, task);
}

static void
vfs_async_thread_cleanup(VFSAsyncTask* task, bool finalize)
{
    if (task->idle_id)
    {
        g_source_remove(task->idle_id);
        task->idle_id = 0;
    }
    if (G_LIKELY(task->thread))
    {
        g_thread_join(task->thread);
        task->thread = nullptr;
        task->finished = true;
        /* Only emit the signal when we are not finalizing.
            Emitting signal on an object during destruction is not allowed. */
        if (G_LIKELY(!finalize))
            g_signal_emit(task, signals[FINISH_SIGNAL], 0, task->cancelled);
    }
}

void
vfs_async_task_real_cancel(VFSAsyncTask* task, bool finalize)
{
    if (!task->thread)
        return;

    /*
     * NOTE: Well, this dirty hack is needed. Since the function is always
     * called from main thread, the GTK+ main loop may have this gdk lock locked
     * when this function gets called.  However, our task running in another thread
     * might need to use GTK+, too. If we don't release the gdk lock in main thread
     * temporarily, the task in another thread will be blocked due to waiting for
     * the gdk lock locked by our main thread, and hence cannot be finished.
     * Then we'll end up in endless waiting for that thread to finish, the so-called deadlock.
     *
     * The doc of GTK+ really sucks. GTK+ use this GTK_THREADS_ENTER everywhere internally,
     * but the behavior of the lock is not well-documented. So it's very difficult for use
     * to get things right.
     */

    vfs_async_task_lock(task);
    task->cancel = true;
    vfs_async_task_unlock(task);

    vfs_async_thread_cleanup(task, finalize);
    task->cancelled = true;
}

void
vfs_async_task_cancel(VFSAsyncTask* task)
{
    vfs_async_task_real_cancel(task, false);
}

static void
vfs_async_task_finish(VFSAsyncTask* task, bool is_cancelled)
{
    (void)task;
    (void)is_cancelled;
    /* default handler of "finish" signal. */
}

bool
vfs_async_task_is_finished(VFSAsyncTask* task)
{
    return task->finished;
}

bool
vfs_async_task_is_cancelled(VFSAsyncTask* task)
{
    return task->cancel;
}
