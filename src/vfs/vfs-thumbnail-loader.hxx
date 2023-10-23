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
#include "vfs/vfs-file-info.hxx"
#include "vfs/vfs-async-task.hxx"

// forward declare types
struct VFSThumbnailRequest;
struct VFSThumbnailLoader;

namespace vfs
{
    using thumbnail_loader = std::shared_ptr<VFSThumbnailLoader>;
    using thumbnail_request_t = std::shared_ptr<VFSThumbnailRequest>;
} // namespace vfs

struct VFSThumbnailLoader : public std::enable_shared_from_this<VFSThumbnailLoader>
{
  public:
    VFSThumbnailLoader() = delete;
    VFSThumbnailLoader(const std::shared_ptr<vfs::dir>& dir);
    ~VFSThumbnailLoader();

    void loader_request(const vfs::file_info& file, bool is_big) noexcept;

    std::shared_ptr<vfs::dir> dir{nullptr};
    vfs::async_task task{nullptr};

    u32 idle_handler{0};

    std::mutex mtx;

    std::deque<vfs::thumbnail_request_t> queue{};
    std::deque<vfs::file_info> update_queue{};
};

// Ensure the thumbnail dirs exist and have proper file permission.
void vfs_thumbnail_init();

void vfs_thumbnail_loader_request(const std::shared_ptr<vfs::dir>& dir, const vfs::file_info& file,
                                  const bool is_big);

GdkPixbuf* vfs_thumbnail_load(const vfs::file_info& file, i32 thumb_size);
