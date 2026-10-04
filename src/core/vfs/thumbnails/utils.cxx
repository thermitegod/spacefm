/**
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
#include <utility>

#include <cstdint>

#include "vfs/user-dirs.hxx"

#include "vfs/thumbnails/utils.hxx"

vfs::thumbnail::thumbnail_size
vfs::thumbnail::get_spec_size(const std::int32_t size) noexcept
{
    if (size <= 128)
    {
        return thumbnail_size::normal;
    }
    else if (size <= 256)
    {
        return thumbnail_size::large;
    }
    else if (size <= 512)
    {
        return thumbnail_size::x_large;
    }
    else if (size <= 1024)
    {
        return thumbnail_size::xx_large;
    }
    else
    {
        std::unreachable();
    }
}

std::filesystem::path
vfs::thumbnail::get_cache_path(const thumbnail_size size) noexcept
{
    switch (size)
    {
        case thumbnail_size::normal:
            return cache_normal();
        case thumbnail_size::large:
            return cache_large();
        case thumbnail_size::x_large:
            return cache_x_large();
        case thumbnail_size::xx_large:
            return cache_xx_large();
        default:
            std::unreachable();
    }
}

std::filesystem::path
vfs::thumbnail::cache_parent() noexcept
{
    return vfs::user::cache() / "thumbnails";
}

std::filesystem::path
vfs::thumbnail::cache_normal() noexcept
{
    return vfs::user::cache() / "thumbnails" / "normal";
}

std::filesystem::path
vfs::thumbnail::cache_large() noexcept
{
    return vfs::user::cache() / "thumbnails" / "large";
}

std::filesystem::path
vfs::thumbnail::cache_x_large() noexcept
{
    return vfs::user::cache() / "thumbnails" / "x-large";
}

std::filesystem::path
vfs::thumbnail::cache_xx_large() noexcept
{
    return vfs::user::cache() / "thumbnails" / "xx-large";
}

std::filesystem::path
vfs::thumbnail::cache_fail() noexcept
{
    return vfs::user::cache() / "thumbnails" / "fail" /
           std::format("{}-{}", PACKAGE_NAME, PACKAGE_VERSION);
}

vfs::thumbnail::cache_paths
vfs::thumbnail::thumbnail_cache() noexcept
{
    static const auto data = cache_paths{
        .normal = cache_normal(),
        .large = cache_large(),
        .x_large = cache_x_large(),
        .xx_large = cache_xx_large(),
        .fail = cache_fail(),
    };
    return data;
}
