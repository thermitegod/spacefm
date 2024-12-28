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

#include <glaze/glaze.hpp>

#if __has_include(<magic_enum/magic_enum.hpp>)
// >=magic_enum-0.9.7
#include <magic_enum/magic_enum.hpp>
#else
// <=magic_enum-0.9.6
#include <magic_enum.hpp>
#endif

#include <ztd/ztd.hxx>

#include "logger.hxx"

#include "xset/xset.hxx"

#include "settings/upgrade/config-upgrade.hxx"

#include "settings/settings.hxx"
#include "settings/config.hxx"

static void
config_parse_settings(const u64 version, const config::settings& loaded_settings,
                      const std::shared_ptr<config::settings>& settings) noexcept
{
    (void)version;

    settings->show_thumbnails = loaded_settings.show_thumbnails;
    settings->thumbnail_max_size = loaded_settings.thumbnail_max_size;
    settings->icon_size_big = loaded_settings.icon_size_big;
    settings->icon_size_small = loaded_settings.icon_size_small;
    settings->icon_size_tool = loaded_settings.icon_size_tool;
    settings->use_si_prefix = loaded_settings.use_si_prefix;
    settings->click_executes = loaded_settings.click_executes;
    settings->confirm = loaded_settings.confirm;
    settings->confirm_delete = loaded_settings.confirm_delete;
    settings->confirm_trash = loaded_settings.confirm_trash;
    settings->maximized = loaded_settings.maximized;
    settings->always_show_tabs = loaded_settings.always_show_tabs;
    settings->show_close_tab_buttons = loaded_settings.show_close_tab_buttons;
    settings->new_tab_here = loaded_settings.new_tab_here;
    settings->show_toolbar_home = loaded_settings.show_toolbar_home;
    settings->show_toolbar_refresh = loaded_settings.show_toolbar_refresh;
    settings->show_toolbar_search = loaded_settings.show_toolbar_search;
}

static void
config_parse_xset(const u64 version, const config::xsetpak_t& pak) noexcept
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
config::load(const std::filesystem::path& session,
             const std::shared_ptr<config::settings>& settings) noexcept
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

    config_parse_settings(config_data.version, config_data.settings, settings);
    config_parse_xset(config_data.version, config_data.xset);

    // upgrade config
    config_upgrade(config_data.version);
}
