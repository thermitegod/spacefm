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

#include <iostream>
#include <fstream>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

template<class T>
bool
write_file(std::string_view path, const T data)
{
    std::ofstream file(path.data());
    if (file.is_open())
    {
        file << data;
    }

    const bool result = file.good();
    if (!result)
    {
        ztd::logger::error("Failed to write file: {}", path);
    }
    return result;
}
