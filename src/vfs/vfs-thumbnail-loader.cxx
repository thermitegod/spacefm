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
#include <string_view>

#include <format>

#include <filesystem>

#include <array>

#include <chrono>

#include <memory>

#include <glibmm.h>
#include <glibmm/convert.h>

#if !defined(HAVE_USE_FFMPEGTHUMBNAILER_CLI)
#include <libffmpegthumbnailer/imagetypes.h>
#include <libffmpegthumbnailer/videothumbnailer.h>
#endif

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-user-dirs.hxx"
#include "vfs/vfs-thumbnail-loader.hxx"

static void* thumbnail_loader_thread(vfs::async_task task, vfs::thumbnail_loader loader);
static bool on_thumbnail_idle(vfs::thumbnail_loader loader);

namespace vfs
{
    enum class thumbnail_size
    {
        big,
        small,
    };
}

struct VFSThumbnailRequest
{
    VFSThumbnailRequest(vfs::file_info file);
    ~VFSThumbnailRequest();

    vfs::file_info file{nullptr};
    std::map<vfs::thumbnail_size, i32> n_requests;
};

VFSThumbnailRequest::VFSThumbnailRequest(vfs::file_info file)
{
    this->file = vfs_file_info_ref(file);
}

VFSThumbnailRequest::~VFSThumbnailRequest()
{
    vfs_file_info_unref(this->file);
}

VFSThumbnailLoader::VFSThumbnailLoader(vfs::dir dir)
{
    this->dir = g_object_ref(dir);
    this->idle_handler = 0;
    this->task = vfs_async_task_new((VFSAsyncFunc)thumbnail_loader_thread, this);
}

VFSThumbnailLoader::~VFSThumbnailLoader()
{
    if (this->idle_handler)
    {
        g_source_remove(this->idle_handler);
        this->idle_handler = 0;
    }

    // stop the running thread, if any.
    this->task->cancel();

    if (this->idle_handler)
    {
        g_source_remove(this->idle_handler);
        this->idle_handler = 0;
    }

    g_object_unref(this->task);

    if (!this->update_queue.empty())
    {
        std::ranges::for_each(this->update_queue, vfs_file_info_unref);
    }

    // ztd::logger::debug("FREE THUMBNAIL LOADER");

    // prevent recursive unref called from vfs_dir_finalize
    this->dir->thumbnail_loader = nullptr;
    g_object_unref(this->dir);
}

vfs::thumbnail_loader
vfs_thumbnail_loader_new(vfs::dir dir)
{
    const auto loader = new VFSThumbnailLoader(dir);
    return loader;
}

void
vfs_thumbnail_loader_free(vfs::thumbnail_loader loader)
{
    delete loader;
}

static bool
on_thumbnail_idle(vfs::thumbnail_loader loader)
{
    // ztd::logger::debug("ENTER ON_THUMBNAIL_IDLE");

    while (!loader->update_queue.empty())
    {
        vfs::file_info file = loader->update_queue.front();
        loader->update_queue.pop_front();

        loader->dir->emit_thumbnail_loaded(file);
        vfs_file_info_unref(file);
    }

    loader->idle_handler = 0;

    if (loader->task->is_finished())
    {
        // ztd::logger::debug("FREE LOADER IN IDLE HANDLER");
        loader->dir->thumbnail_loader = nullptr;
        vfs_thumbnail_loader_free(loader);
    }
    // ztd::logger::debug("LEAVE ON_THUMBNAIL_IDLE");

    return false;
}

static void*
thumbnail_loader_thread(vfs::async_task task, vfs::thumbnail_loader loader)
{
    while (!task->is_cancelled())
    {
        if (loader->queue.empty())
        {
            break;
        }

        const vfs::thumbnail::request req = loader->queue.front();
        loader->queue.pop_front();
        if (!req)
        {
            break;
        }
        // ztd::logger::debug("pop: {}", req->file->name());

        // Only we have the reference. That means, no body is using the file
        if (req->file->ref_count() == 1)
        {
            continue;
        }

        bool need_update = false;
        for (const auto [index, value] : ztd::enumerate(req->n_requests))
        {
            if (value.second == 0)
            {
                continue;
            }

            const bool load_big = (value.first == vfs::thumbnail_size::big);
            if (!req->file->is_thumbnail_loaded(load_big))
            {
                const auto full_path =
                    std::filesystem::path() / loader->dir->path / req->file->name();

                // ztd::logger::info("loader->dir->path    = {}", loader->dir->path);
                // ztd::logger::info("req->file->name()    = {}", req->file->name());
                // ztd::logger::info("full_path            = {}", full_path);

                req->file->load_thumbnail(full_path, load_big);

                // Slow down for debugging.
                // ztd::logger::debug("DELAY!!");
                // Glib::usleep(G_USEC_PER_SEC/2);
                // ztd::logger::debug("thumbnail loaded: {}", req->file);
            }
            need_update = true;
        }

        if (!task->is_cancelled() && need_update)
        {
            loader->update_queue.emplace_back(vfs_file_info_ref(req->file));
            if (loader->idle_handler == 0)
            {
                loader->idle_handler = g_idle_add_full(G_PRIORITY_LOW,
                                                       (GSourceFunc)on_thumbnail_idle,
                                                       loader,
                                                       nullptr);
            }
        }
        // ztd::logger::debug("NEED_UPDATE: {}", need_update);
    }

    if (task->is_cancelled())
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
            // FIXME: add2 This source always causes a "Source ID was not
            // found" critical warning if removed in vfs_thumbnail_loader_free.
            // Where is this being removed?  See comment in
            // vfs_thumbnail_loader_cancel_all_requests

            // ztd::logger::debug("ADD IDLE HANDLER BEFORE THREAD ENDING");
            loader->idle_handler =
                g_idle_add_full(G_PRIORITY_LOW, (GSourceFunc)on_thumbnail_idle, loader, nullptr);
        }
    }
    // ztd::logger::debug("THREAD ENDED!");
    return nullptr;
}

void
vfs_thumbnail_loader_request(vfs::dir dir, vfs::file_info file, bool is_big)
{
    bool new_task = false;

    // ztd::logger::debug("request thumbnail: {}, is_big: {}", file->name(), is_big);
    if (!dir->thumbnail_loader)
    {
        dir->thumbnail_loader = vfs_thumbnail_loader_new(dir);
        new_task = true;
    }

    vfs::thumbnail_loader loader = dir->thumbnail_loader;

    if (!loader->task)
    {
        loader->task = vfs_async_task_new((VFSAsyncFunc)thumbnail_loader_thread, loader);
        new_task = true;
    }

    // Check if the request is already scheduled
    vfs::thumbnail::request req;
    for (const vfs::thumbnail::request& queued_req : loader->queue)
    {
        req = queued_req;
        // ztd::logger::info("req->file->name={} | file->name={}", req->file->name(), file->name());
        // If file with the same name is already in our queue
        if (req->file == file || ztd::same(req->file->name(), file->name()))
        {
            break;
        }
        req = nullptr;
    }

    if (!req)
    {
        req = std::make_shared<VFSThumbnailRequest>(file);
        dir->thumbnail_loader->queue.emplace_back(req);
    }

    ++req->n_requests[is_big ? vfs::thumbnail_size::big : vfs::thumbnail_size::small];

    if (new_task)
    {
        loader->task->run_thread();
    }
}

void
vfs_thumbnail_loader_cancel_all_requests(vfs::dir dir, bool is_big)
{
    (void)is_big;

    vfs::thumbnail_loader loader = dir->thumbnail_loader;
    if (!loader)
    {
        return;
    }

    vfs_thumbnail_loader_free(loader);
}

static GdkPixbuf*
vfs_thumbnail_load(const std::filesystem::path& file_path, const std::string_view file_uri,
                   i32 thumb_size)
{
    const std::string file_hash = ztd::compute_checksum(ztd::checksum::type::md5, file_uri);
    const std::string file_name = std::format("{}.png", file_hash);

    const auto thumbnail_file = vfs::user_dirs->cache_dir() / "thumbnails/normal" / file_name;

    // ztd::logger::debug("thumbnail_load()={} | uri={} | thumb_size={}", file_path, file_uri,
    // thumb_size);

    // get file mtime
    const auto ftime = std::filesystem::last_write_time(file_path);
    const auto stime = std::chrono::file_clock::to_sys(ftime);
    const auto mtime = std::chrono::system_clock::to_time_t(stime);

    // if the mtime of the file being thumbnailed is less than 5 sec ago,
    // do not create a thumbnail. This means that newly created files
    // will not have a thumbnail until a refresh
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
#if defined(HAVE_USE_FFMPEGTHUMBNAILER_CLI)
        const std::string command = std::format("ffmpegthumbnailer -s {} -i {} -o {}",
                                                thumb_size,
                                                ztd::shell::quote(file_path),
                                                ztd::shell::quote(thumbnail_file));
        // ztd::logger::info("COMMAND={}", command);
        Glib::spawn_command_line_sync(command);
#else
        try
        {
            ffmpegthumbnailer::VideoThumbnailer video_thumb;
            // video_thumb.setLogCallback(nullptr);
            // video_thumb.clearFilters();
            video_thumb.setSeekPercentage(25);
            video_thumb.setThumbnailSize(thumb_size);
            video_thumb.setMaintainAspectRatio(true);
            video_thumb.generateThumbnail(file_path,
                                          ThumbnailerImageType::Png,
                                          thumbnail_file,
                                          nullptr);
        }
        catch (...) // std::logic_error
        {
            // file cannot be opened
            return nullptr;
        }
#endif
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

GdkPixbuf*
vfs_thumbnail_load_for_uri(const std::string_view uri, i32 thumb_size)
{
    const std::filesystem::path file = Glib::filename_from_uri(uri.data());
    GdkPixbuf* ret = vfs_thumbnail_load(file, uri, thumb_size);
    return ret;
}

GdkPixbuf*
vfs_thumbnail_load_for_file(const std::filesystem::path& file, i32 thumb_size)
{
    const std::string uri = Glib::filename_to_uri(file.string());
    GdkPixbuf* ret = vfs_thumbnail_load(file, uri, thumb_size);
    return ret;
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
