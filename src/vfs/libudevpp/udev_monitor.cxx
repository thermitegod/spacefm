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

#include <string_view>

#include <optional>

#include "libudevpp.hxx"

bool
libudev::monitor::enable_receiving() const noexcept
{
    return udev_monitor_enable_receiving(this->handle.get()) >= 0;
}

int
libudev::monitor::get_fd() const noexcept
{
    return udev_monitor_get_fd(this->handle.get());
}

const std::optional<libudev::device>
libudev::monitor::receive_device() const noexcept
{
    const auto device = udev_monitor_receive_device(this->handle.get());
    if (device)
    {
        return libudev::device(device);
    }
    return std::nullopt;
}

bool
libudev::monitor::filter_add_match_subsystem_devtype(
    const std::string_view subsystem) const noexcept
{
    return udev_monitor_filter_add_match_subsystem_devtype(this->handle.get(),
                                                           subsystem.data(),
                                                           nullptr) == 0;
}

bool
libudev::monitor::filter_add_match_subsystem_devtype(const std::string_view subsystem,
                                                     const std::string_view devtype) const noexcept
{
    return udev_monitor_filter_add_match_subsystem_devtype(this->handle.get(),
                                                           subsystem.data(),
                                                           devtype.data()) == 0;
}

bool
libudev::monitor::filter_add_match_tag(const std::string_view tag) const noexcept
{
    return udev_monitor_filter_add_match_tag(this->handle.get(), tag.data()) == 0;
}

bool
libudev::monitor::is_initialized() const noexcept
{
    return this->handle != nullptr;
}
