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

#include <magic_enum.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#pragma GCC diagnostic ignored "-Wunused-result"
#include <toml.hpp> // toml11
#pragma GCC diagnostic pop

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "xset/xset.hxx"

#include "settings/upgrade/config-upgrade.hxx"

#include "settings/settings.hxx"
#include "settings/config.hxx"

static u64
get_config_file_version(const toml::value& tbl) noexcept
{
    if (!tbl.contains(config::disk_format::toml::section::version.data()))
    {
        ztd::logger::error("config missing TOML section [{}]",
                           config::disk_format::toml::section::version);
        return 0;
    }

    const auto& version = toml::find(tbl, config::disk_format::toml::section::version.data());

    return toml::find<u64>(version, config::disk_format::toml::key::version.data());
}

static void
config_parse_general(const toml::value& tbl, u64 version) noexcept
{
    (void)version;

    if (!tbl.contains(config::disk_format::toml::section::general.data()))
    {
        ztd::logger::error("config missing TOML section [{}]",
                           config::disk_format::toml::section::general);
        return;
    }

    const auto& section = toml::find(tbl, config::disk_format::toml::section::general.data());

    if (section.contains(config::disk_format::toml::key::show_thumbnail.data()))
    {
        config::settings.show_thumbnails =
            toml::find<bool>(section, config::disk_format::toml::key::show_thumbnail.data());
    }

    if (section.contains(config::disk_format::toml::key::thumbnail_max_size.data()))
    {
        config::settings.thumbnail_max_size =
            toml::find<u32>(section, config::disk_format::toml::key::thumbnail_max_size.data())
            << 10;
    }

    if (section.contains(config::disk_format::toml::key::icon_size_big.data()))
    {
        config::settings.icon_size_big =
            toml::find<i32>(section, config::disk_format::toml::key::icon_size_big.data());
    }

    if (section.contains(config::disk_format::toml::key::icon_size_small.data()))
    {
        config::settings.icon_size_small =
            toml::find<i32>(section, config::disk_format::toml::key::icon_size_small.data());
    }

    if (section.contains(config::disk_format::toml::key::icon_size_tool.data()))
    {
        config::settings.icon_size_tool =
            toml::find<i32>(section, config::disk_format::toml::key::icon_size_tool.data());
    }

    if (section.contains(config::disk_format::toml::key::single_click.data()))
    {
        config::settings.single_click =
            toml::find<bool>(section, config::disk_format::toml::key::single_click.data());
    }

    if (section.contains(config::disk_format::toml::key::single_hover.data()))
    {
        config::settings.single_hover =
            toml::find<bool>(section, config::disk_format::toml::key::single_hover.data());
    }

    if (section.contains(config::disk_format::toml::key::use_si_prefix.data()))
    {
        config::settings.use_si_prefix =
            toml::find<bool>(section, config::disk_format::toml::key::use_si_prefix.data());
    }

    if (section.contains(config::disk_format::toml::key::click_execute.data()))
    {
        config::settings.click_executes =
            toml::find<bool>(section, config::disk_format::toml::key::click_execute.data());
    }

    if (section.contains(config::disk_format::toml::key::confirm.data()))
    {
        config::settings.confirm =
            toml::find<bool>(section, config::disk_format::toml::key::confirm.data());
    }

    if (section.contains(config::disk_format::toml::key::confirm_delete.data()))
    {
        config::settings.confirm_delete =
            toml::find<bool>(section, config::disk_format::toml::key::confirm_delete.data());
    }

    if (section.contains(config::disk_format::toml::key::confirm_trash.data()))
    {
        config::settings.confirm_trash =
            toml::find<bool>(section, config::disk_format::toml::key::confirm_trash.data());
    }

    if (section.contains(config::disk_format::toml::key::thumbnailer_backend.data()))
    {
        config::settings.thumbnailer_use_api =
            toml::find<bool>(section, config::disk_format::toml::key::thumbnailer_backend.data());
    }
}

static void
config_parse_window(const toml::value& tbl, u64 version) noexcept
{
    (void)version;

    if (!tbl.contains(config::disk_format::toml::section::window.data()))
    {
        ztd::logger::error("config missing TOML section [{}]",
                           config::disk_format::toml::section::window);
        return;
    }

    const auto& section = toml::find(tbl, config::disk_format::toml::section::window.data());

    if (section.contains(config::disk_format::toml::key::height.data()))
    {
        config::settings.height =
            toml::find<u64>(section, config::disk_format::toml::key::height.data());
    }

    if (section.contains(config::disk_format::toml::key::width.data()))
    {
        config::settings.width =
            toml::find<u64>(section, config::disk_format::toml::key::width.data());
    }

    if (section.contains(config::disk_format::toml::key::maximized.data()))
    {
        config::settings.maximized =
            toml::find<bool>(section, config::disk_format::toml::key::maximized.data());
    }
}

static void
config_parse_interface(const toml::value& tbl, u64 version) noexcept
{
    (void)version;

    if (!tbl.contains(config::disk_format::toml::section::interface.data()))
    {
        ztd::logger::error("config missing TOML section [{}]",
                           config::disk_format::toml::section::interface);
        return;
    }

    const auto& section = toml::find(tbl, config::disk_format::toml::section::interface.data());

    if (section.contains(config::disk_format::toml::key::show_tabs.data()))
    {
        config::settings.always_show_tabs =
            toml::find<bool>(section, config::disk_format::toml::key::show_tabs.data());
    }

    if (section.contains(config::disk_format::toml::key::show_close.data()))
    {
        config::settings.show_close_tab_buttons =
            toml::find<bool>(section, config::disk_format::toml::key::show_close.data());
    }

    if (section.contains(config::disk_format::toml::key::new_tab_here.data()))
    {
        config::settings.new_tab_here =
            toml::find<bool>(section, config::disk_format::toml::key::new_tab_here.data());
    }

    if (section.contains(config::disk_format::toml::key::show_toolbar_home.data()))
    {
        config::settings.show_toolbar_home =
            toml::find<bool>(section, config::disk_format::toml::key::show_toolbar_home.data());
    }

    if (section.contains(config::disk_format::toml::key::show_toolbar_refresh.data()))
    {
        config::settings.show_toolbar_refresh =
            toml::find<bool>(section, config::disk_format::toml::key::show_toolbar_refresh.data());
    }

    if (section.contains(config::disk_format::toml::key::show_toolbar_search.data()))
    {
        config::settings.show_toolbar_search =
            toml::find<bool>(section, config::disk_format::toml::key::show_toolbar_search.data());
    }
}

static void
config_parse_xset(const toml::value& tbl, u64 version) noexcept
{
    (void)version;

    // loop over all of [[XSet]]
    for (const auto& section :
         toml::find<toml::array>(tbl, config::disk_format::toml::section::xset.data()))
    {
        // get [XSet.name] and all vars
        for (const auto& [toml_name, toml_vars] : section.as_table())
        {
            const auto enum_name_value = magic_enum::enum_cast<xset::name>(toml_name);
            if (!enum_name_value.has_value())
            {
                ztd::logger::warn("Invalid xset::name enum name, xset::var::{}", toml_name);
                continue;
            }
            const auto set = xset::set::get(toml_name);

            // get var and value
            for (const auto& [toml_var, toml_value] : toml_vars.as_table())
            {
                const std::string setvar = toml_var;
                const std::string value =
                    ztd::strip(toml::format(toml_value, std::numeric_limits<usize>::max()), "\"");

                // ztd::logger::info("name: {} | var: {} | value: {}", name, setvar, value);

                const auto enum_var_value = magic_enum::enum_cast<xset::var>(setvar);
                if (!enum_var_value.has_value())
                {
                    ztd::logger::warn("Invalid xset::var enum name, xset::var::{}", toml_name);
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
                        ztd::logger::error("Config: Failed trying to set xset.{} to {}",
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
                        ztd::logger::error("Config: Failed trying to set xset.{} to {}",
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
}

void
config::load(const std::filesystem::path& session) noexcept
{
    try
    {
        const auto tbl = toml::parse(session);

        // DEBUG
        // std::println("###### TOML PARSE ######");
        // std::println("{}", tbl.as_string());

        const u64 version = get_config_file_version(tbl);

        config_parse_general(tbl, version);
        config_parse_window(tbl, version);
        config_parse_interface(tbl, version);
        config_parse_xset(tbl, version);

        // upgrade config
        config_upgrade(version);
    }
    catch (const toml::syntax_error& e)
    {
        ztd::logger::error("Config file parsing failed: {}", e.what());
        return;
    }
}
