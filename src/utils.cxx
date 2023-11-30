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

#include "utils.hxx"

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
        if (split[0].ends_with(".tar"))
        {
            // Find the second last dot in the filename
            const auto split_second = ztd::rpartition(split[0], ".");

            return {split_second[0], std::format(".{}.{}", split_second[2], split[2]), true};
        }
        else
        {
            // Return the basename and the extension
            return {split[0], std::format(".{}", split[2]), false};
        }
    }

    // No valid extension found, return the whole filename as the basename
    return {filename.string()};
}

const std::string
clean_label(const std::string_view menu_label, bool kill_special, bool escape) noexcept
{
    if (menu_label.empty())
    {
        return "";
    }

    std::string new_menu_label = menu_label.data();

    if (menu_label.contains("\\_"))
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
