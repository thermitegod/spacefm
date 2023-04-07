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

#include <optional>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

namespace vfs::linux::sysfs
{
    const std::optional<std::string> get_string(std::string_view dir, std::string_view attribute);
    const std::optional<i64> get_i64(std::string_view dir, std::string_view attribute);
    const std::optional<u64> get_u64(std::string_view dir, std::string_view attribute);
    const std::optional<f64> get_f64(std::string_view dir, std::string_view attribute);

    bool file_exists(std::string_view dir, std::string_view attribute);
    const std::optional<std::string> resolve_link(std::string_view path, std::string_view name);
} // namespace vfs::linux::sysfs
