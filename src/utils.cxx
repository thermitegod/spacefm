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

#include <utility>

#include <fcntl.h>

#include <fmt/format.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "extern.hxx"

#include "settings.hxx"

#include "utils.hxx"

void
print_command(const std::string& command) noexcept
{
    LOG_INFO("COMMAND={}", command);
}

void
print_task_command(const char* ptask, const char* cmd) noexcept
{
    LOG_INFO("TASK_COMMAND({:p})={}", ptask, cmd);
}

void
print_task_command_spawn(const std::vector<std::string>& argv, int pid) noexcept
{
    LOG_INFO("SPAWN=\"{}\" pid={} ", ztd::join(argv, " "), pid);
}

const std::string
randhex8() noexcept
{
    return ztd::randhex(8);
}

const std::string
replace_line_subs(const std::string& line) noexcept
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
    cmd = ztd::replace(cmd, "%t", "\"${fm_task_pwd}\"");
    cmd = ztd::replace(cmd, "%p", "\"${fm_task_pid}\"");
    cmd = ztd::replace(cmd, "%a", "\"${fm_value}\"");

    return cmd;
}

bool
have_x_access(const std::string& path) noexcept
{
    return (faccessat(0, path.c_str(), R_OK | X_OK, AT_EACCESS) == 0);
}

bool
have_rw_access(const std::string& path) noexcept
{
    return (faccessat(0, path.c_str(), R_OK | W_OK, AT_EACCESS) == 0);
}

bool
dir_has_files(const std::string& path) noexcept
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

const std::pair<std::string, std::string>
get_name_extension(const std::string& full_name) noexcept
{
    if (std::filesystem::is_directory(full_name))
        return {full_name, ""};

    if (!ztd::contains(full_name, "."))
        return {full_name, ""};

    std::string fullpath_filebase;
    std::string file_ext;

    if (ztd::contains(full_name, ".tar."))
    {
        // compressed tar archive
        const auto parts = ztd::rpartition(full_name, ".tar.");
        fullpath_filebase = parts[0];
        file_ext = fmt::format("tar.{}", parts[2]);
    }
    else
    {
        const auto parts = ztd::rpartition(full_name, ".");
        fullpath_filebase = parts[0];
        file_ext = parts[2];
    }

    return {fullpath_filebase, file_ext};
}

const std::string
get_prog_executable() noexcept
{
    return std::filesystem::read_symlink("/proc/self/exe");
}

void
open_in_prog(const char* path) noexcept
{
    const std::string exe = get_prog_executable();
    const std::string qpath = bash_quote(path);
    const std::string command = fmt::format("{} {}", exe, qpath);
    print_command(command);
    Glib::spawn_command_line_async(command);
}

const std::string
bash_quote(const std::string& str) noexcept
{
    if (str.empty())
        return "\"\"";
    std::string s1 = ztd::replace(str, "\"", "\\\"");
    s1 = fmt::format("\"{}\"", s1);
    return s1;
}

const std::string
clean_label(const std::string& menu_label, bool kill_special, bool escape) noexcept
{
    if (menu_label.empty())
        return "";

    std::string new_menu_label = menu_label;

    if (ztd::contains(menu_label, "\\_"))
    {
        new_menu_label = ztd::replace(new_menu_label, "\\_", "@UNDERSCORE@");
        new_menu_label = ztd::replace(new_menu_label, "_", "");
        new_menu_label = ztd::replace(new_menu_label, "@UNDERSCORE@", "_");
    }
    else
        new_menu_label = ztd::replace(new_menu_label, "_", "");
    if (kill_special)
    {
        new_menu_label = ztd::replace(new_menu_label, "&", "");
        new_menu_label = ztd::replace(new_menu_label, " ", "-");
    }
    else if (escape)
        new_menu_label = Glib::Markup::escape_text(new_menu_label);

    return new_menu_label;
}

const std::string
get_valid_su() noexcept
{
    std::string use_su;
    if (!xset_get_s(XSetName::SU_COMMAND))
    {
        for (std::size_t i = 0; i < terminal_programs.size(); ++i)
        {
            use_su = Glib::find_program_in_path(su_commands.at(i));
            if (!use_su.empty())
                break;
        }
        if (use_su.empty())
            use_su = su_commands.at(0);
        xset_set(XSetName::SU_COMMAND, XSetVar::S, use_su);
    }
    const std::string su_path = Glib::find_program_in_path(use_su);
    return su_path;
}
