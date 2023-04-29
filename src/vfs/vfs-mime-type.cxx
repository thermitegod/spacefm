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

#include "mime-type/mime-action.hxx"
#include "mime-type/mime-type.hxx"

#include "vfs/vfs-mime-type.hxx"
#include "vfs/vfs-file-monitor.hxx"

#include "vfs/vfs-utils.hxx"

#define VFS_MIME_TYPE_CALLBACK_DATA(obj) (static_cast<VFSMimeReloadCbEnt*>(obj))

static std::map<std::string, vfs::mime_type> mime_map;
std::mutex mime_map_lock;

static u32 reload_callback_id = 0;
static GList* reload_cb = nullptr;

static i32 big_icon_size{32};
static i32 small_icon_size{16};

static std::vector<vfs::file_monitor> mime_caches_monitors;

struct VFSMimeReloadCbEnt
{
    VFSMimeReloadCbEnt(GFreeFunc cb, void* user_data);

    GFreeFunc cb{nullptr};
    void* user_data{nullptr};
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
    const auto action = [](const auto& mime) { vfs_mime_type_unref(mime.second); };
    std::ranges::for_each(mime_map, action);
    mime_map.clear();
    mime_map_lock.unlock();

    g_source_remove(reload_callback_id);
    reload_callback_id = 0;

    // ztd::logger::debug("reload mime-types");

    mime_type_regen_all_caches();

    return false;
}

static void
on_mime_cache_changed(vfs::file_monitor monitor, VFSFileMonitorEvent event,
                      std::string_view file_name, void* user_data)
{
    (void)monitor;
    (void)event;
    (void)file_name;
    (void)user_data;

    // ztd::logger::debug("reloading all mime caches");
    if (reload_callback_id == 0)
    {
        reload_callback_id = g_idle_add((GSourceFunc)vfs_mime_type_reload, nullptr);
    }
}

void
vfs_mime_type_init()
{
    mime_type_init();

    /* install file alteration monitor for mime-cache */
    for (const mime_cache_t& cache : mime_type_get_caches())
    {
        // MOD NOTE1  check to see if path exists - otherwise it later tries to
        //  remove nullptr monitor with inotify which caused segfault
        if (!std::filesystem::exists(cache->get_file_path()))
        {
            continue;
        }

        const vfs::file_monitor monitor =
            vfs_file_monitor_add(cache->get_file_path(), on_mime_cache_changed, nullptr);

        mime_caches_monitors.emplace_back(monitor);
    }
}

void
vfs_mime_type_clean()
{
    // remove file alteration monitor for mime-cache
    const auto action_remove = [](vfs::file_monitor monitor)
    { vfs_file_monitor_remove(monitor, on_mime_cache_changed, nullptr); };
    std::ranges::for_each(mime_caches_monitors, action_remove);

    mime_type_finalize();

    const auto action_unref = [](const auto& mime) { vfs_mime_type_unref(mime.second); };
    std::ranges::for_each(mime_map, action_unref);

    mime_map.clear();
}

vfs::mime_type
vfs_mime_type_get_from_file(std::string_view file_path)
{
    const std::string type = mime_type_get_by_file(file_path);
    return vfs_mime_type_get_from_type(type);
}

vfs::mime_type
vfs_mime_type_get_from_type(std::string_view type)
{
    mime_map_lock.lock();
    vfs::mime_type mime_type = nullptr;
    try
    {
        mime_type = mime_map.at(type.data());
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
vfs_mime_type_new(std::string_view type_name)
{
    const auto mime_type = new VFSMimeType();
    mime_type->type = type_name.data();
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
        if (mime_type->big_icon)
        {
            g_object_unref(mime_type->big_icon);
        }
        if (mime_type->small_icon)
        {
            g_object_unref(mime_type->small_icon);
        }

        delete mime_type;
    }
}

GdkPixbuf*
vfs_mime_type_get_icon(vfs::mime_type mime_type, bool big)
{
    i32 size;

    if (big)
    {
        if (mime_type->big_icon)
        { /* big icon */
            return g_object_ref(mime_type->big_icon);
        }
        size = big_icon_size;
    }
    else /* small icon */
    {
        if (mime_type->small_icon)
        {
            return g_object_ref(mime_type->small_icon);
        }
        size = small_icon_size;
    }

    GdkPixbuf* icon = nullptr;

    if (ztd::same(mime_type->type, XDG_MIME_TYPE_DIRECTORY))
    {
        icon = vfs_load_icon("gtk-directory", size);
        if (!icon)
        {
            icon = vfs_load_icon("gnome-fs-directory", size);
        }
        if (!icon)
        {
            icon = vfs_load_icon("folder", size);
        }
        if (big)
        {
            mime_type->big_icon = icon;
        }
        else
        {
            mime_type->small_icon = icon;
        }
        return icon ? g_object_ref(icon) : nullptr;
    }

    // get description and icon from freedesktop XML - these are fetched
    // together for performance.
    const auto [mime_icon, mime_desc] = mime_type_get_desc_icon(mime_type->type);

    if (!mime_icon.empty())
    {
        icon = vfs_load_icon(mime_icon, size);
    }
    if (!mime_desc.empty())
    {
        if (mime_type->description.empty())
        {
            mime_type->description = mime_desc;
        }
    }
    if (mime_type->description.empty())
    {
        ztd::logger::warn("mime-type {} has no description (comment)", mime_type->type);
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
            // convert mime-type foo/bar to foo-bar
            std::string icon_name = ztd::replace(mime_type->type, "/", "-");

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
        if (!ztd::same(mime_type->type, XDG_MIME_TYPE_UNKNOWN))
        {
            /* FIXME: fallback to icon of parent mime-type */
            vfs::mime_type unknown = vfs_mime_type_get_from_type(XDG_MIME_TYPE_UNKNOWN);
            icon = vfs_mime_type_get_icon(unknown, big);
            vfs_mime_type_unref(unknown);
        }
        else /* unknown */
        {
            icon = vfs_load_icon("unknown", size);
        }
    }

    if (big)
    {
        mime_type->big_icon = icon;
    }
    else
    {
        mime_type->small_icon = icon;
    }
    return icon ? g_object_ref(icon) : nullptr;
}

static void
free_cached_big_icons(vfs::mime_type mime_type)
{
    if (!mime_type->big_icon)
    {
        return;
    }
    g_object_unref(mime_type->big_icon);
    mime_type->big_icon = nullptr;
}

static void
free_cached_small_icons(vfs::mime_type mime_type)
{
    if (!mime_type->small_icon)
    {
        return;
    }
    g_object_unref(mime_type->small_icon);
    mime_type->small_icon = nullptr;
}

void
vfs_mime_type_set_icon_size_big(i32 size)
{
    if (size == big_icon_size)
    {
        return;
    }

    mime_map_lock.lock();
    big_icon_size = size;
    // Unload old cached icons
    const auto action = [](const auto& mime) { free_cached_big_icons(mime.second); };
    std::ranges::for_each(mime_map, action);
    mime_map_lock.unlock();
}

void
vfs_mime_type_set_icon_size_small(i32 size)
{
    if (size == small_icon_size)
    {
        return;
    }

    mime_map_lock.lock();
    small_icon_size = size;
    // Unload old cached icons
    const auto action = [](const auto& mime) { free_cached_small_icons(mime.second); };
    std::ranges::for_each(mime_map, action);
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
    return mime_type->type.data();
}

/* Get human-readable description of mime type */
const char*
vfs_mime_type_get_description(vfs::mime_type mime_type)
{
    if (mime_type->description.empty())
    {
        const auto icon_data = mime_type_get_desc_icon(mime_type->type);
        mime_type->description = icon_data[1];
        if (mime_type->description.empty())
        {
            ztd::logger::warn("mime-type {} has no description (comment)", mime_type->type);
            vfs::mime_type vfs_mime = vfs_mime_type_get_from_type(XDG_MIME_TYPE_UNKNOWN);
            if (vfs_mime)
            {
                mime_type->description = ztd::strdup(vfs_mime_type_get_description(vfs_mime));
                vfs_mime_type_unref(vfs_mime);
            }
        }
    }
    return mime_type->description.data();
}

const std::vector<std::string>
vfs_mime_type_get_actions(vfs::mime_type mime_type)
{
    return mime_type_get_actions(mime_type->type);
}

const char*
vfs_mime_type_get_default_action(vfs::mime_type mime_type)
{
    const char* def = mime_type_get_default_action(mime_type->type);

    /* FIXME:
     * If default app is not set, choose one from all availble actions.
     * Is there any better way to do this?
     * Should we put this fallback handling here, or at API of higher level?
     */
    if (!def)
    {
        const std::vector<std::string> actions = mime_type_get_actions(mime_type->type);
        if (!actions.empty())
        {
            def = ztd::strdup(actions.at(0));
        }
    }
    return def;
}

/*
 * Set default app.desktop for specified file.
 * app can be the name of the desktop file or a command line.
 */
void
vfs_mime_type_set_default_action(vfs::mime_type mime_type, std::string_view desktop_id)
{
    const std::string custom_desktop = vfs_mime_type_add_action(mime_type, desktop_id);

    mime_type_update_association(mime_type->type,
                                 custom_desktop.empty() ? desktop_id : custom_desktop,
                                 MimeTypeAction::DEFAULT);
}

void
vfs_mime_type_remove_action(vfs::mime_type mime_type, std::string_view desktop_id)
{
    mime_type_update_association(mime_type->type, desktop_id, MimeTypeAction::REMOVE);
}

/* If user-custom desktop file is created, it is returned in custom_desktop. */
const std::string
vfs_mime_type_add_action(vfs::mime_type mime_type, std::string_view desktop_id)
{
    // MOD  do not create custom desktop file if desktop_id is not a command
    if (!ztd::endswith(desktop_id, ".desktop"))
    {
        return mime_type_add_action(mime_type->type, desktop_id);
    }
    return desktop_id.data();
}

GList*
vfs_mime_type_add_reload_cb(GFreeFunc cb, void* user_data)
{
    const auto ent = new VFSMimeReloadCbEnt(cb, user_data);
    reload_cb = g_list_append(reload_cb, ent);
    return g_list_last(reload_cb);
}

void
vfs_mime_type_remove_reload_cb(GList* cb)
{
    delete VFS_MIME_TYPE_CALLBACK_DATA(cb->data);
    reload_cb = g_list_delete_link(reload_cb, cb);
}

const char*
vfs_mime_type_locate_desktop_file(std::string_view desktop_id)
{
    return mime_type_locate_desktop_file(desktop_id);
}

const char*
vfs_mime_type_locate_desktop_file(std::string_view dir, std::string_view desktop_id)
{
    return mime_type_locate_desktop_file(dir, desktop_id);
}

void
vfs_mime_type_append_action(std::string_view type, std::string_view desktop_id)
{
    mime_type_update_association(type, desktop_id, MimeTypeAction::APPEND);
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
