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

#include <filesystem>

#include <iostream>
#include <fstream>

#include <fmt/format.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-user-dir.hxx"

#include "settings/etc.hxx"

static void
parse_etc_conf(const std::string& etc_path, const std::string& raw_line)
{
    std::size_t sep = raw_line.find("=");
    if (sep == std::string::npos)
        return;

    std::string line = ztd::strip(raw_line);

    if (line.at(0) == '#')
        return;

    std::string token = line.substr(0, sep);
    std::string value = line.substr(sep + 1, std::string::npos - 1);

    // remove any quotes
    value = ztd::replace(value, "\"", "");

    if (value.empty())
        return;

    if (ztd::same(token, "terminal_su") || ztd::same(token, "graphical_su"))
    {
        if (value.at(0) != '/' || !std::filesystem::exists(value))
            LOG_WARN("{}: {} '{}' file not found", etc_path, token, value);
        else if (ztd::same(token, "terminal_su"))
            etc_settings.set_terminal_su(value);
    }
    else if (ztd::same(token, "font_view_icon"))
    {
        etc_settings.set_font_view_icon(value);
    }
    else if (ztd::same(token, "font_view_compact"))
    {
        etc_settings.set_font_view_compact(value);
    }
    else if (ztd::same(token, "font_general"))
    {
        etc_settings.set_font_general(value);
    }
}

void
load_etc_conf()
{
    // Set default config values
    etc_settings.set_tmp_dir(vfs_user_cache_dir());

    // load spacefm.conf
    std::string config_path =
        Glib::build_filename(vfs_user_config_dir(), PACKAGE_NAME, "spacefm.conf");
    if (!std::filesystem::exists(config_path))
        config_path = Glib::build_filename(SYSCONFDIR, PACKAGE_NAME, "spacefm.conf");

    if (!std::filesystem::is_regular_file(config_path))
        return;

    std::string line;
    std::ifstream file(config_path);
    if (file.is_open())
    {
        while (std::getline(file, line))
        {
            parse_etc_conf(config_path, line);
        }
    }
    file.close();
}
