/*
 *      vfs-thumbnail-loader.c
 *
 *      Copyright 2008 PCMan <pcman.tw@gmail.com>
 *      Copyright 2015 OmegaPhil <OmegaPhil@startmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 3 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#include <stdbool.h>

#include "vfs-mime-type.h"
#include "vfs-thumbnail-loader.h"
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <libffmpegthumbnailer/videothumbnailerc.h>

#ifdef USE_XXHASH
#include "xxhash.h"
#endif

typedef struct VFSThumbnailLoader
{
    VFSDir* dir;
    GQueue* queue;
    VFSAsyncTask* task;
    unsigned int idle_handler;
    GQueue* update_queue;
} VFSThumbnailLoader;

enum
{
    LOAD_BIG_THUMBNAIL,
    LOAD_SMALL_THUMBNAIL,
    N_LOAD_TYPES
};

typedef struct ThumbnailRequest
{
    int n_requests[N_LOAD_TYPES];
    VFSFileInfo* file;
} ThumbnailRequest;

static void* thumbnail_loader_thread(VFSAsyncTask* task, VFSThumbnailLoader* loader);
// static void on_load_finish(VFSAsyncTask* task, bool is_cancelled, VFSThumbnailLoader* loader);
static void thumbnail_request_free(ThumbnailRequest* req);
static bool on_thumbnail_idle(VFSThumbnailLoader* loader);

VFSThumbnailLoader* vfs_thumbnail_loader_new(VFSDir* dir)
{
    VFSThumbnailLoader* loader = g_slice_new0(VFSThumbnailLoader);
    loader->idle_handler = 0;
    loader->dir = g_object_ref(dir);
    loader->queue = g_queue_new();
    loader->update_queue = g_queue_new();
    loader->task = vfs_async_task_new((VFSAsyncFunc)thumbnail_loader_thread, loader);
    /* g_signal_connect( loader->task, "finish", G_CALLBACK(on_load_finish), loader ); */
    return loader;
}

void vfs_thumbnail_loader_free(VFSThumbnailLoader* loader)
{
    if (loader->idle_handler)
    {
        g_source_remove(loader->idle_handler);
        loader->idle_handler = 0;
    }

    /* g_signal_handlers_disconnect_by_func( loader->task, on_load_finish, loader ); */
    /* stop the running thread, if any. */
    vfs_async_task_cancel(loader->task);

    if (loader->idle_handler)
    {
        g_source_remove(loader->idle_handler);
        loader->idle_handler = 0;
    }

    g_object_unref(loader->task);

    if (loader->queue)
    {
        g_queue_foreach(loader->queue, (GFunc)thumbnail_request_free, NULL);
        g_queue_free(loader->queue);
    }
    if (loader->update_queue)
    {
        g_queue_foreach(loader->update_queue, (GFunc)vfs_file_info_unref, NULL);
        g_queue_free(loader->update_queue);
    }
    /* g_debug( "FREE THUMBNAIL LOADER" ); */

    /* prevent recursive unref called from vfs_dir_finalize */
    loader->dir->thumbnail_loader = NULL;
    g_object_unref(loader->dir);
}

#if 0 /* This is not used in the program. For debug only */
void on_load_finish( VFSAsyncTask* task, bool is_cancelled, VFSThumbnailLoader* loader )
{
    g_debug( "TASK FINISHED" );
}
#endif

static void thumbnail_request_free(ThumbnailRequest* req)
{
    vfs_file_info_unref(req->file);
    g_slice_free(ThumbnailRequest, req);
    /* g_debug( "FREE REQUEST!" ); */
}

static bool on_thumbnail_idle(VFSThumbnailLoader* loader)
{
    VFSFileInfo* file;

    /* g_debug( "ENTER ON_THUMBNAIL_IDLE" ); */
    vfs_async_task_lock(loader->task);

    while ((file = (VFSFileInfo*)g_queue_pop_head(loader->update_queue)))
    {
        vfs_dir_emit_thumbnail_loaded(loader->dir, file);
        vfs_file_info_unref(file);
    }

    loader->idle_handler = 0;

    vfs_async_task_unlock(loader->task);

    if (vfs_async_task_is_finished(loader->task))
    {
        /* g_debug( "FREE LOADER IN IDLE HANDLER" ); */
        loader->dir->thumbnail_loader = NULL;
        vfs_thumbnail_loader_free(loader);
    }
    /* g_debug( "LEAVE ON_THUMBNAIL_IDLE" ); */

    return FALSE;
}

static void* thumbnail_loader_thread(VFSAsyncTask* task, VFSThumbnailLoader* loader)
{
    while (G_LIKELY(!vfs_async_task_is_cancelled(task)))
    {
        vfs_async_task_lock(task);
        ThumbnailRequest* req = (ThumbnailRequest*)g_queue_pop_head(loader->queue);
        vfs_async_task_unlock(task);
        if (G_UNLIKELY(!req))
            break;
        /* g_debug("pop: %s", req->file->name); */

        /* Only we have the reference. That means, no body is using the file */
        if (req->file->n_ref == 1)
        {
            thumbnail_request_free(req);
            continue;
        }

        bool need_update = FALSE;
        int i;
        for (i = 0; i < 2; ++i)
        {
            if (req->n_requests[i] == 0)
                continue;

            bool load_big = (i == LOAD_BIG_THUMBNAIL);
            if (!vfs_file_info_is_thumbnail_loaded(req->file, load_big))
            {
                char* full_path;
                full_path =
                    g_build_filename(loader->dir->path, vfs_file_info_get_name(req->file), NULL);
                vfs_file_info_load_thumbnail(req->file, full_path, load_big);
                g_free(full_path);
                /*  Slow donwn for debugging.
                g_debug( "DELAY!!" );
                g_usleep(G_USEC_PER_SEC/2);
                */

                /* g_debug( "thumbnail loaded: %s", req->file ); */
            }
            need_update = TRUE;
        }

        if (!vfs_async_task_is_cancelled(task) && need_update)
        {
            vfs_async_task_lock(task);
            g_queue_push_tail(loader->update_queue, vfs_file_info_ref(req->file));
            if (loader->idle_handler == 0)
            {
                loader->idle_handler =
                    g_idle_add_full(G_PRIORITY_LOW, (GSourceFunc)on_thumbnail_idle, loader, NULL);
            }
            vfs_async_task_unlock(task);
        }
        /* g_debug( "NEED_UPDATE: %d", need_update ); */
        thumbnail_request_free(req);
    }

    if (vfs_async_task_is_cancelled(task))
    {
        /* g_debug( "THREAD CANCELLED!!!" ); */
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
            /* g_debug( "ADD IDLE HANDLER BEFORE THREAD ENDING" ); */
            /* FIXME: add2 This source always causes a "Source ID was not
             * found" critical warning if removed in vfs_thumbnail_loader_free.
             * Where is this being removed?  See comment in
             * vfs_thumbnail_loader_cancel_all_requests. */
            loader->idle_handler =
                g_idle_add_full(G_PRIORITY_LOW, (GSourceFunc)on_thumbnail_idle, loader, NULL);
        }
    }
    /* g_debug("THREAD ENDED!");  */
    return NULL;
}

void vfs_thumbnail_loader_request(VFSDir* dir, VFSFileInfo* file, bool is_big)
{
    bool new_task = FALSE;

    /* g_debug( "request thumbnail: %s, is_big: %d", file->name, is_big ); */
    if (G_UNLIKELY(!dir->thumbnail_loader))
    {
        dir->thumbnail_loader = vfs_thumbnail_loader_new(dir);
        new_task = TRUE;
    }

    VFSThumbnailLoader* loader = dir->thumbnail_loader;

    if (G_UNLIKELY(!loader->task))
    {
        loader->task = vfs_async_task_new((VFSAsyncFunc)thumbnail_loader_thread, loader);
        new_task = TRUE;
    }
    vfs_async_task_lock(loader->task);

    /* Check if the request is already scheduled */
    ThumbnailRequest* req;
    GList* l;
    for (l = loader->queue->head; l; l = l->next)
    {
        req = (ThumbnailRequest*)l->data;
        /* If file with the same name is already in our queue */
        if (req->file == file || !strcmp(req->file->name, file->name))
            break;
    }
    if (l)
    {
        req = (ThumbnailRequest*)l->data;
    }
    else
    {
        req = g_slice_new0(ThumbnailRequest);
        req->file = vfs_file_info_ref(file);
        g_queue_push_tail(dir->thumbnail_loader->queue, req);
    }

    ++req->n_requests[is_big ? LOAD_BIG_THUMBNAIL : LOAD_SMALL_THUMBNAIL];

    vfs_async_task_unlock(loader->task);

    if (new_task)
        vfs_async_task_execute(loader->task);
}

void vfs_thumbnail_loader_cancel_all_requests(VFSDir* dir, bool is_big)
{
    VFSThumbnailLoader* loader;

    if (G_UNLIKELY((loader = dir->thumbnail_loader)))
    {
        vfs_async_task_lock(loader->task);
        /* g_debug( "TRY TO CANCEL REQUESTS!!" ); */
        GList* l;
        for (l = loader->queue->head; l;)
        {
            ThumbnailRequest* req = (ThumbnailRequest*)l->data;
            --req->n_requests[is_big ? LOAD_BIG_THUMBNAIL : LOAD_SMALL_THUMBNAIL];

            if (req->n_requests[0] <= 0 && req->n_requests[1] <= 0) /* nobody needs this */
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
            /* g_debug( "FREE LOADER IN vfs_thumbnail_loader_cancel_all_requests!" ); */
            vfs_async_task_unlock(loader->task);
            loader->dir->thumbnail_loader = NULL;

            /* FIXME: added idle_handler = 0 to prevent idle_handler being
             * removed in vfs_thumbnail_loader_free - BUT causes a segfault
             * in vfs_async_task_lock ??
             * If source is removed here or in vfs_thumbnail_loader_free
             * it causes a "GLib-CRITICAL **: Source ID N was not found when
             * attempting to remove it" warning.  Such a source ID is always
             * the one added in thumbnail_loader_thread at the "add2" comment. */
            // loader->idle_handler = 0;

            vfs_thumbnail_loader_free(loader);
            return;
        }
        vfs_async_task_unlock(loader->task);
    }
}

static GdkPixbuf* _vfs_thumbnail_load(const char* file_path, const char* uri, int size,
                                      time_t mtime)
{
    char file_name[64];
    char mtime_str[64];

    char* thumbnail_file;
    char* thumb_mtime;
    int w;
    int h;
    struct stat statbuf;
    GdkPixbuf* result = NULL;
    int create_size;

    if (size > 256)
        create_size = 512;
    else if (size > 128)
        create_size = 256;
    else
        create_size = 128;

    bool file_is_video = FALSE;
    VFSMimeType* mimetype = vfs_mime_type_get_from_file_name(file_path);
    if (mimetype)
    {
        if (!strncmp(vfs_mime_type_get_type(mimetype), "video/", 6))
            file_is_video = TRUE;
        vfs_mime_type_unref(mimetype);
    }

    if (!file_is_video)
    {
        if (!gdk_pixbuf_get_file_info(file_path, &w, &h))
            return NULL; /* image format cannot be recognized */

        /* If the image itself is very small, we should load it directly */
        if (w <= create_size && h <= create_size)
        {
            if (w <= size && h <= size)
                return gdk_pixbuf_new_from_file(file_path, NULL);
            return gdk_pixbuf_new_from_file_at_size(file_path, size, size, NULL);
        }
    }

#ifdef USE_XXHASH
    XXH64_hash_t hash = XXH3_64bits(uri, strlen(uri));
    g_snprintf(file_name, 64, "%lu.png", hash);
#else
    GChecksum* cs = g_checksum_new(G_CHECKSUM_MD5);
    g_checksum_update(cs, uri, strlen(uri));
    g_snprintf(file_name, 64, "%s.png", g_checksum_get_string(cs));
    g_checksum_free(cs);
#endif

    thumbnail_file = g_build_filename(g_get_user_cache_dir(), "thumbnails/normal", file_name, NULL);

    // printf("%s\n", thumbnail_file);

    if (G_UNLIKELY(mtime == 0))
    {
        if (stat(file_path, &statbuf) != -1)
            mtime = statbuf.st_mtime;
    }

    /* if mod time of video being thumbnailed is less than 5 sec ago,
     * don't create a thumbnail (is copying?)
     * FIXME: This means that a newly saved file may not show a thumbnail
     * until refresh. */
    /*
    if (file_is_video && time(NULL) - mtime < 5)
        return NULL;
    */

    /* load existing thumbnail */
    GdkPixbuf* thumbnail = gdk_pixbuf_new_from_file(thumbnail_file, NULL);
    if (thumbnail)
    {
        w = gdk_pixbuf_get_width(thumbnail);
        h = gdk_pixbuf_get_height(thumbnail);
    }
    if (!thumbnail || (w < size && h < size) ||
        !(thumb_mtime = gdk_pixbuf_get_option(thumbnail, "tEXt::Thumb::MTime")) ||
        strtol(thumb_mtime, NULL, 10) != mtime)
    {
        if (thumbnail)
            g_object_unref(thumbnail);
        /* create new thumbnail */
        if (!file_is_video)
        {
            thumbnail = gdk_pixbuf_new_from_file_at_size(file_path, create_size, create_size, NULL);
            if (thumbnail)
            {
                // Note: gdk_pixbuf_apply_embedded_orientation returns a new
                // pixbuf or same with incremented ref count, so unref
                GdkPixbuf* thumbnail_old = thumbnail;
                thumbnail = gdk_pixbuf_apply_embedded_orientation(thumbnail);
                g_object_unref(thumbnail_old);
                g_snprintf(mtime_str, sizeof(mtime_str), "%lu", mtime);
                gdk_pixbuf_save(thumbnail,
                                thumbnail_file,
                                "png",
                                NULL,
                                "tEXt::Thumb::URI",
                                uri,
                                "tEXt::Thumb::MTime",
                                mtime_str,
                                NULL);
            }
        }
        else
        {
            video_thumbnailer* video_thumb = video_thumbnailer_create();

            /* Setting a callback to allow silencing of stdout/stderr messages
             * from the library. This is no longer required since v2.0.11, where
             * silence is the default.  It can be used for debugging in 2.0.11
             * and later. */
            // video_thumbnailer_set_log_callback(on_video_thumbnailer_log_message);

            if (video_thumb)
            {
                video_thumb->seek_percentage = 25;
                video_thumb->overlay_film_strip = 1;
                video_thumbnailer_set_size(video_thumb, 0, 128);
                video_thumbnailer_generate_thumbnail_to_file(video_thumb,
                                                             file_path,
                                                             thumbnail_file);
                video_thumbnailer_destroy(video_thumb);

                thumbnail = gdk_pixbuf_new_from_file(thumbnail_file, NULL);
            }
        }
    }

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

    g_free(thumbnail_file);
    return result;
}

GdkPixbuf* vfs_thumbnail_load_for_uri(const char* uri, int size, time_t mtime)
{
    char* file = g_filename_from_uri(uri, NULL, NULL);
    GdkPixbuf* ret = _vfs_thumbnail_load(file, uri, size, mtime);
    g_free(file);
    return ret;
}

GdkPixbuf* vfs_thumbnail_load_for_file(const char* file, int size, time_t mtime)
{
    char* uri = g_filename_to_uri(file, NULL, NULL);
    GdkPixbuf* ret = _vfs_thumbnail_load(file, uri, size, mtime);
    g_free(uri);
    return ret;
}

/* Ensure the thumbnail dirs exist and have proper file permission. */
void vfs_thumbnail_init()
{
    char* dir = g_build_filename(g_get_user_cache_dir(), "thumbnails/normal", NULL);

    if (G_LIKELY(g_file_test(dir, G_FILE_TEST_IS_DIR)))
        chmod(dir, 0700);
    else
        g_mkdir_with_parents(dir, 0700);

    g_free(dir);
}
