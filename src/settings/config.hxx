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
constexpr std::string_view version{"Version"};
constexpr std::string_view general{"General"};
constexpr std::string_view window{"Window"};
constexpr std::string_view interface{"Interface"};
constexpr std::string_view xset{"XSet"};
} // namespace toml::section
namespace toml::key
{
// TOML config on disk names - TOML section keys
constexpr std::string_view version{"version"};

// General keys
constexpr std::string_view show_thumbnail{"show_thumbnail"};
constexpr std::string_view max_thumb_size{"max_thumb_size"};
constexpr std::string_view icon_size_big{"icon_size_big"};
constexpr std::string_view icon_size_small{"icon_size_small"};
constexpr std::string_view icon_size_tool{"icon_size_tool"};
constexpr std::string_view single_click{"single_click"};
constexpr std::string_view single_hover{"single_hover"};
constexpr std::string_view use_si_prefix{"use_si_prefix"};
constexpr std::string_view click_execute{"click_executes"};
constexpr std::string_view confirm{"confirm"};
constexpr std::string_view confirm_delete{"confirm_delete"};
constexpr std::string_view confirm_trash{"confirm_trash"};
constexpr std::string_view thumbnailer_backend{"thumbnailer_backend"};

// Window keys
constexpr std::string_view height{"height"};
constexpr std::string_view width{"width"};
constexpr std::string_view maximized{"maximized"};

// Interface keys
constexpr std::string_view show_tabs{"always_show_tabs"};
constexpr std::string_view show_close{"show_close_tab_buttons"};
constexpr std::string_view new_tab_here{"new_tab_here"};

// XSet keys
// The names for XSet member variables are deduced using magic_enum::enum_name()
} // namespace toml::key
} // namespace disk_format
} // namespace config
