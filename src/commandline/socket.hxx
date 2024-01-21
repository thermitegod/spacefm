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

#include <string>

#include <vector>

#include <CLI/CLI.hpp>

#include "types.hxx"

struct socket_subcommand_data
{
    // flags
    std::string window{};
    panel_t panel{0};
    tab_t tab{0};

    // socket data
    std::string command{};                  // get, set, etc...
    std::string property{};                 // window-size, window-position, etc...
    std::string subproperty{};              // values inside a property, name, size, type, etc...
    std::vector<std::string> socket_data{}; // data to be use with property/subproperty
};

using socket_subcommand_data_t = std::shared_ptr<socket_subcommand_data>;

void setup_subcommand_socket(CLI::App& app) noexcept;
