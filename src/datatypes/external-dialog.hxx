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
#include <string>
#include <string_view>

#include <glibmm.h>

#include <glaze/glaze.hpp>

#include <ztd/extra/glaze.hxx>
#include <ztd/ztd.hxx>

#include "utils/shell-quote.hxx"

#include "vfs/execute.hxx"

#include "logger.hxx"
#include "package.hxx"

namespace datatype
{
template<typename Response = std::optional<std::monostate>>
std::expected<Response, std::string>
run_dialog_sync(const std::string_view program, const auto& request) noexcept
{
#if defined(DEV_MODE)
    const auto binary = std::format("{}/{}", spacefm::package.dialog.build_root, program);
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

    const auto result = vfs::execute::command_line_sync(R"({} --json {})",
                                                        binary,
                                                        utils::shell_quote(buffer.value()));

#if defined(DEV_MODE) && __has_feature(address_sanitizer)
    if (result.standard_output.empty())
#else
    if (result.exit_status != 0 || result.standard_output.empty())
#endif
    {
        return std::unexpected("");
    }

    if constexpr (std::is_same_v<Response, std::optional<std::monostate>>)
    {
        return {};
    }

    const auto response = glz::read_json<Response>(result.standard_output);
    if (!response)
    {
        logger::error("Failed to decode JSON: {}",
                      glz::format_error(response.error(), result.standard_output));
        return std::unexpected("");
    }

    return response.value();
}

inline void
run_dialog_async(const std::string_view program, const auto& request) noexcept
{
#if defined(DEV_MODE)
    const auto binary = std::format("{}/{}", spacefm::package.dialog.build_root, program);
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

    vfs::execute::command_line_async(R"({} --json {})", binary, utils::shell_quote(buffer.value()));
}

inline void
run_dialog_async(const std::string_view program) noexcept
{
#if defined(DEV_MODE)
    const auto binary = std::format("{}/{}", spacefm::package.dialog.build_root, program);
#else
    const auto binary = Glib::find_program_in_path(program.data());
#endif

    if (binary.empty())
    {
        logger::error("Failed to find dialog binary: {}", program);
        return;
    }

    vfs::execute::command_line_async(binary);
}
} // namespace datatype
