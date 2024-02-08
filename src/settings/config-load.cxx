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

#include "settings/app.hxx"
#include "settings/config.hxx"

static u64
get_config_file_version(const toml::value& tbl)
{
    if (!tbl.contains(config::disk_format::toml::section::version.data()))
    {
        ztd::logger::error("config missing TOML section [{}]",
                           config::disk_format::toml::section::version);
        return 0;
    }

    const auto& version = toml::find(tbl, config::disk_format::toml::section::version.data());

    const auto config_version =
        toml::find<u64>(version, config::disk_format::toml::key::version.data());
    return config_version;
}

static void
config_parse_general(const toml::value& tbl, u64 version)
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
        const auto show_thumbnail =
            toml::find<bool>(section, config::disk_format::toml::key::show_thumbnail.data());
        app_settings.show_thumbnail(show_thumbnail);
    }

    if (section.contains(config::disk_format::toml::key::max_thumb_size.data()))
    {
        const auto max_thumb_size =
            toml::find<u32>(section, config::disk_format::toml::key::max_thumb_size.data());
        app_settings.max_thumb_size(max_thumb_size << 10);
    }

    if (section.contains(config::disk_format::toml::key::icon_size_big.data()))
    {
        const auto icon_size_big =
            toml::find<i32>(section, config::disk_format::toml::key::icon_size_big.data());
        app_settings.icon_size_big(icon_size_big);
    }

    if (section.contains(config::disk_format::toml::key::icon_size_small.data()))
    {
        const auto icon_size_small =
            toml::find<i32>(section, config::disk_format::toml::key::icon_size_small.data());
        app_settings.icon_size_small(icon_size_small);
    }

    if (section.contains(config::disk_format::toml::key::icon_size_tool.data()))
    {
        const auto icon_size_tool =
            toml::find<i32>(section, config::disk_format::toml::key::icon_size_tool.data());
        app_settings.icon_size_tool(icon_size_tool);
    }

    if (section.contains(config::disk_format::toml::key::single_click.data()))
    {
        const auto single_click =
            toml::find<bool>(section, config::disk_format::toml::key::single_click.data());
        app_settings.single_click(single_click);
    }

    if (section.contains(config::disk_format::toml::key::single_hover.data()))
    {
        const auto single_hover =
            toml::find<bool>(section, config::disk_format::toml::key::single_hover.data());
        app_settings.single_hover(single_hover);
    }

    if (section.contains(config::disk_format::toml::key::use_si_prefix.data()))
    {
        const auto use_si_prefix =
            toml::find<bool>(section, config::disk_format::toml::key::use_si_prefix.data());
        app_settings.use_si_prefix(use_si_prefix);
    }

    if (section.contains(config::disk_format::toml::key::click_execute.data()))
    {
        const auto click_executes =
            toml::find<bool>(section, config::disk_format::toml::key::click_execute.data());
        app_settings.click_executes(click_executes);
    }

    if (section.contains(config::disk_format::toml::key::confirm.data()))
    {
        const auto confirm =
            toml::find<bool>(section, config::disk_format::toml::key::confirm.data());
        app_settings.confirm(confirm);
    }

    if (section.contains(config::disk_format::toml::key::confirm_delete.data()))
    {
        const auto confirm_delete =
            toml::find<bool>(section, config::disk_format::toml::key::confirm_delete.data());
        app_settings.confirm_delete(confirm_delete);
    }

    if (section.contains(config::disk_format::toml::key::confirm_trash.data()))
    {
        const auto confirm_trash =
            toml::find<bool>(section, config::disk_format::toml::key::confirm_trash.data());
        app_settings.confirm_trash(confirm_trash);
    }

    if (section.contains(config::disk_format::toml::key::thumbnailer_backend.data()))
    {
        const auto thumbnailer_backend =
            toml::find<bool>(section, config::disk_format::toml::key::thumbnailer_backend.data());
        app_settings.thumbnailer_use_api(thumbnailer_backend);
    }
}

static void
config_parse_window(const toml::value& tbl, u64 version)
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
        const auto height = toml::find<u64>(section, config::disk_format::toml::key::height.data());
        app_settings.height(height);
    }

    if (section.contains(config::disk_format::toml::key::width.data()))
    {
        const auto width = toml::find<u64>(section, config::disk_format::toml::key::width.data());
        app_settings.width(width);
    }

    if (section.contains(config::disk_format::toml::key::maximized.data()))
    {
        const auto maximized =
            toml::find<bool>(section, config::disk_format::toml::key::maximized.data());
        app_settings.maximized(maximized);
    }
}

static void
config_parse_interface(const toml::value& tbl, u64 version)
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
        const auto always_show_tabs =
            toml::find<bool>(section, config::disk_format::toml::key::show_tabs.data());
        app_settings.always_show_tabs(always_show_tabs);
    }

    if (section.contains(config::disk_format::toml::key::show_close.data()))
    {
        const auto show_close_tab_buttons =
            toml::find<bool>(section, config::disk_format::toml::key::show_close.data());
        app_settings.show_close_tab_buttons(show_close_tab_buttons);
    }
}

static void
config_parse_xset(const toml::value& tbl, u64 version)
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
                const xset::var var = enum_value.value();

                if (set->name.starts_with("cstm_"))
                { // custom
                    if (set->lock)
                    {
                        set->lock = false;
                    }
                    xset_set_var(set, var, value);
                }
                else
                { // normal (lock)
                    xset_set_var(set, var, value);
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
