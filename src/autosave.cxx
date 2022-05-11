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

#include "autosave.hxx"

struct AutoSave
{
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
        terminate = true;
        cv.notify_all();
    }

  public:
    std::atomic<bool> request{false};
    // const unsigned int timer{5}; // 5 seconds
    const unsigned int timer{300}; // 5 minutes

  private:
    std::condition_variable cv;
    std::mutex m;
    bool terminate{false};
};

AutoSave autosave;

std::vector<std::future<void>> threads;

static void
autosave_thread(void (*autosave_func)(void)) noexcept
{
    const std::chrono::duration<unsigned int> duration(autosave.timer);
    while (autosave.wait(duration))
    {
        // LOG_INFO("AUTOSAVE save_thread_loop");
        if (autosave.request)
        {
            // LOG_INFO("AUTOSAVE save_settings");
            autosave.request.store(false);
            autosave_func();
        }
    }
}

void
autosave_request_add() noexcept
{
    // LOG_INFO("AUTOSAVE request add");
    autosave.request.store(true);
}

void
autosave_request_cancel() noexcept
{
    // LOG_INFO("AUTOSAVE request cancel");
    autosave.request.store(false);
}

void
autosave_init(void (*autosave_func)(void)) noexcept
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
