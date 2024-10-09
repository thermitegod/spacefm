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

#include <magic_enum.hpp>

#if defined(HAVE_DEPRECATED)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#pragma GCC diagnostic ignored "-Wunused-result"
#include <toml.hpp> // toml11
#pragma GCC diagnostic pop
#endif

#include <glaze/glaze.hpp>

#include <ztd/ztd.hxx>

#include "logger.hxx"

#include "xset/xset.hxx"

#include "settings/upgrade/config-upgrade.hxx"

#include "settings/settings.hxx"
#include "settings/config.hxx"

#if defined(HAVE_DEPRECATED)

static u64
get_config_file_version(const toml::value& tbl) noexcept
{
    if (!tbl.contains(config::disk_format::toml::section::version))
    {
        logger::error("config missing TOML section [{}]",
                      config::disk_format::toml::section::version);
        return 0;
    }

    const auto& version = toml::find(tbl, config::disk_format::toml::section::version);

    return toml::find<u64>(version, config::disk_format::toml::key::version);
}

static void
config_parse_general(const toml::value& tbl, u64 version) noexcept
{
    (void)version;

    if (!tbl.contains(config::disk_format::toml::section::general))
    {
        logger::error("config missing TOML section [{}]",
                      config::disk_format::toml::section::general);
        return;
    }

    const auto& section = toml::find(tbl, config::disk_format::toml::section::general);

    if (section.contains(config::disk_format::toml::key::show_thumbnail))
    {
        config::settings.show_thumbnails =
            toml::find<bool>(section, config::disk_format::toml::key::show_thumbnail);
    }

    if (section.contains(config::disk_format::toml::key::thumbnail_max_size))
    {
        config::settings.thumbnail_max_size =
            toml::find<u32>(section, config::disk_format::toml::key::thumbnail_max_size) << 10;
    }

    if (section.contains(config::disk_format::toml::key::icon_size_big))
    {
        config::settings.icon_size_big =
            toml::find<i32>(section, config::disk_format::toml::key::icon_size_big);
    }

    if (section.contains(config::disk_format::toml::key::icon_size_small))
    {
        config::settings.icon_size_small =
            toml::find<i32>(section, config::disk_format::toml::key::icon_size_small);
    }

    if (section.contains(config::disk_format::toml::key::icon_size_tool))
    {
        config::settings.icon_size_tool =
            toml::find<i32>(section, config::disk_format::toml::key::icon_size_tool);
    }

    if (section.contains(config::disk_format::toml::key::single_click))
    {
        config::settings.single_click =
            toml::find<bool>(section, config::disk_format::toml::key::single_click);
    }

    if (section.contains(config::disk_format::toml::key::single_hover))
    {
        config::settings.single_hover =
            toml::find<bool>(section, config::disk_format::toml::key::single_hover);
    }

    if (section.contains(config::disk_format::toml::key::use_si_prefix))
    {
        config::settings.use_si_prefix =
            toml::find<bool>(section, config::disk_format::toml::key::use_si_prefix);
    }

    if (section.contains(config::disk_format::toml::key::click_execute))
    {
        config::settings.click_executes =
            toml::find<bool>(section, config::disk_format::toml::key::click_execute);
    }

    if (section.contains(config::disk_format::toml::key::confirm))
    {
        config::settings.confirm =
            toml::find<bool>(section, config::disk_format::toml::key::confirm);
    }

    if (section.contains(config::disk_format::toml::key::confirm_delete))
    {
        config::settings.confirm_delete =
            toml::find<bool>(section, config::disk_format::toml::key::confirm_delete);
    }

    if (section.contains(config::disk_format::toml::key::confirm_trash))
    {
        config::settings.confirm_trash =
            toml::find<bool>(section, config::disk_format::toml::key::confirm_trash);
    }

    if (section.contains(config::disk_format::toml::key::thumbnailer_backend))
    {
        config::settings.thumbnailer_use_api =
            toml::find<bool>(section, config::disk_format::toml::key::thumbnailer_backend);
    }
}

static void
config_parse_window(const toml::value& tbl, u64 version) noexcept
{
    (void)version;

    if (!tbl.contains(config::disk_format::toml::section::window))
    {
        logger::error("config missing TOML section [{}]",
                      config::disk_format::toml::section::window);
        return;
    }

    const auto& section = toml::find(tbl, config::disk_format::toml::section::window);

    if (section.contains(config::disk_format::toml::key::height))
    {
        config::settings.height = toml::find<u64>(section, config::disk_format::toml::key::height);
    }

    if (section.contains(config::disk_format::toml::key::width))
    {
        config::settings.width = toml::find<u64>(section, config::disk_format::toml::key::width);
    }

    if (section.contains(config::disk_format::toml::key::maximized))
    {
        config::settings.maximized =
            toml::find<bool>(section, config::disk_format::toml::key::maximized);
    }
}

static void
config_parse_interface(const toml::value& tbl, u64 version) noexcept
{
    (void)version;

    if (!tbl.contains(config::disk_format::toml::section::interface))
    {
        logger::error("config missing TOML section [{}]",
                      config::disk_format::toml::section::interface);
        return;
    }

    const auto& section = toml::find(tbl, config::disk_format::toml::section::interface);

    if (section.contains(config::disk_format::toml::key::show_tabs))
    {
        config::settings.always_show_tabs =
            toml::find<bool>(section, config::disk_format::toml::key::show_tabs);
    }

    if (section.contains(config::disk_format::toml::key::show_close))
    {
        config::settings.show_close_tab_buttons =
            toml::find<bool>(section, config::disk_format::toml::key::show_close);
    }

    if (section.contains(config::disk_format::toml::key::new_tab_here))
    {
        config::settings.new_tab_here =
            toml::find<bool>(section, config::disk_format::toml::key::new_tab_here);
    }

    if (section.contains(config::disk_format::toml::key::show_toolbar_home))
    {
        config::settings.show_toolbar_home =
            toml::find<bool>(section, config::disk_format::toml::key::show_toolbar_home);
    }

    if (section.contains(config::disk_format::toml::key::show_toolbar_refresh))
    {
        config::settings.show_toolbar_refresh =
            toml::find<bool>(section, config::disk_format::toml::key::show_toolbar_refresh);
    }

    if (section.contains(config::disk_format::toml::key::show_toolbar_search))
    {
        config::settings.show_toolbar_search =
            toml::find<bool>(section, config::disk_format::toml::key::show_toolbar_search);
    }
}

static void
config_parse_xset(const toml::value& tbl, u64 version) noexcept
{
    (void)version;

    // loop over all of [[XSet]]
    for (const auto& section :
         toml::find<toml::array>(tbl, config::disk_format::toml::section::xset))
    {
        // get [XSet.name] and all vars
        for (const auto& [toml_name, toml_vars] : section.as_table())
        {
            const auto enum_name_value = magic_enum::enum_cast<xset::name>(toml_name);
            if (!enum_name_value.has_value())
            {
                logger::warn("Invalid xset::name enum name, xset::var::{}", toml_name);
                continue;
            }
            const auto set = xset::set::get(toml_name);

            // get var and value
            for (const auto& [toml_var, toml_value] : toml_vars.as_table())
            {
                const std::string setvar = toml_var;
                const std::string value =
                    ztd::strip(toml::format(toml_value, std::numeric_limits<usize>::max()), "\"");

                // logger::info("name: {} | var: {} | value: {}", name, setvar, value);

                const auto enum_var_value = magic_enum::enum_cast<xset::var>(setvar);
                if (!enum_var_value.has_value())
                {
                    logger::warn("Invalid xset::var enum name, xset::var::{}", toml_name);
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
}

#endif

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
    if (session.extension() == ".json")
    {
        logger::info("Loading JSON config");

        config_file_data config_data;
        std::string buffer;
        const auto ec = glz::read_file_json(config_data, session.c_str(), buffer);

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
    else if (session.extension() == ".toml")
    { // Legacy toml config
#if defined(HAVE_DEPRECATED)
        logger::info("Loading TOML config");

        try
        {
            const auto tbl = toml::parse(session);
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
            logger::error("Config file parsing failed: {}", e.what());
            return;
        }
#else
        logger::error("Built without support for TOML config files");
#endif
    }
    else
    {
        logger::error("Unsupported config file: {}", session.c_str());
    }
}
