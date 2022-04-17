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

void print_command(const std::string& command) noexcept;
void print_task_command(const char* ptask, const char* cmd) noexcept;
void print_task_command_spawn(const std::vector<std::string>& argv, int pid) noexcept;

char* randhex8() noexcept;

bool have_rw_access(const std::string& path) noexcept;
bool have_x_access(const std::string& path) noexcept;

bool dir_has_files(const std::string& path) noexcept;

std::string replace_line_subs(const std::string& line) noexcept;
std::string get_name_extension(const std::string& full_name, std::string& ext) noexcept;
const std::string get_prog_executable() noexcept;
void open_in_prog(const char* path) noexcept;

std::string bash_quote(const std::string& str) noexcept;

std::string clean_label(const std::string& menu_label, bool kill_special, bool escape) noexcept;

std::string get_valid_su();
