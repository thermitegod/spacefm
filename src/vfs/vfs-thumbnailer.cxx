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

#include <ranges>

#include <cassert>

#include <glibmm.h>

#include <libffmpegthumbnailer/imagetypes.h>
#include <libffmpegthumbnailer/videothumbnailer.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-dir.hxx"
#include "vfs/vfs-file.hxx"
#include "vfs/vfs-async-task.hxx"

#include "vfs/vfs-thumbnailer.hxx"

static void* thumbnailer_thread(vfs::async_task* task,
                                const std::shared_ptr<vfs::thumbnailer>& loader);
static bool on_thumbnail_idle(void* user_data);

vfs::thumbnailer::thumbnailer(const std::shared_ptr<vfs::dir>& dir) : dir(dir)
{
    // ztd::logger::debug("vfs::dir::thumbnailer({})", ztd::logger::utils::ptr(this));

    this->task = vfs::async_task::create((vfs::async_task::function_t)thumbnailer_thread, this);
}

vfs::thumbnailer::~thumbnailer()
{
    // ztd::logger::debug("vfs::dir::~thumbnailer({})", ztd::logger::utils::ptr(this));

    if (this->idle_handler)
    {
        g_source_remove(this->idle_handler);
        this->idle_handler = 0;
    }

    // stop the running thread, if any.
    this->task->cancel();
    g_object_unref(this->task);
}

const std::shared_ptr<vfs::thumbnailer>
vfs::thumbnailer::create(const std::shared_ptr<vfs::dir>& dir) noexcept
{
    return std::make_shared<vfs::thumbnailer>(dir);
}

void
vfs::thumbnailer::loader_request(const std::shared_ptr<vfs::file>& file,
                                 const vfs::file::thumbnail_size size) noexcept
{
    // Check if the request is already scheduled
    for (const auto& req : this->queue)
    {
        // ztd::logger::debug("req->file->name={} | file->name={}", req->file->name(), file->name());
        // If file with the same name is already in our queue
        if (req->file == file || req->file->name() == file->name())
        {
            ++req->n_requests[size];
            return;
        }
    }

    // ztd::logger::debug("this->queue add file={}", file->name());
    const auto req = std::make_shared<vfs::thumbnailer::request>(file);
    ++req->n_requests[size];

    this->queue.push_back(req);
}

static bool
on_thumbnail_idle(void* user_data)
{
    // ztd::logger::debug("ENTER ON_THUMBNAIL_IDLE");
    const auto loader = static_cast<vfs::thumbnailer*>(user_data)->shared_from_this();

    while (!loader->update_queue.empty())
    {
        const auto& file = loader->update_queue.front();
        loader->update_queue.pop_front();

        loader->dir->emit_thumbnail_loaded(file);
    }

    loader->idle_handler = 0;

    if (loader->task->is_finished())
    {
        // ztd::logger::debug("FREE LOADER IN IDLE HANDLER");
        // ztd::logger::trace("dir->thumbnailer({})", ztd::logger::utils::ptr(loader->dir->thumbnailer));
        loader->dir->thumbnailer = nullptr;
    }

    // ztd::logger::debug("LEAVE ON_THUMBNAIL_IDLE");

    return false;
}

static void*
thumbnailer_thread(vfs::async_task* task, const std::shared_ptr<vfs::thumbnailer>& loader)
{
    // ztd::logger::debug("thumbnailer_thread");
    while (!task->is_canceled())
    {
        if (loader->queue.empty())
        {
            break;
        }
        // ztd::logger::debug("loader->queue={}", loader->queue.size());

        const auto req = loader->queue.front();
        assert(req != nullptr);
        assert(req->file != nullptr);
        // ztd::logger::debug("pop: {}", req->file->name());
        loader->queue.pop_front();

        bool need_update = false;
        for (const auto [index, value] : std::views::enumerate(req->n_requests))
        {
            if (value.second == 0)
            {
                continue;
            }

            if (!req->file->is_thumbnail_loaded(value.first))
            {
                // ztd::logger::debug("loader->dir->path    = {}", loader->dir->path);
                // ztd::logger::debug("req->file->name()    = {}", req->file->name());
                // ztd::logger::debug("req->file->path()    = {}", req->file->path().string());

                req->file->load_thumbnail(value.first);

                // Slow down for debugging.
                // ztd::logger::debug("thumbnail loaded: {}", req->file);
                // std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            need_update = true;
        }
        // ztd::logger::debug("task->is_canceled()={} need_update={}", task->is_canceled(), need_update);
        if (!task->is_canceled() && need_update)
        {
            loader->update_queue.push_back(req->file);
            if (loader->idle_handler == 0)
            {
                loader->idle_handler = g_idle_add_full(G_PRIORITY_LOW,
                                                       (GSourceFunc)on_thumbnail_idle,
                                                       loader.get(),
                                                       nullptr);
            }
        }
        // ztd::logger::debug("NEED_UPDATE: {}", need_update);
    }

    if (task->is_canceled())
    {
        // ztd::logger::debug("THREAD CANCELLED!!!");
        if (loader->idle_handler)
        {
            g_source_remove(loader->idle_handler);
            loader->idle_handler = 0;
        }
    }
    else
    {
        if (loader->idle_handler == 0)
        {
            // ztd::logger::debug("ADD IDLE HANDLER BEFORE THREAD ENDING");
            loader->idle_handler = g_idle_add_full(G_PRIORITY_LOW,
                                                   (GSourceFunc)on_thumbnail_idle,
                                                   loader.get(),
                                                   nullptr);
        }
    }
    // ztd::logger::debug("THREAD ENDED!");
    return nullptr;
}
