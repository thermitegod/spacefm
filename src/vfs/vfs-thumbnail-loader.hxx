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

#include <string>
#include <string_view>

#include <chrono>

#include "vfs/vfs-dir.hxx"
#include "vfs/vfs-file-info.hxx"

#include <magic_enum.hpp>

#define VFS_THUMBNAIL_REQUEST(obj) (static_cast<VFSThumbnailRequest*>(obj))

enum VFSThumbnailSize
{
    LOAD_BIG_THUMBNAIL,
    LOAD_SMALL_THUMBNAIL,
};

struct VFSThumbnailRequest
{
    VFSThumbnailRequest(vfs::file_info file);
    ~VFSThumbnailRequest();

    vfs::file_info file;
    u32 n_requests[magic_enum::enum_count<VFSThumbnailSize>()];
};

// forward declare types
struct VFSThumbnailLoader;

namespace vfs
{
    using thumbnail_loader = ztd::raw_ptr<VFSThumbnailLoader>;
} // namespace vfs

struct VFSThumbnailLoader
{
    VFSThumbnailLoader(vfs::dir dir);
    ~VFSThumbnailLoader();

    vfs::dir dir;
    GQueue* queue;
    vfs::async_task task;
    u32 idle_handler;
    GQueue* update_queue;
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
GdkPixbuf* vfs_thumbnail_load_for_uri(std::string_view uri, i32 size);
GdkPixbuf* vfs_thumbnail_load_for_file(std::string_view file, i32 size);
