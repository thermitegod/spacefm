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

#include <format>

#include <filesystem>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "xset/xset.hxx"

#include "extern.hxx"

#include "utils.hxx"

const std::string
replace_line_subs(const std::string_view line) noexcept
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
have_x_access(const std::filesystem::path& path) noexcept
{
    const auto status = std::filesystem::status(path);

    return ((status.permissions() & std::filesystem::perms::owner_exec) !=
                std::filesystem::perms::none ||
            (status.permissions() & std::filesystem::perms::group_exec) !=
                std::filesystem::perms::none ||
            (status.permissions() & std::filesystem::perms::others_exec) !=
                std::filesystem::perms::none);
}

bool
have_rw_access(const std::filesystem::path& path) noexcept
{
    const auto status = std::filesystem::status(path);

    return ((status.permissions() & std::filesystem::perms::owner_read) !=
                std::filesystem::perms::none &&
            (status.permissions() & std::filesystem::perms::owner_write) !=
                std::filesystem::perms::none) ||

           ((status.permissions() & std::filesystem::perms::group_read) !=
                std::filesystem::perms::none &&
            (status.permissions() & std::filesystem::perms::group_write) !=
                std::filesystem::perms::none) ||

           ((status.permissions() & std::filesystem::perms::others_read) !=
                std::filesystem::perms::none &&
            (status.permissions() & std::filesystem::perms::others_write) !=
                std::filesystem::perms::none);
}

bool
dir_has_files(const std::filesystem::path& path) noexcept
{
    if (!std::filesystem::is_directory(path))
    {
        return false;
    }

    for (const auto& file : std::filesystem::directory_iterator(path))
    {
        if (file.exists())
        {
            return true;
        }
    }

    return false;
}

const split_basename_extension_data
split_basename_extension(const std::filesystem::path& filename) noexcept
{
    if (std::filesystem::is_directory(filename))
    {
        return {filename.string()};
    }

    // Find the last dot in the filename
    const auto dot_pos = filename.string().find_last_of('.');

    // Check if the dot is not at the beginning or end of the filename
    if (dot_pos != std::string::npos && dot_pos != 0 && dot_pos != filename.string().length() - 1)
    {
        const auto split = ztd::rpartition(filename.string(), ".");

        // Check if the extension is a compressed tar archive
        if (ztd::endswith(split[0], ".tar"))
        {
            // Find the second last dot in the filename
            const auto split_second = ztd::rpartition(split[0], ".");

            return {split_second[0], std::format("{}.{}", split_second[2], split[2]), true};
        }
        else
        {
            // Return the basename and the extension
            return {split[0], split[2]};
        }
    }

    // No valid extension found, return the whole filename as the basename
    return {filename.string()};
}

void
open_in_prog(const std::filesystem::path& path) noexcept
{
    const std::string exe = ztd::program::exe();
    const std::string qpath = ztd::shell::quote(path.string());
    const std::string command = std::format("{} {}", exe, qpath);
    ztd::logger::info("COMMAND={}", command);
    Glib::spawn_command_line_async(command);
}

const std::string
clean_label(const std::string_view menu_label, bool kill_special, bool escape) noexcept
{
    if (menu_label.empty())
    {
        return "";
    }

    std::string new_menu_label = menu_label.data();

    if (ztd::contains(menu_label, "\\_"))
    {
        new_menu_label = ztd::replace(new_menu_label, "\\_", "@UNDERSCORE@");
        new_menu_label = ztd::replace(new_menu_label, "_", "");
        new_menu_label = ztd::replace(new_menu_label, "@UNDERSCORE@", "_");
    }
    else
    {
        new_menu_label = ztd::replace(new_menu_label, "_", "");
    }
    if (kill_special)
    {
        new_menu_label = ztd::replace(new_menu_label, "&", "");
        new_menu_label = ztd::replace(new_menu_label, " ", "-");
    }
    else if (escape)
    {
        new_menu_label = Glib::Markup::escape_text(new_menu_label);
    }

    return new_menu_label;
}

const std::string
get_valid_su() noexcept
{
    std::string use_su;
    if (!xset_get_s(xset::name::su_command))
    {
        for (auto su_command : su_commands)
        {
            use_su = Glib::find_program_in_path(su_command.data());
            if (!use_su.empty())
            {
                break;
            }
        }
        if (use_su.empty())
        {
            use_su = su_commands.at(0);
        }
        xset_set(xset::name::su_command, xset::var::s, use_su);
    }
    const std::string su_path = Glib::find_program_in_path(use_su);
    return su_path;
}
