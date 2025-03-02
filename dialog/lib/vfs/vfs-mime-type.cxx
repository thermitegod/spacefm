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
#include <string>
#include <string_view>
#include <unordered_map>

#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "vfs/mime-type/mime-type.hxx"
#include "vfs/vfs-mime-type.hxx"

#include "logger.hxx"

namespace global
{
static std::unordered_map<std::string, std::shared_ptr<vfs::mime_type>> mime_map;
static std::mutex mime_map_lock;
} // namespace global

std::shared_ptr<vfs::mime_type>
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

vfs::mime_type::~mime_type() noexcept {}

std::string_view
vfs::mime_type::type() const noexcept
{
    return this->type_;
}

/* Get human-readable description of mime type */
std::string_view
vfs::mime_type::description() const noexcept
{
    return this->description_;
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
