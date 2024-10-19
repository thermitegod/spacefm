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

#include <mutex>

#include <glibmm.h>

#include "ptk/deprecated/async-task.hxx"

struct VFSAsyncTaskClass
{
    GObjectClass parent_class;
    void (*finish)(async_task* task, bool is_cancelled);
};

static GType async_task_get_type() noexcept;

#define ASYNC_TASK_REINTERPRET(obj) (reinterpret_cast<async_task*>(obj))

#define ASYNC_TASK_TYPE (async_task_get_type())

static void async_task_class_init(VFSAsyncTaskClass* klass) noexcept;
static void async_task_init(async_task* task) noexcept;
static void async_task_finalize(GObject* object) noexcept;
static void async_task_finish(async_task* task, bool is_cancelled) noexcept;

/* Local data */
static GObjectClass* parent_class = nullptr;

GType
async_task_get_type() noexcept
{
    static GType self_type = 0;
    if (!self_type)
    {
        static const GTypeInfo self_info = {
            sizeof(VFSAsyncTaskClass),
            nullptr, /* base_init */
            nullptr, /* base_finalize */
            (GClassInitFunc)async_task_class_init,
            nullptr, /* class_finalize */
            nullptr, /* class_data */
            sizeof(async_task),
            0,
            (GInstanceInitFunc)async_task_init,
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
async_task_class_init(VFSAsyncTaskClass* klass) noexcept
{
    GObjectClass* g_object_class = nullptr;
    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = async_task_finalize;
    parent_class = (GObjectClass*)g_type_class_peek(G_TYPE_OBJECT);

    klass->finish = async_task_finish;
}

static void
async_task_init(async_task* task) noexcept
{
    (void)task;
}

async_task*
async_task::create(async_task::function_t task_func, void* user_data) noexcept
{
    auto* const task = ASYNC_TASK(g_object_new(ASYNC_TASK_TYPE, nullptr));
    task->func = task_func;
    task->user_data_ = user_data;
    return ASYNC_TASK(task);
}

static void
async_task_finalize(GObject* object) noexcept
{
    async_task* task = nullptr;
    // FIXME: destroying the object without calling async_task_cancel
    // currently induces unknown errors.
    task = ASYNC_TASK_REINTERPRET(object);

    // finalize = true, inhibit the emission of signal
    task->real_cancel(true);
    task->cleanup(true);

    if (G_OBJECT_CLASS(parent_class)->finalize)
    {
        (*G_OBJECT_CLASS(parent_class)->finalize)(object);
    }
}

static bool
on_idle(void* _task) noexcept
{
    auto* const task = ASYNC_TASK(_task);
    task->cleanup(false);
    return true; // the idle handler is removed in task->cleanup.
}

static void*
async_task_thread(void* _task) noexcept
{
    auto* const task = ASYNC_TASK(_task);
    void* ret = task->func(task, task->user_data_);

    const std::unique_lock<std::mutex> lock(task->mutex);

    task->idle_id = g_idle_add((GSourceFunc)on_idle, task); // runs in main loop thread
    task->thread_finished = true;

    return ret;
}

static void
async_task_finish(async_task* task, bool is_cancelled) noexcept
{
    (void)task;
    (void)is_cancelled;
    // default handler of spacefm::signal::task_finish signal.
}

void*
async_task::user_data() const noexcept
{
    return this->user_data_;
}

void
async_task::run() noexcept
{
    this->thread = g_thread_new("async_task", async_task_thread, this);
}

bool
async_task::is_finished() const noexcept
{
    return this->thread_finished;
}

bool
async_task::is_canceled() const noexcept
{
    return this->thread_cancel;
}

void
async_task::real_cancel(bool finalize) noexcept
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
async_task::cancel() noexcept
{
    this->real_cancel(false);
}

void
async_task::cleanup(bool finalize) noexcept
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
