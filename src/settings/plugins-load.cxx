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
load_user_plugin(const std::string_view plug_dir, PluginUse* use, const std::string_view plugin,
                 plugin_func_t plugin_func)
{
    toml::value tbl;
    try
    {
        tbl = toml::parse(plugin.data());
        // DEBUG
        // std::cout << "###### TOML PARSE ######" << "\n\n";
        // std::cout << tbl << "\n\n";
    }
    catch (const toml::syntax_error& e)
    {
        const std::string msg =
            fmt::format("Plugin file parsing failed:\n\"{}\"\n{}", plugin, e.what());
        ztd::logger::error("{}", msg);
        ptk_show_error(nullptr, "Plugin Load Error", msg);
        return false;
    }

    bool plugin_good = false;

    if (!tbl.contains(PLUGIN_FILE_SECTION_PLUGIN))
    {
        ztd::logger::error("plugin missing TOML section [{}]", PLUGIN_FILE_SECTION_PLUGIN);
        return plugin_good;
    }

    // loop over all of [[Plugin]]
    for (const auto& section : toml::find<toml::array>(tbl, PLUGIN_FILE_SECTION_PLUGIN))
    {
        // get [Plugin.name] and all vars
        for (const auto& [toml_name, toml_vars] : section.as_table())
        {
            const std::string name = toml_name.data();
            // get var and value
            for (const auto& [toml_var, toml_value] : toml_vars.as_table())
            {
                const std::string setvar = toml_var.data();
                const std::string value =
                    ztd::strip(toml::format(toml_value, std::numeric_limits<usize>::max()), "\"");

                plugin_func(plug_dir, use, name, setvar, value);

                if (!plugin_good)
                {
                    plugin_good = true;
                }
            }
        }
    }

    return plugin_good;
}
