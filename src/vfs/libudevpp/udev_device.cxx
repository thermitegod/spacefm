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
#include <unordered_map>

#include <optional>

#include <algorithm>

#include <libudev.h>

#include "libudevpp.hxx"

bool
libudev::device::is_initialized() const noexcept
{
    return udev_device_get_is_initialized(this->handle.get()) == 1;
}

bool
libudev::device::has_action() const noexcept
{
    return udev_device_get_action(this->handle.get()) != nullptr;
}

const std::optional<std::string>
libudev::device::get_action() const noexcept
{
    if (this->has_action())
    {
        return udev_device_get_action(this->handle.get());
    }
    return std::nullopt;
}

bool
libudev::device::has_devnode() const noexcept
{
    return udev_device_get_devnode(this->handle.get()) != nullptr;
}

const std::optional<std::string>
libudev::device::get_devnode() const noexcept
{
    if (this->has_devnode())
    {
        return udev_device_get_devnode(this->handle.get());
    }
    return std::nullopt;
}

dev_t
libudev::device::get_devnum() const noexcept
{
    return udev_device_get_devnum(this->handle.get());
}

bool
libudev::device::has_devtype() const noexcept
{
    return udev_device_get_devtype(this->handle.get()) != nullptr;
}

const std::optional<std::string>
libudev::device::get_devtype() const noexcept
{
    if (this->has_devtype())
    {
        return udev_device_get_devtype(this->handle.get());
    }
    return std::nullopt;
}

bool
libudev::device::has_subsystem() const noexcept
{
    return udev_device_get_subsystem(this->handle.get()) != nullptr;
}

const std::optional<std::string>
libudev::device::get_subsystem() const noexcept
{
    if (this->has_subsystem())
    {
        return udev_device_get_subsystem(this->handle.get());
    }
    return std::nullopt;
}

bool
libudev::device::has_devpath() const noexcept
{
    return udev_device_get_devpath(this->handle.get()) != nullptr;
}

const std::optional<std::string>
libudev::device::get_devpath() const noexcept
{
    if (this->has_devpath())
    {
        return udev_device_get_devpath(this->handle.get());
    }
    return std::nullopt;
}

bool
libudev::device::has_syspath() const noexcept
{
    return udev_device_get_syspath(this->handle.get()) != nullptr;
}

const std::optional<std::filesystem::path>
libudev::device::get_syspath() const noexcept
{
    if (this->has_syspath())
    {
        return udev_device_get_syspath(this->handle.get());
    }
    return std::nullopt;
}

bool
libudev::device::has_sysname() const noexcept
{
    return udev_device_get_sysname(this->handle.get()) != nullptr;
}

const std::optional<std::string>
libudev::device::get_sysname() const noexcept
{
    if (this->has_sysname())
    {
        return udev_device_get_sysname(this->handle.get());
    }
    return std::nullopt;
}

bool
libudev::device::has_sysnum() const noexcept
{
    return udev_device_get_sysnum(this->handle.get()) != nullptr;
}

const std::optional<std::string>
libudev::device::get_sysnum() const noexcept
{
    if (this->has_sysnum())
    {
        return udev_device_get_sysnum(this->handle.get());
    }
    return std::nullopt;
}

bool
libudev::device::has_driver() const noexcept
{
    return udev_device_get_driver(this->handle.get()) != nullptr;
}

const std::optional<std::string>
libudev::device::get_driver() const noexcept
{
    if (this->has_driver())
    {
        return udev_device_get_driver(this->handle.get());
    }
    return std::nullopt;
}

bool
libudev::device::has_sysattr(const std::string_view named) const noexcept
{
    const auto keys = get_sysattr_keys();
    return std::ranges::find(keys.cbegin(), keys.cend(), named) != keys.cend();
}

const std::optional<std::string>
libudev::device::get_sysattr(const std::string_view named) const noexcept
{
    if (this->has_sysattr(named))
    {
        return udev_device_get_sysattr_value(this->handle.get(), named.data());
    }
    return std::nullopt;
}

bool
libudev::device::set_sysattr(const std::string_view named,
                             const std::string_view value) const noexcept
{
    return udev_device_set_sysattr_value(this->handle.get(), named.data(), value.data()) >= 0;
}

const std::vector<std::string>
libudev::device::get_sysattr_keys() const noexcept
{
    std::vector<std::string> keys;

    auto sysattr_list = udev_device_get_sysattr_list_entry(this->handle.get());
    struct udev_list_entry* entry = nullptr;
    udev_list_entry_foreach(entry, sysattr_list)
    {
        keys.emplace_back(udev_list_entry_get_name(entry));
    }

    return keys;
}

const std::unordered_map<std::string, std::string>
libudev::device::get_sysattr_map() const noexcept
{
    std::unordered_map<std::string, std::string> attr;

    auto sysattr_list = udev_device_get_sysattr_list_entry(this->handle.get());
    struct udev_list_entry* entry = nullptr;
    udev_list_entry_foreach(entry, sysattr_list)
    {
        const char* key = udev_list_entry_get_name(entry);
        const char* value = udev_device_get_sysattr_value(this->handle.get(), key);
        if (entry != nullptr)
        {
            if (key != nullptr && value != nullptr)
            {
                attr[std::string(udev_list_entry_get_name(entry))] = std::string(value);
            }
        }
    }

    return attr;
}

const std::vector<std::string>
libudev::device::get_devlinks() const noexcept
{
    std::vector<std::string> links;

    struct udev_list_entry* entry = nullptr;
    udev_list_entry_foreach(entry, udev_device_get_devlinks_list_entry(this->handle.get()))
    {
        links.emplace_back(udev_list_entry_get_name(entry));
    }
    return links;
}

bool
libudev::device::has_property(const std::string_view named) const noexcept
{
    return udev_device_get_property_value(this->handle.get(), named.data()) != nullptr;
}

const std::optional<std::string>
libudev::device::get_property(const std::string_view named) const noexcept
{
    if (this->has_property(named))
    {
        return udev_device_get_property_value(this->handle.get(), named.data());
    }
    return std::nullopt;
}

const std::unordered_map<std::string, std::string>
libudev::device::get_properties() const noexcept
{
    std::unordered_map<std::string, std::string> property_map;

    struct udev_list_entry* entry = nullptr;
    struct udev_list_entry* properties = udev_device_get_properties_list_entry(this->handle.get());
    udev_list_entry_foreach(entry, properties)
    {
        property_map[std::string(udev_list_entry_get_name(entry))] =
            std::string(udev_list_entry_get_value(entry));
    }
    return property_map;
}

bool
libudev::device::has_tag(const std::string_view named) const noexcept
{
    return udev_device_has_tag(this->handle.get(), named.data());
}

const std::vector<std::string>
libudev::device::get_tags() const noexcept
{
    std::vector<std::string> tags;

    struct udev_list_entry* entry = nullptr;
    struct udev_list_entry* tags_list = udev_device_get_tags_list_entry(this->handle.get());

    udev_list_entry_foreach(entry, tags_list)
    {
        tags.emplace_back(udev_list_entry_get_name(entry));
    }

    return tags;
}

bool
libudev::device::has_current_tag(const std::string_view named) const noexcept
{
    return udev_device_has_current_tag(this->handle.get(), named.data());
}

const std::vector<std::string>
libudev::device::get_current_tags() const noexcept
{
    std::vector<std::string> tags;

    struct udev_list_entry* entry = nullptr;
    struct udev_list_entry* tags_list = udev_device_get_current_tags_list_entry(this->handle.get());

    udev_list_entry_foreach(entry, tags_list)
    {
        tags.emplace_back(udev_list_entry_get_name(entry));
    }

    return tags;
}

const std::optional<libudev::device>
libudev::device::get_parent_device() const noexcept
{
    if (!this->handle || this->is_disk())
    {
        return std::nullopt;
    }

    const auto parent = udev_device_get_parent(this->handle.get());
    if (parent)
    {
        const auto parent_syspath = device(parent).get_syspath();
        if (parent_syspath)
        {
            const udev udev;

            return udev.device_from_syspath(parent_syspath.value());
        }
    }
    return std::nullopt;
}

const std::optional<libudev::device>
libudev::device::get_parent_device(const std::string_view subsystem,
                                   const std::string_view type) const noexcept
{
    if (!this->handle || this->is_disk())
    {
        return std::nullopt;
    }

    const auto parent = udev_device_get_parent_with_subsystem_devtype(this->handle.get(),
                                                                      subsystem.data(),
                                                                      type.data());
    if (parent)
    {
        return device(parent);
    }
    return std::nullopt;
}

bool
libudev::device::is_disk() const noexcept
{
    const auto devtype = this->get_devtype();
    if (devtype)
    {
        return devtype.value() == std::string("disk");
    }
    return false;
}

bool
libudev::device::is_partition() const noexcept
{
    const auto devtype = this->get_devtype();
    if (devtype)
    {
        return devtype.value() == std::string("partition");
    }
    return false;
}

bool
libudev::device::is_usb() const noexcept
{
    if (!this->is_disk())
    {
        return false;
    }

    const auto check_prop = this->get_property("ID_BUS");
    if (check_prop)
    {
        return check_prop.value() == std::string("usb");
    }

    return false;
}

bool
libudev::device::is_cdrom() const noexcept
{
    if (!this->is_disk())
    {
        return false;
    }

    const auto check_prop = this->get_property("ID_CDROM");
    if (check_prop)
    {
        const auto& prop = check_prop.value();
        if (std::stoi(prop) == 1)
        {
            return true;
        }
    }

    return false;
}

bool
libudev::device::is_hdd() const noexcept
{
    if (!this->is_disk())
    {
        return false;
    }

    const auto check_prop = this->get_property("ID_ATA_ROTATION_RATE_RPM");
    if (check_prop)
    {
        const auto& prop = check_prop.value();
        if (std::stoi(prop) > 0)
        {
            return true;
        }
    }

    return false;
}

bool
libudev::device::is_ssd() const noexcept
{
    if (!this->is_disk() || this->is_hdd())
    {
        return false;
    }

    const auto check_prop = this->get_property("ID_ATA_ROTATION_RATE_RPM");
    if (check_prop)
    {
        const auto& prop = check_prop.value();
        if (std::stoi(prop) == 0)
        {
            return true;
        }
    }

    // nvme is an ssd but does not have the ROTATION property
    return this->is_nvme();
}

bool
libudev::device::is_nvme() const noexcept
{
    if (!this->is_disk())
    {
        return false;
    }

    const auto driver = this->get_driver();
    if (driver)
    {
        return driver.value() == std::string("nvme");
    }

    return false;
}

bool
libudev::device::is_hotswapable() const noexcept
{
    if (!this->is_disk())
    {
        return false;
    }

    const auto check_prop = this->get_property("ID_HOTPLUG");
    if (check_prop)
    {
        const auto& prop = check_prop.value();
        if (std::stoi(prop) == 1)
        {
            return true;
        }
    }

    return false;
}

bool
libudev::device::is_removable() const noexcept
{
    // only usb, ieee1394, firewire, mmc, and pcmcia devices have this property
    // and they are all removable.
    return this->has_property("ID_BUS");
}

bool
libudev::device::is_internal() const noexcept
{
    return !this->is_removable();
}
