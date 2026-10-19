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

vfs::volume::volume(const std::shared_ptr<vfs::device>& device) noexcept
{
    // logger::debug<logger::vfs>("vfs::volume::volume({})", logger::utils::ptr(this));

    devnum_ = device->devnum();
    device_file_ = device->devnode();
    udi_ = device->id();
    is_optical_ = device->is_optical_disc();
    is_removable_ = !device->is_system_internal();
    requires_eject_ = device->is_media_ejectable();
    is_mountable_ = device->is_media_available();
    is_mounted_ = device->is_mounted();
    is_user_visible_ = device->udevice.is_partition() ||
                       (device->udevice.is_removable() && !device->udevice.is_disk());

    if (!device->mount_points().empty())
    {
        if (device->mount_points().contains(','))
        {
            mount_point_ = ztd::partition(device->mount_points(), ",")[0];
        }
        else
        {
            mount_point_ = device->mount_points();
        }
    }
    size_ = device->size();
    label_ = device->id_label();
    fstype_ = device->fstype();

    // adjustments
    ever_mounted_ = is_mounted();

    set_info();

    // logger::debug<logger::vfs>("====devnum={}:{}", gnu_dev_major(devnum_), gnu_dev_minor(devnum_));
    // logger::debug<logger::vfs>("    device_file={}", device_file_);
    // logger::debug<logger::vfs>("    udi={}", udi_);
    // logger::debug<logger::vfs>("    label={}", label_);
    // logger::debug<logger::vfs>("    icon={}", icon_);
    // logger::debug<logger::vfs>("    is_mounted={}", is_mounted_);
    // logger::debug<logger::vfs>("    is_mountable={}", is_mountable_);
    // logger::debug<logger::vfs>("    is_optical={}", is_optical_);
    // logger::debug<logger::vfs>("    is_removable={}", is_removable_);
    // logger::debug<logger::vfs>("    requires_eject={}", requires_eject_);
    // logger::debug<logger::vfs>("    is_user_visible={}", is_user_visible_);
    // logger::debug<logger::vfs>("    mount_point={}", mount_point_);
    // logger::debug<logger::vfs>("    size={}", size_);
    // logger::debug<logger::vfs>("    disp_name={}", disp_name_);
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
    return std::format("{} {}", path, vfs::execute::quote(device_file_));
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

std::string_view
vfs::volume::display_name() const noexcept
{
    return disp_name_;
}

std::string_view
vfs::volume::mount_point() const noexcept
{
    return mount_point_;
}

std::string_view
vfs::volume::device_file() const noexcept
{
    return device_file_;
}
std::string_view
vfs::volume::fstype() const noexcept
{
    return fstype_;
}

std::string_view
vfs::volume::icon() const noexcept
{
    return icon_;
}

std::string_view
vfs::volume::udi() const noexcept
{
    return udi_;
}

std::string_view
vfs::volume::label() const noexcept
{
    return label_;
}

dev_t
vfs::volume::devnum() const noexcept
{
    return devnum_;
}

u64
vfs::volume::size() const noexcept
{
    return size_;
}

bool
vfs::volume::is_device_type(const vfs::volume::device_type type) const noexcept
{
    return type == device_type_;
}

bool
vfs::volume::is_mounted() const noexcept
{
    return is_mounted_;
}

bool
vfs::volume::is_removable() const noexcept
{
    return is_removable_;
}

bool
vfs::volume::is_mountable() const noexcept
{
    return is_mountable_;
}

bool
vfs::volume::is_user_visible() const noexcept
{
    return is_user_visible_;
}

bool
vfs::volume::is_optical() const noexcept
{
    return is_optical_;
}

bool
vfs::volume::requires_eject() const noexcept
{
    return requires_eject_;
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

    // detect changed mount point
    const auto was_mounted = is_mounted_;
    std::string changed_mount_point;
    if (was_mounted != device->is_mounted())
    {
        changed_mount_point = device->is_mounted() ? device->mount_points() : mount_point_;
    }

    devnum_ = device->devnum();
    device_file_ = device->devnode();
    udi_ = device->id();
    label_ = device->id_label();
    size_ = device->size();
    fstype_ = device->fstype();

    is_optical_ = device->is_optical_disc();
    is_removable_ = !device->is_system_internal();
    requires_eject_ = device->is_media_ejectable();
    is_mountable_ = device->is_media_available();
    is_mounted_ = device->is_mounted();

    is_user_visible_ = device->udevice.is_partition() ||
                       (device->udevice.is_removable() && !device->udevice.is_disk());

    if (!device->mount_points().empty())
    {
        if (device->mount_points().contains(','))
        {
            mount_point_ = ztd::partition(device->mount_points(), ",")[0];
        }
        else
        {
            mount_point_ = device->mount_points();
        }
    }
    else
    {
        mount_point_.clear();
    }

    // Mount and ejection detect for automount
    if (is_mounted_)
    {
        ever_mounted_ = true;
    }
    else if (is_removable_ && !is_mountable_)
    { // ejected
        ever_mounted_ = false;
    }

    set_info();

    if (!changed_mount_point.empty())
    {
        // TODO: main_window_refresh_all_tabs_matching(changed_mount_point);
    }
}

void
vfs::volume::set_info() noexcept
{
    std::string disp_device;
    std::string disp_label;
    std::string disp_size;
    std::string disp_mount;
    std::string disp_fstype;
    std::string disp_devnum;

    if (is_mounted())
    {
        disp_label = label_;

        if (size_ > 0)
        {
            disp_size = vfs::utils::format_file_size(size_, false);
        }

        if (!mount_point_.empty())
        {
            disp_mount = std::format("{}", mount_point_);
        }
        else
        {
            disp_mount = "???";
        }
    }
    else if (is_mountable())
    { // has_media
        disp_label = label_;

        if (size_ > 0)
        {
            disp_size = vfs::utils::format_file_size(size_, false);
        }
        disp_mount = "---";
    }
    else
    {
        disp_label = "[no media]";
    }

    disp_device = device_file_;
    disp_fstype = fstype_;
    disp_devnum = std::format("{}:{}", gnu_dev_major(devnum_), gnu_dev_minor(devnum_));

    auto parameter = std::format("{} {} {} {} {} {}",
                                 disp_device,
                                 disp_size,
                                 disp_fstype,
                                 disp_label,
                                 disp_mount,
                                 disp_devnum);

    disp_name_ = ztd::replace(parameter, "  ", " ");
    if (udi_.empty())
    {
        udi_ = device_file_;
    }
}
