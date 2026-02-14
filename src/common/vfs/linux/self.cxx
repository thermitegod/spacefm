/**
 * Copyright (C) 2023 Brandon Zorn <brandonzorn@cock.li>
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
#include <string>

#include "vfs/linux/self.hxx"

[[nodiscard]] std::filesystem::path
vfs::linux::proc::self::exe() noexcept
{
    return std::filesystem::read_symlink(detail::proc_self_exe);
}

[[nodiscard]] std::string
vfs::linux::proc::self::name() noexcept
{
    return std::filesystem::read_symlink(detail::proc_self_exe).filename();
}
