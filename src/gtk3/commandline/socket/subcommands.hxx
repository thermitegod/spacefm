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

void setup_subcommand_set(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void setup_subcommand_get(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void setup_subcommand_set_task(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void setup_subcommand_get_task(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void setup_subcommand_run_task(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void setup_subcommand_emit_key(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void setup_subcommand_activate(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void setup_subcommand_add_event(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void setup_subcommand_replace_event(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void setup_subcommand_remove_event(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void setup_subcommand_help(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
void setup_subcommand_ping(CLI::App* app, const socket_subcommand_data_t& opt) noexcept;
