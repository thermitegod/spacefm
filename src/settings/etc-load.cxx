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

#include <fmt/format.h>

#include <toml.hpp> // toml11

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-user-dirs.hxx"

#include "settings/etc.hxx"
#include "settings/etc-load.hxx"

#include "settings/disk-format.hxx"

static void
parse_etc_conf(const toml::value& tbl)
{
    if (!tbl.contains(ETC_SECTION_CONFIG))
    {
        // ztd::logger::error("etc missing TOML section [{}]", ETC_SECTION_CONFIG);
        return;
    }

    const auto& section = toml::find(tbl, ETC_SECTION_CONFIG);

    if (section.contains(ETC_KEY_TERMINAL_SU))
    {
        const auto terminal_su = toml::find<std::string>(section, ETC_KEY_TERMINAL_SU);
        etc_settings.set_terminal_su(terminal_su);
    }

    if (section.contains(ETC_KEY_FONT_VIEW_ICON))
    {
        const auto font_view_icon = toml::find<std::string>(section, ETC_KEY_FONT_VIEW_ICON);
        etc_settings.set_font_view_icon(font_view_icon);
    }

    if (section.contains(ETC_KEY_FONT_VIEW_COMPACT))
    {
        const auto font_view_compact = toml::find<std::string>(section, ETC_KEY_FONT_VIEW_COMPACT);
        etc_settings.set_font_view_compact(font_view_compact);
    }

    if (section.contains(ETC_KEY_FONT_GENERAL))
    {
        const auto font_general = toml::find<std::string>(section, ETC_KEY_FONT_GENERAL);
        etc_settings.set_font_general(font_general);
    }
}

void
load_etc_conf()
{
    const std::string config_path = Glib::build_filename(SYSCONFDIR, PACKAGE_NAME, "spacefm.cfg");

    if (!std::filesystem::exists(config_path))
    {
        ztd::logger::warn("Config file missing {}", config_path);
        return;
    }

    try
    {
        const auto tbl = toml::parse(config_path);
        // DEBUG
        // std::cout << "###### TOML PARSE ######" << "\n\n";
        // std::cout << tbl << "\n\n";

        parse_etc_conf(tbl);
    }
    catch (const toml::syntax_error& e)
    {
        ztd::logger::error("TOML parsing failed {}", e.what());
        return;
    }
    catch (...)
    {
        ztd::logger::error("Failed to parse {}", config_path);
        return;
    }
}
