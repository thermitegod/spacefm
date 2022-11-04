/**
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#include <vector>

#include <map>

#include <algorithm>
#include <ranges>

#include <mutex>

#include <glibmm.h>

#include <gtk/gtk.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-mime-type.hxx"
#include "vfs/vfs-file-monitor.hxx"

#include "vfs/vfs-utils.hxx"

static std::map<const char*, vfs::mime_type> mime_map;
std::mutex mime_map_lock;

static u32 reload_callback_id = 0;
static GList* reload_cb = nullptr;

static i32 big_icon_size = 32, small_icon_size = 16;

static std::vector<vfs::file_monitor_t> mime_caches_monitors;

struct VFSMimeReloadCbEnt
{
    VFSMimeReloadCbEnt(GFreeFunc cb, void* user_data);

    GFreeFunc cb;
    void* user_data;
};

VFSMimeReloadCbEnt::VFSMimeReloadCbEnt(GFreeFunc cb, void* user_data)
{
    this->cb = cb;
    this->user_data = user_data;
}

static bool
vfs_mime_type_reload(void* user_data)
{
    (void)user_data;

    /* FIXME: process mime database reloading properly. */
    /* Remove all items in the hash table */

    mime_map_lock.lock();

    std::ranges::for_each(mime_map,
                          [](const auto& mime)
                          {
                              vfs_mime_type_unref(mime.second);
                          });

    mime_map.clear();
    mime_map_lock.unlock();

    g_source_remove(reload_callback_id);
    reload_callback_id = 0;

    // LOG_DEBUG("reload mime-types");

    mime_type_regen_all_caches();

    return false;
}

static void
on_mime_cache_changed(vfs::file_monitor_t monitor, VFSFileMonitorEvent event,
                      std::string_view file_name, void* user_data)
{
    (void)monitor;
    (void)file_name;

    switch (event)
    {
        case VFSFileMonitorEvent::VFS_FILE_MONITOR_CREATE:
        case VFSFileMonitorEvent::VFS_FILE_MONITOR_DELETE:
        case VFSFileMonitorEvent::VFS_FILE_MONITOR_CHANGE:
            // LOG_DEBUG("reloading all mime caches");
            if (reload_callback_id == 0)
                reload_callback_id = g_idle_add((GSourceFunc)vfs_mime_type_reload, nullptr);
            break;
    }
}

void
vfs_mime_type_init()
{
    mime_type_init();

    /* install file alteration monitor for mime-cache */
    std::vector<MimeCache> caches = mime_type_get_caches();
    for (MimeCache& cache: caches)
    {
        // MOD NOTE1  check to see if path exists - otherwise it later tries to
        //  remove nullptr monitor with inotify which caused segfault
        if (!std::filesystem::exists(cache.get_file_path()))
            continue;

        vfs::file_monitor_t monitor =
            vfs_file_monitor_add(cache.get_file_path(), on_mime_cache_changed, nullptr);

        mime_caches_monitors.push_back(monitor);
    }
}

void
vfs_mime_type_clean()
{
    // remove file alteration monitor for mime-cache
    std::ranges::for_each(mime_caches_monitors,
                          [](vfs::file_monitor_t monitor)
                          {
                              vfs_file_monitor_remove(monitor, on_mime_cache_changed, nullptr);
                          });

    mime_type_finalize();

    std::ranges::for_each(mime_map,
                          [](const auto& mime)
                          {
                              vfs_mime_type_unref(mime.second);
                          });

    mime_map.clear();
}

vfs::mime_type
vfs_mime_type_get_from_file_name(const char* ufile_name)
{
    /* type = xdg_mime_get_mime_type_from_file_name( ufile_name ); */
    const char* type = mime_type_get_by_filename(ufile_name, nullptr);
    return vfs_mime_type_get_from_type(type);
}

vfs::mime_type
vfs_mime_type_get_from_file(const char* file_path, const char* base_name, struct stat* pstat)
{
    const char* type = mime_type_get_by_file(file_path, pstat, base_name);
    return vfs_mime_type_get_from_type(type);
}

vfs::mime_type
vfs_mime_type_get_from_type(const char* type)
{
    mime_map_lock.lock();
    vfs::mime_type mime_type = nullptr;
    try
    {
        mime_type = mime_map.at(type);
    }
    catch (std::out_of_range)
    {
        mime_type = nullptr;
    }
    mime_map_lock.unlock();

    if (!mime_type)
    {
        mime_type = vfs_mime_type_new(type);
        mime_map_lock.lock();
        mime_map.insert({mime_type->type, mime_type});
        mime_map_lock.unlock();
    }
    vfs_mime_type_ref(mime_type);
    return mime_type;
}

vfs::mime_type
vfs_mime_type_new(const char* type_name)
{
    vfs::mime_type mime_type = g_slice_new0(VFSMimeType);
    mime_type->type = ztd::strdup(type_name);
    mime_type->ref_inc();
    return mime_type;
}

void
vfs_mime_type_ref(vfs::mime_type mime_type)
{
    mime_type->ref_inc();
}

void
vfs_mime_type_unref(vfs::mime_type mime_type)
{
    mime_type->ref_dec();
    if (mime_type->ref_count() == 0)
    {
        free(mime_type->type);
        if (mime_type->big_icon)
            g_object_unref(mime_type->big_icon);
        if (mime_type->small_icon)
            g_object_unref(mime_type->small_icon);

        g_slice_free(VFSMimeType, mime_type);
    }
}

GdkPixbuf*
vfs_mime_type_get_icon(vfs::mime_type mime_type, bool big)
{
    i32 size;

    if (big)
    {
        if (mime_type->big_icon) /* big icon */
            return g_object_ref(mime_type->big_icon);
        size = big_icon_size;
    }
    else /* small icon */
    {
        if (mime_type->small_icon)
            return g_object_ref(mime_type->small_icon);
        size = small_icon_size;
    }

    GdkPixbuf* icon = nullptr;

    if (!strcmp(mime_type->type, XDG_MIME_TYPE_DIRECTORY))
    {
        icon = vfs_load_icon("gtk-directory", size);
        if (!icon)
            icon = vfs_load_icon("gnome-fs-directory", size);
        if (!icon)
            icon = vfs_load_icon("folder", size);
        if (big)
            mime_type->big_icon = icon;
        else
            mime_type->small_icon = icon;
        return icon ? g_object_ref(icon) : nullptr;
    }

    // get description and icon from freedesktop XML - these are fetched
    // together for performance.
    char* xml_icon = nullptr;
    char* xml_desc = mime_type_get_desc_icon(mime_type->type, nullptr, &xml_icon);
    if (xml_icon)
    {
        if (xml_icon[0])
            icon = vfs_load_icon(xml_icon, size);
        free(xml_icon);
    }
    if (xml_desc)
    {
        if (!mime_type->description && xml_desc[0])
            mime_type->description = xml_desc;
        else
            free(xml_desc);
    }
    if (!mime_type->description)
    {
        LOG_WARN("mime-type {} has no description (comment)", mime_type->type);
        vfs::mime_type vfs_mime = vfs_mime_type_get_from_type(XDG_MIME_TYPE_UNKNOWN);
        if (vfs_mime)
        {
            mime_type->description = ztd::strdup(vfs_mime_type_get_description(vfs_mime));
            vfs_mime_type_unref(vfs_mime);
        }
    }

    if (!icon)
    {
        // guess icon

        const auto mime_parts = ztd::partition(mime_type->type, "/");
        const std::string mime = mime_parts[0];
        const std::string type = mime_parts[2];

        if (ztd::contains(mime_type->type, "/"))
        {
            std::string icon_name;

            // convert mime-type foo/bar to foo-bar
            icon_name = ztd::replace(mime_type->type, "/", "-");

            // is there an icon named foo-bar?
            icon = vfs_load_icon(icon_name, size);
            if (!icon)
            {
                // maybe we can find a legacy icon named gnome-mime-foo-bar
                icon_name = fmt::format("gnome-mime-{}-{}", mime, type);
                icon = vfs_load_icon(icon_name, size);
            }

            // try gnome-mime-foo
            if (!icon)
            {
                icon_name = fmt::format("gnome-mime-{}", mime);
                icon = vfs_load_icon(icon_name, size);
            }

            // try foo-x-generic
            if (!icon)
            {
                icon_name = fmt::format("{}-x-generic", mime);
                icon = vfs_load_icon(icon_name, size);
            }
        }
    }

    if (!icon)
    {
        /* prevent endless recursion of XDG_MIME_TYPE_UNKNOWN */
        if (strcmp(mime_type->type, XDG_MIME_TYPE_UNKNOWN))
        {
            /* FIXME: fallback to icon of parent mime-type */
            vfs::mime_type unknown;
            unknown = vfs_mime_type_get_from_type(XDG_MIME_TYPE_UNKNOWN);
            icon = vfs_mime_type_get_icon(unknown, big);
            vfs_mime_type_unref(unknown);
        }
        else /* unknown */
        {
            icon = vfs_load_icon("unknown", size);
        }
    }

    if (big)
        mime_type->big_icon = icon;
    else
        mime_type->small_icon = icon;
    return icon ? g_object_ref(icon) : nullptr;
}

static void
free_cached_big_icons(vfs::mime_type mime_type)
{
    if (!mime_type->big_icon)
        return;
    g_object_unref(mime_type->big_icon);
    mime_type->big_icon = nullptr;
}

static void
free_cached_small_icons(vfs::mime_type mime_type)
{
    if (!mime_type->small_icon)
        return;
    g_object_unref(mime_type->small_icon);
    mime_type->small_icon = nullptr;
}

void
vfs_mime_type_set_icon_size_big(i32 size)
{
    if (size == big_icon_size)
        return;

    mime_map_lock.lock();
    big_icon_size = size;
    // Unload old cached icons
    std::ranges::for_each(mime_map,
                          [](const auto& mime)
                          {
                              free_cached_big_icons(mime.second);
                          });
    mime_map_lock.unlock();
}

void
vfs_mime_type_set_icon_size_small(i32 size)
{
    if (size == small_icon_size)
        return;

    mime_map_lock.lock();
    small_icon_size = size;
    // Unload old cached icons
    std::ranges::for_each(mime_map,
                          [](const auto& mime)
                          {
                              free_cached_small_icons(mime.second);
                          });
    mime_map_lock.unlock();
}

i32
vfs_mime_type_get_icon_size_big()
{
    return big_icon_size;
}

i32
vfs_mime_type_get_icon_size_small()
{
    return small_icon_size;
}

const char*
vfs_mime_type_get_type(vfs::mime_type mime_type)
{
    return mime_type->type;
}

/* Get human-readable description of mime type */
const char*
vfs_mime_type_get_description(vfs::mime_type mime_type)
{
    if (!mime_type->description)
    {
        mime_type->description = mime_type_get_desc_icon(mime_type->type, nullptr, nullptr);
        if (!mime_type->description || !*mime_type->description)
        {
            LOG_WARN("mime-type {} has no description (comment)", mime_type->type);
            vfs::mime_type vfs_mime = vfs_mime_type_get_from_type(XDG_MIME_TYPE_UNKNOWN);
            if (vfs_mime)
            {
                mime_type->description = ztd::strdup(vfs_mime_type_get_description(vfs_mime));
                vfs_mime_type_unref(vfs_mime);
            }
        }
    }
    return mime_type->description;
}

const std::vector<std::string>
vfs_mime_type_get_actions(vfs::mime_type mime_type)
{
    return mime_type_get_actions(mime_type->type);
}

char*
vfs_mime_type_get_default_action(vfs::mime_type mime_type)
{
    char* def = mime_type_get_default_action(mime_type->type);

    /* FIXME:
     * If default app is not set, choose one from all availble actions.
     * Is there any better way to do this?
     * Should we put this fallback handling here, or at API of higher level?
     */
    if (!def)
    {
        const std::vector<std::string> actions = mime_type_get_actions(mime_type->type);
        if (!actions.empty())
            def = ztd::strdup(actions.at(0));
    }
    return def;
}

/*
 * Set default app.desktop for specified file.
 * app can be the name of the desktop file or a command line.
 */
void
vfs_mime_type_set_default_action(vfs::mime_type mime_type, const char* desktop_id)
{
    char* cust_desktop = nullptr;
    /*
        if( ! ztd::endswith( desktop_id, ".desktop" ) )
            return;
    */
    vfs_mime_type_add_action(mime_type, desktop_id, &cust_desktop);
    if (cust_desktop)
        desktop_id = cust_desktop;
    mime_type_update_association(mime_type->type,
                                 desktop_id,
                                 MimeTypeAction::MIME_TYPE_ACTION_DEFAULT);
    free(cust_desktop);
}

void
vfs_mime_type_remove_action(vfs::mime_type mime_type, const char* desktop_id)
{
    mime_type_update_association(mime_type->type,
                                 desktop_id,
                                 MimeTypeAction::MIME_TYPE_ACTION_REMOVE);
}

/* If user-custom desktop file is created, it is returned in custom_desktop. */
void
vfs_mime_type_add_action(vfs::mime_type mime_type, const char* desktop_id, char** custom_desktop)
{
    // MOD  do not create custom desktop file if desktop_id is not a command
    if (!ztd::endswith(desktop_id, ".desktop"))
        mime_type_add_action(mime_type->type, desktop_id, custom_desktop);
    else if (custom_desktop) // sfm
        *custom_desktop = ztd::strdup(desktop_id);
}

GList*
vfs_mime_type_add_reload_cb(GFreeFunc cb, void* user_data)
{
    VFSMimeReloadCbEnt* ent = new VFSMimeReloadCbEnt(cb, user_data);
    reload_cb = g_list_append(reload_cb, ent);
    return g_list_last(reload_cb);
}

void
vfs_mime_type_remove_reload_cb(GList* cb)
{
    delete VFS_MIME_TYPE_CALLBACK_DATA(cb->data);
    reload_cb = g_list_delete_link(reload_cb, cb);
}

char*
vfs_mime_type_locate_desktop_file(const char* dir, const char* desktop_id)
{
    return mime_type_locate_desktop_file(dir, desktop_id);
}

void
vfs_mime_type_append_action(const char* type, const char* desktop_id)
{
    mime_type_update_association(type, desktop_id, MimeTypeAction::MIME_TYPE_ACTION_APPEND);
}

void
VFSMimeType::ref_inc()
{
    ++n_ref;
}

void
VFSMimeType::ref_dec()
{
    --n_ref;
}

u32
VFSMimeType::ref_count()
{
    return n_ref;
}
