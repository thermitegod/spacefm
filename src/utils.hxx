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

#include <string>
#include <string_view>

#include <filesystem>

#include <vector>

#include <utility>

bool have_rw_access(const std::filesystem::path& path) noexcept;
bool have_x_access(const std::filesystem::path& path) noexcept;

bool dir_has_files(const std::string_view path) noexcept;

const std::string replace_line_subs(const std::string_view line) noexcept;

const std::pair<std::string, std::string>
get_name_extension(const std::string_view filename) noexcept;

void open_in_prog(const std::string_view path) noexcept;

const std::string clean_label(const std::string_view menu_label, bool kill_special,
                              bool escape) noexcept;

const std::string get_valid_su() noexcept;
