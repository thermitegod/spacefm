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

#include <array>
#include <span>

using bookmark_t = std::array<std::filesystem::path, 2>;

[[nodiscard]] const std::span<bookmark_t> get_all_bookmarks() noexcept;

void load_bookmarks() noexcept;
void save_bookmarks() noexcept;

void add_bookmarks(const std::filesystem::path& book_path) noexcept;
void remove_bookmarks() noexcept;
