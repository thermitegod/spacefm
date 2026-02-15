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

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <stop_token>

#include <gdkmm.h>

#include <ztd/ztd.hxx>

#include "vfs/file.hxx"

namespace vfs
{
class thumbnailer
{
  public:
    struct request_data final
    {
        std::shared_ptr<vfs::file> file;
#if (GTK_MAJOR_VERSION == 4)
        std::int32_t size;
#elif (GTK_MAJOR_VERSION == 3)
        vfs::file::thumbnail_size size;
#endif
    };

    void request(const request_data& request) noexcept;

    void run(const std::stop_token& stoken) noexcept;
    void run_once(const std::stop_token& stoken) noexcept;

    [[nodiscard]] auto
    signal_thumbnail_created() noexcept
    {
        return this->signal_thumbnail_created_;
    }

  private:
    std::queue<request_data> queue_;

    std::mutex mutex_;
    std::condition_variable_any cv_;

    // Signals
    sigc::signal<void(const std::shared_ptr<vfs::file>&)> signal_thumbnail_created_;
};
} // namespace vfs
