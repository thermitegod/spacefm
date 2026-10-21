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
#include <memory>
#include <string_view>

#include <sys/types.h>

#include <ztd/ztd.hxx>

#include "vfs/volume/device-mount.hxx"

#undef major
#undef minor
#undef makedev

vfs::device_mount::device_mount(const ztd::statmount& statmount) noexcept
    : mount_id_(statmount.mount_id()), dev_major_(statmount.dev_major()),
      dev_minor_(statmount.dev_minor()), fs_type_(statmount.fs_type()),
      mount_point_(statmount.mount_point()), root_(statmount.root())
{
}

std::shared_ptr<vfs::device_mount>
vfs::device_mount::create(const ztd::statmount& statmount) noexcept
{
    struct hack : public vfs::device_mount
    {
        hack(const ztd::statmount& statmount) : device_mount(statmount) {}
    };
    return std::make_shared<hack>(statmount);
}

std::uint64_t
vfs::device_mount::mount_id() const noexcept
{
    return mount_id_;
}

std::uint32_t
vfs::device_mount::dev_major() const noexcept
{
    return dev_major_;
}

std::uint32_t
vfs::device_mount::dev_minor() const noexcept
{
    return dev_minor_;
}

std::string_view
vfs::device_mount::fs_type() const noexcept
{
    return fs_type_;
}

const std::filesystem::path&
vfs::device_mount::mount_point() const noexcept
{
    return mount_point_;
}

const std::filesystem::path&
vfs::device_mount::root() const noexcept
{
    return root_;
}
