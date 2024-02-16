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
#include <unordered_map>

#include <memory>

#include <gdkmm.h>

#include <ztd/ztd.hxx>

#include "vfs/vfs-file.hxx"

namespace vfs
{
struct async_task;
struct dir;

struct thumbnailer : public std::enable_shared_from_this<thumbnailer>
{
  public:
    thumbnailer() = delete;
    thumbnailer(const std::shared_ptr<vfs::dir>& dir);
    ~thumbnailer();
    thumbnailer(const thumbnailer& other) = delete;
    thumbnailer(thumbnailer&& other) = delete;
    thumbnailer& operator=(const thumbnailer& other) = delete;
    thumbnailer& operator=(thumbnailer&& other) = delete;

    [[nodiscard]] static const std::shared_ptr<vfs::thumbnailer>
    create(const std::shared_ptr<vfs::dir>& dir) noexcept;

    void loader_request(const std::shared_ptr<vfs::file>& file,
                        const vfs::file::thumbnail_size size) noexcept;

    std::shared_ptr<vfs::dir> dir{nullptr};
    vfs::async_task* task{nullptr};

    u32 idle_handler{0};

    struct request
    {
        std::shared_ptr<vfs::file> file{nullptr};
        std::unordered_map<vfs::file::thumbnail_size, i32> n_requests;
    };

    std::deque<std::shared_ptr<vfs::thumbnailer::request>> queue{};
    std::deque<std::shared_ptr<vfs::file>> update_queue{};
};

// Ensure the thumbnail dirs exist and have proper file permission.
void thumbnail_init() noexcept;

GdkPixbuf* thumbnail_load(const std::shared_ptr<vfs::file>& file, const i32 thumb_size) noexcept;
} // namespace vfs
