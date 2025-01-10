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

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <ztd/ztd.hxx>

namespace vfs::linux::sysfs
{
std::optional<std::string> get_string(const std::filesystem::path& dir,
                                      const std::string_view attribute) noexcept;
std::optional<i64> get_i64(const std::filesystem::path& dir,
                           const std::string_view attribute) noexcept;
std::optional<u64> get_u64(const std::filesystem::path& dir,
                           const std::string_view attribute) noexcept;
std::optional<f64> get_f64(const std::filesystem::path& dir,
                           const std::string_view attribute) noexcept;
} // namespace vfs::linux::sysfs
