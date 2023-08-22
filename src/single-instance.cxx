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

#include <format>

#include <filesystem>

#include <fstream>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "write.hxx"

#include "vfs/vfs-user-dirs.hxx"

#include "single-instance.hxx"

static const std::filesystem::path
get_pid_path() noexcept
{
    return vfs::user_dirs->runtime_dir() / std::format("{}.pid", PACKAGE_NAME);
}

static bool
is_process_running(pid_t pid) noexcept
{
    // could add another check here to make sure pid has
    // not been reissued in case of a stale pid file.
    return (::kill(pid, 0) == 0);
}

bool
single_instance_check() noexcept
{
    const auto pid_path = get_pid_path();

    std::ifstream pid_file(pid_path);
    if (pid_file)
    {
        pid_t pid;
        pid_file >> pid;
        pid_file.close();

        if (is_process_running(pid))
        {
            return false;
        }
    }

    // use std::to_string to avoid locale formating of pid
    // from '12345' -> '12,345'
    write_file(pid_path, std::to_string(::getpid()));

    return true;
}

void
single_instance_finalize() noexcept
{
    const auto pid_path = get_pid_path();
    if (std::filesystem::exists(pid_path))
    {
        std::filesystem::remove(pid_path);
    }
}
