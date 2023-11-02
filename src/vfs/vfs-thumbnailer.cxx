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

#include <string>

#include <format>

#include <filesystem>

#include <chrono>

#include <mutex>

#include <memory>

#include <cassert>

#include <glibmm.h>

#include <libffmpegthumbnailer/imagetypes.h>
#include <libffmpegthumbnailer/videothumbnailer.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "settings/app.hxx"

#include "vfs/vfs-dir.hxx"
#include "vfs/vfs-file.hxx"
#include "vfs/vfs-async-task.hxx"
#include "vfs/vfs-user-dirs.hxx"

#include "vfs/vfs-thumbnailer.hxx"

static void* thumbnailer_thread(vfs::async_task* task,
                                const std::shared_ptr<vfs::thumbnailer>& loader);
static bool on_thumbnail_idle(void* user_data);

vfs::thumbnailer::thumbnailer(const std::shared_ptr<vfs::dir>& dir) : dir(dir)
{
    // ztd::logger::debug("vfs::dir::thumbnailer({})", fmt::ptr(this));

    this->task = vfs::async_task::create((vfs::async_task::function_t)thumbnailer_thread, this);
}

vfs::thumbnailer::~thumbnailer()
{
    // ztd::logger::debug("vfs::dir::~thumbnailer({})", fmt::ptr(this));

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
vfs::thumbnailer::loader_request(const std::shared_ptr<vfs::file>& file, bool is_big) noexcept
{
    // Check if the request is already scheduled
    std::shared_ptr<vfs::thumbnailer::request> req;
    for (const auto& queued_req : this->queue)
    {
        req = queued_req;
        // ztd::logger::debug("req->file->name={} | file->name={}", req->file->name(), file->name());
        // If file with the same name is already in our queue
        if (req->file == file || ztd::same(req->file->name(), file->name()))
        {
            break;
        }
        req = nullptr;
    }

    if (!req)
    {
        req = std::make_shared<vfs::thumbnailer::request>(file);
        // ztd::logger::debug("this->queue add file={}", req->file->name());
        this->queue.emplace_back(req);
    }

    ++req->n_requests[is_big ? vfs::thumbnailer::request::size::big
                             : vfs::thumbnailer::request::size::small];
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
        // ztd::logger::trace("dir->thumbnailer({})", fmt::ptr(loader->dir->thumbnailer));
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
        std::scoped_lock<std::mutex> lock(loader->mtx);
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
        for (const auto [index, value] : ztd::enumerate(req->n_requests))
        {
            if (value.second == 0)
            {
                continue;
            }

            const bool load_big = (value.first == vfs::thumbnailer::request::size::big);
            if (!req->file->is_thumbnail_loaded(load_big))
            {
                // ztd::logger::debug("loader->dir->path    = {}", loader->dir->path);
                // ztd::logger::debug("req->file->name()    = {}", req->file->name());
                // ztd::logger::debug("req->file->path()    = {}", req->file->path().string());

                req->file->load_thumbnail(load_big);

                // Slow down for debugging.
                // ztd::logger::debug("DELAY!!");
                // Glib::usleep(G_USEC_PER_SEC/2);
                // ztd::logger::debug("thumbnail loaded: {}", req->file);
            }
            need_update = true;
        }
        // ztd::logger::debug("task->is_canceled()={} need_update={}", task->is_canceled(), need_update);
        if (!task->is_canceled() && need_update)
        {
            loader->update_queue.emplace_back(req->file);
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

GdkPixbuf*
vfs_thumbnail_load(const std::shared_ptr<vfs::file>& file, i32 thumb_size)
{
    const std::string file_hash = ztd::compute_checksum(ztd::checksum::type::md5, file->uri());
    const std::string filename = std::format("{}.png", file_hash);

    const auto thumbnail_file = vfs::user_dirs->cache_dir() / "thumbnails/normal" / filename;

    // ztd::logger::debug("thumbnail_load()={} | uri={} | thumb_size={}", file->path().string(), file->uri(), thumb_size);

    // if the mtime of the file being thumbnailed is less than 5 sec ago,
    // do not create a thumbnail. This means that newly created files
    // will not have a thumbnail until a refresh
    const auto mtime = file->mtime();
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    if (now - mtime < 5)
    {
        return nullptr;
    }

    // load existing thumbnail
    i32 w;
    i32 h;
    std::time_t embeded_mtime = 0;
    GdkPixbuf* thumbnail = nullptr;
    if (std::filesystem::is_regular_file(thumbnail_file))
    {
        // ztd::logger::debug("Existing thumb: {}", thumbnail_file);
        thumbnail = gdk_pixbuf_new_from_file(thumbnail_file.c_str(), nullptr);
        if (thumbnail)
        { // need to check for broken thumbnail images
            w = gdk_pixbuf_get_width(thumbnail);
            h = gdk_pixbuf_get_height(thumbnail);
            const char* thumb_mtime = gdk_pixbuf_get_option(thumbnail, "tEXt::Thumb::MTime");
            if (thumb_mtime != nullptr)
            {
                embeded_mtime = std::stol(thumb_mtime);
            }
        }
    }

    if (!thumbnail || (w < thumb_size && h < thumb_size) || embeded_mtime != mtime)
    {
        // ztd::logger::debug("New thumb: {}", thumbnail_file);

        if (thumbnail)
        {
            g_object_unref(thumbnail);
        }

        // create new thumbnail
        if (app_settings.thumbnailer_use_api())
        {
            try
            {
                ffmpegthumbnailer::VideoThumbnailer video_thumb;
                // video_thumb.setLogCallback(nullptr);
                // video_thumb.clearFilters();
                video_thumb.setSeekPercentage(25);
                video_thumb.setThumbnailSize(thumb_size);
                video_thumb.setMaintainAspectRatio(true);
                video_thumb.generateThumbnail(file->path(),
                                              ThumbnailerImageType::Png,
                                              thumbnail_file,
                                              nullptr);
            }
            catch (const std::logic_error& e)
            {
                // file cannot be opened
                return nullptr;
            }
        }
        else
        {
            const auto command = std::format("ffmpegthumbnailer -s {} -i {} -o {}",
                                             thumb_size,
                                             ztd::shell::quote(file->path().string()),
                                             ztd::shell::quote(thumbnail_file.string()));
            // ztd::logger::info("COMMAND={}", command);
            Glib::spawn_command_line_sync(command);

            if (!std::filesystem::exists(thumbnail_file))
            {
                return nullptr;
            }
        }

        thumbnail = gdk_pixbuf_new_from_file(thumbnail_file.c_str(), nullptr);
    }

    GdkPixbuf* result = nullptr;
    if (thumbnail)
    {
        w = gdk_pixbuf_get_width(thumbnail);
        h = gdk_pixbuf_get_height(thumbnail);

        if (w > h)
        {
            h = h * thumb_size / w;
            w = thumb_size;
        }
        else if (h > w)
        {
            w = w * thumb_size / h;
            h = thumb_size;
        }
        else
        {
            w = h = thumb_size;
        }

        if (w > 0 && h > 0)
        {
            result = gdk_pixbuf_scale_simple(thumbnail, w, h, GdkInterpType::GDK_INTERP_BILINEAR);
        }

        g_object_unref(thumbnail);
    }

    return result;
}

void
vfs_thumbnail_init()
{
    const auto dir = vfs::user_dirs->cache_dir() / "thumbnails/normal";
    if (!std::filesystem::is_directory(dir))
    {
        std::filesystem::create_directories(dir);
    }
    std::filesystem::permissions(dir, std::filesystem::perms::owner_all);
}
