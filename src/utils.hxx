/*
 *
 * License: See COPYING file
 *
 */

#pragma once

#include <string>

void print_command(const std::string& command);
void print_task_command(const char* ptask, const char* cmd);
void print_task_command_spawn(char* argv[], int pid);

char* randhex8();
bool have_rw_access(const char* path);
bool have_x_access(const char* path);
bool dir_has_files(const char* path);
std::string replace_line_subs(const std::string& line);
std::string get_name_extension(const std::string& full_name, std::string& ext);
const std::string get_prog_executable();
void open_in_prog(const char* path);

std::string bash_quote(const std::string& str);

std::string clean_label(const std::string& menu_label, bool kill_special, bool escape);

void string_copy_free(char** s, const char* src);
char* unescape(const char* t);

char* get_valid_su();
