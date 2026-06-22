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

#include <concepts>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>

#include <cstdint>

#include <ztd/ztd.hxx>

namespace vfs::execute
{
/**
 * @brief add quotes and escape existing quotes in a string,
 *        other special shell characters do not need to be escaped.
 * @param[in] input The string like object to be quoted
 * @return a quoted string, if string is empty returns empty quotes
 */
template<typename T>
[[nodiscard]] std::string
quote(const T& input) noexcept
    requires std::convertible_to<T, std::string_view> ||
             std::same_as<std::remove_cvref_t<T>, std::filesystem::path>
{
    std::string_view str;
    if constexpr (std::same_as<std::decay_t<decltype(input)>, std::filesystem::path>)
    {
        str = input.native();
    }
    else
    {
        str = input;
    }

    if (str.empty())
    {
        return R"("")";
    }
    return std::format(R"("{}")", ztd::replace(str, "\"", "\\\""));
}

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
