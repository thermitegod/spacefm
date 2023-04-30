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

#include <string>
#include <string_view>

#include <filesystem>

#include <vector>

#include <map>

#include <fmt/format.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "terminal-handlers.hxx"

TerminalHandler::TerminalHandler(std::string_view name, std::string_view exec)
    : name(name), exec(exec)
{
    this->path = Glib::find_program_in_path(name.data());
}

TerminalHandlers::TerminalHandlers()
{
    // clang-format off
    this->handlers = {
        {"alacritty",      TerminalHandler("alacritty",      "-e")},
        {"aterm",          TerminalHandler("aterm",          "-e")},
        {"Eterm",          TerminalHandler("Eterm",          "-e")},
        {"gnome-terminal", TerminalHandler("gnome-terminal", "-x")},
        {"kitty",          TerminalHandler("kitty",          "")},
        {"Konsole",        TerminalHandler("Konsole",        "-e")},
        {"lxterminal",     TerminalHandler("lxterminal",     "-e")},
        {"mlterm",         TerminalHandler("mlterm",         "-e")},
        {"mrxvt",          TerminalHandler("mrxvt",          "-e")},
        {"qterminal",      TerminalHandler("qterminal",      "-e")},
        {"rxvt",           TerminalHandler("rxvt",           "-e")},
        {"sakura",         TerminalHandler("sakura",         "-x")},
        {"st",             TerminalHandler("st",             "-e")},
        {"tabby",          TerminalHandler("tabby",          "-e")},
        {"terminal",       TerminalHandler("terminal",       "--disable-server")},
        {"terminator",     TerminalHandler("terminator",     "-x")},
        {"terminology",    TerminalHandler("terminology",    "-e")},
        {"tilix",          TerminalHandler("tilix",          "-e")},
        {"urxvt",          TerminalHandler("urxvt",          "-e")},
        {"xfce4-terminal", TerminalHandler("xfce4-terminal", "-x")},
        {"xterm",          TerminalHandler("xterm",          "-e")},
    };
    // clang-format on
}

const std::vector<std::string>
TerminalHandlers::get_terminal_args(std::string_view terminal)
{
    // ztd::logger::debug("get_terminal_args={}", terminal);
    if (!this->handlers.contains(terminal.data()))
    {
        ztd::logger::error("Unknown terminal: {}", terminal);
        return {};
    }

    try
    {
        if (ztd::contains(terminal, "/"))
        {
            const auto handler = this->handlers.at(ztd::rpartition(terminal, "/")[2]);
            // ztd::logger::error("Terminal={}, name={}, exec={}", terminal, handler.name, handler.exec);
            return {handler.path, handler.exec};
        }
        else
        {
            const auto handler = this->handlers.at(terminal.data());
            // ztd::logger::error("Terminal={}, name={}, exec={}", terminal, handler.name, handler.exec);
            return {handler.path, handler.exec};
        }
    }
    catch (const std::out_of_range& e)
    {
        ztd::logger::error("Failed to get terminal: {}", terminal);
        return {};
    }
}

const std::vector<std::string>
TerminalHandlers::get_supported_terminal_names()
{
    std::vector<std::string> terminal_names;
    terminal_names.reserve(handlers.size());
    for (const auto& handler : this->handlers)
    {
        terminal_names.emplace_back(handler.second.name);
    }
    return terminal_names;
}

const terminal_handlers_t terminal_handlers = std::make_unique<TerminalHandlers>();
