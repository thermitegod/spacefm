/*
 *
 * License: See COPYING file
 *
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
int send_socket_command(int argc, char* argv[], char** reply);
