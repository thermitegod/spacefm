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

void print_command(const std::string& command);
void print_task_command(const char* ptask, const char* cmd);
void print_task_command_spawn(char* argv[], int pid);

char* randhex8();
bool have_rw_access(const char* path);
bool have_x_access(const char* path);

bool dir_has_files(const std::string& path);

std::string replace_line_subs(const std::string& line);
std::string get_name_extension(const std::string& full_name, std::string& ext);
const std::string get_prog_executable();
void open_in_prog(const char* path);

std::string bash_quote(const std::string& str);

std::string clean_label(const std::string& menu_label, bool kill_special, bool escape);

char* unescape(const char* t);

char* get_valid_su();
