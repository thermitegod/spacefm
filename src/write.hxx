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

#pragma once

#include <string>
#include <string_view>

#include <filesystem>

#include <iostream>
#include <fstream>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

template<class T>
bool
write_file(const std::filesystem::path& path, const T& data)
{
    std::ofstream file(path);
    if (!file.is_open())
    {
        ztd::logger::error("Failed to open file: {}", path.string());
        return false;
    }

    file << data;

    if (file.fail())
    {
        ztd::logger::error("Failed to write file: {}", path.string());
        file.close();
        return false;
    }

    file.close();

    if (file.fail())
    {
        ztd::logger::error("Failed to close file: {}", path.string());
        return false;
    }

    return true;
}
