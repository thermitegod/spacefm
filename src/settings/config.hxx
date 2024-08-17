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

#include <string_view>

#include <filesystem>

#include <ztd/ztd.hxx>

namespace config
{
void load(const std::filesystem::path& session) noexcept;
void save() noexcept;

namespace disk_format
{
constexpr u64 version{3}; // 3.0.0-dev
constexpr std::string_view filename{"session.toml"};

// The delimiter used in the config file to
// store a list of tabs as single sting
constexpr std::string_view tab_delimiter{"///"};

namespace toml::section
{
// toml11 does not work with std::string_view as a key,
// have to use std::string_view::data()

// TOML config on disk names - TOML sections
constexpr const char* const version{"Version"};
constexpr const char* const general{"General"};
constexpr const char* const window{"Window"};
constexpr const char* const interface{"Interface"};
constexpr const char* const xset{"XSet"};
} // namespace toml::section
namespace toml::key
{
// TOML config on disk names - TOML section keys
constexpr const char* const version{"version"};

// General keys
constexpr const char* const show_thumbnail{"show_thumbnail"};
constexpr const char* const thumbnail_max_size{"max_thumb_size"};
constexpr const char* const icon_size_big{"icon_size_big"};
constexpr const char* const icon_size_small{"icon_size_small"};
constexpr const char* const icon_size_tool{"icon_size_tool"};
constexpr const char* const single_click{"single_click"};
constexpr const char* const single_hover{"single_hover"};
constexpr const char* const use_si_prefix{"use_si_prefix"};
constexpr const char* const click_execute{"click_executes"};
constexpr const char* const confirm{"confirm"};
constexpr const char* const confirm_delete{"confirm_delete"};
constexpr const char* const confirm_trash{"confirm_trash"};
constexpr const char* const thumbnailer_backend{"thumbnailer_backend"};

// Window keys
constexpr const char* const height{"height"};
constexpr const char* const width{"width"};
constexpr const char* const maximized{"maximized"};

// Interface keys
constexpr const char* const show_tabs{"always_show_tabs"};
constexpr const char* const show_close{"show_close_tab_buttons"};
constexpr const char* const new_tab_here{"new_tab_here"};
constexpr const char* const show_toolbar_home{"show_toolbar_home_button"};
constexpr const char* const show_toolbar_refresh{"show_toolbar_refresh_button"};
constexpr const char* const show_toolbar_search{"show_toolbar_search_bar"};

// XSet keys
// The names for XSet member variables are deduced using magic_enum::enum_name()
} // namespace toml::key
} // namespace disk_format
} // namespace config
