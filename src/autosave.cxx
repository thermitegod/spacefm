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

#include <vector>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <future>
#include <chrono>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "types.hxx"
#include "program-timer.hxx"

#include "autosave.hxx"

class AutoSave
{
  public:
    // returns false when killed:
    template<class R, class P>
    bool
    wait(std::chrono::duration<R, P> const& time) noexcept
    {
        std::unique_lock<std::mutex> lock(m);
        return !cv.wait_for(lock,
                            time,
                            [&]
                            {
                                return terminate;
                            });
    }
    void
    kill() noexcept
    {
        std::unique_lock<std::mutex> lock(m);
        this->terminate = true;
        cv.notify_all();
    }

  public:
    std::atomic<bool> accepting_requests{false};
    std::atomic<bool> pending_requests{false};
    // const u64 timer{5}; // 5 seconds
    const u64 timer{300}; // 5 minutes

  private:
    std::condition_variable cv;
    std::mutex m;
    bool terminate{false};
};

AutoSave autosave;

std::vector<std::future<void>> threads;

static void
autosave_thread(autosave_f autosave_func) noexcept
{
    const std::chrono::duration<u64> duration(autosave.timer);
    while (autosave.wait(duration))
    {
        // LOG_INFO("AUTOSAVE save_thread_loop");
        if (autosave.pending_requests)
        {
            // LOG_INFO("AUTOSAVE save_settings");
            autosave.pending_requests.store(false);
            autosave_func();
        }
    }
}

void
autosave_request_add() noexcept
{
    // LOG_INFO("AUTOSAVE request add");
    if (autosave.accepting_requests)
    {
        autosave.pending_requests.store(true);
    }
    else
    { // At program start lots of requests can be sent, this ignores them
        if (program_timer::elapsed() >= 10.0)
        {
            // LOG_INFO("AUTOSAVE now accepting request add");
            autosave.accepting_requests.store(true);
            autosave.pending_requests.store(true);
        }
        // else
        // {
        //     LOG_INFO("AUTOSAVE ignoring request add");
        // }
    }
}

void
autosave_request_cancel() noexcept
{
    // LOG_INFO("AUTOSAVE request cancel");
    autosave.pending_requests.store(false);
}

void
autosave_init(autosave_f autosave_func) noexcept
{
    // LOG_INFO("AUTOSAVE init");
    threads.push_back(std::async(std::launch::async,
                                 [autosave_func]
                                 {
                                     autosave_thread(autosave_func);
                                 }));
}

void
autosave_terminate() noexcept
{
    // LOG_INFO("AUTOSAVE kill thread");
    autosave.kill();

    for (auto&& f: threads)
        f.wait();
}
