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

#include <memory>

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "vfs/thumbnailer.hxx"

#include "concurrency.hxx"

vfs::thumbnailer::thumbnailer() noexcept
{
    // logger::debug<logger::vfs>("vfs::thumbnailer::thumbnailer({})", logger::utils::ptr(this));
    this->executor_ = global::runtime.thread_executor();
    this->executor_result_ = this->executor_->submit([this] { return this->thumbnailer_thread(); });
}

vfs::thumbnailer::~thumbnailer() noexcept
{
    // logger::debug<logger::vfs>("vfs::thumbnailer::~thumbnailer({})", logger::utils::ptr(this));
    {
        auto guard = this->lock_.lock(this->executor_);
        this->abort_ = true;
    }
    this->condition_.notify_all();

    this->executor_result_.get().get();
}

concurrencpp::result<void>
vfs::thumbnailer::request(vfs::thumbnailer::request_data request) noexcept
{
    // logger::debug<logger::vfs>("vfs::thumbnailer::request({})    {}", logger::utils::ptr(this), request.file->name());
    {
        auto guard = co_await this->lock_.lock(this->executor_);
        this->queue_.push(request);
    }
    this->condition_.notify_one();
}

concurrencpp::result<bool>
vfs::thumbnailer::thumbnailer_thread() noexcept
{
    while (true)
    {
        auto guard = co_await this->lock_.lock(this->executor_);
        co_await this->condition_.await(this->executor_,
                                        guard,
                                        [this] { return this->abort_ || !this->queue_.empty(); });

        if (this->abort_)
        {
            break;
        }

        const auto request = this->queue_.front();
        this->queue_.pop();

        if (!request.file->is_thumbnail_loaded(request.size))
        {
            request.file->load_thumbnail(request.size);
            // Slow down for debugging.
            // logger::debug<logger::vfs>("thumbnail loaded: {}", request.file->name());
            // std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        if (this->abort_)
        {
            // since thumbnail generation can take an indeterminate amount of time there
            // needs to be another abort check before calling the callback.
            break;
        }

        this->signal_thumbnail_created_.emit(request.file);
    }

    co_return true;
}
