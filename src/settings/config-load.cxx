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

#include "xset/xset.hxx"

#include "ptk/ptk-error.hxx"

#include "vfs/vfs-user-dir.hxx"

#include "settings/app.hxx"
#include "settings/config-load.hxx"
#include "settings/names.hxx"

#ifdef HAVE_DEPRECATED_INI_LOADING
static void xset_parse(std::string& line);

using SettingsParseFunc = void (*)(std::string& line);

static void
parse_general_settings(std::string& line)
{
    usize sep = line.find("=");
    if (sep == std::string::npos)
        return;

    line = ztd::strip(line);

    if (line.at(0) == '#')
        return;

    std::string token = line.substr(0, sep);
    std::string value = line.substr(sep + 1, std::string::npos - 1);

    // remove any quotes
    value = ztd::replace(value, "\"", "");

    if (value.empty())
        return;

    if (ztd::same(token, INI_KEY_SHOW_THUMBNAIL))
        app_settings.set_show_thumbnail(std::stoi(value));
    else if (ztd::same(token, INI_KEY_MAX_THUMB_SIZE))
        app_settings.set_max_thumb_size(std::stoi(value) << 10);
    else if (ztd::same(token, INI_KEY_ICON_SIZE_BIG))
        app_settings.set_icon_size_big(std::stoi(value));
    else if (ztd::same(token, INI_KEY_ICON_SIZE_SMALL))
        app_settings.set_icon_size_small(std::stoi(value));
    else if (ztd::same(token, INI_KEY_ICON_SIZE_TOOL))
        app_settings.set_icon_size_tool(std::stoi(value));
    else if (ztd::same(token, INI_KEY_SINGLE_CLICK))
        app_settings.set_single_click(std::stoi(value));
    else if (ztd::same(token, INI_KEY_NO_SINGLE_HOVER))
        app_settings.set_single_hover(std::stoi(value));
    else if (ztd::same(token, INI_KEY_SORT_ORDER))
        app_settings.set_sort_order(std::stoi(value));
    else if (ztd::same(token, INI_KEY_SORT_TYPE))
        app_settings.set_sort_type(std::stoi(value));
    else if (ztd::same(token, INI_KEY_USE_SI_PREFIX))
        app_settings.set_use_si_prefix(std::stoi(value));
    else if (ztd::same(token, INI_KEY_NO_EXECUTE))
        app_settings.set_click_executes(!std::stoi(value));
    else if (ztd::same(token, INI_KEY_NO_CONFIRM))
        app_settings.set_confirm(!std::stoi(value));
    else if (ztd::same(token, INI_KEY_NO_CONFIRM_TRASH))
        app_settings.set_confirm_trash(!std::stoi(value));
}

static void
parse_window_state(std::string& line)
{
    usize sep = line.find("=");
    if (sep == std::string::npos)
        return;

    line = ztd::strip(line);

    if (line.at(0) == '#')
        return;

    std::string token = line.substr(0, sep);
    std::string value = line.substr(sep + 1, std::string::npos - 1);

    // remove any quotes
    value = ztd::replace(value, "\"", "");

    if (value.empty())
        return;

    if (ztd::same(token, INI_KEY_WIDTH))
        app_settings.set_width(std::stoi(value));
    else if (ztd::same(token, INI_KEY_HEIGHT))
        app_settings.set_height(std::stoi(value));
    else if (ztd::same(token, INI_KEY_MAXIMIZED))
        app_settings.set_maximized(std::stoi(value));
}

static void
parse_interface_settings(std::string& line)
{
    usize sep = line.find("=");
    if (sep == std::string::npos)
        return;

    line = ztd::strip(line);

    if (line.at(0) == '#')
        return;

    std::string token = line.substr(0, sep);
    std::string value = line.substr(sep + 1, std::string::npos - 1);

    // remove any quotes
    value = ztd::replace(value, "\"", "");

    if (value.empty())
        return;

    if (ztd::same(token, INI_KEY_SHOW_TABS))
        app_settings.set_always_show_tabs(std::stoi(value));
    else if (ztd::same(token, INI_KEY_SHOW_CLOSE))
        app_settings.set_show_close_tab_buttons(std::stoi(value));
}

static void
xset_parse(std::string& line)
{
    usize sep = line.find("=");
    if (sep == std::string::npos)
        return;

    usize sep2 = line.find("-");
    if (sep2 == std::string::npos)
        return;

    line = ztd::strip(line);

    if (line.at(0) == '#')
        return;

    std::string token = line.substr(0, sep2);
    std::string value = line.substr(sep + 1, std::string::npos - 1);
    std::string token_var = line.substr(sep2 + 1, sep - sep2 - 1);

    XSetVar var;
    try
    {
        var = xset_get_xsetvar_from_name(token_var);
    }
    catch (const std::logic_error& e)
    {
        std::string msg = fmt::format("XSet parse error:\n\n{}", e.what());
        ptk_show_error(nullptr, "Error", e.what());
        return;
    }

    // remove any quotes
    value = ztd::replace(value, "\"", "");

    if (value.empty())
        return;

    xset_t set = xset_get(token);
    if (ztd::startswith(set->name, "cstm_") || ztd::startswith(set->name, "hand_"))
    { // custom
        if (set->lock)
            set->lock = false;
    }
    else
    { // normal (lock)
        xset_set_var(set, var, value);
    }
}
#endif // Deprecated INI loader - end

static u64
get_config_file_version(const toml::value& data)
{
    const auto& version = toml::find(data, TOML_SECTION_VERSION);

    const auto config_version = toml::find<u64>(version, TOML_KEY_VERSION);
    return config_version;
}

static void
config_parse_general(const toml::value& toml_data, u64 version)
{
    (void)version;

    const auto& section = toml::find(toml_data, TOML_SECTION_GENERAL);

    const auto show_thumbnail = toml::find<bool>(section, TOML_KEY_SHOW_THUMBNAIL);
    app_settings.set_show_thumbnail(show_thumbnail);

    const auto max_thumb_size = toml::find<u64>(section, TOML_KEY_MAX_THUMB_SIZE);
    app_settings.set_max_thumb_size(max_thumb_size << 10);

    const auto icon_size_big = toml::find<u64>(section, TOML_KEY_ICON_SIZE_BIG);
    app_settings.set_icon_size_big(icon_size_big);

    const auto icon_size_small = toml::find<u64>(section, TOML_KEY_ICON_SIZE_SMALL);
    app_settings.set_icon_size_small(icon_size_small);

    const auto icon_size_tool = toml::find<u64>(section, TOML_KEY_ICON_SIZE_TOOL);
    app_settings.set_icon_size_tool(icon_size_tool);

    const auto single_click = toml::find<bool>(section, TOML_KEY_SINGLE_CLICK);
    app_settings.set_single_click(single_click);

    const auto single_hover = toml::find<bool>(section, TOML_KEY_SINGLE_HOVER);
    app_settings.set_single_hover(single_hover);

    const auto sort_order = toml::find<u64>(section, TOML_KEY_SORT_ORDER);
    app_settings.set_sort_order(sort_order);

    const auto sort_type = toml::find<u64>(section, TOML_KEY_SORT_TYPE);
    app_settings.set_sort_type(sort_type);

    const auto use_si_prefix = toml::find<bool>(section, TOML_KEY_USE_SI_PREFIX);
    app_settings.set_use_si_prefix(use_si_prefix);

    const auto click_executes = toml::find<bool>(section, TOML_KEY_CLICK_EXECUTE);
    app_settings.set_click_executes(click_executes);

    const auto confirm = toml::find<bool>(section, TOML_KEY_CONFIRM);
    app_settings.set_confirm(confirm);

    const auto confirm_delete = toml::find<bool>(section, TOML_KEY_CONFIRM_DELETE);
    app_settings.set_confirm_delete(confirm_delete);

    const auto confirm_trash = toml::find<bool>(section, TOML_KEY_CONFIRM_TRASH);
    app_settings.set_confirm_trash(confirm_trash);
}

static void
config_parse_window(const toml::value& toml_data, u64 version)
{
    (void)version;

    const auto& section = toml::find(toml_data, TOML_SECTION_WINDOW);

    const auto height = toml::find<u64>(section, TOML_KEY_HEIGHT);
    app_settings.set_height(height);

    const auto width = toml::find<u64>(section, TOML_KEY_WIDTH);
    app_settings.set_width(width);

    const auto maximized = toml::find<bool>(section, TOML_KEY_MAXIMIZED);
    app_settings.set_maximized(maximized);
}

static void
config_parse_interface(const toml::value& toml_data, u64 version)
{
    (void)version;

    const auto& section = toml::find(toml_data, TOML_SECTION_INTERFACE);

    const auto always_show_tabs = toml::find<bool>(section, TOML_KEY_SHOW_TABS);
    app_settings.set_always_show_tabs(always_show_tabs);

    const auto show_close_tab_buttons = toml::find<bool>(section, TOML_KEY_SHOW_CLOSE);
    app_settings.set_show_close_tab_buttons(show_close_tab_buttons);
}

static void
config_parse_xset(const toml::value& toml_data, u64 version)
{
    (void)version;

    // loop over all of [[XSet]]
    for (const auto& section : toml::find<toml::array>(toml_data, TOML_SECTION_XSET))
    {
        // get [XSet.name] and all vars
        for (const auto& [toml_name, toml_vars] : section.as_table())
        {
            // get var and value
            for (const auto& [toml_var, toml_value] : toml_vars.as_table())
            {
                std::stringstream ss_name;
                std::stringstream ss_var;
                std::stringstream ss_value;

                ss_name << toml_name;
                ss_var << toml_var;
                ss_value << toml_value;

                const std::string name{ss_name.str()};
                const std::string setvar{ss_var.str()};
                const std::string value{ztd::strip(ss_value.str(), "\"")};

                // LOG_INFO("name: {} | var: {} | value: {}", name, setvar, value);

                XSetVar var;
                try
                {
                    var = xset_get_xsetvar_from_name(setvar);
                }
                catch (const std::logic_error& e)
                {
                    const std::string msg = fmt::format("XSet parse error:\n\n{}", e.what());
                    ptk_show_error(nullptr, "Error", e.what());
                    return;
                }

                xset_t set = xset_get(name);
                if (ztd::startswith(set->name, "cstm_") || ztd::startswith(set->name, "hand_"))
                { // custom
                    if (set->lock)
                        set->lock = false;
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

#ifndef HAVE_DEPRECATED_INI_LOADING

void
load_user_confing(std::string_view session)
{
    toml::value toml_data;
    try
    {
        toml_data = toml::parse(session.data());
        // DEBUG
        // std::cout << "###### TOML PARSE ######" << "\n\n";
        // std::cout << toml_data << "\n\n";
    }
    catch (const toml::syntax_error& e)
    {
        LOG_ERROR("Config file parsing failed: {}", e.what());
        return;
    }

    const u64 version = get_config_file_version(toml_data);

    config_parse_general(toml_data, version);
    config_parse_window(toml_data, version);
    config_parse_interface(toml_data, version);
    config_parse_xset(toml_data, version);
}

#else

void
load_user_confing(std::string_view session, bool load_deprecated_ini_config)
{
    if (!load_deprecated_ini_config)
    { // TOML
        // LOG_INFO("Parse TOML");

        toml::value toml_data;
        try
        {
            toml_data = toml::parse(session.data());
            // DEBUG
            // std::cout << "###### TOML PARSE ######" << "\n\n";
            // std::cout << toml_data << "\n\n";
        }
        catch (const toml::syntax_error& e)
        {
            LOG_ERROR("Config file parsing failed: {}", e.what());
            return;
        }

        const u64 version = get_config_file_version(toml_data);

        config_parse_general(toml_data, version);
        config_parse_window(toml_data, version);
        config_parse_interface(toml_data, version);
        config_parse_xset(toml_data, version);
    }
    else
    { // INI
        // LOG_INFO("Parse INI");

        std::string line;
        std::ifstream file(session.data());
        if (file.is_open())
        {
            SettingsParseFunc func = nullptr;

            while (std::getline(file, line))
            {
                if (line.empty())
                    continue;

                if (line.at(0) == '[')
                {
                    if (ztd::same(line, INI_SECTION_GENERAL))
                        func = &parse_general_settings;
                    else if (ztd::same(line, INI_SECTION_WINDOW))
                        func = &parse_window_state;
                    else if (ztd::same(line, INI_SECTION_INTERFACE))
                        func = &parse_interface_settings;
                    else if (ztd::same(line, INI_SECTION_MOD))
                        func = &xset_parse;
                    else
                        func = nullptr;
                    continue;
                }

                if (func)
                    (*func)(line);
            }
        }
        file.close();
    }
}

#endif
