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

#include <algorithm>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <sys/types.h>

#include <ztd/ztd.hxx>

#include "vfs/volume/device-mount.hxx"

#include "logger.hxx"

#undef major
#undef minor
#undef makedev

vfs::device_mount::device_mount(dev_t major, dev_t minor) noexcept : major(major), minor(minor)
{
    mount_points = ztd::join(mounts, ",");

    // logger::debug<logger::vfs>("device {}:{} {}", devmount->major, devmount->minor, devmount->mount_points);
}

std::shared_ptr<vfs::device_mount>
vfs::device_mount::create(dev_t major, dev_t minor) noexcept
{
    struct hack : public vfs::device_mount
    {
        hack(dev_t major, dev_t minor) : device_mount(major, minor) {}
    };

    return std::make_shared<hack>(major, minor);
}
