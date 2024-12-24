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

#include "utils/misc.hxx"

bool
utils::have_x_access(const std::filesystem::path& path) noexcept
{
    const auto status = std::filesystem::status(path);

    return ((status.permissions() & std::filesystem::perms::owner_exec) !=
                std::filesystem::perms::none ||
            (status.permissions() & std::filesystem::perms::group_exec) !=
                std::filesystem::perms::none ||
            (status.permissions() & std::filesystem::perms::others_exec) !=
                std::filesystem::perms::none);
}

bool
utils::have_rw_access(const std::filesystem::path& path) noexcept
{
    const auto status = std::filesystem::status(path);

    return ((status.permissions() & std::filesystem::perms::owner_read) !=
                std::filesystem::perms::none &&
            (status.permissions() & std::filesystem::perms::owner_write) !=
                std::filesystem::perms::none) ||

           ((status.permissions() & std::filesystem::perms::group_read) !=
                std::filesystem::perms::none &&
            (status.permissions() & std::filesystem::perms::group_write) !=
                std::filesystem::perms::none) ||

           ((status.permissions() & std::filesystem::perms::others_read) !=
                std::filesystem::perms::none &&
            (status.permissions() & std::filesystem::perms::others_write) !=
                std::filesystem::perms::none);
}
