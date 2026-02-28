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

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <thread>

#include <pthread.h>

#include <ztd/ztd.hxx>

#include "autosave.hxx"
#include "logger.hxx"

class autosave_backend
{
  public:
    void run(const std::stop_token& stoken) noexcept;
    void run_once(const std::stop_token& stoken) noexcept;

    void add() noexcept;
    void cancel() noexcept;

    void
    set_autosave_func(const std::function<void()>& autosave_func) noexcept
    {
        this->autosave_func_ = autosave_func;
    }

  private:
    std::mutex mutex_;
    std::condition_variable_any cv_;
    std::chrono::minutes thread_sleep_ = std::chrono::minutes(5);

    std::function<void()> autosave_func_;
    i32 total_ = 0;
    bool pending_ = false;
};

void
autosave_backend::run(const std::stop_token& stoken) noexcept
{
    while (!stoken.stop_requested())
    {
        this->run_once(stoken);
    }
}

void
autosave_backend::run_once(const std::stop_token& stoken) noexcept
{
    {
        std::unique_lock lock(this->mutex_);
        this->cv_.wait_for(lock, stoken, this->thread_sleep_, [this]() { return this->pending_; });

        if (stoken.stop_requested())
        {
            return;
        }
    }

    logger::trace<logger::autosave>("checking for pending autosave requests");
    if (this->pending_)
    {
        std::scoped_lock lock(this->mutex_);

        logger::trace<logger::autosave>(
            "found autosave requests, saving settings, total request for this period {}",
            this->total_);

        this->autosave_func_();

        this->total_ = 0;
        this->pending_ = false;
    }
}

void
autosave_backend::add() noexcept
{
    std::scoped_lock lock(this->mutex_);

    this->total_ += 1;
    logger::trace<logger::autosave>("adding request, total {}", this->total_);
    this->pending_ = true;
}

void
autosave_backend::cancel() noexcept
{
    std::scoped_lock lock(this->mutex_);

    logger::trace<logger::autosave>("canceling '{}' requests", this->total_);
    this->total_ = 0;
    this->pending_ = false;
}

namespace
{
autosave_backend autosave_;
std::jthread autosave_thread_;
} // namespace

void
autosave::request_add() noexcept
{
    autosave_.add();
}

void
autosave::request_cancel() noexcept
{
    autosave_.cancel();
}

void
autosave::create(const std::function<void()>& autosave_func) noexcept
{
    autosave_.set_autosave_func(autosave_func);

    logger::trace<logger::autosave>("starting autosave thread");

    autosave_thread_ = std::jthread([&](const std::stop_token& stoken) { autosave_.run(stoken); });
    pthread_setname_np(autosave_thread_.native_handle(), "autosave");
}

void
autosave::close() noexcept
{
    autosave_thread_.request_stop();
    autosave_thread_.join();
}
