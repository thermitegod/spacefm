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

#include <vector>

#include <utility>

const std::string randhex8() noexcept;

bool have_rw_access(std::string_view path) noexcept;
bool have_x_access(std::string_view path) noexcept;

bool dir_has_files(std::string_view path) noexcept;

const std::string replace_line_subs(std::string_view line) noexcept;

const std::pair<std::string, std::string> get_name_extension(std::string_view full_name) noexcept;

void open_in_prog(std::string_view path) noexcept;

const std::string bash_quote(std::string_view str) noexcept;

const std::string clean_label(std::string_view menu_label, bool kill_special, bool escape) noexcept;

const std::string get_valid_su() noexcept;
