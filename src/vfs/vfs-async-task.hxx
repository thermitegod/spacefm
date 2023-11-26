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

#include <atomic>
#include <mutex>

#include <gtkmm.h>
#include <glibmm.h>
#include <sigc++/sigc++.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "signals.hxx"

#define VFS_ASYNC_TASK(obj) (static_cast<vfs::async_task*>(obj))

namespace vfs
{
    struct async_task
    {
        using function_t = void* (*)(vfs::async_task*, void*);

        GObject parent;

        static vfs::async_task* create(vfs::async_task::function_t task_func,
                                       void* user_data) noexcept;

        /*private*/

        function_t func;
        void* user_data_{nullptr};

        GThread* thread{nullptr};
        u32 idle_id{0};

        std::mutex mutex;

        // private:
        std::atomic<bool> thread_cancel{true};
        std::atomic<bool> thread_cancelled{true};
        std::atomic<bool> thread_finished{true};

      public:
        // Execute the async task
        void run();

        // Cancel the async task running in another thread.
        // NOTE: Only can be called from main thread.
        void cancel();

        void* user_data();

        bool is_finished();
        bool is_canceled();

        // private
        void cleanup(bool finalize);
        void real_cancel(bool finalize);
        // void* task_thread(void* _task)

        // Signals
      public:
        // Signals function types
        using evt_task_finished_load_app_t = void(vfs::async_task*, bool, GtkWidget*);

        // Signals Add Event
        template<spacefm::signal evt>
        typename std::enable_if_t<evt == spacefm::signal::task_finish, sigc::connection>
        add_event(evt_task_finished_load_app_t fun, GtkWidget* app)
        {
            // ztd::logger::trace("Signal Connect   : spacefm::signal::task_finish");
            this->evt_data_load_app = app;
            return this->evt_task_finished_load_app.connect(sigc::ptr_fun(fun));
        }

        // Signals Run Event
        template<spacefm::signal evt>
        typename std::enable_if_t<evt == spacefm::signal::task_finish, void>
        run_event(bool is_cancelled)
        {
            // ztd::logger::trace("Signal Execute   : spacefm::signal::task_finish");
            this->evt_task_finished_load_app.emit(this, is_cancelled, this->evt_data_load_app);
        }

        // Signals
      private:
        // Signal types
        sigc::signal<evt_task_finished_load_app_t> evt_task_finished_load_app;

      private:
        // Signal data
        // TODO/FIXME has to be a better way to do this
        GtkWidget* evt_data_load_app{nullptr};
    };
} // namespace vfs
