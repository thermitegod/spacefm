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

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "logger.hxx"

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

template<typename T>
[[nodiscard]] sync_data
command_line_sync(const T& command) noexcept
    requires std::convertible_to<T, std::string_view> ||
             std::same_as<std::remove_cvref_t<T>, std::filesystem::path>
{
    logger::info<logger::execute>("{}", command);

    std::string cmd;
    if constexpr (std::same_as<std::decay_t<decltype(command)>, std::filesystem::path>)
    {
        cmd = command.string();
    }
    else
    {
        cmd = std::string(command);
    }

    sync_data data;
    Glib::spawn_command_line_sync(cmd.c_str(),
                                  &data.standard_output,
                                  &data.standard_error,
                                  &data.exit_status);

    return data;
}

template<typename... Args>
[[nodiscard]] sync_data
command_line_sync(std::format_string<Args...> fmt, Args&&... args) noexcept
{
    return command_line_sync(std::format(fmt, std::forward<Args>(args)...));
}

template<typename T>
void
command_line_async(const T& command) noexcept
    requires std::convertible_to<T, std::string_view> ||
             std::same_as<std::remove_cvref_t<T>, std::filesystem::path>
{
    logger::info<logger::execute>("{}", command);

    std::string cmd;
    if constexpr (std::same_as<std::decay_t<decltype(command)>, std::filesystem::path>)
    {
        cmd = command.string();
    }
    else
    {
        cmd = std::string(command);
    }

    Glib::spawn_command_line_async(cmd.c_str());
}

template<typename... Args>
void
command_line_async(std::format_string<Args...> fmt, Args&&... args) noexcept
{
    command_line_async(std::format(fmt, std::forward<Args>(args)...));
}
} // namespace vfs::execute
