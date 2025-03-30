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

#include <ztd/ztd.hxx>

#include "utils/permissions.hxx"

bool
utils::has_read_permission(const std::filesystem::path& path) noexcept
{
    if (!std::filesystem::exists(path))
    {
        return false;
    }

    const auto stat = ztd::stat::create(path);
    if (!stat)
    {
        return false;
    }

    const auto uid = getuid();
    const auto gid = getgid();

    if (stat->uid() == uid)
    {
        return stat->mode() & S_IRUSR;
    }

    if (stat->gid() == gid)
    {
        return stat->mode() & S_IRGRP;
    }

    return stat->mode() & S_IROTH;
}

bool
utils::has_write_permission(const std::filesystem::path& path) noexcept
{
    if (!std::filesystem::exists(path))
    {
        return false;
    }

    const auto stat = ztd::stat::create(path);
    if (!stat)
    {
        return false;
    }

    const auto uid = getuid();
    const auto gid = getgid();

    if (stat->uid() == uid)
    {
        return stat->mode() & S_IWUSR;
    }

    if (stat->gid() == gid)
    {
        return stat->mode() & S_IWGRP;
    }

    return stat->mode() & S_IWOTH;
}

bool
utils::has_execute_permission(const std::filesystem::path& path) noexcept
{
    if (!std::filesystem::exists(path))
    {
        return false;
    }

    const auto stat = ztd::stat::create(path);
    if (!stat)
    {
        return false;
    }

    const auto uid = getuid();
    const auto gid = getgid();

    if (stat->uid() == uid)
    {
        return stat->mode() & S_IXUSR;
    }

    if (stat->gid() == gid)
    {
        return stat->mode() & S_IXGRP;
    }

    return stat->mode() & S_IXOTH;
}

bool
utils::check_directory_permissions(const std::filesystem::path& path) noexcept
{
    if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path))
    {
        return false;
    }

    if (!has_read_permission(path))
    {
        return false;
    }

    std::filesystem::path parent = path.parent_path();
    while (parent != "/")
    {
        if (!has_execute_permission(parent))
        {
            return false;
        }
        parent = parent.parent_path();
    }
    return true;
}
