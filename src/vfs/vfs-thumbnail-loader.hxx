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

#include <deque>

#include <mutex>

#include <memory>

#include "vfs/vfs-dir.hxx"
#include "vfs/vfs-file.hxx"
#include "vfs/vfs-async-task.hxx"

// forward declare types
struct VFSThumbnailRequest;

namespace vfs
{
    using thumbnail_request_t = std::shared_ptr<VFSThumbnailRequest>;
} // namespace vfs

namespace vfs
{
    struct thumbnail_loader : public std::enable_shared_from_this<thumbnail_loader>
    {
      public:
        thumbnail_loader() = delete;
        thumbnail_loader(const std::shared_ptr<vfs::dir>& dir);
        ~thumbnail_loader();

        void loader_request(const std::shared_ptr<vfs::file>& file, bool is_big) noexcept;

        std::shared_ptr<vfs::dir> dir{nullptr};
        vfs::async_task task{nullptr};

        u32 idle_handler{0};

        std::mutex mtx;

        std::deque<vfs::thumbnail_request_t> queue{};
        std::deque<std::shared_ptr<vfs::file>> update_queue{};
    };
} // namespace vfs

// Ensure the thumbnail dirs exist and have proper file permission.
void vfs_thumbnail_init();

void vfs_thumbnail_loader_request(const std::shared_ptr<vfs::dir>& dir,
                                  const std::shared_ptr<vfs::file>& file, const bool is_big);

GdkPixbuf* vfs_thumbnail_load(const std::shared_ptr<vfs::file>& file, i32 thumb_size);
