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

#include <expected>
#include <format>
#include <optional>
#include <print>
#include <string>
#include <string_view>

#include <glibmm.h>

#include <glaze/glaze.hpp>

#include "utils/shell-quote.hxx"

#include "logger.hxx"

namespace datatype
{
template<typename Response = std::optional<std::monostate>>
std::expected<Response, std::string>
run_dialog_sync(const std::string_view program, const auto& request) noexcept
{
#if defined(HAVE_DEV)
    const auto binary = std::format("{}/{}", DIALOG_BUILD_ROOT, program);
#else
    const auto binary = Glib::find_program_in_path(program.data());
#endif

    if (binary.empty())
    {
        logger::error("Failed to find dialog binary: {}", program);
        return std::unexpected("");
    }

    const auto buffer = glz::write_json(request);
    if (!buffer)
    {
        logger::error("Failed to create JSON: {}", glz::format_error(buffer));
        return std::unexpected("");
    }

    // std::println(R"({} --json '{}')", binary, buffer.value())
    const auto command = std::format(R"({} --json {})", binary, utils::shell_quote(buffer.value()));

    std::int32_t exit_status = 0;
    std::string standard_output;
    Glib::spawn_command_line_sync(command, &standard_output, nullptr, &exit_status);

#if defined(HAVE_DEV) && __has_feature(address_sanitizer)
    if (standard_output.empty())
#else
    if (exit_status != 0 || standard_output.empty())
#endif
    {
        return std::unexpected("");
    }

    if constexpr (std::is_same_v<Response, std::optional<std::monostate>>)
    {
        return {};
    }

    const auto response = glz::read_json<Response>(standard_output);
    if (!response)
    {
        logger::error("Failed to decode JSON: {}",
                      glz::format_error(response.error(), standard_output));
        return std::unexpected("");
    }

    return response.value();
}

inline void
run_dialog_async(const std::string_view program, const auto& request) noexcept
{
#if defined(HAVE_DEV)
    const auto binary = std::format("{}/{}", DIALOG_BUILD_ROOT, program);
#else
    const auto binary = Glib::find_program_in_path(program.data());
#endif

    if (binary.empty())
    {
        logger::error("Failed to find dialog binary: {}", program);
        return;
    }

    const auto buffer = glz::write_json(request);
    if (!buffer)
    {
        logger::error("Failed to create JSON: {}", glz::format_error(buffer));
        return;
    }

    // std::println(R"({} --json '{}')", binary, buffer.value())
    const auto command = std::format(R"({} --json {})", binary, utils::shell_quote(buffer.value()));

    Glib::spawn_command_line_async(command);
}

inline void
run_dialog_async(const std::string_view program) noexcept
{
#if defined(HAVE_DEV)
    const auto binary = std::format("{}/{}", DIALOG_BUILD_ROOT, program);
#else
    const auto binary = Glib::find_program_in_path(program.data());
#endif

    if (binary.empty())
    {
        logger::error("Failed to find dialog binary: {}", program);
        return;
    }

    // std::println(R"({})", binary)
    Glib::spawn_command_line_async(binary);
}
} // namespace datatype
