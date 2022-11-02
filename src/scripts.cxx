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

#include <map>

#include <fmt/format.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-user-dir.hxx"

#include "scripts.hxx"

// #define SPACEFM_USER_SCRIPT_OVERRIDE

using namespace std::literals::string_view_literals;

static const std::map<Scripts, const std::string_view> script_map{
    {Scripts::SPACEFM_AUTH, "spacefm-auth"sv},
    {Scripts::CONFIG_UPDATE, "config-update"sv},
    {Scripts::CONFIG_UPDATE_GIT, "config-update-git"sv},
};

bool
script_exists(Scripts script) noexcept
{
    const std::string script_name = get_script_path(script);

    if (!std::filesystem::exists(script_name))
    {
        LOG_ERROR("Missing script: {}", script_name);
        return false;
    }
    return true;
}

bool
script_exists(std::string_view script) noexcept
{
    if (!std::filesystem::exists(script))
    {
        LOG_ERROR("Missing script {}", script);
        return false;
    }
    return true;
}

const std::string
get_script_path(Scripts script) noexcept
{
    const std::string script_name = script_map.at(script).data();

#ifdef SPACEFM_USER_SCRIPT_OVERRIDE
    const std::string script_path =
        Glib::build_filename(vfs_user_get_config_dir(), "scripts", script_name);
    // LOG_INFO("user script: {}", script_path);
    if (std::filesystem::exists(script_path))
        return script_path;
#endif

    return Glib::build_filename(PACKAGE_SCRIPTS_PATH, script_name);
}
