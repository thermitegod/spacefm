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

#include <string_view>

#include <filesystem>

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
    // using thumbnail_loader = std::shared_ptr<VFSThumbnailLoader>;
    using thumbnail_loader = ztd::raw_ptr<VFSThumbnailLoader>;
    using thumbnail_request_t = std::shared_ptr<VFSThumbnailRequest>;
} // namespace vfs

struct VFSThumbnailLoader
{
  public:
    VFSThumbnailLoader() = delete;
    VFSThumbnailLoader(vfs::dir dir);
    ~VFSThumbnailLoader();

    vfs::dir dir{nullptr};
    vfs::async_task task{nullptr};

    u32 idle_handler{0};

    std::mutex mtx;

    std::deque<vfs::thumbnail_request_t> queue{};
    std::deque<vfs::file_info> update_queue{};
};

// Ensure the thumbnail dirs exist and have proper file permission.
void vfs_thumbnail_init();

void vfs_thumbnail_loader_free(vfs::thumbnail_loader loader);

void vfs_thumbnail_loader_request(vfs::dir dir, vfs::file_info file, bool is_big);
void vfs_thumbnail_loader_cancel_all_requests(vfs::dir dir, bool is_big);

// Load thumbnail for the specified file
// If the caller knows mtime of the file, it should pass mtime to this function to
// prevent unnecessary disk I/O and this can speed up the loading.
// Otherwise, it should pass 0 for mtime, and the function will do stat() on the file
// to get mtime.
GdkPixbuf* vfs_thumbnail_load_for_uri(const std::string_view uri, i32 thumb_size);
GdkPixbuf* vfs_thumbnail_load_for_file(const std::filesystem::path& file, i32 thumb_size);
