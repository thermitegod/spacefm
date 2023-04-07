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

#include <cassert>

#include "libudevpp.hxx"

std::optional<libudev::monitor>
libudev::udev::monitor_new_from_netlink(const std::string_view name)
{
    assert(std::string(name) == "udev" || std::string(name) == "kernel");

    const auto monitor = udev_monitor_new_from_netlink(this->handle.get(), name.data());
    if (monitor)
    {
        return libudev::monitor(monitor);
    }
    return std::nullopt;
}

std::optional<libudev::monitor>
libudev::udev::monitor_new_from_netlink(netlink_type name)
{
    if (name == netlink_type::udev)
    {
        const auto monitor = udev_monitor_new_from_netlink(this->handle.get(), "udev");
        if (monitor)
        {
            return libudev::monitor(monitor);
        }
    }
    else if (name == netlink_type::kernel)
    {
        const auto monitor = udev_monitor_new_from_netlink(this->handle.get(), "kernel");
        if (monitor)
        {
            return libudev::monitor(monitor);
        }
    }
    return std::nullopt;
}

std::optional<libudev::device>
libudev::udev::device_from_syspath(const std::filesystem::path& syspath) const noexcept
{
    const auto device = udev_device_new_from_syspath(this->handle.get(), syspath.c_str());
    if (device)
    {
        return libudev::device(device);
    }
    return std::nullopt;
}

std::optional<libudev::device>
libudev::udev::device_from_devnum(char type, dev_t devnum) const noexcept
{
    assert(type == 'b' || type == 'c');
    const auto device = udev_device_new_from_devnum(this->handle.get(), type, devnum);
    if (device)
    {
        return libudev::device(device);
    }
    return std::nullopt;
}

std::optional<libudev::device>
libudev::udev::device_from_devnum(device_type type, dev_t devnum) const noexcept
{
    if (type == device_type::block)
    {
        const auto device = udev_device_new_from_devnum(this->handle.get(), 'b', devnum);
        if (device)
        {
            return libudev::device(device);
        }
    }
    else if (type == device_type::character)
    {
        const auto device = udev_device_new_from_devnum(this->handle.get(), 'c', devnum);
        if (device)
        {
            return libudev::device(device);
        }
    }
    return std::nullopt;
}

libudev::enumerate
libudev::udev::enumerate_new()
{
    return libudev::enumerate(udev_enumerate_new(this->handle.get()));
}

bool
libudev::udev::is_initialized() const noexcept
{
    return this->handle != nullptr;
}
