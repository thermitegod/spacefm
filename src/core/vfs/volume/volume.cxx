/**
 * Copyright (C) 2014 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *
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
#include <string>
#include <string_view>
#include <vector>

#include <sys/types.h>

#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "vfs/execute.hxx"
#include "vfs/volume/device.hxx"
#include "vfs/volume/volume.hxx"

#include "vfs/linux/sysfs.hxx"
#include "vfs/utils/utils.hxx"

#include "logger.hxx"

#undef major
#undef minor
#undef makedev

vfs::volume::volume(const std::shared_ptr<vfs::device>& device) noexcept : device_(device)
{
    // logger::debug<logger::vfs>("vfs::volume::volume({})", logger::utils::ptr(this));

    if (!device->mount_points().empty())
    {
        if (device_->mount_points().contains(','))
        {
            mount_point_ = ztd::partition(device_->mount_points(), ",")[0];
        }
        else
        {
            mount_point_ = device_->mount_points();
        }
    }

    // adjustments
    ever_mounted_ = is_mounted();

    // logger::debug<logger::vfs>("====devnum={}:{}", gnu_dev_major(device_->devnum()), gnu_dev_minor(device_->devnum()));
    // logger::debug<logger::vfs>("    device_file={}", device_->devnode());
    // logger::debug<logger::vfs>("    udi={}", device_->id());
    // logger::debug<logger::vfs>("    label={}", device_->id_label());
    // logger::debug<logger::vfs>("    is_mounted={}", device_->is_mounted());
    // logger::debug<logger::vfs>("    is_mountable={}", device_->is_media_available());
    // logger::debug<logger::vfs>("    is_optical={}", device_->is_optical_disc());
    // logger::debug<logger::vfs>("    is_removable={}", device_->is_removable());
    // logger::debug<logger::vfs>("    requires_eject={}", device_->is_media_ejectable());
    // logger::debug<logger::vfs>("    is_user_visible={}", is_user_visible());
    // logger::debug<logger::vfs>("    mount_point={}", mount_point_);
    // logger::debug<logger::vfs>("    size={}", device_->size());
    // logger::debug<logger::vfs>("    disp_name={}", display_name());
}

std::shared_ptr<vfs::volume>
vfs::volume::create(const std::shared_ptr<vfs::device>& device) noexcept
{
    struct hack : public vfs::volume
    {
        hack(const std::shared_ptr<vfs::device>& device) : volume(device) {}
    };

    return std::make_shared<hack>(device);
}

std::optional<std::string>
vfs::volume::device_mount_cmd() noexcept
{
    const std::filesystem::path path = Glib::find_program_in_path("udiskie-mount");
    if (path.empty())
    {
        return std::nullopt;
    }
    return std::format("{} {}", path, vfs::execute::quote(device_->devnode()));
}

std::optional<std::string>
vfs::volume::device_unmount_cmd() noexcept
{
    const std::filesystem::path path = Glib::find_program_in_path("udiskie-umount");
    if (path.empty())
    {
        return std::nullopt;
    }
    return std::format("{} {}", path, vfs::execute::quote(mount_point_));
}

std::string
vfs::volume::display_name() const noexcept
{
    std::string disp_label;
    std::string disp_size;
    std::string disp_mount;

    if (is_mounted())
    {
        disp_label = device_->id_label();

        if (device_->size() > 0)
        {
            disp_size = vfs::utils::format_file_size(device_->size(), false);
        }

        if (!mount_point_.empty())
        {
            disp_mount = mount_point_;
        }
        else
        {
            disp_mount = "???";
        }
    }
    else if (is_mountable())
    { // has_media
        disp_label = device_->id_label();

        if (device_->size() > 0)
        {
            disp_size = vfs::utils::format_file_size(device_->size(), false);
        }
        disp_mount = "---";
    }
    else
    {
        disp_label = "[no media]";
    }

    return std::format("{} {} {} {} {} {}:{}",
                       device_->devnode(),
                       disp_size,
                       device_->fstype(),
                       disp_label,
                       disp_mount,
                       gnu_dev_major(device_->devnum()),
                       gnu_dev_minor(device_->devnum()));
}

std::string_view
vfs::volume::mount_point() const noexcept
{
    return mount_point_;
}

std::string_view
vfs::volume::device_file() const noexcept
{
    return device_->devnode();
}
std::string_view
vfs::volume::fstype() const noexcept
{
    return device_->fstype();
}

std::string_view
vfs::volume::udi() const noexcept
{
    return device_->id();
}

std::string_view
vfs::volume::label() const noexcept
{
    return device_->id_label();
}

dev_t
vfs::volume::devnum() const noexcept
{
    return device_->devnum();
}

u64
vfs::volume::size() const noexcept
{
    return device_->size();
}

bool
vfs::volume::is_device_type(const vfs::volume::device_type type) const noexcept
{
    return type == device_type_;
}

bool
vfs::volume::is_mounted() const noexcept
{
    return device_->is_mounted();
}

bool
vfs::volume::is_removable() const noexcept
{
    return device_->is_removable();
}

bool
vfs::volume::is_mountable() const noexcept
{
    return device_->is_media_available();
}

bool
vfs::volume::is_user_visible() const noexcept
{
    return device_->udevice.is_partition() ||
           (device_->udevice.is_removable() && !device_->udevice.is_disk());
}

bool
vfs::volume::is_optical() const noexcept
{
    return device_->is_optical_disc();
}

bool
vfs::volume::requires_eject() const noexcept
{
    return device_->is_media_ejectable();
}

bool
vfs::volume::ever_mounted() const noexcept
{
    return ever_mounted_;
}

void
vfs::volume::update_from_device(const std::shared_ptr<vfs::device>& device) noexcept
{
    if (!device)
    {
        return;
    }
    device_ = device;

    // detect changed mount point
    const auto was_mounted = device_->is_mounted();
    std::string changed_mount_point;
    if (was_mounted != device_->is_mounted())
    {
        changed_mount_point = device_->is_mounted() ? device_->mount_points() : mount_point_;
    }

    if (!device_->mount_points().empty())
    {
        if (device_->mount_points().contains(','))
        {
            mount_point_ = ztd::partition(device_->mount_points(), ",")[0];
        }
        else
        {
            mount_point_ = device_->mount_points();
        }
    }
    else
    {
        mount_point_.clear();
    }

    // Mount and ejection detect for automount
    if (device_->is_mounted())
    {
        ever_mounted_ = true;
    }
    else if (device_->is_removable() && !device_->is_media_available())
    { // ejected
        ever_mounted_ = false;
    }

    if (!changed_mount_point.empty())
    {
        // TODO: main_window_refresh_all_tabs_matching(changed_mount_point);
    }
}
