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

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>

#include "logger.hxx"

#include "signals.hxx"

#define ASYNC_TASK(obj) (static_cast<async_task*>(obj))

struct async_task
{
    using function_t = void* (*)(async_task*, void*) noexcept;

    GObject parent;

    static async_task* create(async_task::function_t task_func, void* user_data) noexcept;

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
    void run() noexcept;

    // Cancel the async task running in another thread.
    // NOTE: Only can be called from main thread.
    void cancel() noexcept;

    void* user_data() const noexcept;

    bool is_finished() const noexcept;
    bool is_canceled() const noexcept;

    // private
    void cleanup(bool finalize) noexcept;
    void real_cancel(bool finalize) noexcept;
    // void* task_thread(void* _task)

    // Signals
  public:
    // Signals function types
    using evt_task_finished_load_app_t = void(async_task*, bool, GtkWidget*);

    // Signals Add Event
    template<spacefm::signal evt>
    typename std::enable_if_t<evt == spacefm::signal::task_finish, sigc::connection>
    add_event(evt_task_finished_load_app_t fun, GtkWidget* app) noexcept
    {
        logger::trace<logger::domain::signals>(
            std::format("Connect({}): {}", logger::utils::ptr(this), magic_enum::enum_name(evt)));
        this->evt_data_load_app = app;
        return this->evt_task_finished_load_app.connect(sigc::ptr_fun(fun));
    }

    // Signals Run Event
    template<spacefm::signal evt>
    typename std::enable_if_t<evt == spacefm::signal::task_finish, void>
    run_event(bool is_cancelled) noexcept
    {
        logger::trace<logger::domain::signals>(
            std::format("Execute({}): {}", logger::utils::ptr(this), magic_enum::enum_name(evt)));
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
