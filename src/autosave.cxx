/*
 *
 * License: See COPYING file
 *
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

#include "settings.hxx"
#include "autosave.hxx"

struct AutoSave
{
    // returns false when killed:
    template<class R, class P>
    bool
    wait(std::chrono::duration<R, P> const& time)
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
    kill()
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
autosave_thread()
{
    const std::chrono::duration<unsigned int> duration(autosave.timer);
    while (autosave.wait(duration))
    {
        // LOG_INFO("AUTOSAVE save_thread_loop");
        if (autosave.request)
        {
            // LOG_INFO("AUTOSAVE save_settings");
            autosave.request.store(false);
            save_settings(nullptr);
        }
    }
}

void
autosave_request()
{
    // LOG_INFO("AUTOSAVE request add");
    autosave.request.store(true);
}

void
autosave_cancel()
{
    // LOG_INFO("AUTOSAVE request cancel");
    autosave.request.store(false);
}

void
autosave_init()
{
    // LOG_INFO("AUTOSAVE init");
    threads.push_back(std::async(std::launch::async,
                                 []
                                 {
                                     autosave_thread();
                                 }));
}

void
autosave_terminate()
{
    // LOG_INFO("AUTOSAVE kill thread");
    autosave.kill();

    for (auto&& f: threads)
        f.wait();
}
