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
#include <format>
#include <string>

#include <csignal>

#include <ztd/ztd.hxx>

#include "vfs/user-dirs.hxx"

#include "vfs/utils/file-ops.hxx"

#include "logger.hxx"
#include "single-instance.hxx"

static std::filesystem::path
pid_path() noexcept
{
    return vfs::user::runtime() / std::format("{}.pid", PACKAGE_NAME);
}

static bool
is_process_running(const pid_t pid) noexcept
{
    // could add another check here to make sure pid has
    // not been reissued in case of a stale pid file.
    return (kill(pid, 0) == 0);
}

static void
single_instance_finalize() noexcept
{
    const auto path = pid_path();
    if (std::filesystem::exists(path))
    {
        std::filesystem::remove(path);
    }
}

bool
single_instance_check() noexcept
{
    const auto path = pid_path();

    if (std::filesystem::exists(path))
    {
        const auto buffer = vfs::utils::read_file(path);
        if (!buffer)
        {
            logger::critical("Failed to read pid file: {} {}",
                             path.string(),
                             buffer.error().message());
            return true;
        }

        const auto pid = ztd::from_string<pid_t>(*buffer);

        if (is_process_running(pid.value()))
        {
            return false;
        }
    }

    std::atexit(single_instance_finalize);

    const auto ec = vfs::utils::write_file(path, std::format("{}", ::getpid()));
    logger::critical_if(bool(ec), "Failed to write pid file: {} {}", path.string(), ec.message());

    return true;
}
