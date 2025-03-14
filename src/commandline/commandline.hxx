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
#include <memory>
#include <unordered_map>
#include <vector>

#include <CLI/CLI.hpp>

#include <ztd/ztd.hxx>

#include "types.hxx"

struct commandline_opt_data : public std::enable_shared_from_this<commandline_opt_data>
{
    std::vector<std::filesystem::path> files;

    bool new_tab{true};
    bool reuse_tab{false};
    bool no_tabs{false};
    bool new_window{false};

    panel_t panel{0};

    std::filesystem::path config_dir;

    std::vector<std::string> raw_log_levels;
    std::unordered_map<std::string, std::string> log_levels;
    // std::filesystem::path logfile{"/tmp/test.log"};
    std::filesystem::path logfile;

    bool version{false};
};

using commandline_opt_data_t = std::shared_ptr<commandline_opt_data>;

void setup_commandline(CLI::App& app, const commandline_opt_data_t& opt) noexcept;
