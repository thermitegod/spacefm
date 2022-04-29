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

struct CliFlags
{
    char** files{nullptr};
    bool new_tab{true};
    bool reuse_tab{false};
    bool no_tabs{false};
    bool new_window{false};
    bool socket_cmd{false};
    bool version_opt{false};

    bool daemon_mode{false};

    int panel{0};

    bool find_files{false};
    char* config_dir{nullptr};
    bool disable_git_settings{false};
};

extern CliFlags cli_flags;

bool check_socket_daemon();
bool single_instance_check();
void single_instance_finalize();
int send_socket_command(int argc, char* argv[], std::string& reply);
