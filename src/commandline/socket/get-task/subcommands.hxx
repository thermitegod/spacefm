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

namespace commandline::socket::get_task
{
    void icon(CLI::App* app, const socket_subcommand_data_t& opt);
    void count(CLI::App* app, const socket_subcommand_data_t& opt);
    void directory(CLI::App* app, const socket_subcommand_data_t& opt);
    void from(CLI::App* app, const socket_subcommand_data_t& opt);
    void item(CLI::App* app, const socket_subcommand_data_t& opt);
    void to(CLI::App* app, const socket_subcommand_data_t& opt);
    void progress(CLI::App* app, const socket_subcommand_data_t& opt);
    void total(CLI::App* app, const socket_subcommand_data_t& opt);
    void curspeed(CLI::App* app, const socket_subcommand_data_t& opt);
    void curremain(CLI::App* app, const socket_subcommand_data_t& opt);
    void avgspeed(CLI::App* app, const socket_subcommand_data_t& opt);
    void avgremain(CLI::App* app, const socket_subcommand_data_t& opt);
    void elapsed(CLI::App* app, const socket_subcommand_data_t& opt);
    void started(CLI::App* app, const socket_subcommand_data_t& opt);
    void status(CLI::App* app, const socket_subcommand_data_t& opt);
    void queue_state(CLI::App* app, const socket_subcommand_data_t& opt);
    void popup_handler(CLI::App* app, const socket_subcommand_data_t& opt);

} // namespace commandline::socket::get_task
