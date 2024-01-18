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

#include <filesystem>

#include <memory>

#include <fcntl.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/libudevpp/libudevpp.hxx"

#include "vfs/linux/procfs.hxx"
#include "vfs/linux/sysfs.hxx"

#include "vfs/vfs-device.hxx"

const std::shared_ptr<vfs::device>
vfs::device::create(const libudev::device& udevice) noexcept
{
    return std::make_shared<vfs::device>(udevice);
}

vfs::device::device(const libudev::device& udevice) : udevice(udevice)
{
    this->is_valid_ = this->device_get_info();
}

dev_t
vfs::device::devnum() const noexcept
{
    return this->devnum_;
}

const std::string_view
vfs::device::devnode() const noexcept
{
    return this->devnode_;
}

const std::string_view
vfs::device::native_path() const noexcept
{
    return this->native_path_;
}

const std::string_view
vfs::device::mount_points() const noexcept
{
    return this->mount_points_;
}

bool
vfs::device::is_valid() const noexcept
{
    return this->is_valid_;
}

bool
vfs::device::is_system_internal() const noexcept
{
    return this->is_system_internal_;
}

bool
vfs::device::is_removable() const noexcept
{
    return this->is_removable_;
}

bool
vfs::device::is_media_available() const noexcept
{
    return this->is_media_available_;
}

bool
vfs::device::is_optical_disc() const noexcept
{
    return this->is_optical_disc_;
}

bool
vfs::device::is_mounted() const noexcept
{
    return this->is_mounted_;
}

bool
vfs::device::is_media_ejectable() const noexcept
{
    return this->is_media_ejectable_;
}

const std::string_view
vfs::device::id() const noexcept
{
    return this->id_;
}

const std::string_view
vfs::device::id_label() const noexcept
{
    return this->id_label_;
}

u64
vfs::device::size() const noexcept
{
    return this->size_;
}

u64
vfs::device::block_size() const noexcept
{
    return this->block_size_;
}

const std::string_view
vfs::device::fstype() const noexcept
{
    return this->fstype_;
}

const std::optional<std::string>
vfs::device::info_mount_points() noexcept
{
    const dev_t dmajor = gnu_dev_major(this->devnum_);
    const dev_t dminor = gnu_dev_minor(this->devnum_);

    // TODO MAYBE - pass devmounts as an argument to the constructor?

    // if we have the mount point list, use this instead of reading mountinfo
    // if (!devmounts.empty())
    // {
    //     for (const devmount_t& devmount : devmounts)
    //     {
    //         if (devmount->major == dmajor && devmount->minor == dminor)
    //         {
    //             return devmount->mount_points;
    //         }
    //     }
    //     return std::nullopt;
    // }

    std::vector<std::string> device_mount_points;

    for (const auto& mount : vfs::linux::procfs::mountinfo())
    {
        // ignore mounts where only a subtree of a filesystem is mounted
        // this function is only used for block devices.
        if (mount.root == "/")
        {
            continue;
        }

        if (mount.major != dmajor || mount.minor != dminor)
        {
            continue;
        }

        if (!std::ranges::contains(device_mount_points, mount.mount_point))
        {
            device_mount_points.push_back(mount.mount_point);
        }
    }

    if (device_mount_points.empty())
    {
        return std::nullopt;
    }

    // Sort the list to ensure that shortest mount paths appear first
    // std::ranges::sort(device_mount_points, ztd::sort::compare);

    return ztd::join(device_mount_points, ",");
}

bool
vfs::device::device_get_info() noexcept
{
    const auto device_syspath = this->udevice.get_syspath();
    const auto device_devnode = this->udevice.get_devnode();
    const auto device_devnum = this->udevice.get_devnum();
    if (!device_syspath || !device_devnode || device_devnum == 0)
    {
        return false;
    }

    this->native_path_ = device_syspath.value();
    this->devnode_ = device_devnode.value();
    this->devnum_ = device_devnum;

    const auto prop_id_fs_usage = this->udevice.get_property("ID_FS_USAGE");
    const auto prop_id_fs_uuid = this->udevice.get_property("ID_FS_UUID");

    const auto prop_id_fs_type = this->udevice.get_property("ID_FS_TYPE");
    if (prop_id_fs_type)
    {
        this->fstype_ = prop_id_fs_type.value();
    }
    const auto prop_id_fs_label = this->udevice.get_property("ID_FS_LABEL");
    if (prop_id_fs_label)
    {
        this->id_label_ = prop_id_fs_label.value();
    }

    // device_is_media_available
    bool media_available = false;

    if (prop_id_fs_usage || prop_id_fs_type || prop_id_fs_uuid || prop_id_fs_label)
    {
        media_available = true;
    }
    else if (this->devnode_.starts_with("/dev/loop"))
    {
        media_available = false;
    }
    else if (this->is_removable())
    {
        bool is_cd = false;

        const auto prop_id_cdrom = this->udevice.get_property("ID_CDROM");
        if (prop_id_cdrom)
        {
            is_cd = std::stol(prop_id_cdrom.value()) != 0;
        }
        else
        {
            is_cd = false;
        }

        if (!is_cd)
        {
            // this test is limited for non-root - user may not have read
            // access to device file even if media is present
            const i32 fd = open(this->devnode_.data(), O_RDONLY);
            if (fd >= 0)
            {
                media_available = true;
                close(fd);
            }
        }
        else
        {
            const auto prop_id_cdrom_media = this->udevice.get_property("ID_CDROM_MEDIA");
            if (prop_id_cdrom_media)
            {
                media_available = (std::stol(prop_id_cdrom_media.value()) == 1);
            }
        }
    }
    else
    {
        const auto prop_id_cdrom_media = this->udevice.get_property("ID_CDROM_MEDIA");
        if (prop_id_cdrom_media)
        {
            media_available = (std::stol(prop_id_cdrom_media.value()) == 1);
        }
        else
        {
            media_available = true;
        }
    }

    this->is_media_available_ = media_available;

    if (this->is_media_available())
    {
        const auto check_size = vfs::linux::sysfs::get_u64(this->native_path_, "size");
        if (check_size)
        {
            this->size_ = check_size.value() * 512;
        }

        //  This is not available on all devices so fall back to 512 if unavailable.
        //
        //  Another way to get this information is the BLKSSZGET ioctl but we do not want
        //  to open the device. Ideally vol_id would export it.
        const auto check_block_size =
            vfs::linux::sysfs::get_u64(this->native_path_, "queue/hw_sector_size");
        if (check_block_size)
        {
            if (check_block_size.value() != 0)
            {
                this->block_size_ = check_block_size.value();
            }
            else
            {
                this->block_size_ = 512;
            }
        }
        else
        {
            this->block_size_ = 512;
        }
    }

    // links
    const auto entrys = this->udevice.get_devlinks();
    for (const std::string_view entry : entrys)
    {
        if (entry.starts_with("/dev/disk/by-id/") || entry.starts_with("/dev/disk/by-uuid/"))
        {
            this->id_ = entry;
            break;
        }
    }

    if (this->native_path_.empty() || this->devnum_ == 0)
    {
        return false;
    }

    this->is_removable_ = this->udevice.is_removable();

    // is_ejectable
    bool drive_is_ejectable = false;
    const auto prop_id_drive_ejectable = this->udevice.get_property("ID_DRIVE_EJECTABLE");
    if (prop_id_drive_ejectable)
    {
        drive_is_ejectable = std::stol(prop_id_drive_ejectable.value()) != 0;
    }
    else
    {
        drive_is_ejectable = this->udevice.has_property("ID_CDROM");
    }
    this->is_media_ejectable_ = drive_is_ejectable;

    // devices with removable media are never system internal
    this->is_system_internal_ = !this->is_removable_;

    this->mount_points_ = this->info_mount_points().value_or("");
    this->is_mounted_ = !this->mount_points_.empty();

    const auto prop_id_cdrom = this->udevice.get_property("ID_CDROM");
    if (prop_id_cdrom && std::stol(prop_id_cdrom.value()) != 0)
    {
        this->is_optical_disc_ = true;
    }

    return true;
}
