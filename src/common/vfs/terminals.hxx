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

#include <expected>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace vfs
{
class terminals final
{
  public:
    [[nodiscard]] static std::expected<std::string, std::error_code>
    create_command(const std::string_view terminal, const std::string_view command) noexcept;

    [[nodiscard]] static std::vector<std::string> supported_names() noexcept;
};
} // namespace vfs
