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

#include "vfs/utils/permissions.hxx"

bool
vfs::utils::has_read_permission(const std::filesystem::path& path) noexcept
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
        return stat->mode().data() & S_IRUSR;
    }

    if (stat->gid() == gid)
    {
        return stat->mode().data() & S_IRGRP;
    }

    return stat->mode().data() & S_IROTH;
}

bool
vfs::utils::has_write_permission(const std::filesystem::path& path) noexcept
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
        return stat->mode().data() & S_IWUSR;
    }

    if (stat->gid() == gid)
    {
        return stat->mode().data() & S_IWGRP;
    }

    return stat->mode().data() & S_IWOTH;
}

bool
vfs::utils::has_execute_permission(const std::filesystem::path& path) noexcept
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
        return stat->mode().data() & S_IXUSR;
    }

    if (stat->gid() == gid)
    {
        return stat->mode().data() & S_IXGRP;
    }

    return stat->mode().data() & S_IXOTH;
}

bool
vfs::utils::check_directory_permissions(const std::filesystem::path& path) noexcept
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
