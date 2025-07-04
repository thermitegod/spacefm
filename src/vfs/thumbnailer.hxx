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

#include <memory>
#include <queue>

#include <gdkmm.h>

#include <ztd/ztd.hxx>

#include "vfs/file.hxx"

#include "concurrency.hxx"

namespace vfs
{
class thumbnailer final
{
  public:
    thumbnailer() noexcept;
    ~thumbnailer() noexcept;
    thumbnailer(const thumbnailer& other) = delete;
    thumbnailer(thumbnailer&& other) = delete;
    thumbnailer& operator=(const thumbnailer& other) = delete;
    thumbnailer& operator=(thumbnailer&& other) = delete;

    struct request_data final
    {
        std::shared_ptr<vfs::file> file;
        vfs::file::thumbnail_size size;
    };
    concurrencpp::result<void> request(const request_data request) noexcept;

    [[nodiscard]] auto
    signal_thumbnail_created()
    {
        return this->signal_thumbnail_created_;
    }

  private:
    concurrencpp::result<bool> thumbnailer_thread() noexcept;

    std::queue<request_data> queue_;

    std::shared_ptr<concurrencpp::thread_executor> executor_;
    concurrencpp::result<concurrencpp::result<bool>> executor_result_;
    concurrencpp::async_condition_variable condition_;
    concurrencpp::async_lock lock_;
    bool abort_{false};

    // Signals
    sigc::signal<void(const std::shared_ptr<vfs::file>&)> signal_thumbnail_created_;
};
} // namespace vfs
