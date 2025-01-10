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

#include <vector>

#include "libudevpp.hxx"

bool
libudev::enumerate::is_initialized() const noexcept
{
    return udev_enumerate_get_udev(this->handle.get()) != nullptr;
}

void
libudev::enumerate::add_match_subsystem(const std::string_view subsystem) const noexcept
{
    udev_enumerate_add_match_subsystem(this->handle.get(), subsystem.data());
}

void
libudev::enumerate::add_nomatch_subsystem(const std::string_view subsystem) const noexcept
{
    udev_enumerate_add_nomatch_subsystem(this->handle.get(), subsystem.data());
}

void
libudev::enumerate::add_match_sysattr(const std::string_view sysattr,
                                      const std::string_view value) const noexcept
{
    udev_enumerate_add_match_sysattr(this->handle.get(),
                                     sysattr.data(),
                                     value.length() > 0 ? value.data() : nullptr);
}

void
libudev::enumerate::add_nomatch_sysattr(const std::string_view sysattr,
                                        const std::string_view value) const noexcept
{
    udev_enumerate_add_nomatch_sysattr(this->handle.get(),
                                       sysattr.data(),
                                       value.length() > 0 ? value.data() : nullptr);
}

void
libudev::enumerate::add_match_property(const std::string_view property,
                                       const std::string_view value) const noexcept
{
    udev_enumerate_add_match_property(this->handle.get(), property.data(), value.data());
}

void
libudev::enumerate::add_match_tag(const std::string_view tag) const noexcept
{
    udev_enumerate_add_match_tag(this->handle.get(), tag.data());
}

void
libudev::enumerate::add_match_sysname(const std::string_view sysname) const noexcept
{
    udev_enumerate_add_match_sysname(this->handle.get(), sysname.data());
}

void
libudev::enumerate::add_match_parent(const device& device) const noexcept
{
    const auto check_sysname = device.get_sysname();
    if (check_sysname)
    {
        udev_enumerate_add_match_sysname(this->handle.get(), check_sysname->data());
    }
}

void
libudev::enumerate::add_match_is_initialized() const noexcept
{
    udev_enumerate_add_match_is_initialized(this->handle.get());
}

void
libudev::enumerate::scan_devices() const noexcept
{
    udev_enumerate_scan_devices(this->handle.get());
}

void
libudev::enumerate::scan_subsystems() const noexcept
{
    udev_enumerate_scan_subsystems(this->handle.get());
}

std::vector<libudev::device>
libudev::enumerate::enumerate_devices() const noexcept
{
    std::vector<device> devices;
    struct udev_list_entry* entry = nullptr;
    struct udev_list_entry* enumeration_list = udev_enumerate_get_list_entry(this->handle.get());
    udev_list_entry_foreach(entry, enumeration_list)
    {
        const char* device_path = udev_list_entry_get_name(entry);
        devices.emplace_back(
            udev_device_new_from_syspath(udev_enumerate_get_udev(this->handle.get()), device_path));
    }

    return devices;
}
