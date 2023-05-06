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

#include <toml.hpp> // toml11

#include <utility>
#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "types.hxx"

#include "settings/disk-format.hxx"

#include "write.hxx"

void
save_user_plugin(const std::filesystem::path& path, xsetpak_t xsetpak)
{
    // Plugin TOML
    const toml::value toml_data = toml::value{
        {TOML_SECTION_VERSION,
         toml::value{
             {TOML_KEY_VERSION, CONFIG_FILE_VERSION},
         }},

        {PLUGIN_FILE_SECTION_PLUGIN,
         toml::value{
             std::move(xsetpak),
         }},
    };

    write_file(path, toml_data);
}
