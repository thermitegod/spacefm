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

#include <format>
#include <string>
#include <string_view>

#include <cstdint>

namespace vfs::execute
{
struct sync_data final
{
    std::int32_t exit_status = 0;
    std::string standard_output;
    std::string standard_error;
};
[[nodiscard]] sync_data command_line_sync(const std::string_view command) noexcept;

template<typename... Args>
[[nodiscard]] sync_data
command_line_sync(std::format_string<Args...> fmt, Args&&... args) noexcept
{
    return command_line_sync(std::format(fmt, std::forward<Args>(args)...));
}

void command_line_async(const std::string_view command) noexcept;

template<typename... Args>
void
command_line_async(std::format_string<Args...> fmt, Args&&... args) noexcept
{
    command_line_async(std::format(fmt, std::forward<Args>(args)...));
}
} // namespace vfs::execute
