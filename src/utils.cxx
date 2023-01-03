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
#include <string_view>

#include <vector>

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

const std::string
randhex8() noexcept
{
    return ztd::randhex(8);
}

const std::string
replace_line_subs(std::string_view line) noexcept
{
    std::string cmd = line.data();

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
have_x_access(std::string_view path) noexcept
{
    return (faccessat(0, path.data(), R_OK | X_OK, AT_EACCESS) == 0);
}

bool
have_rw_access(std::string_view path) noexcept
{
    return (faccessat(0, path.data(), R_OK | W_OK, AT_EACCESS) == 0);
}

bool
dir_has_files(std::string_view path) noexcept
{
    if (!std::filesystem::is_directory(path))
        return false;

    for (const auto& file : std::filesystem::directory_iterator(path))
    {
        if (file.exists())
            return true;
    }

    return false;
}

const std::pair<std::string, std::string>
get_name_extension(std::string_view full_name) noexcept
{
    if (std::filesystem::is_directory(full_name))
        return {full_name.data(), ""};

    if (!ztd::contains(full_name, "."))
        return {full_name.data(), ""};

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

void
open_in_prog(std::string_view path) noexcept
{
    const std::string exe = ztd::program::exe();
    const std::string qpath = bash_quote(path);
    const std::string command = fmt::format("{} {}", exe, qpath);
    LOG_INFO("COMMAND={}", command);
    Glib::spawn_command_line_async(command);
}

const std::string
bash_quote(std::string_view str) noexcept
{
    if (str.empty())
        return "\"\"";
    std::string s1 = ztd::replace(str, "\"", "\\\"");
    s1 = fmt::format("\"{}\"", s1);
    return s1;
}

const std::string
clean_label(std::string_view menu_label, bool kill_special, bool escape) noexcept
{
    if (menu_label.empty())
        return "";

    std::string new_menu_label = menu_label.data();

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
        for (auto su_command : su_commands)
        {
            use_su = Glib::find_program_in_path(su_command.data());
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
