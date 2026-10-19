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

#include <memory>
#include <string>
#include <vector>

#include <sys/types.h>
namespace vfs
{
class device_mount
{
  private:
    device_mount(dev_t major, dev_t minor) noexcept;

  public:
    device_mount() = delete;
    device_mount(const device_mount& other) = delete;
    device_mount(device_mount&& other) = delete;
    device_mount& operator=(const device_mount& other) = delete;
    device_mount& operator=(device_mount&& other) = delete;
    ~device_mount() = default;

    [[nodiscard]] static std::shared_ptr<device_mount> create(dev_t major, dev_t minor) noexcept;

    dev_t major{0};
    dev_t minor{0};
    std::string mount_points;
    std::string fstype;
    std::vector<std::string> mounts;
};
} // namespace vfs
