/*
 *
 * License: See COPYING file
 *
 */

#pragma once

#include <string>

#include <glib.h>

void print_command(const std::string& command);
void print_task_command(const char* ptask, const char* cmd);
void print_task_command_spawn(char* argv[], int pid);

char* randhex8();
bool have_rw_access(const char* path);
bool have_x_access(const char* path);
bool dir_has_files(const char* path);
char* replace_line_subs(const char* line);
char* get_name_extension(const char* full_name, bool is_dir, char** ext);
void open_in_prog(const char* path);

char* bash_quote(const char* str);
char* clean_label(const char* menu_label, bool kill_special, bool convert_amp);
void string_copy_free(char** s, const char* src);
char* unescape(const char* t);

char* get_valid_su();
