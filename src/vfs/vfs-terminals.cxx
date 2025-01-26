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

#include <algorithm>
#include <expected>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "vfs/vfs-error.hxx"
#include "vfs/vfs-terminals.hxx"

namespace
{
struct TerminalHandler final
{
    std::string name;
    std::string exec;
};
const std::array<TerminalHandler, 22> handlers{
    // clang-format off
    TerminalHandler{"alacritty",      "-e"},
    TerminalHandler{"aterm",          "-e"},
    TerminalHandler{"Eterm",          "-e"},
    TerminalHandler{"ghostty",        "-e"},
    TerminalHandler{"gnome-terminal", "-x"},
    TerminalHandler{"kitty",          ""},
    TerminalHandler{"Konsole",        "-e"},
    TerminalHandler{"lxterminal",     "-e"},
    TerminalHandler{"mlterm",         "-e"},
    TerminalHandler{"mrxvt",          "-e"},
    TerminalHandler{"qterminal",      "-e"},
    TerminalHandler{"rxvt",           "-e"},
    TerminalHandler{"sakura",         "-x"},
    TerminalHandler{"st",             "-e"},
    TerminalHandler{"tabby",          "-e"},
    TerminalHandler{"terminal",       "--disable-server"},
    TerminalHandler{"terminator",     "-x"},
    TerminalHandler{"terminology",    "-e"},
    TerminalHandler{"tilix",          "-e"},
    TerminalHandler{"urxvt",          "-e"},
    TerminalHandler{"xfce4-terminal", "-x"},
    TerminalHandler{"xterm",          "-e"},
    // clang-format on
};
} // namespace

std::expected<std::string, std::error_code>
vfs::terminals::create_command(const std::string_view terminal,
                               const std::string_view command) noexcept
{
    const auto* const it =
        std::ranges::find_if(handlers,
                             [terminal](const auto& term) { return term.name == terminal; });

    if (it == handlers.cend())
    {
        return std::unexpected{vfs::error_code::program_unknown};
    }

    const auto path = Glib::find_program_in_path(it->name);
    if (path.empty())
    {
        return std::unexpected{vfs::error_code::program_not_in_path};
    }

    return std::format("{} {} {}", path, it->exec, command);
}

std::vector<std::string>
vfs::terminals::supported_names() noexcept
{
    std::vector<std::string> names;
    names.reserve(handlers.size());
    for (const auto& handler : handlers)
    {
        names.push_back(handler.name);
    }
    std::ranges::sort(names);
    return names;
}
