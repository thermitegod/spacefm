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

namespace commandline::socket::set_task
{
void icon(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void count(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void directory(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void from(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void item(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void to(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void progress(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void total(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void curspeed(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void curremain(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void avgspeed(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void avgremain(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void queue_state(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;

} // namespace commandline::socket::set_task
