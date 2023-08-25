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

#include <thread>

#include <memory>

#include "vfs/vfs-async-thread.hxx"

vfs::async_thread::async_thread(vfs::async_thread_function_t task_function, void* user_data)
    : task_function_(task_function), user_data_(user_data)
{
}

vfs::async_thread::~async_thread()
{
    this->cleanup(true);
}

void
vfs::async_thread::run()
{
    if (this->running_)
    {
        return;
    }

    this->running_ = true;
    this->finished_ = false;
    this->canceled_ = false;

    this->thread_ = std::jthread([this]() { this->task_function_(this, this->user_data_); });

    this->thread_.join();
    this->running_ = false;
    this->finished_ = true;

    // this->cleanup(true);

    this->run_event<spacefm::signal::task_finish>(this->canceled_);
}

void
vfs::async_thread::cancel()
{
    if (!this->thread_.joinable())
    {
        return;
    }

    this->cancel_ = true;
    this->cleanup(false);
    this->canceled_ = true;
}

void*
vfs::async_thread::user_data() const
{
    return this->user_data_;
}

bool
vfs::async_thread::is_running() const
{
    return this->running_;
}

bool
vfs::async_thread::is_finished() const
{
    return this->finished_;
}

bool
vfs::async_thread::is_canceled() const
{
    return this->cancel_;
}

void
vfs::async_thread::cleanup(bool finalize)
{
    if (!this->thread_.joinable())
    {
        return;
    }

    this->thread_.join();
    this->finished_ = true;

    // Only emit the signal when we are not finalizing.
    if (!finalize)
    {
        this->run_event<spacefm::signal::task_finish>(this->canceled_);
    }
}

vfs::async_thread_t
vfs_async_thread_new(vfs::async_thread_function_t task_function, void* user_data)
{
    return std::make_shared<vfs::async_thread>(task_function, user_data);
}
