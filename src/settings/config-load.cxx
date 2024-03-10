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

#include <toml.hpp> // toml11

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
            const std::string name = toml_name;
            const xset_t set = xset_get(name);

            // get var and value
            for (const auto& [toml_var, toml_value] : toml_vars.as_table())
            {
                const std::string setvar = toml_var;
                const std::string value =
                    ztd::strip(toml::format(toml_value, std::numeric_limits<usize>::max()), "\"");

                // ztd::logger::info("name: {} | var: {} | value: {}", name, setvar, value);

                const auto enum_value = magic_enum::enum_cast<xset::var>(setvar);
                if (!enum_value.has_value())
                {
                    ztd::logger::critical("Invalid xset::var enum name xset::var::{}", name);
                    continue;
                }
                xset_set_var(set, enum_value.value(), value);
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
        // std::cout << "###### TOML PARSE ######" << "\n\n";
        // std::cout << tbl << "\n\n";

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
