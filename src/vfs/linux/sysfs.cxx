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

#include <fmt/format.h>

#include <glibmm.h>
#include <glibmm/convert.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/linux/sysfs.hxx"

const std::optional<std::string>
vfs::linux::sysfs::get_string(std::string_view dir, std::string_view attribute)
{
    const std::string filename = Glib::build_filename(dir.data(), attribute.data());

    std::string contents;
    try
    {
        contents = Glib::file_get_contents(filename);
    }
    catch (Glib::FileError)
    {
        return std::nullopt;
    }
    return contents;
}

const std::optional<i64>
vfs::linux::sysfs::get_i64(std::string_view dir, std::string_view attribute)
{
    const std::string filename = Glib::build_filename(dir.data(), attribute.data());

    std::string contents;
    try
    {
        contents = Glib::file_get_contents(filename);
    }
    catch (Glib::FileError)
    {
        return std::nullopt;
    }
    return std::stoul(contents);
}

const std::optional<u64>
vfs::linux::sysfs::get_u64(std::string_view dir, std::string_view attribute)
{
    const std::string filename = Glib::build_filename(dir.data(), attribute.data());

    std::string contents;
    try
    {
        contents = Glib::file_get_contents(filename);
    }
    catch (Glib::FileError)
    {
        return std::nullopt;
    }
    return std::stoll(contents);
}

const std::optional<f64>
vfs::linux::sysfs::get_f64(std::string_view dir, std::string_view attribute)
{
    const std::string filename = Glib::build_filename(dir.data(), attribute.data());

    std::string contents;
    try
    {
        contents = Glib::file_get_contents(filename);
    }
    catch (Glib::FileError)
    {
        return std::nullopt;
    }
    return std::stod(contents);
}

bool
vfs::linux::sysfs::file_exists(std::string_view dir, std::string_view attribute)
{
    return std::filesystem::exists(Glib::build_filename(dir.data(), attribute.data()));
}

const std::optional<std::string>
vfs::linux::sysfs::resolve_link(std::string_view path, std::string_view name)
{
    const std::string full_path = Glib::build_filename(path.data(), name.data());

    try
    {
        return std::filesystem::read_symlink(full_path);
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        return std::nullopt;
    }
}
