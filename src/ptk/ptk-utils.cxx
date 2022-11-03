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

#include <filesystem>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "ptk/ptk-utils.hxx"

const std::string
get_real_link_target(std::string_view link_path)
{
    std::string target_path;
    try
    {
        target_path = std::filesystem::read_symlink(link_path);
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        LOG_WARN("{}", e.what());
        target_path = link_path.data();
    }

    return target_path;
}
