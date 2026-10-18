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
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "vfs/mime-action.hxx"
#include "vfs/mime-type.hxx"

#include "vfs/utils/icon.hxx"

#include "logger.hxx"

namespace
{
std::shared_mutex mime_cache_mutex;
std::unordered_map<std::string, std::shared_ptr<vfs::mime_type>> mime_cache;
} // namespace

std::shared_ptr<vfs::mime_type>
vfs::mime_type::create(std::string_view type) noexcept
{
    {
        std::shared_lock lock(mime_cache_mutex);
        const auto it = mime_cache.find(type.data());
        if (it != mime_cache.cend())
        {
            // logger::trace<logger::vfs>("vfs::mime_type({})  cache   {}", logger::utils::ptr(it->second), type);
            return it->second;
        }
    }

    struct hack : public vfs::mime_type
    {
        hack(std::string_view type) : mime_type(type) {}
    };
    const auto mime_type = std::make_shared<hack>(type);

    std::unique_lock lock(mime_cache_mutex);
    const auto [it, _] = mime_cache.try_emplace(type.data(), mime_type);
    // logger::trace<logger::vfs>("vfs::mime_type({})  new     {}", logger::utils::ptr(it->second), type);
    return it->second;
}

std::shared_ptr<vfs::mime_type>
vfs::mime_type::create_from_file(const std::filesystem::path& path) noexcept
{
    [[maybe_unused]] bool uncertain;
    auto type = Gio::content_type_guess(path, nullptr, 0, uncertain);

    return vfs::mime_type::create(type.raw());
}

std::shared_ptr<vfs::mime_type>
vfs::mime_type::create_from_type(std::string_view type) noexcept
{
    return vfs::mime_type::create(type);
}

vfs::mime_type::mime_type(std::string_view type) noexcept
    : type_(type), description_(Gio::content_type_get_description(type_)),
      icon_(Gio::content_type_get_icon(type_))
{
    //
}

Glib::RefPtr<Gtk::IconPaintable>
vfs::mime_type::icon(const std::int32_t size) const noexcept
{
    return vfs::utils::load_icon(icon_, size);
}

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
    return vfs::mime::get_actions(type_);
}

std::optional<std::string>
vfs::mime_type::default_action() const noexcept
{
    auto def = vfs::mime::get_default_action(type_);

    /* FIXME:
     * If default app is not set, choose one from all availble actions.
     * Is there any better way to do this?
     * Should we put this fallback handling here, or at API of higher level?
     */
    if (def)
    {
        return def;
    }

    const std::vector<std::string> actions = vfs::mime::get_actions(type_);
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
vfs::mime_type::set_default_action(std::string_view desktop_id) const noexcept
{
    if (!desktop_id.ends_with(".desktop"))
    {
        logger::error<logger::vfs>("Setting default action requires a desktop file");
        return;
    }

    const auto custom_desktop = add_action(desktop_id);

    vfs::mime::set_default_action(type_, custom_desktop.empty() ? desktop_id : custom_desktop);
}

/* If user-custom desktop file is created, it is returned in custom_desktop. */
std::string
vfs::mime_type::add_action(std::string_view desktop_id) const noexcept
{
    // do not create custom desktop file if desktop_id is not a command
    if (!desktop_id.ends_with(".desktop"))
    {
        return vfs::mime::add_action(type_, desktop_id);
    }
    return desktop_id.data();
}

bool
vfs::mime_type::is_unknown() const noexcept
{
    return type_ == vfs::mime_type::unknown;
}

bool
vfs::mime_type::is_archive() const noexcept
{
    // Taken from file-roller .desktop file
    static constexpr std::array<std::string_view, 67> archive_mime_types{
        "application/bzip2",
        "application/gzip",
        "application/vnd.android.package-archive",
        "application/vnd.ms-cab-compressed",
        "application/vnd.debian.binary-package",
        "application/vnd.rar",
        "application/x-7z-compressed",
        "application/x-7z-compressed-tar",
        "application/x-ace",
        "application/x-alz",
        "application/x-apple-diskimage",
        "application/x-ar",
        "application/x-archive",
        "application/x-arj",
        "application/x-brotli",
        "application/x-bzip-brotli-tar",
        "application/x-bzip",
        "application/x-bzip-compressed-tar",
        "application/x-bzip1",
        "application/x-bzip1-compressed-tar",
        "application/x-bzip3",
        "application/x-bzip3-compressed-tar",
        "application/x-cabinet",
        "application/x-cd-image",
        "application/x-compress",
        "application/x-compressed-tar",
        "application/x-cpio",
        "application/x-chrome-extension",
        "application/x-deb",
        "application/x-ear",
        "application/x-ms-dos-executable",
        "application/x-gtar",
        "application/x-gzip",
        "application/x-gzpostscript",
        "application/x-java-archive",
        "application/x-lha",
        "application/x-lhz",
        "application/x-lrzip",
        "application/x-lrzip-compressed-tar",
        "application/x-lz4",
        "application/x-lzip",
        "application/x-lzip-compressed-tar",
        "application/x-lzma",
        "application/x-lzma-compressed-tar",
        "application/x-lzop",
        "application/x-lz4-compressed-tar",
        "application/x-ms-wim",
        "application/x-rar",
        "application/x-rar-compressed",
        "application/x-rpm",
        "application/x-source-rpm",
        "application/x-rzip",
        "application/x-rzip-compressed-tar",
        "application/x-tar",
        "application/x-tarz",
        "application/x-tzo",
        "application/x-stuffit",
        "application/x-war",
        "application/x-xar",
        "application/x-xz",
        "application/x-xz-compressed-tar",
        "application/x-zip",
        "application/x-zip-compressed",
        "application/x-zstd-compressed-tar",
        "application/x-zoo",
        "application/zip",
        "application/zstd",
    };

    return std::ranges::contains(archive_mime_types, type_);
}

bool
vfs::mime_type::is_executable() const noexcept
{
    return Gio::content_type_can_be_executable(type_) || type_ == vfs::mime_type::executable ||
           type_ == "application/x-shellscript";
}

bool
vfs::mime_type::is_text() const noexcept
{
    return Gio::content_type_is_a(type_, "text/plain");
}

bool
vfs::mime_type::is_image() const noexcept
{
    return type_.starts_with("image/");
}

bool
vfs::mime_type::is_video() const noexcept
{
    return type_.starts_with("video/");
}

bool
vfs::mime_type::is_audio() const noexcept
{
    return type_.starts_with("audio/");
}

bool
vfs::mime_type::is_media() const noexcept
{
    return is_image() || is_video() || is_audio();
}
