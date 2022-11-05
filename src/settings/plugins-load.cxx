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

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include <toml.hpp> // toml11

#include "types.hxx"

#include "ptk/ptk-error.hxx"

#include "settings/disk-format.hxx"

#include "settings/plugins-load.hxx"

bool
load_user_plugin(std::string_view plug_dir, PluginUse* use, std::string_view plugin,
                 plugin_func_t plugin_func)
{
    toml::value toml_data;
    try
    {
        toml_data = toml::parse(plugin.data());
        // DEBUG
        // std::cout << "###### TOML PARSE ######" << "\n\n";
        // std::cout << toml_data << "\n\n";
    }
    catch (const toml::syntax_error& e)
    {
        const std::string msg =
            fmt::format("Plugin file parsing failed:\n\"{}\"\n{}", plugin, e.what());
        LOG_ERROR("{}", msg);
        ptk_show_error(nullptr, "Plugin Load Error", msg);
        return false;
    }

    bool plugin_good = false;

    // const u64 version = get_config_file_version(toml_data);

    // loop over all of [[Plugin]]
    for (const auto& section : toml::find<toml::array>(toml_data, PLUGIN_FILE_SECTION_PLUGIN))
    {
        // get [Plugin.name] and all vars
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
                const std::string var{ss_var.str()};
                const std::string value{ztd::strip(ss_value.str(), "\"")};

                plugin_func(plug_dir, use, name, var, value);

                if (!plugin_good)
                    plugin_good = true;
            }
        }
    }

    return plugin_good;
}
