/**
 * Copyright 2008 PCMan <pcman.tw@gmail.com>
 * Copyright 2015 OmegaPhil <OmegaPhil@startmail.com>
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

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "vfs/thumbnailer.hxx"

void
vfs::thumbnailer::request(const request_data& request) noexcept
{
    {
        std::lock_guard<std::mutex> lock(this->mutex_);
        this->queue_.push(request);
    }
    this->cv_.notify_one();
}

void
vfs::thumbnailer::run() noexcept
{
    while (!this->is_stopped())
    {
        this->run_once();
    }
}

void
vfs::thumbnailer::run_once() noexcept
{
    request_data request;
    {
        std::unique_lock<std::mutex> lock(this->mutex_);
        this->cv_.wait(lock, [this] { return this->is_stopped() || !this->queue_.empty(); });

        if (this->is_stopped() && this->queue_.empty())
        {
            return;
        }

        if (!this->queue_.empty())
        {
            request = this->queue_.front();
            this->queue_.pop();
        }
    }

    if (!request.file->is_thumbnail_loaded(request.size))
    {
        request.file->load_thumbnail(request.size);
        // Slow down for debugging.
        // logger::debug<logger::vfs>("thumbnail loaded: {}", request.file->name());
        // std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (this->is_stopped())
    {
        // since thumbnail generation can take an indeterminate amount of time there
        // needs to be another abort check before calling the callback.
        return;
    }

    this->signal_thumbnail_created().emit(request.file);
}

void
vfs::thumbnailer::stop() noexcept
{
    {
        std::lock_guard<std::mutex> lock(this->mutex_);
        this->stopped_ = true;
    }
    this->cv_.notify_all();
}

bool
vfs::thumbnailer::is_stopped() const noexcept
{
    return this->stopped_;
}
