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

#include <memory>
#include <string>
#include <string_view>

#include <filesystem>

#include <utility>
#include <vector>

#include <map>

#include <algorithm>
#include <ranges>

#include <thread>
#include <mutex>
#include <chrono>

#include <memory>

#include <glibmm.h>

#include <gtk/gtk.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "mime-type/mime-action.hxx"
#include "mime-type/mime-type.hxx"

#include "vfs/vfs-mime-type.hxx"
#include "vfs/vfs-file-monitor.hxx"

#include "vfs/vfs-dir.hxx"

#include "vfs/vfs-utils.hxx"

static std::map<std::string, vfs::mime_type> mime_map;
std::mutex mime_map_lock;

static i32 big_icon_size{32};
static i32 small_icon_size{16};

static std::vector<vfs::file_monitor> mime_caches_monitors;

vfs::mime_type
vfs_mime_type_new(const std::string_view type_name)
{
    auto mime_type = std::make_shared<VFSMimeType>(type_name);
    return mime_type;
}

vfs::mime_type
vfs_mime_type_get_from_file(const std::filesystem::path& file_path)
{
    const std::string type = mime_type_get_by_file(file_path);
    return vfs_mime_type_get_from_type(type);
}

vfs::mime_type
vfs_mime_type_get_from_type(const std::string_view type)
{
    std::unique_lock<std::mutex> lock(mime_map_lock);

    if (mime_map.contains(type.data()))
    {
        return mime_map.at(type.data());
    }

    vfs::mime_type mime_type = vfs_mime_type_new(type);
    mime_map.insert({mime_type->get_type(), mime_type});

    return mime_type;
}

static bool
vfs_mime_type_reload()
{
    std::unique_lock<std::mutex> lock(mime_map_lock);
    mime_map.clear();
    // ztd::logger::debug("reload mime-types");
    vfs_dir_mime_type_reload();
    mime_type_regen_all_caches();
    return false;
}

static void
on_mime_cache_changed(const vfs::file_monitor& monitor, VFSFileMonitorEvent event,
                      const std::filesystem::path& file_name, void* user_data)
{
    (void)monitor;
    (void)event;
    (void)file_name;
    (void)user_data;

    // ztd::logger::debug("reloading all mime caches");
    std::jthread idle_thread(
        []()
        {
            vfs_mime_type_reload();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        });

    idle_thread.join();
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
vfs_mime_type_finalize()
{
    // remove file alteration monitor for mime-cache
    const auto action_remove = [](const vfs::file_monitor& monitor)
    { vfs_file_monitor_remove(monitor, on_mime_cache_changed, nullptr); };
    std::ranges::for_each(mime_caches_monitors, action_remove);

    mime_type_finalize();

    mime_map.clear();
}

/////////////////////////////////////

VFSMimeType::VFSMimeType(const std::string_view type_name) : type(type_name)
{
}

VFSMimeType::~VFSMimeType()
{
    if (this->big_icon)
    {
        g_object_unref(this->big_icon);
    }
    if (this->small_icon)
    {
        g_object_unref(this->small_icon);
    }
}

GdkPixbuf*
VFSMimeType::get_icon(bool big) noexcept
{
    i32 icon_size;

    if (big)
    {
        if (this->big_icon)
        { /* big icon */
            return g_object_ref(this->big_icon);
        }
        icon_size = big_icon_size;
    }
    else /* small icon */
    {
        if (this->small_icon)
        {
            return g_object_ref(this->small_icon);
        }
        icon_size = small_icon_size;
    }

    GdkPixbuf* icon = nullptr;

    if (ztd::same(this->type, XDG_MIME_TYPE_DIRECTORY))
    {
        icon = vfs_load_icon("gtk-directory", icon_size);
        if (!icon)
        {
            icon = vfs_load_icon("gnome-fs-directory", icon_size);
        }
        if (!icon)
        {
            icon = vfs_load_icon("folder", icon_size);
        }
        if (big)
        {
            this->big_icon = icon;
        }
        else
        {
            this->small_icon = icon;
        }
        return icon ? g_object_ref(icon) : nullptr;
    }

    // get description and icon from freedesktop XML - these are fetched
    // together for performance.
    const auto [mime_icon, mime_desc] = mime_type_get_desc_icon(this->type);

    if (!mime_icon.empty())
    {
        icon = vfs_load_icon(mime_icon, icon_size);
    }
    if (!mime_desc.empty())
    {
        if (this->description.empty())
        {
            this->description = mime_desc;
        }
    }
    if (this->description.empty())
    {
        ztd::logger::warn("mime-type {} has no description (comment)", this->type);
        vfs::mime_type vfs_mime = vfs_mime_type_get_from_type(XDG_MIME_TYPE_UNKNOWN);
        if (vfs_mime)
        {
            this->description = vfs_mime->get_description();
        }
    }

    if (!icon)
    {
        // guess icon
        const auto mime_parts = ztd::partition(this->type, "/");
        const std::string& split_mime = mime_parts[0];
        const std::string& split_type = mime_parts[2];

        if (ztd::contains(this->type, "/"))
        {
            // convert mime-type foo/bar to foo-bar
            std::string icon_name = ztd::replace(this->type, "/", "-");

            // is there an icon named foo-bar?
            icon = vfs_load_icon(icon_name, icon_size);
            if (!icon)
            {
                // maybe we can find a legacy icon named gnome-mime-foo-bar
                icon_name = std::format("gnome-mime-{}-{}", split_mime, split_type);
                icon = vfs_load_icon(icon_name, icon_size);
            }

            // try gnome-mime-foo
            if (!icon)
            {
                icon_name = std::format("gnome-mime-{}", split_mime);
                icon = vfs_load_icon(icon_name, icon_size);
            }

            // try foo-x-generic
            if (!icon)
            {
                icon_name = std::format("{}-x-generic", split_mime);
                icon = vfs_load_icon(icon_name, icon_size);
            }
        }
    }

    if (!icon)
    {
        /* prevent endless recursion of XDG_MIME_TYPE_UNKNOWN */
        if (!ztd::same(this->type, XDG_MIME_TYPE_UNKNOWN))
        {
            /* FIXME: fallback to icon of parent mime-type */
            vfs::mime_type unknown = vfs_mime_type_get_from_type(XDG_MIME_TYPE_UNKNOWN);
            icon = unknown->get_icon(big);
        }
        else /* unknown */
        {
            icon = vfs_load_icon("unknown", icon_size);
        }
    }

    if (big)
    {
        this->big_icon = icon;
    }
    else
    {
        this->small_icon = icon;
    }
    return icon ? g_object_ref(icon) : nullptr;
}

const std::string
VFSMimeType::get_type() const noexcept
{
    return this->type;
}

/* Get human-readable description of mime type */
const std::string
VFSMimeType::get_description() noexcept
{
    if (this->description.empty())
    {
        const auto icon_data = mime_type_get_desc_icon(this->type);
        this->description = icon_data[1];
        if (this->description.empty())
        {
            ztd::logger::warn("mime-type {} has no description (comment)", this->type);
            vfs::mime_type vfs_mime = vfs_mime_type_get_from_type(XDG_MIME_TYPE_UNKNOWN);
            if (vfs_mime)
            {
                this->description = vfs_mime->get_description();
            }
        }
    }
    return this->description;
}

const std::vector<std::string>
VFSMimeType::get_actions() const noexcept
{
    return mime_type_get_actions(this->type);
}

const std::string
VFSMimeType::get_default_action() const noexcept
{
    const char* def = mime_type_get_default_action(this->type);

    /* FIXME:
     * If default app is not set, choose one from all availble actions.
     * Is there any better way to do this?
     * Should we put this fallback handling here, or at API of higher level?
     */
    if (def != nullptr)
    {
        return def;
    }

    const std::vector<std::string> actions = mime_type_get_actions(this->type);
    if (!actions.empty())
    {
        return actions.at(0).data();
    }
    return "";
}

/*
 * Set default app.desktop for specified file.
 * app can be the name of the desktop file or a command line.
 */
void
VFSMimeType::set_default_action(const std::string_view desktop_id) noexcept
{
    const std::string custom_desktop = this->add_action(desktop_id);

    mime_type_update_association(this->type,
                                 custom_desktop.empty() ? desktop_id : custom_desktop,
                                 MimeTypeAction::DEFAULT);
}

void
VFSMimeType::remove_action(const std::string_view desktop_id) noexcept
{
    mime_type_update_association(this->type, desktop_id, MimeTypeAction::REMOVE);
}

/* If user-custom desktop file is created, it is returned in custom_desktop. */
const std::string
VFSMimeType::add_action(const std::string_view desktop_id) noexcept
{
    // MOD  do not create custom desktop file if desktop_id is not a command
    if (!ztd::endswith(desktop_id, ".desktop"))
    {
        return mime_type_add_action(this->type, desktop_id);
    }
    return desktop_id.data();
}

void
VFSMimeType::free_cached_big_icons() noexcept
{
    if (!this->big_icon)
    {
        return;
    }
    g_object_unref(this->big_icon);
    this->big_icon = nullptr;
}

void
VFSMimeType::free_cached_small_icons() noexcept
{
    if (!this->small_icon)
    {
        return;
    }
    g_object_unref(this->small_icon);
    this->small_icon = nullptr;
}

void
vfs_mime_type_set_icon_size_big(i32 size)
{
    std::unique_lock<std::mutex> lock(mime_map_lock);

    if (size == big_icon_size)
    {
        return;
    }

    big_icon_size = size;
    // Unload old cached icons
    const auto action = [](const auto& mime) { mime.second->free_cached_big_icons(); };
    std::ranges::for_each(mime_map, action);
}

void
vfs_mime_type_set_icon_size_small(i32 size)
{
    std::unique_lock<std::mutex> lock(mime_map_lock);

    if (size == small_icon_size)
    {
        return;
    }

    small_icon_size = size;
    // Unload old cached icons
    const auto action = [](const auto& mime) { mime.second->free_cached_small_icons(); };
    std::ranges::for_each(mime_map, action);
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
vfs_mime_type_locate_desktop_file(const std::string_view desktop_id)
{
    return mime_type_locate_desktop_file(desktop_id);
}

const char*
vfs_mime_type_locate_desktop_file(const std::filesystem::path& dir,
                                  const std::string_view desktop_id)
{
    return mime_type_locate_desktop_file(dir, desktop_id);
}

void
vfs_mime_type_append_action(const std::string_view type, const std::string_view desktop_id)
{
    mime_type_update_association(type, desktop_id, MimeTypeAction::APPEND);
}
