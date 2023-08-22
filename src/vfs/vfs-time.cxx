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

#include <format>

#include <chrono>

#include <sstream>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "settings/app.hxx"

#include "vfs/vfs-time.hxx"

#if 1

const std::string
vfs_create_display_date(const std::time_t time) noexcept
{
    std::tm* local_time = std::localtime(&time);
    std::stringstream ss;
    ss << std::put_time(local_time, app_settings.date_format().data());
    return ss.str();
}

#else

// TODO - support app_settings.date_format()

const std::string
vfs_create_display_date(const std::time_t time) noexcept
{
    const auto point = std::chrono::system_clock::from_time_t(time);

    const auto date = std::chrono::floor<std::chrono::days>(point);

    const auto midnight = point - std::chrono::floor<std::chrono::days>(point);
    const auto hours = std::chrono::duration_cast<std::chrono::hours>(midnight);
    const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(midnight - hours);
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(midnight - hours - minutes);

    return std::format("{0:%Y-%m-%d} {1:%H}:{2:%M}:{3:%S}", date, hours, minutes, seconds);
}

#endif
