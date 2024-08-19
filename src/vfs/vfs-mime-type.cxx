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

#include <unordered_map>

#include <mutex>

#include <optional>

#include <memory>

#include <gtkmm.h>
#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "logger.hxx"

#include "settings/settings.hxx"

#include "vfs/mime-type/mime-action.hxx"
#include "vfs/mime-type/mime-type.hxx"

#include "vfs/utils/vfs-utils.hxx"

#include "vfs/vfs-mime-type.hxx"

namespace global
{
std::unordered_map<std::string, std::shared_ptr<vfs::mime_type>> mime_map;
std::mutex mime_map_lock;
} // namespace global

const std::shared_ptr<vfs::mime_type>
vfs::mime_type::create(const std::string_view type) noexcept
{
    const std::unique_lock<std::mutex> lock(global::mime_map_lock);
    if (global::mime_map.contains(type.data()))
    {
        return global::mime_map.at(type.data());
    }

    const auto mime_type = std::make_shared<vfs::mime_type>(type);
    global::mime_map.insert({type.data(), mime_type});
    return mime_type;
}

const std::shared_ptr<vfs::mime_type>
vfs::mime_type::create_from_file(const std::filesystem::path& path) noexcept
{
    return vfs::mime_type::create_from_type(vfs::detail::mime_type::get_by_file(path));
}

const std::shared_ptr<vfs::mime_type>
vfs::mime_type::create_from_type(const std::string_view type) noexcept
{
    return vfs::mime_type::create(type);
}

vfs::mime_type::mime_type(const std::string_view type) noexcept : type_(type)
{
    const auto icon_data = vfs::detail::mime_type::get_desc_icon(this->type_);
    this->description_ = icon_data[1];
    if (this->description_.empty() && this->type_ != vfs::constants::mime_type::unknown)
    {
        logger::warn<logger::domain::vfs>("mime-type {} has no description (comment)", this->type_);
        const auto mime_unknown =
            vfs::mime_type::create_from_type(vfs::constants::mime_type::unknown);
        if (mime_unknown)
        {
            this->description_ = mime_unknown->description();
        }
    }
}

vfs::mime_type::~mime_type() noexcept
{
    if (this->big_icon_)
    {
        g_object_unref(this->big_icon_);
    }
    if (this->small_icon_)
    {
        g_object_unref(this->small_icon_);
    }
}

GdkPixbuf*
vfs::mime_type::icon(const bool big) noexcept
{
    i32 icon_size = 0;

    if (big)
    { // big icon
        if (this->icon_size_big_ != config::settings.icon_size_big)
        { // big icon size has changed
            if (this->big_icon_)
            {
                g_object_unref(this->big_icon_);
                this->big_icon_ = nullptr;
            }
        }
        if (this->big_icon_)
        {
            return g_object_ref(this->big_icon_);
        }
        this->icon_size_big_ = config::settings.icon_size_big;
        icon_size = this->icon_size_big_;
    }
    else
    { // small icon
        if (this->icon_size_small_ != config::settings.icon_size_small)
        { // small icon size has changed
            if (this->small_icon_)
            {
                g_object_unref(this->small_icon_);
                this->small_icon_ = nullptr;
            }
        }
        if (this->small_icon_)
        {
            return g_object_ref(this->small_icon_);
        }
        this->icon_size_small_ = config::settings.icon_size_small;
        icon_size = this->icon_size_small_;
    }

    GdkPixbuf* icon = nullptr;

    if (this->type_ == vfs::constants::mime_type::directory)
    {
        icon = vfs::utils::load_icon("folder", icon_size);
        if (big)
        {
            this->big_icon_ = icon;
        }
        else
        {
            this->small_icon_ = icon;
        }
        return icon ? g_object_ref(icon) : nullptr;
    }

    // get description and icon from freedesktop XML - these are fetched
    // together for performance.
    const auto [mime_icon, mime_desc] = vfs::detail::mime_type::get_desc_icon(this->type_);

    if (!mime_icon.empty())
    {
        icon = vfs::utils::load_icon(mime_icon, icon_size);
    }
    if (!mime_desc.empty())
    {
        if (this->description_.empty())
        {
            this->description_ = mime_desc;
        }
    }
    if (this->description_.empty())
    {
        logger::warn<logger::domain::vfs>("mime-type {} has no description (comment)", this->type_);
        const auto vfs_mime = vfs::mime_type::create_from_type(vfs::constants::mime_type::unknown);
        if (vfs_mime)
        {
            this->description_ = vfs_mime->description();
        }
    }

    if (!icon)
    {
        // guess icon
        if (this->type_.contains('/'))
        {
            // convert mime-type foo/bar to foo-bar
            const std::string icon_name = ztd::replace(this->type_, "/", "-");

            // is there an icon named foo-bar?
            icon = vfs::utils::load_icon(icon_name, icon_size);
            // fallback try foo-x-generic
            if (!icon)
            {
                const auto mime = ztd::partition(this->type_, "/")[0];
                const std::string generic_icon_name = std::format("{}-x-generic", mime);
                icon = vfs::utils::load_icon(generic_icon_name, icon_size);
            }
        }
    }

    if (!icon)
    {
        /* prevent endless recursion of mime_type::type::unknown */
        if (this->type_ != vfs::constants::mime_type::unknown)
        {
            /* FIXME: fallback to icon of parent mime-type */
            const auto unknown =
                vfs::mime_type::create_from_type(vfs::constants::mime_type::unknown);
            icon = unknown->icon(big);
        }
        else /* unknown */
        {
            icon = vfs::utils::load_icon("unknown", icon_size);
        }
    }

    if (big)
    {
        this->big_icon_ = icon;
    }
    else
    {
        this->small_icon_ = icon;
    }
    return icon ? g_object_ref(icon) : nullptr;
}

const std::string_view
vfs::mime_type::type() const noexcept
{
    return this->type_;
}

/* Get human-readable description of mime type */
const std::string_view
vfs::mime_type::description() const noexcept
{
    return this->description_;
}

const std::vector<std::string>
vfs::mime_type::actions() const noexcept
{
    return vfs::detail::mime_type::get_actions(this->type_);
}

const std::optional<std::string>
vfs::mime_type::default_action() const noexcept
{
    auto def = vfs::detail::mime_type::get_default_action(this->type_);

    /* FIXME:
     * If default app is not set, choose one from all availble actions.
     * Is there any better way to do this?
     * Should we put this fallback handling here, or at API of higher level?
     */
    if (def)
    {
        return def;
    }

    const std::vector<std::string> actions = vfs::detail::mime_type::get_actions(this->type_);
    if (!actions.empty())
    {
        return actions.at(0).data();
    }
    return std::nullopt;
}

/*
 * Set default app.desktop for specified file.
 * app can be the name of the desktop file or a command line.
 */
void
vfs::mime_type::set_default_action(const std::string_view desktop_id) const noexcept
{
    const auto custom_desktop = this->add_action(desktop_id);

    vfs::detail::mime_type::set_default_action(this->type_,
                                               custom_desktop.empty() ? desktop_id
                                                                      : custom_desktop);
}

/* If user-custom desktop file is created, it is returned in custom_desktop. */
const std::string
vfs::mime_type::add_action(const std::string_view desktop_id) const noexcept
{
    // do not create custom desktop file if desktop_id is not a command
    if (!desktop_id.ends_with(".desktop"))
    {
        return vfs::detail::mime_type::add_action(this->type_, desktop_id);
    }
    return desktop_id.data();
}

bool
vfs::mime_type::is_archive() const noexcept
{
    return vfs::detail::mime_type::is_archive(this->type_);
}

bool
vfs::mime_type::is_executable() const noexcept
{
    return vfs::detail::mime_type::is_executable(this->type_);
}

bool
vfs::mime_type::is_text() const noexcept
{
    return vfs::detail::mime_type::is_text(this->type_);
}

bool
vfs::mime_type::is_image() const noexcept
{
    return vfs::detail::mime_type::is_image(this->type_);
}

bool
vfs::mime_type::is_video() const noexcept
{
    return vfs::detail::mime_type::is_video(this->type_);
}

bool
vfs::mime_type::is_audio() const noexcept
{
    return vfs::detail::mime_type::is_audio(this->type_);
}

const std::optional<std::filesystem::path>
vfs::mime_type_locate_desktop_file(const std::string_view desktop_id) noexcept
{
    return vfs::detail::mime_type::locate_desktop_file(desktop_id);
}

const std::optional<std::filesystem::path>
vfs::mime_type_locate_desktop_file(const std::filesystem::path& dir,
                                   const std::string_view desktop_id) noexcept
{
    return vfs::detail::mime_type::locate_desktop_file(dir, desktop_id);
}
