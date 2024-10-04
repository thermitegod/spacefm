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

#include <string>
#include <string_view>

#include <filesystem>

#include <optional>

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "vfs/linux/sysfs.hxx"

std::optional<std::string>
vfs::linux::sysfs::get_string(const std::filesystem::path& dir,
                              const std::string_view attribute) noexcept
{
    const auto filename = dir / attribute;

    std::string contents;
    try
    {
        contents = Glib::file_get_contents(filename);
    }
    catch (const Glib::FileError& e)
    {
        return std::nullopt;
    }
    return contents;
}

std::optional<i64>
vfs::linux::sysfs::get_i64(const std::filesystem::path& dir,
                           const std::string_view attribute) noexcept
{
    const auto filename = dir / attribute;

    std::string contents;
    try
    {
        contents = Glib::file_get_contents(filename);
    }
    catch (const Glib::FileError& e)
    {
        return std::nullopt;
    }
    return std::stoul(contents);
}

std::optional<u64>
vfs::linux::sysfs::get_u64(const std::filesystem::path& dir,
                           const std::string_view attribute) noexcept
{
    const auto filename = dir / attribute;

    std::string contents;
    try
    {
        contents = Glib::file_get_contents(filename);
    }
    catch (const Glib::FileError& e)
    {
        return std::nullopt;
    }
    return std::stoll(contents);
}

std::optional<f64>
vfs::linux::sysfs::get_f64(const std::filesystem::path& dir,
                           const std::string_view attribute) noexcept
{
    const auto filename = dir / attribute;

    std::string contents;
    try
    {
        contents = Glib::file_get_contents(filename);
    }
    catch (const Glib::FileError& e)
    {
        return std::nullopt;
    }
    return std::stod(contents);
}

bool
vfs::linux::sysfs::file_exists(const std::filesystem::path& dir,
                               const std::string_view attribute) noexcept
{
    return std::filesystem::exists(dir / attribute);
}

std::optional<std::string>
vfs::linux::sysfs::resolve_link(const std::filesystem::path& path,
                                const std::string_view name) noexcept
{
    const auto full_path = path / name;

    try
    {
        return std::filesystem::read_symlink(full_path);
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        return std::nullopt;
    }
}
