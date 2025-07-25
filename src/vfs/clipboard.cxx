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

#include <format>
#include <optional>
#include <string>
#include <string_view>

#include <glibmm.h>

#include <glaze/glaze.hpp>

#include <ztd/extra/glaze.hxx>
#include <ztd/ztd.hxx>

#include "vfs/clipboard.hxx"
#include "vfs/execute.hxx"

#include "logger.hxx"

bool
vfs::clipboard::is_valid() noexcept
{
    auto data = vfs::clipboard::get();

    return data != std::nullopt;
}

void
vfs::clipboard::clear() noexcept
{
    const auto binary = Glib::find_program_in_path("wl-copy");
    if (binary.empty())
    {
        logger::error("Failed to find wl-copy");
        return;
    }

    vfs::execute::command_line_async(R"({} -c)", binary);
}

void
vfs::clipboard::set(const clipboard_data& data) noexcept
{
    const auto buffer = glz::write_json(data);
    if (!buffer)
    {
        logger::error("Failed to create JSON: {}", glz::format_error(buffer));
        return;
    }

    vfs::clipboard::set_text(buffer.value());
}

void
vfs::clipboard::set_text(const std::string_view data) noexcept
{
    const auto binary = Glib::find_program_in_path("wl-copy");
    if (binary.empty())
    {
        logger::error("Failed to find wl-copy");
        return;
    }

    vfs::execute::command_line_async(R"({} -- {})", binary, vfs::execute::quote(data));
}

std::optional<vfs::clipboard::clipboard_data>
vfs::clipboard::get() noexcept
{
    const auto text = vfs::clipboard::get_text();
    if (!text)
    {
        return std::nullopt;
    }

    const auto response = glz::read_json<clipboard_data>(text.value());
    if (!response)
    {
        return std::nullopt;
    }

    return response.value();
}

std::optional<std::string>
vfs::clipboard::get_text() noexcept
{
    const auto binary = Glib::find_program_in_path("wl-paste");
    if (binary.empty())
    {
        logger::error("Failed to find wl-paste");
        return std::nullopt;
    }

    const auto result = vfs::execute::command_line_sync(R"({} --no-newline)", binary);

#if defined(DEV_MODE) && __has_feature(address_sanitizer)
    if (result.standard_output.empty())
#else
    if (result.exit_status != 0 || result.standard_output.empty())
#endif
    {
        return std::nullopt;
    }

    return result.standard_output;
}
