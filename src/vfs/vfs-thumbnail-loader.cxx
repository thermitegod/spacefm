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

#include <filesystem>

#include <chrono>

#include <memory>

#include <sys/stat.h>

#include <fmt/format.h>

#include <glibmm.h>
#include <glibmm/convert.h>

#ifndef FFMPEGTHUMBNAILER_CLI
#include <libffmpegthumbnailer/imagetypes.h>
#include <libffmpegthumbnailer/videothumbnailer.h>
#endif

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-user-dir.hxx"
#include "vfs/vfs-thumbnail-loader.hxx"

#include "utils.hxx"

static void* thumbnail_loader_thread(vfs::async_task task, vfs::thumbnail_loader loader);
static bool on_thumbnail_idle(vfs::thumbnail_loader loader);

enum VFSThumbnailSize
{
    BIG,
    SMALL,
};

struct VFSThumbnailRequest
{
    VFSThumbnailRequest(vfs::file_info file);
    ~VFSThumbnailRequest();

    vfs::file_info file;
    i32 n_requests[magic_enum::enum_count<VFSThumbnailSize>()];
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

    // LOG_DEBUG("FREE THUMBNAIL LOADER");

    // prevent recursive unref called from vfs_dir_finalize
    this->dir->thumbnail_loader = nullptr;
    g_object_unref(this->dir);
}

vfs::thumbnail_loader
vfs_thumbnail_loader_new(vfs::dir dir)
{
    vfs::thumbnail_loader loader = new VFSThumbnailLoader(dir);
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
    // LOG_DEBUG("ENTER ON_THUMBNAIL_IDLE");

    while (!loader->update_queue.empty())
    {
        vfs::file_info file = loader->update_queue.front();
        loader->update_queue.pop_front();

        vfs_dir_emit_thumbnail_loaded(loader->dir, file);
        vfs_file_info_unref(file);
    }

    loader->idle_handler = 0;

    if (loader->task->is_finished())
    {
        // LOG_DEBUG("FREE LOADER IN IDLE HANDLER");
        loader->dir->thumbnail_loader = nullptr;
        vfs_thumbnail_loader_free(loader);
    }
    // LOG_DEBUG("LEAVE ON_THUMBNAIL_IDLE");

    return false;
}

static void*
thumbnail_loader_thread(vfs::async_task task, vfs::thumbnail_loader loader)
{
    while (!task->is_cancelled())
    {
        if (loader->queue.empty())
            break;

        vfs::thumbnail::request req = loader->queue.front();
        loader->queue.pop_front();
        if (!req)
            break;
        // LOG_DEBUG("pop: {}", req->file->name);

        // Only we have the reference. That means, no body is using the file
        if (req->file->ref_count() == 1)
            continue;

        bool need_update = false;
        for (u32 i = 0; i < 2; ++i)
        {
            if (req->n_requests[i] == 0)
                continue;

            bool load_big = (i == VFSThumbnailSize::BIG);
            if (!req->file->is_thumbnail_loaded(load_big))
            {
                const std::string full_path =
                    Glib::build_filename(loader->dir->path, req->file->get_name());
                req->file->load_thumbnail(full_path, load_big);
                // Slow down for debugging.
                // LOG_DEBUG("DELAY!!");
                // Glib::usleep(G_USEC_PER_SEC/2);
                // LOG_DEBUG("thumbnail loaded: {}", req->file);
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
        // LOG_DEBUG("NEED_UPDATE: {}", need_update);
    }

    if (task->is_cancelled())
    {
        // LOG_DEBUG("THREAD CANCELLED!!!");
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

            // LOG_DEBUG("ADD IDLE HANDLER BEFORE THREAD ENDING");
            loader->idle_handler =
                g_idle_add_full(G_PRIORITY_LOW, (GSourceFunc)on_thumbnail_idle, loader, nullptr);
        }
    }
    // LOG_DEBUG("THREAD ENDED!");
    return nullptr;
}

void
vfs_thumbnail_loader_request(vfs::dir dir, vfs::file_info file, bool is_big)
{
    bool new_task = false;

    // LOG_DEBUG("request thumbnail: {}, is_big: {}", file->name, is_big);
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
    for (vfs::thumbnail::request& queued_req : loader->queue)
    {
        req = queued_req;
        // LOG_INFO("req->file->name={} | file->name={}", req->file->name, file->name);
        // If file with the same name is already in our queue
        if (req->file == file || ztd::same(req->file->name, file->name))
            break;
        req = nullptr;
    }

    if (!req)
    {
        req = std::make_shared<VFSThumbnailRequest>(file);
        dir->thumbnail_loader->queue.emplace_back(req);
    }

    ++req->n_requests[is_big ? VFSThumbnailSize::BIG : VFSThumbnailSize::SMALL];

    if (new_task)
        loader->task->run_thread();
}

void
vfs_thumbnail_loader_cancel_all_requests(vfs::dir dir, bool is_big)
{
    vfs::thumbnail_loader loader = dir->thumbnail_loader;

    if (!loader)
        return;

    u32 idx = 0;
    std::vector<u32> remove_idx;
    // LOG_DEBUG("TRY TO CANCEL REQUESTS!!");
    for (vfs::thumbnail::request req : loader->queue)
    {
        --req->n_requests[is_big ? VFSThumbnailSize::BIG : VFSThumbnailSize::SMALL];

        // nobody needs this
        if (req->n_requests[VFSThumbnailSize::BIG] <= 0 &&
            req->n_requests[VFSThumbnailSize::SMALL] <= 0)
        {
            remove_idx.emplace_back(idx);
        }

        ++idx;
    }

    // std::ranges::sort(remove_idx);
    // std::ranges::reverse(remove_idx);

    for (u32 i : remove_idx)
    {
        loader->queue.erase(loader->queue.cbegin() + i);
    }

    if (loader->queue.empty())
    {
        // LOG_DEBUG("FREE LOADER IN vfs_thumbnail_loader_cancel_all_requests!");
        loader->dir->thumbnail_loader = nullptr;

        // FIXME: added idle_handler = 0 to prevent idle_handler being
        // removed in vfs_thumbnail_loader_free - BUT causes a segfault
        // in vfs_async_task_lock ??
        // If source is removed here or in vfs_thumbnail_loader_free
        // it causes a "GLib-CRITICAL **: Source ID N was not found when
        // attempting to remove it" warning.  Such a source ID is always
        // the one added in thumbnail_loader_thread at the "add2" comment.

        // loader->idle_handler = 0;

        vfs_thumbnail_loader_free(loader);
        return;
    }
}

static GdkPixbuf*
vfs_thumbnail_load(std::string_view file_path, std::string_view file_uri, i32 thumb_size)
{
    Glib::Checksum hash = Glib::Checksum();
    const std::string file_hash = hash.compute_checksum(Glib::Checksum::Type::MD5, file_uri.data());
    const std::string file_name = fmt::format("{}.png", file_hash);

    const std::string thumbnail_file =
        Glib::build_filename(vfs_user_cache_dir(), "thumbnails/normal", file_name);

    // LOG_DEBUG("thumbnail_load()={} | uri={} | thumb_size={}", file_path, file_uri, thumb_size);

    // get file mtime
    const auto ftime = std::filesystem::last_write_time(file_path);
    const auto stime = std::chrono::file_clock::to_sys(ftime);
    const auto mtime = std::chrono::system_clock::to_time_t(stime);

    // if the mtime of the file being thumbnailed is less than 5 sec ago,
    // do not create a thumbnail. This means that newly created files
    // will not have a thumbnail until a refresh
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    if (now - mtime < 5)
        return nullptr;

    // load existing thumbnail
    i32 w;
    i32 h;
    std::time_t embeded_mtime = 0;
    GdkPixbuf* thumbnail = nullptr;
    if (std::filesystem::is_regular_file(thumbnail_file))
    {
        // LOG_DEBUG("Existing thumb: {}", thumbnail_file);
        thumbnail = gdk_pixbuf_new_from_file(thumbnail_file.data(), nullptr);
        if (thumbnail)
        { // need to check for broken thumbnail images
            w = gdk_pixbuf_get_width(thumbnail);
            h = gdk_pixbuf_get_height(thumbnail);
            const char* thumb_mtime = gdk_pixbuf_get_option(thumbnail, "tEXt::Thumb::MTime");
            embeded_mtime = std::stol(thumb_mtime);
        }
    }

    if (!thumbnail || (w < thumb_size && h < thumb_size) || embeded_mtime != mtime)
    {
        // LOG_DEBUG("New thumb: {}", thumbnail_file);

        if (thumbnail)
        {
            g_object_unref(thumbnail);
        }

        // create new thumbnail
#ifndef FFMPEGTHUMBNAILER_CLI
        try
        {
            ffmpegthumbnailer::VideoThumbnailer video_thumb;
            // video_thumb.setLogCallback(nullptr);
            // video_thumb.clearFilters();
            video_thumb.setSeekPercentage(25);
            video_thumb.setThumbnailSize(thumb_size);
            video_thumb.setMaintainAspectRatio(true);
            video_thumb.generateThumbnail(file_path.data(),
                                          ThumbnailerImageType::Png,
                                          thumbnail_file,
                                          nullptr);
        }
        catch (...) // std::logic_error
        {
            // file cannot be opened
            return nullptr;
        }
#else
        const std::string command = fmt::format("ffmpegthumbnailer -s {} -i {} -o {}",
                                                thumb_size,
                                                bash_quote(file_path),
                                                bash_quote(thumbnail_file));
        // print_command(command);
        Glib::spawn_command_line_sync(command);
#endif

        thumbnail = gdk_pixbuf_new_from_file(thumbnail_file.data(), nullptr);
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
            result = gdk_pixbuf_scale_simple(thumbnail, w, h, GdkInterpType::GDK_INTERP_BILINEAR);

        g_object_unref(thumbnail);
    }

    return result;
}

GdkPixbuf*
vfs_thumbnail_load_for_uri(std::string_view uri, i32 thumb_size)
{
    const std::string file = Glib::filename_from_uri(uri.data());
    GdkPixbuf* ret = vfs_thumbnail_load(file, uri, thumb_size);
    return ret;
}

GdkPixbuf*
vfs_thumbnail_load_for_file(std::string_view file, i32 thumb_size)
{
    const std::string uri = Glib::filename_to_uri(file.data());
    GdkPixbuf* ret = vfs_thumbnail_load(file, uri, thumb_size);
    return ret;
}

void
vfs_thumbnail_init()
{
    const std::string dir = Glib::build_filename(vfs_user_cache_dir(), "thumbnails/normal");

    if (!std::filesystem::is_directory(dir))
        std::filesystem::create_directories(dir);

    std::filesystem::permissions(dir, std::filesystem::perms::owner_all);
}
