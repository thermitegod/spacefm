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
#include <optional>
#include <string_view>
#include <vector>

namespace vfs::clipboard
{
enum class mode : std::uint8_t
{
    copy,
    move,
};

struct clipboard_data final
{
    mode mode;
    std::vector<std::filesystem::path> files;
};

[[nodiscard]] bool is_valid() noexcept;

void clear() noexcept;

void set(const clipboard_data& data) noexcept;
void set_text(const std::string_view data) noexcept;

[[nodiscard]] std::optional<clipboard_data> get() noexcept;
[[nodiscard]] std::optional<std::string> get_text() noexcept;
} // namespace vfs::clipboard
