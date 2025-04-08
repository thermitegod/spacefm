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

#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <string_view>

#include <ztd/ztd.hxx>

#include "settings/settings.hxx"

namespace config
{
// map<var, value>
using setvars_t = std::map<std::string_view, std::string>;
// map<xset_name, setvars_t>
using xsetpak_t = std::map<std::string_view, setvars_t>;

struct config_file_data
{
    u64 version;
    datatype::settings settings;
    xsetpak_t xset;
};

void load(const std::filesystem::path& session,
          const std::shared_ptr<config::settings>& settings) noexcept;
void save(const std::filesystem::path& session,
          const std::shared_ptr<config::settings>& settings) noexcept;

namespace disk_format
{
constexpr u64 version{4}; // 3.0.0-dev
constexpr std::string_view filename{"session.json"};

// The delimiter used in the config file to
// store a list of tabs as a single string
constexpr std::string_view tab_delimiter{"///"};
} // namespace disk_format
} // namespace config
