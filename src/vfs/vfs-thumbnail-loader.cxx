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
#include <filesystem>

#include <chrono>

#include <sys/stat.h>

#include <glibmm.h>

#include <libffmpegthumbnailer/imagetypes.h>
#include <libffmpegthumbnailer/videothumbnailer.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-user-dir.hxx"
#include "vfs/vfs-thumbnail-loader.hxx"

#include "utils.hxx"

enum VFSThumbnailSize
{
    LOAD_BIG_THUMBNAIL,
    LOAD_SMALL_THUMBNAIL,
    N_LOAD_TYPES
};

struct VFSThumbnailRequest
{
    int n_requests[VFSThumbnailSize::N_LOAD_TYPES];
    VFSFileInfo* file;
};

static void* thumbnail_loader_thread(VFSAsyncTask* task, VFSThumbnailLoader* loader);
static void thumbnail_request_free(VFSThumbnailRequest* req);
static bool on_thumbnail_idle(VFSThumbnailLoader* loader);

VFSThumbnailLoader*
vfs_thumbnail_loader_new(VFSDir* dir)
{
    VFSThumbnailLoader* loader = g_slice_new0(VFSThumbnailLoader);
    loader->idle_handler = 0;
    loader->dir = g_object_ref(dir);
    loader->queue = g_queue_new();
    loader->update_queue = g_queue_new();
    loader->task = vfs_async_task_new((VFSAsyncFunc)thumbnail_loader_thread, loader);
    return loader;
}

void
vfs_thumbnail_loader_free(VFSThumbnailLoader* loader)
{
    if (loader->idle_handler)
    {
        g_source_remove(loader->idle_handler);
        loader->idle_handler = 0;
    }

    // stop the running thread, if any.
    vfs_async_task_cancel(loader->task);

    if (loader->idle_handler)
    {
        g_source_remove(loader->idle_handler);
        loader->idle_handler = 0;
    }

    g_object_unref(loader->task);

    if (loader->queue)
    {
        g_queue_foreach(loader->queue, (GFunc)thumbnail_request_free, nullptr);
        g_queue_free(loader->queue);
    }
    if (loader->update_queue)
    {
        g_queue_foreach(loader->update_queue, (GFunc)vfs_file_info_unref, nullptr);
        g_queue_free(loader->update_queue);
    }
    // LOG_DEBUG("FREE THUMBNAIL LOADER");

    // prevent recursive unref called from vfs_dir_finalize
    loader->dir->thumbnail_loader = nullptr;
    g_object_unref(loader->dir);
}

static void
thumbnail_request_free(VFSThumbnailRequest* req)
{
    vfs_file_info_unref(req->file);
    g_slice_free(VFSThumbnailRequest, req);
    // LOG_DEBUG("FREE REQUEST!");
}

static bool
on_thumbnail_idle(VFSThumbnailLoader* loader)
{
    VFSFileInfo* file;

    // LOG_DEBUG("ENTER ON_THUMBNAIL_IDLE");
    vfs_async_task_lock(loader->task);

    while ((file = static_cast<VFSFileInfo*>(g_queue_pop_head(loader->update_queue))))
    {
        vfs_dir_emit_thumbnail_loaded(loader->dir, file);
        vfs_file_info_unref(file);
    }

    loader->idle_handler = 0;

    vfs_async_task_unlock(loader->task);

    if (vfs_async_task_is_finished(loader->task))
    {
        // LOG_DEBUG("FREE LOADER IN IDLE HANDLER");
        loader->dir->thumbnail_loader = nullptr;
        vfs_thumbnail_loader_free(loader);
    }
    // LOG_DEBUG("LEAVE ON_THUMBNAIL_IDLE");

    return false;
}

static void*
thumbnail_loader_thread(VFSAsyncTask* task, VFSThumbnailLoader* loader)
{
    while (!vfs_async_task_is_cancelled(task))
    {
        vfs_async_task_lock(task);
        VFSThumbnailRequest* req =
            static_cast<VFSThumbnailRequest*>(g_queue_pop_head(loader->queue));
        vfs_async_task_unlock(task);
        if (!req)
            break;
        // LOG_DEBUG("pop: {}", req->file->name);

        // Only we have the reference. That means, no body is using the file
        if (req->file->ref_count() == 1)
        {
            thumbnail_request_free(req);
            continue;
        }

        bool need_update = false;
        int i;
        for (i = 0; i < 2; ++i)
        {
            if (req->n_requests[i] == 0)
                continue;

            bool load_big = (i == VFSThumbnailSize::LOAD_BIG_THUMBNAIL);
            if (!vfs_file_info_is_thumbnail_loaded(req->file, load_big))
            {
                std::string full_path;
                full_path =
                    g_build_filename(loader->dir->path, vfs_file_info_get_name(req->file), nullptr);
                vfs_file_info_load_thumbnail(req->file, full_path.c_str(), load_big);
                // Slow down for debugging.
                // LOG_DEBUG("DELAY!!");
                // Glib::usleep(G_USEC_PER_SEC/2);
                // LOG_DEBUG("thumbnail loaded: %s", req->file);
            }
            need_update = true;
        }

        if (!vfs_async_task_is_cancelled(task) && need_update)
        {
            vfs_async_task_lock(task);
            g_queue_push_tail(loader->update_queue, vfs_file_info_ref(req->file));
            if (loader->idle_handler == 0)
            {
                loader->idle_handler = g_idle_add_full(G_PRIORITY_LOW,
                                                       (GSourceFunc)on_thumbnail_idle,
                                                       loader,
                                                       nullptr);
            }
            vfs_async_task_unlock(task);
        }
        // LOG_DEBUG("NEED_UPDATE: {}", need_update);
        thumbnail_request_free(req);
    }

    if (vfs_async_task_is_cancelled(task))
    {
        // LOG_DEBUG("THREAD CANCELLED!!!");
        vfs_async_task_lock(task);
        if (loader->idle_handler)
        {
            g_source_remove(loader->idle_handler);
            loader->idle_handler = 0;
        }
        vfs_async_task_unlock(task);
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
vfs_thumbnail_loader_request(VFSDir* dir, VFSFileInfo* file, bool is_big)
{
    bool new_task = false;

    // LOG_DEBUG("request thumbnail: {}, is_big: {}", file->name, is_big);
    if (!dir->thumbnail_loader)
    {
        dir->thumbnail_loader = vfs_thumbnail_loader_new(dir);
        new_task = true;
    }

    VFSThumbnailLoader* loader = dir->thumbnail_loader;

    if (!loader->task)
    {
        loader->task = vfs_async_task_new((VFSAsyncFunc)thumbnail_loader_thread, loader);
        new_task = true;
    }
    vfs_async_task_lock(loader->task);

    // Check if the request is already scheduled
    VFSThumbnailRequest* req;
    GList* l;
    for (l = loader->queue->head; l; l = l->next)
    {
        req = static_cast<VFSThumbnailRequest*>(l->data);
        // If file with the same name is already in our queue
        if (req->file == file || ztd::same(req->file->name, file->name))
            break;
    }
    if (l)
    {
        req = static_cast<VFSThumbnailRequest*>(l->data);
    }
    else
    {
        req = g_slice_new0(VFSThumbnailRequest);
        req->file = vfs_file_info_ref(file);
        g_queue_push_tail(dir->thumbnail_loader->queue, req);
    }

    ++req->n_requests[is_big ? VFSThumbnailSize::LOAD_BIG_THUMBNAIL
                             : VFSThumbnailSize::LOAD_SMALL_THUMBNAIL];

    vfs_async_task_unlock(loader->task);

    if (new_task)
        vfs_async_task_execute(loader->task);
}

void
vfs_thumbnail_loader_cancel_all_requests(VFSDir* dir, bool is_big)
{
    VFSThumbnailLoader* loader;

    if ((loader = dir->thumbnail_loader))
    {
        vfs_async_task_lock(loader->task);
        // LOG_DEBUG("TRY TO CANCEL REQUESTS!!");
        GList* l;
        for (l = loader->queue->head; l;)
        {
            VFSThumbnailRequest* req = static_cast<VFSThumbnailRequest*>(l->data);
            --req->n_requests[is_big ? VFSThumbnailSize::LOAD_BIG_THUMBNAIL
                                     : VFSThumbnailSize::LOAD_SMALL_THUMBNAIL];

            // nobody needs this
            if (req->n_requests[0] <= 0 && req->n_requests[1] <= 0)
            {
                GList* next = l->next;
                g_queue_delete_link(loader->queue, l);
                l = next;
            }
            else
                l = l->next;
        }

        if (g_queue_get_length(loader->queue) == 0)
        {
            // LOG_DEBUG("FREE LOADER IN vfs_thumbnail_loader_cancel_all_requests!");
            vfs_async_task_unlock(loader->task);
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
        vfs_async_task_unlock(loader->task);
    }
}

static GdkPixbuf*
vfs_thumbnail_load(const std::string& file_path, const std::string& uri, int size,
                   std::time_t mtime)
{
    int w;
    int h;
    struct stat statbuf;
    int create_size;

    if (size > 256)
        create_size = 512;
    else if (size > 128)
        create_size = 256;
    else
        create_size = 128;

    bool file_is_video = false;
    VFSMimeType* mimetype = vfs_mime_type_get_from_file_name(file_path.c_str());
    if (mimetype)
    {
        if (!strncmp(vfs_mime_type_get_type(mimetype), "video/", 6))
            file_is_video = true;
        vfs_mime_type_unref(mimetype);
    }

    if (!file_is_video)
    {
        // image format cannot be recognized
        if (!gdk_pixbuf_get_file_info(file_path.c_str(), &w, &h))
            return nullptr;

        // If the image itself is very small, we should load it directly
        if (w <= create_size && h <= create_size)
        {
            if (w <= size && h <= size)
                return gdk_pixbuf_new_from_file(file_path.c_str(), nullptr);
            return gdk_pixbuf_new_from_file_at_size(file_path.c_str(), size, size, nullptr);
        }
    }

    Glib::Checksum check = Glib::Checksum();
    const std::string file_hash = check.compute_checksum(Glib::Checksum::Type::MD5, uri.data());
    const std::string file_name = fmt::format("{}.png", file_hash);

    const std::string thumbnail_file =
        Glib::build_filename(vfs_user_cache_dir(), "thumbnails/normal", file_name);

    // LOG_INFO("{}", thumbnail_file);

    if (mtime == 0)
    {
        if (stat(file_path.c_str(), &statbuf) != -1)
            mtime = statbuf.st_mtime;
    }

    // if mtime of video being thumbnailed is less than 5 sec ago,
    // don't create a thumbnail. This means that newly created video
    // files will not have a thumbnail until a refresh.
    if (file_is_video && std::time(nullptr) - mtime < 5)
        return nullptr;

    // load existing thumbnail
    GdkPixbuf* thumbnail = gdk_pixbuf_new_from_file(thumbnail_file.c_str(), nullptr);

    const char* thumb_mtime;
    if (thumbnail)
    {
        w = gdk_pixbuf_get_width(thumbnail);
        h = gdk_pixbuf_get_height(thumbnail);
        thumb_mtime = gdk_pixbuf_get_option(thumbnail, "tEXt::Thumb::MTime");
    }

    if (!thumbnail || (w < size && h < size) || !thumb_mtime ||
        strtol(thumb_mtime, nullptr, 10) != mtime)
    {
        if (thumbnail)
            g_object_unref(thumbnail);

        // create new thumbnail
        if (!file_is_video)
        {
            thumbnail = gdk_pixbuf_new_from_file_at_size(file_path.c_str(),
                                                         create_size,
                                                         create_size,
                                                         nullptr);
            if (thumbnail)
            {
                std::string mtime_str;

                // Note: gdk_pixbuf_apply_embedded_orientation returns a new
                // pixbuf or same with incremented ref count, so unref
                GdkPixbuf* thumbnail_old = thumbnail;
                thumbnail = gdk_pixbuf_apply_embedded_orientation(thumbnail);
                g_object_unref(thumbnail_old);
                mtime_str = fmt::format("{}", mtime);
                gdk_pixbuf_save(thumbnail,
                                thumbnail_file.c_str(),
                                "png",
                                nullptr,
                                "tEXt::Thumb::URI",
                                uri.c_str(),
                                "tEXt::Thumb::MTime",
                                mtime_str.c_str(),
                                nullptr);
            }
        }
        else
        {
            try
            {
                ffmpegthumbnailer::VideoThumbnailer video_thumb;
                video_thumb.setSeekPercentage(25);
                video_thumb.setThumbnailSize(128);
                video_thumb.setMaintainAspectRatio(true);
                // video_thumb.clearFilters();
                video_thumb.generateThumbnail(file_path,
                                              ThumbnailerImageType::Png,
                                              thumbnail_file,
                                              nullptr);
            }
            catch (...) // std::logic_error
            {
                // Video file cannot be opened
                if (thumbnail)
                    g_object_unref(thumbnail);
                return nullptr;
            }
            thumbnail = gdk_pixbuf_new_from_file(thumbnail_file.c_str(), nullptr);
        }
    }

    GdkPixbuf* result = nullptr;
    if (thumbnail)
    {
        w = gdk_pixbuf_get_width(thumbnail);
        h = gdk_pixbuf_get_height(thumbnail);

        if (w > h)
        {
            h = h * size / w;
            w = size;
        }
        else if (h > w)
        {
            w = w * size / h;
            h = size;
        }
        else
        {
            w = h = size;
        }

        if (w > 0 && h > 0)
            result = gdk_pixbuf_scale_simple(thumbnail, w, h, GDK_INTERP_BILINEAR);

        g_object_unref(thumbnail);
    }

    return result;
}

GdkPixbuf*
vfs_thumbnail_load_for_uri(const std::string& uri, int size, std::time_t mtime)
{
    std::string file = g_filename_from_uri(uri.c_str(), nullptr, nullptr);
    GdkPixbuf* ret = vfs_thumbnail_load(file, uri, size, mtime);
    return ret;
}

GdkPixbuf*
vfs_thumbnail_load_for_file(const std::string& file, int size, std::time_t mtime)
{
    std::string uri = g_filename_to_uri(file.c_str(), nullptr, nullptr);
    GdkPixbuf* ret = vfs_thumbnail_load(file, uri, size, mtime);
    return ret;
}

void
vfs_thumbnail_init()
{
    std::string dir = Glib::build_filename(vfs_user_cache_dir(), "thumbnails/normal");

    if (!std::filesystem::is_directory(dir))
        std::filesystem::create_directories(dir);

    std::filesystem::permissions(dir, std::filesystem::perms::owner_all);
}
