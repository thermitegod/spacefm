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

#include <map>

#include <deque>

#include <mutex>

#include <memory>

#include <gdkmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

namespace vfs
{
    struct async_task;
    struct dir;
    struct file;

    struct thumbnailer : public std::enable_shared_from_this<thumbnailer>
    {
      public:
        thumbnailer() = delete;
        thumbnailer(const std::shared_ptr<vfs::dir>& dir);
        ~thumbnailer();

        static const std::shared_ptr<vfs::thumbnailer>
        create(const std::shared_ptr<vfs::dir>& dir) noexcept;

        void loader_request(const std::shared_ptr<vfs::file>& file, bool is_big) noexcept;

        std::shared_ptr<vfs::dir> dir{nullptr};
        vfs::async_task* task{nullptr};

        u32 idle_handler{0};

        std::mutex mtx;

        struct request
        {
            enum class size
            {
                big,
                small,
            };
            std::shared_ptr<vfs::file> file{nullptr};
            std::map<size, i32> n_requests;
        };

        std::deque<std::shared_ptr<vfs::thumbnailer::request>> queue{};
        std::deque<std::shared_ptr<vfs::file>> update_queue{};
    };
} // namespace vfs

// Ensure the thumbnail dirs exist and have proper file permission.
void vfs_thumbnail_init();

void vfs_thumbnail_request(const std::shared_ptr<vfs::dir>& dir,
                           const std::shared_ptr<vfs::file>& file, const bool is_big);

GdkPixbuf* vfs_thumbnail_load(const std::shared_ptr<vfs::file>& file, i32 thumb_size);
