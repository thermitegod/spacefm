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

#include <filesystem>
#include <flat_map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "vfs/mime-type.hxx"
#if (GTK_MAJOR_VERSION == 3)
#include "vfs/settings.hxx"
#endif

#include "vfs/mime-type/mime-action.hxx"
#include "vfs/mime-type/mime-type.hxx"
#include "vfs/utils/icon.hxx"

#include "logger.hxx"

namespace global
{
static std::flat_map<std::string, std::shared_ptr<vfs::mime_type>> mime_map;
static std::mutex mime_map_lock;
} // namespace global

#if (GTK_MAJOR_VERSION == 4)
std::shared_ptr<vfs::mime_type>
vfs::mime_type::create(const std::string_view type) noexcept
#elif (GTK_MAJOR_VERSION == 3)
std::shared_ptr<vfs::mime_type>
vfs::mime_type::create(const std::string_view type,
                       const std::shared_ptr<vfs::settings>& settings) noexcept
#endif
{
    std::scoped_lock lock(global::mime_map_lock);
    if (global::mime_map.contains(type.data()))
    {
        return global::mime_map.at(type.data());
    }

#if (GTK_MAJOR_VERSION == 4)
    struct hack : public vfs::mime_type
    {
        hack(const std::string_view type) : mime_type(type) {}
    };

    const auto mime_type = std::make_shared<hack>(type);
#elif (GTK_MAJOR_VERSION == 3)
    struct hack : public vfs::mime_type
    {
        hack(const std::string_view type, const std::shared_ptr<vfs::settings>& settings)
            : mime_type(type, settings)
        {
        }
    };

    const auto mime_type = std::make_shared<hack>(type, settings);
#endif
    global::mime_map.insert({type.data(), mime_type});
    return mime_type;
}

#if (GTK_MAJOR_VERSION == 4)

std::shared_ptr<vfs::mime_type>
vfs::mime_type::create_from_file(const std::filesystem::path& path) noexcept
{
    return vfs::mime_type::create(vfs::detail::mime_type::get_by_file(path));
}

std::shared_ptr<vfs::mime_type>
vfs::mime_type::create_from_type(const std::string_view type) noexcept
{
    return vfs::mime_type::create(type);
}

#elif (GTK_MAJOR_VERSION == 3)

std::shared_ptr<vfs::mime_type>
vfs::mime_type::create_from_file(const std::filesystem::path& path,
                                 const std::shared_ptr<vfs::settings>& settings) noexcept
{
    return vfs::mime_type::create(vfs::detail::mime_type::get_by_file(path), settings);
}

std::shared_ptr<vfs::mime_type>
vfs::mime_type::create_from_type(const std::string_view type,
                                 const std::shared_ptr<vfs::settings>& settings) noexcept
{
    return vfs::mime_type::create(type, settings);
}

#endif

#if (GTK_MAJOR_VERSION == 4)
vfs::mime_type::mime_type(const std::string_view type) noexcept : type_(type)
#elif (GTK_MAJOR_VERSION == 3)
vfs::mime_type::mime_type(const std::string_view type,
                          const std::shared_ptr<vfs::settings>& settings) noexcept
    : type_(type), settings_(settings)
#endif
{
    const auto icon_data = vfs::detail::mime_type::get_desc_icon(type_);
    description_ = icon_data[1];
    if (description_.empty() && type_ != vfs::constants::mime_type::unknown)
    {
        logger::warn<logger::vfs>("mime-type {} has no description (comment)", type_);

#if (GTK_MAJOR_VERSION == 4)
        const auto mime_unknown =
            vfs::mime_type::create_from_type(vfs::constants::mime_type::unknown);
#elif (GTK_MAJOR_VERSION == 3)
        const auto mime_unknown =
            vfs::mime_type::create_from_type(vfs::constants::mime_type::unknown, settings);
#endif
        if (mime_unknown)
        {
            description_ = mime_unknown->description();
        }
    }
}

vfs::mime_type::~mime_type() noexcept
{
#if (GTK_MAJOR_VERSION == 3)
    if (icon_.big)
    {
        g_object_unref(icon_.big);
    }
    if (icon_.small)
    {
        g_object_unref(icon_.small);
    }
#endif
}

#if (GTK_MAJOR_VERSION == 4)

Glib::RefPtr<Gtk::IconPaintable>
vfs::mime_type::icon(const std::int32_t size) noexcept
{
    if (icons_.contains(size))
    {
        return icons_.at(size);
    }

    Glib::RefPtr<Gtk::IconPaintable> icon = nullptr;

    if (type_ == vfs::constants::mime_type::directory)
    {
        icon = vfs::utils::load_icon("folder", size);
        icons_.insert({size, icon});

        return icon;
    }

    // get description and icon from freedesktop XML - these are fetched
    // together for performance.
    const auto [mime_icon, mime_desc] = vfs::detail::mime_type::get_desc_icon(type_);

    if (!mime_icon.empty())
    {
        icon = vfs::utils::load_icon(mime_icon, size);
    }
    if (!mime_desc.empty())
    {
        if (description_.empty())
        {
            description_ = mime_desc;
        }
    }
    if (description_.empty())
    {
        logger::warn<logger::vfs>("mime-type {} has no description (comment)", type_);
#if (GTK_MAJOR_VERSION == 4)
        const auto vfs_mime = vfs::mime_type::create_from_type(vfs::constants::mime_type::unknown);
#elif (GTK_MAJOR_VERSION == 3)
        const auto vfs_mime =
            vfs::mime_type::create_from_type(vfs::constants::mime_type::unknown, settings_);
#endif
        if (vfs_mime)
        {
            description_ = vfs_mime->description();
        }
    }

    if (!icon)
    {
        // guess icon
        if (type_.contains('/'))
        {
            // convert mime-type foo/bar to foo-bar
            const std::string icon_name = ztd::replace(type_, "/", "-");

            // is there an icon named foo-bar?
            icon = vfs::utils::load_icon(icon_name, size);
            // fallback try foo-x-generic
            if (!icon)
            {
                const auto mime = ztd::partition(type_, "/")[0];
                const std::string generic_icon_name = std::format("{}-x-generic", mime);
                icon = vfs::utils::load_icon(generic_icon_name, size);
            }
        }
    }

    if (!icon)
    {
        /* prevent endless recursion of mime_type::type::unknown */
        if (type_ != vfs::constants::mime_type::unknown)
        {
            /* FIXME: fallback to icon of parent mime-type */
#if (GTK_MAJOR_VERSION == 4)
            const auto unknown =
                vfs::mime_type::create_from_type(vfs::constants::mime_type::unknown);
#elif (GTK_MAJOR_VERSION == 3)
            const auto unknown =
                vfs::mime_type::create_from_type(vfs::constants::mime_type::unknown, settings_);
#endif
            icon = unknown->icon(size);
        }
        else /* unknown */
        {
            icon = vfs::utils::load_icon("unknown", size);
        }
    }

    if (!icons_.contains(size))
    {
        icons_.insert({size, icon});
    }

    return icon;
}

#elif (GTK_MAJOR_VERSION == 3)

GdkPixbuf*
vfs::mime_type::icon(const bool big) noexcept
{
    ztd::panic_if(settings_ == nullptr, "Function disabled");

    i32 icon_size = 0;

    if (big)
    { // big icon
        if (icon_size_big_ != settings_->icon_size_big)
        { // big icon size has changed
            if (icon_.big)
            {
                g_object_unref(icon_.big);
                icon_.big = nullptr;
            }
        }
        if (icon_.big)
        {
            return g_object_ref(icon_.big);
        }
        icon_size_big_ = settings_->icon_size_big;
        icon_size = icon_size_big_;
    }
    else
    { // small icon
        if (icon_size_small_ != settings_->icon_size_small)
        { // small icon size has changed
            if (icon_.small)
            {
                g_object_unref(icon_.small);
                icon_.small = nullptr;
            }
        }
        if (icon_.small)
        {
            return g_object_ref(icon_.small);
        }
        icon_size_small_ = settings_->icon_size_small;
        icon_size = icon_size_small_;
    }

    GdkPixbuf* icon = nullptr;

    if (type_ == vfs::constants::mime_type::directory)
    {
        icon = vfs::utils::load_icon("folder", icon_size);
        if (big)
        {
            icon_.big = icon;
        }
        else
        {
            icon_.small = icon;
        }
        return icon ? g_object_ref(icon) : nullptr;
    }

    // get description and icon from freedesktop XML - these are fetched
    // together for performance.
    const auto [mime_icon, mime_desc] = vfs::detail::mime_type::get_desc_icon(type_);

    if (!mime_icon.empty())
    {
        icon = vfs::utils::load_icon(mime_icon, icon_size);
    }
    if (!mime_desc.empty())
    {
        if (description_.empty())
        {
            description_ = mime_desc;
        }
    }
    if (description_.empty())
    {
        logger::warn<logger::vfs>("mime-type {} has no description (comment)", type_);
        const auto vfs_mime =
            vfs::mime_type::create_from_type(vfs::constants::mime_type::unknown, settings_);
        if (vfs_mime)
        {
            description_ = vfs_mime->description();
        }
    }

    if (!icon)
    {
        // guess icon
        if (type_.contains('/'))
        {
            // convert mime-type foo/bar to foo-bar
            const std::string icon_name = ztd::replace(type_, "/", "-");

            // is there an icon named foo-bar?
            icon = vfs::utils::load_icon(icon_name, icon_size);
            // fallback try foo-x-generic
            if (!icon)
            {
                const auto mime = ztd::partition(type_, "/")[0];
                const std::string generic_icon_name = std::format("{}-x-generic", mime);
                icon = vfs::utils::load_icon(generic_icon_name, icon_size);
            }
        }
    }

    if (!icon)
    {
        /* prevent endless recursion of mime_type::type::unknown */
        if (type_ != vfs::constants::mime_type::unknown)
        {
            /* FIXME: fallback to icon of parent mime-type */
            const auto unknown =
                vfs::mime_type::create_from_type(vfs::constants::mime_type::unknown, settings_);
            icon = unknown->icon(big);
        }
        else /* unknown */
        {
            icon = vfs::utils::load_icon("unknown", icon_size);
        }
    }

    if (big)
    {
        icon_.big = icon;
    }
    else
    {
        icon_.small = icon;
    }
    return icon ? g_object_ref(icon) : nullptr;
}

#endif

std::string_view
vfs::mime_type::type() const noexcept
{
    return type_;
}

/* Get human-readable description of mime type */
std::string_view
vfs::mime_type::description() const noexcept
{
    return description_;
}

std::vector<std::string>
vfs::mime_type::actions() const noexcept
{
    return vfs::detail::mime_type::get_actions(type_);
}

std::optional<std::string>
vfs::mime_type::default_action() const noexcept
{
    auto def = vfs::detail::mime_type::get_default_action(type_);

    /* FIXME:
     * If default app is not set, choose one from all availble actions.
     * Is there any better way to do this?
     * Should we put this fallback handling here, or at API of higher level?
     */
    if (def)
    {
        return def;
    }

    const std::vector<std::string> actions = vfs::detail::mime_type::get_actions(type_);
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
    if (!desktop_id.ends_with(".desktop"))
    {
        logger::error<logger::vfs>("Setting default action requires a desktop file");
        return;
    }

    const auto custom_desktop = add_action(desktop_id);

    vfs::detail::mime_type::set_default_action(type_,
                                               custom_desktop.empty() ? desktop_id
                                                                      : custom_desktop);
}

/* If user-custom desktop file is created, it is returned in custom_desktop. */
std::string
vfs::mime_type::add_action(const std::string_view desktop_id) const noexcept
{
    // do not create custom desktop file if desktop_id is not a command
    if (!desktop_id.ends_with(".desktop"))
    {
        return vfs::detail::mime_type::add_action(type_, desktop_id);
    }
    return desktop_id.data();
}

bool
vfs::mime_type::is_archive() const noexcept
{
    return vfs::detail::mime_type::is_archive(type_);
}

bool
vfs::mime_type::is_executable() const noexcept
{
    return vfs::detail::mime_type::is_executable(type_);
}

bool
vfs::mime_type::is_text() const noexcept
{
    return vfs::detail::mime_type::is_text(type_);
}

bool
vfs::mime_type::is_image() const noexcept
{
    return vfs::detail::mime_type::is_image(type_);
}

bool
vfs::mime_type::is_video() const noexcept
{
    return vfs::detail::mime_type::is_video(type_);
}

bool
vfs::mime_type::is_audio() const noexcept
{
    return vfs::detail::mime_type::is_audio(type_);
}

std::optional<std::filesystem::path>
vfs::mime_type_locate_desktop_file(const std::string_view desktop_id) noexcept
{
    return vfs::detail::mime_type::locate_desktop_file(desktop_id);
}

std::optional<std::filesystem::path>
vfs::mime_type_locate_desktop_file(const std::filesystem::path& dir,
                                   const std::string_view desktop_id) noexcept
{
    return vfs::detail::mime_type::locate_desktop_file(dir, desktop_id);
}
