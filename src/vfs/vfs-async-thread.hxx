/**
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

#include <thread>
#include <atomic>

#include <functional>

#include <memory>

#include <sigc++/sigc++.h>

#include <ztd/ztd.hxx>

#include "signals.hxx"

namespace vfs
{
struct dir;

struct async_thread : public std::enable_shared_from_this<async_thread>
{
    using function_t = std::function<void()>;

    async_thread(const vfs::async_thread::function_t& task_function);
    ~async_thread();

    static const std::shared_ptr<vfs::async_thread>
    create(const vfs::async_thread::function_t& task_function) noexcept;

    void run();
    void cancel();

    bool is_running() const;
    bool is_finished() const;
    bool is_canceled() const;

    void cleanup(bool finalize);

  private:
    function_t task_function_;

    std::jthread thread_;
    // std::mutex mutex_{};

    std::atomic<bool> running_{false};
    std::atomic<bool> finished_{false};
    std::atomic<bool> cancel_{false};
    std::atomic<bool> canceled_{false};

    // Signals //
  public:
    // Signals Add Event
    template<spacefm::signal evt, typename bind_fun>
    typename std::enable_if_t<evt == spacefm::signal::task_finish, sigc::connection>
    add_event(bind_fun fun)
    {
        // ztd::logger::trace("Signal Connect   : spacefm::signal::task_finish");
        return this->evt_task_finished_load_dir.connect(fun);
    }

    // Signals Run Event
    template<spacefm::signal evt>
    typename std::enable_if_t<evt == spacefm::signal::task_finish, void>
    run_event(bool is_cancelled)
    {
        // ztd::logger::trace("Signal Execute   : spacefm::signal::task_finish");
        this->evt_task_finished_load_dir.emit(is_cancelled);
    }

  private:
    // Signal types
    sigc::signal<void(bool)> evt_task_finished_load_dir;
};
} // namespace vfs
