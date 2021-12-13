/*
 *
 * License: See COPYING file
 *
 */

#include <thread>
#include <atomic>
#include <chrono>

#include "settings.hxx"
//#include "logger.hxx"
#include "autosave.hxx"

struct AutoSave
{
    std::atomic<bool> keep_saving = true;
    std::atomic<bool> request;
    // size_t timer = 5; // 5 seconds
    size_t timer = 300; // 5 minutes
};

AutoSave autosave = AutoSave();

void autosave_thread();

std::thread thread_save_settings(autosave_thread);

void
autosave_thread()
{
    const std::chrono::duration<int> duration(autosave.timer);
    do
    {
        std::this_thread::sleep_for(duration);
        // LOG_INFO("AUTOSAVE save_thread_loop");

        if (autosave.request)
        {
            // LOG_INFO("AUTOSAVE save_settings");

            autosave.request.store(false);
            save_settings(nullptr);
        }

    } while (autosave.keep_saving);
}

void
autosave_request()
{
    // LOG_INFO("AUTOSAVE request");

    autosave.request.store(true);
}

void
autosave_cancel()
{
    // LOG_INFO("AUTOSAVE cancel");

    autosave.request.store(false);
    // autosave.keep_saving.store(false);
}
