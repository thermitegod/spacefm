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

#include <string>
#include <vector>
#include <iostream>
#include <filesystem>

#include <fcntl.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "extern.hxx"

#include "settings.hxx"

#include "utils.hxx"

void
print_command(const std::string& command)
{
    LOG_INFO("COMMAND={}", command);
}

void
print_task_command(const char* ptask, const char* cmd)
{
    LOG_INFO("TASK_COMMAND({:p})={}", ptask, cmd);
}

void
print_task_command_spawn(std::vector<std::string> argv, int pid)
{
    LOG_INFO("SPAWN=");
    for (std::string arg: argv)
    {
        LOG_INFO("  {}", arg);
    }
    LOG_INFO("    pid = {}", pid);
}

char*
randhex8()
{
    char hex[9];
    unsigned int n = mrand48();
    snprintf(hex, sizeof(hex), "%08x", n);
    // LOG_INFO("rand  : {}", n);
    // LOG_INFO("hex   : {}", hex);
    return ztd::strdup(hex);
}

std::string
replace_line_subs(const std::string& line)
{
    std::string cmd = line;

    cmd = ztd::replace(cmd, "%f", "\"${fm_file}\"");
    cmd = ztd::replace(cmd, "%F", "\"${fm_files[@]}\"");
    cmd = ztd::replace(cmd, "%n", "\"${fm_filename}\"");
    cmd = ztd::replace(cmd, "%N", "\"${fm_filenames[@]}\"");
    cmd = ztd::replace(cmd, "%d", "\"${fm_pwd}\"");
    cmd = ztd::replace(cmd, "%D", "\"${fm_pwd}\"");
    cmd = ztd::replace(cmd, "%v", "\"${fm_device}\"");
    cmd = ztd::replace(cmd, "%l", "\"${fm_device_label}\"");
    cmd = ztd::replace(cmd, "%m", "\"${fm_device_mount_point}\"");
    cmd = ztd::replace(cmd, "%y", "\"${fm_device_fstype}\"");
    cmd = ztd::replace(cmd, "%b", "\"${fm_bookmark}\"");
    cmd = ztd::replace(cmd, "%t", "\"${fm_task_pwd}\"");
    cmd = ztd::replace(cmd, "%p", "\"${fm_task_pid}\"");
    cmd = ztd::replace(cmd, "%a", "\"${fm_value}\"");

    return cmd;
}

bool
have_x_access(const std::string& path)
{
    return (faccessat(0, path.c_str(), R_OK | X_OK, AT_EACCESS) == 0);
}

bool
have_rw_access(const std::string& path)
{
    return (faccessat(0, path.c_str(), R_OK | W_OK, AT_EACCESS) == 0);
}

bool
dir_has_files(const std::string& path)
{
    if (!std::filesystem::is_directory(path))
        return false;

    for (const auto& file: std::filesystem::directory_iterator(path))
    {
        if (file.exists())
            return true;
    }

    return false;
}

std::string
get_name_extension(const std::string& full_name, std::string& ext)
{
    if (std::filesystem::is_directory(full_name))
        return full_name;

    std::filesystem::path name = std::filesystem::path(full_name);
    // std::string filename = name.filename();
    std::string basename = name.stem();
    std::string parent = name.parent_path();
    ext = name.extension();

    if (ext.empty())
        return full_name;

    if (ztd::contains(full_name, ".tar" + ext))
    {
        ext = ".tar" + ext;
        basename = name.stem().stem();
    }
    // remove '.'
    ext.erase(0, 1);

    return Glib::build_filename(parent, basename);
}

const std::string
get_prog_executable()
{
    return std::filesystem::read_symlink("/proc/self/exe");
}

void
open_in_prog(const char* path)
{
    const std::string exe = get_prog_executable();
    const std::string qpath = bash_quote(path);
    const std::string command = fmt::format("{} {}", exe, qpath);
    print_command(command);
    Glib::spawn_command_line_async(command);
}

std::string
bash_quote(const std::string& str)
{
    if (str.empty())
        return "\"\"";
    std::string s1 = ztd::replace(str, "\"", "\\\"");
    s1 = fmt::format("\"{}\"", s1);
    return s1;
}

std::string
clean_label(const std::string& menu_label, bool kill_special, bool escape)
{
    if (menu_label.empty())
        return "";

    std::string new_menu_label;

    if (ztd::contains(menu_label, "\\_"))
    {
        new_menu_label = ztd::replace(menu_label, "\\_", "@UNDERSCORE@");
        new_menu_label = ztd::replace(menu_label, "_", "");
        new_menu_label = ztd::replace(menu_label, "@UNDERSCORE@", "_");
    }
    else
        new_menu_label = ztd::replace(menu_label, "_", "");
    if (kill_special)
    {
        new_menu_label = ztd::replace(menu_label, "&", "");
        new_menu_label = ztd::replace(menu_label, " ", "-");
    }
    else if (escape)
        new_menu_label = g_markup_escape_text(menu_label.c_str(), -1);

    return new_menu_label;
}

std::string
get_valid_su()
{
    std::string use_su;
    if (!xset_get_s("su_command"))
    {
        for (std::size_t i = 0; i < G_N_ELEMENTS(su_commands); i++)
        {
            use_su = Glib::find_program_in_path(su_commands[i]);
            if (!use_su.empty())
                break;
        }
        if (use_su.empty())
            use_su = su_commands[0];
        xset_set("su_command", "s", use_su.c_str());
    }
    std::string su_path = Glib::find_program_in_path(use_su);
    return su_path;
}
