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

#include <type_traits>
#include <system_error>

#include <cstdint>

namespace vfs
{
enum class error_code : std::int32_t
{
    none, // no error
    parse_error,
    // Key errors
    key_not_found,
    unknown_key,
    missing_key,
    // File errors
    file_not_found,
    file_too_large,
    file_open_failure,
    file_read_failure,
    file_write_failure,
    file_close_failure,
};

const std::error_category& error_category() noexcept;

inline std::error_code
make_error_code(vfs::error_code e) noexcept
{
    return std::error_code{static_cast<int>(e), error_category()};
}
} // namespace vfs

template<> struct std::is_error_code_enum<vfs::error_code> : public std::true_type
{
};
