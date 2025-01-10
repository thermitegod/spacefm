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

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct TerminalHandler
{
    TerminalHandler() = delete;
    TerminalHandler(const std::string_view name, const std::string_view exec) noexcept;

    std::string name;
    std::filesystem::path path;
    std::string exec;
};

struct TerminalHandlers
{
    TerminalHandlers() noexcept;

    std::vector<std::string> get_terminal_args(const std::string_view terminal) const noexcept;
    std::vector<std::string> get_supported_terminal_names() const noexcept;

  private:
    std::unordered_map<std::string, TerminalHandler> handlers;
};

using terminal_handlers_t = std::unique_ptr<TerminalHandlers>;

extern const terminal_handlers_t terminal_handlers;
