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

#include <filesystem>

#if __has_include(<magic_enum/magic_enum.hpp>)
// >=magic_enum-0.9.7
#include <magic_enum/magic_enum.hpp>
#else
// <=magic_enum-0.9.6
#include <magic_enum.hpp>
#endif

#include <glaze/glaze.hpp>

#include <ztd/ztd.hxx>

#include "logger.hxx"

#include "xset/xset.hxx"

#include "settings/upgrade/config-upgrade.hxx"

#include "settings/settings.hxx"
#include "settings/config.hxx"

static void
config_parse_settings(const config::detail::settings& settings, const u64 version) noexcept
{
    (void)version;

    config::settings.show_thumbnails = settings.show_thumbnails;
    config::settings.thumbnail_max_size = settings.thumbnail_max_size;
    config::settings.icon_size_big = settings.icon_size_big;
    config::settings.icon_size_small = settings.icon_size_small;
    config::settings.icon_size_tool = settings.icon_size_tool;
    config::settings.single_click = settings.single_click;
    config::settings.single_hover = settings.single_hover;
    config::settings.use_si_prefix = settings.use_si_prefix;
    config::settings.click_executes = settings.click_executes;
    config::settings.confirm = settings.confirm;
    config::settings.confirm_delete = settings.confirm_delete;
    config::settings.confirm_trash = settings.confirm_trash;
    config::settings.thumbnailer_use_api = settings.thumbnailer_use_api;
    config::settings.height = settings.height;
    config::settings.width = settings.width;
    config::settings.maximized = settings.maximized;
    config::settings.always_show_tabs = settings.always_show_tabs;
    config::settings.show_close_tab_buttons = settings.show_close_tab_buttons;
    config::settings.new_tab_here = settings.new_tab_here;
    config::settings.show_toolbar_home = settings.show_toolbar_home;
    config::settings.show_toolbar_refresh = settings.show_toolbar_refresh;
    config::settings.show_toolbar_search = settings.show_toolbar_search;
}

static void
config_parse_xset(const config::xsetpak_t& pak, const u64 version) noexcept
{
    (void)version;

    for (const auto& [name, vars] : pak)
    {
        // const auto set = xset::set::get(name);
        const auto enum_name_value = magic_enum::enum_cast<xset::name>(name);
        if (!enum_name_value.has_value())
        {
            logger::warn("Invalid xset::name enum name, xset::var::{}", name);
            continue;
        }
        const auto set = xset::set::get(enum_name_value.value());

        for (const auto& [setvar, value] : vars)
        {
            // logger::info("name: {} | var: {} | value: {}", name, setvar, value);

            const auto enum_var_value = magic_enum::enum_cast<xset::var>(setvar);
            if (!enum_var_value.has_value())
            {
                logger::warn("Invalid xset::var enum name, xset::var::{}", setvar);
                continue;
            }

            const auto var = enum_var_value.value();

            if (var == xset::var::s)
            {
                set->s = value;
            }
            else if (var == xset::var::x)
            {
                set->x = value;
            }
            else if (var == xset::var::y)
            {
                set->y = value;
            }
            else if (var == xset::var::z)
            {
                set->z = value;
            }
            else if (var == xset::var::key)
            {
                u32 result = 0;
                const auto [ptr, ec] =
                    std::from_chars(value.data(), value.data() + value.size(), result);
                if (ec != std::errc())
                {
                    logger::error("Config: Failed trying to set xset.{} to {}",
                                  magic_enum::enum_name(var),
                                  value);
                    continue;
                }
                set->keybinding.key = result;
            }
            else if (var == xset::var::keymod)
            {
                u32 result = 0;
                const auto [ptr, ec] =
                    std::from_chars(value.data(), value.data() + value.size(), result);
                if (ec != std::errc())
                {
                    logger::error("Config: Failed trying to set xset.{} to {}",
                                  magic_enum::enum_name(var),
                                  value);
                    continue;
                }
                set->keybinding.modifier = result;
            }
            else if (var == xset::var::b)
            {
                if (value == "1")
                {
                    set->b = xset::set::enabled::yes;
                }
                else
                {
                    set->b = xset::set::enabled::no;
                }
            }
        }
    }
}

void
config::load(const std::filesystem::path& session) noexcept
{
    config_file_data config_data;
    std::string buffer;
    const auto ec = glz::read_file_json<glz::opts{.error_on_unknown_keys = false}>(config_data,
                                                                                   session.c_str(),
                                                                                   buffer);

    if (ec)
    {
        logger::error("Failed to load config file: {}", glz::format_error(ec, buffer));
        return;
    }

    config_parse_settings(config_data.settings, config_data.version);
    config_parse_xset(config_data.xset, config_data.version);

    // upgrade config
    config_upgrade(config_data.version);
}
