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

#include <CLI/CLI.hpp>

#include "commandline/socket.hxx"

namespace commandline::socket::run_task
{
void cmd(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void edit(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void mount(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void umount(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void copy(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void move(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void link(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void del(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void trash(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
} // namespace commandline::socket::run_task
