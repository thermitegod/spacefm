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
#include <optional>
#include <string>
#include <string_view>

#include <ztd/ztd.hxx>

#include "vfs/linux/sysfs.hxx"
#include "vfs/utils/file-ops.hxx"

std::optional<std::string>
vfs::linux::sysfs::get_string(const std::filesystem::path& dir,
                              const std::string_view attribute) noexcept
{
    if (std::filesystem::exists(dir / attribute))
    {
        const auto buffer = vfs::utils::read_file(dir / attribute);
        if (buffer)
        {
            return *buffer;
        }
    }
    return std::nullopt;
}

std::optional<i64>
vfs::linux::sysfs::get_i64(const std::filesystem::path& dir,
                           const std::string_view attribute) noexcept
{
    const auto buffer = get_string(dir, attribute);
    if (buffer)
    {
        const auto result = i64::create(*buffer);
        if (result)
        {
            return result.value();
        }
    }
    return std::nullopt;
}

std::optional<u64>
vfs::linux::sysfs::get_u64(const std::filesystem::path& dir,
                           const std::string_view attribute) noexcept
{
    const auto buffer = get_string(dir, attribute);
    if (buffer)
    {
        const auto result = u64::create(*buffer);
        if (result)
        {
            return result.value();
        }
    }
    return std::nullopt;
}
