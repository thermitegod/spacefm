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

#include <string>
#include <string_view>

#include <filesystem>

#include <array>
#include <utility>
#include <vector>

#include <optional>

#include <algorithm>
#include <ranges>

#include <memory>

#include <sys/sysmacros.h>

#include <fmt/format.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "xset/xset.hxx"
#include "xset/xset-event-handler.hxx"

#include "main-window.hxx"

#include "vfs/vfs-utils.hxx"

#include "vfs/libudevpp/libudevpp.hxx"
#include "vfs/linux/procfs.hxx"
#include "vfs/linux/sysfs.hxx"

#include "vfs/vfs-volume.hxx"

static vfs::volume vfs_volume_read_by_device(const libudev::device& udevice);
static void vfs_volume_device_removed(const libudev::device& udevice);
static void call_callbacks(vfs::volume vol, VFSVolumeState state);

struct VFSVolumeCallbackData
{
    VFSVolumeCallbackData() = delete;
    VFSVolumeCallbackData(VFSVolumeCallback callback, void* callback_data);

    VFSVolumeCallback cb{nullptr};
    void* user_data{nullptr};
};

VFSVolumeCallbackData::VFSVolumeCallbackData(VFSVolumeCallback callback, void* callback_data)
{
    this->cb = callback;
    this->user_data = callback_data;
}

using volume_callback_data_t = std::shared_ptr<VFSVolumeCallbackData>;

static std::vector<vfs::volume> volumes;
static std::vector<volume_callback_data_t> callbacks;

struct DeviceMount
{
    DeviceMount() = delete;
    DeviceMount(dev_t major, dev_t minor);
    ~DeviceMount() = default;

    dev_t major{0};
    dev_t minor{0};
    std::string mount_points;
    std::string fstype;
    std::vector<std::string> mounts{};
};

DeviceMount::DeviceMount(dev_t major, dev_t minor)
{
    this->major = major;
    this->minor = minor;
}

using devmount_t = std::shared_ptr<DeviceMount>;

static std::vector<devmount_t> devmounts;

static libudev::udev udev;
static libudev::monitor umonitor;

static Glib::RefPtr<Glib::IOChannel> uchannel = nullptr;
static Glib::RefPtr<Glib::IOChannel> mchannel = nullptr;

/**
 * device info
 */

struct Device
{
    Device() = delete;
    Device(const libudev::device& udevice);
    ~Device() = default;

    libudev::device udevice;

    dev_t devnum{0};

    std::string devnode{};
    std::string native_path{};
    std::string mount_points{};

    bool device_is_system_internal{true};
    bool device_is_removable{false};
    bool device_is_media_available{false};
    bool device_is_optical_disc{false};
    bool device_is_mounted{false};

    std::string device_by_id{};
    u64 device_size{0};
    u64 device_block_size{0};
    std::string id_label{};

    bool drive_is_media_ejectable{false};

    std::string filesystem{};
};

Device::Device(const libudev::device& udevice) : udevice(udevice)
{
}

using device_t = std::shared_ptr<Device>;

static const std::optional<std::string>
info_mount_points(const device_t& device)
{
    const dev_t dmajor = gnu_dev_major(device->devnum);
    const dev_t dminor = gnu_dev_minor(device->devnum);

    // if we have the mount point list, use this instead of reading mountinfo
    if (!devmounts.empty())
    {
        for (const devmount_t& devmount : devmounts)
        {
            if (devmount->major == dmajor && devmount->minor == dminor)
            {
                return devmount->mount_points;
            }
        }
        return std::nullopt;
    }

    std::vector<std::string> device_mount_points;

    for (const auto& mount : vfs::linux::procfs::mountinfo())
    {
        // ignore mounts where only a subtree of a filesystem is mounted
        // this function is only used for block devices.
        if (ztd::same(mount.root, "/"))
        {
            continue;
        }

        if (mount.major != dmajor || mount.minor != dminor)
        {
            continue;
        }

        if (!ztd::contains(device_mount_points, mount.mount_point))
        {
            device_mount_points.emplace_back(mount.mount_point);
        }
    }

    if (device_mount_points.empty())
    {
        return std::nullopt;
    }

    // Sort the list to ensure that shortest mount paths appear first
    std::ranges::sort(device_mount_points, ztd::compare);

    return ztd::join(device_mount_points, ",");
}

static bool
device_get_info(const device_t& device)
{
    const auto device_syspath = device->udevice.get_syspath();
    const auto device_devnode = device->udevice.get_devnode();
    const auto device_devnum = device->udevice.get_devnum();
    if (!device_syspath || !device_devnode || device_devnum == 0)
    {
        return false;
    }

    device->native_path = device_syspath.value();
    device->devnode = device_devnode.value();
    device->devnum = device_devnum;

    const auto prop_id_fs_usage = device->udevice.get_property("ID_FS_USAGE");
    const auto prop_id_fs_uuid = device->udevice.get_property("ID_FS_UUID");

    const auto prop_id_fs_type = device->udevice.get_property("ID_FS_TYPE");
    if (prop_id_fs_type)
    {
        device->filesystem = prop_id_fs_type.value();
    }
    const auto prop_id_fs_label = device->udevice.get_property("ID_FS_LABEL");
    if (prop_id_fs_label)
    {
        device->id_label = prop_id_fs_label.value();
    }

    // device_is_media_available
    bool media_available = false;

    if (prop_id_fs_usage || prop_id_fs_type || prop_id_fs_uuid || prop_id_fs_label)
    {
        media_available = true;
    }
    else if (ztd::startswith(device->devnode, "/dev/loop"))
    {
        media_available = false;
    }
    else if (device->device_is_removable)
    {
        bool is_cd;

        const auto prop_id_cdrom = device->udevice.get_property("ID_CDROM");
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
            const i32 fd = open(device->devnode.data(), O_RDONLY);
            if (fd >= 0)
            {
                media_available = true;
                close(fd);
            }
        }
        else
        {
            const auto prop_id_cdrom_media = device->udevice.get_property("ID_CDROM_MEDIA");
            if (prop_id_cdrom_media)
            {
                media_available = (std::stol(prop_id_cdrom_media.value()) == 1);
            }
        }
    }
    else
    {
        const auto prop_id_cdrom_media = device->udevice.get_property("ID_CDROM_MEDIA");
        if (prop_id_cdrom_media)
        {
            media_available = (std::stol(prop_id_cdrom_media.value()) == 1);
        }
        else
        {
            media_available = true;
        }
    }

    device->device_is_media_available = media_available;

    if (device->device_is_media_available)
    {
        const auto check_size = vfs::linux::sysfs::get_u64(device->native_path, "size");
        if (check_size)
        {
            device->device_size = check_size.value() * ztd::BLOCK_SIZE;
        }

        //  This is not available on all devices so fall back to 512 if unavailable.
        //
        //  Another way to get this information is the BLKSSZGET ioctl but we do not want
        //  to open the device. Ideally vol_id would export it.
        const auto check_block_size =
            vfs::linux::sysfs::get_u64(device->native_path, "queue/hw_sector_size");
        if (check_block_size)
        {
            if (check_block_size.value() != 0)
            {
                device->device_block_size = check_block_size.value();
            }
            else
            {
                device->device_block_size = ztd::BLOCK_SIZE;
            }
        }
        else
        {
            device->device_block_size = ztd::BLOCK_SIZE;
        }
    }

    // links
    const auto entrys = device->udevice.get_devlinks();
    for (const std::string_view entry : entrys)
    {
        if (ztd::startswith(entry, "/dev/disk/by-id/") ||
            ztd::startswith(entry, "/dev/disk/by-uuid/"))
        {
            device->device_by_id = entry;
            break;
        }
    }

    if (device->native_path.empty() || device->devnum == 0)
    {
        return false;
    }

    device->device_is_removable = device->udevice.is_removable();

    // is_ejectable
    bool drive_is_ejectable = false;
    const auto prop_id_drive_ejectable = device->udevice.get_property("ID_DRIVE_EJECTABLE");
    if (prop_id_drive_ejectable)
    {
        drive_is_ejectable = std::stol(prop_id_drive_ejectable.value()) != 0;
    }
    else
    {
        drive_is_ejectable = device->udevice.has_property("ID_CDROM");
    }
    device->drive_is_media_ejectable = drive_is_ejectable;

    // devices with removable media are never system internal
    device->device_is_system_internal = !device->device_is_removable;

    device->mount_points = info_mount_points(device).value_or("");
    device->device_is_mounted = !device->mount_points.empty();

    const auto prop_id_cdrom = device->udevice.get_property("ID_CDROM");
    if (prop_id_cdrom && std::stol(prop_id_cdrom.value()) != 0)
    {
        device->device_is_optical_disc = true;
    }

    return true;
}

/**
 * udev & mount monitors
 */

static void
parse_mounts(bool report)
{
    // ztd::logger::debug("parse_mounts report={}", report);

    // get all mount points for all devices
    std::vector<devmount_t> newmounts;

    for (const auto& mount : vfs::linux::procfs::mountinfo())
    {
        // mount where only a subtree of a filesystem is mounted?
        const bool subdir_mount = !ztd::same(mount.root, "/");

        if (mount.mount_point.empty())
        {
            continue;
        }

        // ztd::logger::debug("mount_point({}:{})={}", mount.major, mount.minor, mount.mount_point);
        devmount_t devmount = nullptr;
        for (const devmount_t& search : newmounts)
        {
            if (search->major == mount.major && search->minor == mount.minor)
            {
                devmount = search;
                break;
            }
        }

        if (!devmount)
        {
            // ztd::logger::debug("     new devmount {}", mount.mount_point);
            if (report)
            {
                if (subdir_mount)
                {
                    // is a subdir mount - ignore if block device
                    const dev_t devnum = makedev(mount.major, mount.minor);
                    const auto check_udevice = udev.device_from_devnum('b', devnum);
                    if (check_udevice)
                    {
                        const auto& udevice = check_udevice.value();
                        if (udevice.is_initialized())
                        {
                            // is block device with subdir mount - ignore
                            continue;
                        }
                    }
                }
                devmount = std::make_shared<DeviceMount>(mount.major, mount.minor);
                devmount->fstype = mount.filesystem_type;

                newmounts.emplace_back(devmount);
            }
            else
            {
                // initial load !report do not add non-block devices
                const dev_t devnum = makedev(mount.major, mount.minor);
                const auto check_udevice = udev.device_from_devnum('b', devnum);
                if (check_udevice)
                {
                    const auto& udevice = check_udevice.value();
                    if (udevice.is_initialized())
                    {
                        // is block device
                        if (subdir_mount)
                        {
                            // is block device with subdir mount - ignore
                            continue;
                        }
                        // add
                        devmount = std::make_shared<DeviceMount>(mount.major, mount.minor);
                        devmount->fstype = mount.filesystem_type;

                        newmounts.emplace_back(devmount);
                    }
                }
            }
        }

        if (devmount && !ztd::contains(devmount->mounts, mount.mount_point))
        {
            // ztd::logger::debug("    prepended");
            devmount->mounts.emplace_back(mount.mount_point);
        }
    }
    // ztd::logger::debug("LINES DONE");
    // translate each mount points list to string
    for (const devmount_t& devmount : newmounts)
    {
        // Sort the list to ensure that shortest mount paths appear first
        std::ranges::sort(devmount->mounts, ztd::compare);

        devmount->mount_points = ztd::join(devmount->mounts, ",");
        devmount->mounts.clear();

        // ztd::logger::debug("translate {}:{} {}", devmount->major, devmount->minor, devmount->mount_points);
    }

    // compare old and new lists
    std::vector<devmount_t> changed;
    if (report)
    {
        for (const devmount_t& devmount : newmounts)
        {
            // ztd::logger::debug("finding {}:{}", devmount->major, devmount->minor);

            devmount_t found = nullptr;

            for (const devmount_t& search : devmounts)
            {
                if (devmount->major == search->major && devmount->minor == search->minor)
                {
                    found = search;
                }
            }

            if (found)
            {
                // ztd::logger::debug("    found");
                if (ztd::same(found->mount_points, devmount->mount_points))
                {
                    // ztd::logger::debug("    freed");
                    // no change to mount points, so remove from old list
                    ztd::remove(devmounts, found);
                }
            }
            else
            {
                // new mount
                // ztd::logger::debug("    new mount {}:{} {}", devmount->major, devmount->minor, devmount->mount_points);
                const devmount_t devcopy =
                    std::make_shared<DeviceMount>(devmount->major, devmount->minor);
                devcopy->mount_points = devmount->mount_points;
                devcopy->fstype = devmount->fstype;

                changed.emplace_back(devcopy);
            }
        }
    }

    // ztd::logger::debug("REMAINING");
    // any remaining devices in old list have changed mount status
    for (const devmount_t& devmount : devmounts)
    {
        // ztd::logger::debug("remain {}:{}", devmount->major, devmount->minor);
        if (report)
        {
            changed.emplace_back(devmount);
        }
    }

    // will free old devmounts, and update with new devmounts
    devmounts = newmounts;

    // report
    if (report && !changed.empty())
    {
        for (const devmount_t& devmount : changed)
        {
            const dev_t devnum = makedev(devmount->major, devmount->minor);
            const auto check_udevice = udev.device_from_devnum('b', devnum);
            if (!check_udevice)
            {
                continue;
            }

            const auto& udevice = check_udevice.value();
            if (udevice.is_initialized())
            {
                const std::string devnode = udevice.get_devnode().value();
                if (!devnode.empty())
                {
                    // block device
                    ztd::logger::info("mount changed: {}", devnode);

                    vfs::volume volume = vfs_volume_read_by_device(udevice);
                    if (volume != nullptr)
                    { // frees volume if needed
                        volume->device_added();
                    }
                }
            }
        }
    }
    // ztd::logger::debug("END PARSE");
}

static const std::optional<std::string>
get_devmount_fstype(dev_t device)
{
    const auto major = gnu_dev_major(device);
    const auto minor = gnu_dev_minor(device);

    for (const devmount_t& devmount : devmounts)
    {
        if (devmount->major == major && devmount->minor == minor)
        {
            return devmount->fstype;
        }
    }
    return std::nullopt;
}

static bool
cb_mount_monitor_watch(Glib::IOCondition condition)
{
    if (condition == Glib::IOCondition::IO_ERR)
    {
        return true;
    }

    // ztd::logger::debug("@@@ {} changed", MOUNTINFO);
    parse_mounts(true);

    return true;
}

static bool
cb_udev_monitor_watch(Glib::IOCondition condition)
{
    if (condition == Glib::IOCondition::IO_NVAL)
    {
        return false;
    }
    else if (condition != Glib::IOCondition::IO_IN)
    {
        if (condition == Glib::IOCondition::IO_HUP)
        {
            return false;
        }
        return true;
    }

    const auto check_udevice = umonitor.receive_device();
    if (check_udevice)
    {
        const auto& udevice = check_udevice.value();

        const auto action = udevice.get_action().value();
        const auto devnode = udevice.get_devnode().value();
        if (action.empty())
        {
            return false;
        }

        // print action
        if (ztd::same(action, "add"))
        {
            ztd::logger::info("udev added:   {}", devnode);
        }
        else if (ztd::same(action, "remove"))
        {
            ztd::logger::info("udev removed: {}", devnode);
        }
        else if (ztd::same(action, "change"))
        {
            ztd::logger::info("udev changed: {}", devnode);
        }
        else if (ztd::same(action, "move"))
        {
            ztd::logger::info("udev moved:   {}", devnode);
        }

        // add/remove volume
        if (ztd::same(action, "add") || ztd::same(action, "change"))
        {
            vfs::volume volume = vfs_volume_read_by_device(udevice);
            if (volume != nullptr)
            { // frees volume if needed
                volume->device_added();
            }
        }
        else if (ztd::same(action, "remove"))
        {
            vfs_volume_device_removed(udevice);
        }
        // what to do for move action?

        // TOOD refresh
        parse_mounts(true);

        main_window_close_all_invalid_tabs();
    }
    return true;
}

void
VFSVolume::set_info() noexcept
{
    std::string disp_device;
    std::string disp_label;
    std::string disp_size;
    std::string disp_mount;
    std::string disp_fstype;
    std::string disp_devnum;

    // set display name
    if (this->is_mounted)
    {
        disp_label = this->label;

        if (this->size > 0)
        {
            disp_size = vfs_file_size_format(this->size, false);
        }

        if (!this->mount_point.empty())
        {
            disp_mount = fmt::format("{}", this->mount_point);
        }
        else
        {
            disp_mount = "???";
        }
    }
    else if (this->is_mountable)
    { // has_media
        disp_label = this->label;

        if (this->size > 0)
        {
            disp_size = vfs_file_size_format(this->size, false);
        }
        disp_mount = "---";
    }
    else
    {
        disp_label = "[no media]";
    }

    disp_device = this->device_file;
    disp_fstype = this->fs_type;
    disp_devnum = fmt::format("{}:{}", gnu_dev_major(this->devnum), gnu_dev_minor(this->devnum));

    std::string parameter;
    const char* user_format = xset_get_s(XSetName::DEV_DISPNAME);
    if (user_format)
    {
        parameter = user_format;
        parameter = ztd::replace(parameter, "%v", disp_device);
        parameter = ztd::replace(parameter, "%s", disp_size);
        parameter = ztd::replace(parameter, "%t", disp_fstype);
        parameter = ztd::replace(parameter, "%l", disp_label);
        parameter = ztd::replace(parameter, "%m", disp_mount);
        parameter = ztd::replace(parameter, "%n", disp_devnum);
    }
    else
    {
        parameter = fmt::format("{} {} {} {} {}",
                                disp_device,
                                disp_size,
                                disp_fstype,
                                disp_label,
                                disp_mount);
    }

    parameter = ztd::replace(parameter, "  ", " ");

    this->disp_name = Glib::filename_display_name(parameter);
    if (this->udi.empty())
    {
        this->udi = this->device_file;
    }
}

static vfs::volume
vfs_volume_read_by_device(const libudev::device& udevice)
{ // uses udev to read device parameters into returned volume
    if (!udevice.is_initialized())
    {
        return nullptr;
    }

    const device_t device = std::make_shared<Device>(udevice);
    if (!device_get_info(device) || device->devnode.empty() || device->devnum == 0 ||
        !ztd::startswith(device->devnode, "/dev/"))
    {
        return nullptr;
    }

    // translate device info to VFSVolume
    const auto volume = new VFSVolume();

    volume->devnum = device->devnum;
    volume->device_type = VFSVolumeDeviceType::BLOCK;
    volume->device_file = device->devnode;
    volume->udi = device->device_by_id;
    volume->is_optical = device->device_is_optical_disc;
    volume->is_removable = !device->device_is_system_internal;
    volume->requires_eject = device->drive_is_media_ejectable;
    volume->is_mountable = device->device_is_media_available;
    volume->is_mounted = device->device_is_mounted;
    volume->is_user_visible = device->udevice.is_partition() ||
                              (device->udevice.is_removable() && !device->udevice.is_disk());
    volume->ever_mounted = false;
    if (!device->mount_points.empty())
    {
        if (ztd::contains(device->mount_points.data(), ","))
        {
            volume->mount_point = ztd::partition(device->mount_points, ",")[0];
        }
        else
        {
            volume->mount_point = device->mount_points;
        }
    }
    volume->size = device->device_size;
    volume->label = device->id_label;
    volume->fs_type = device->filesystem;

    // adjustments
    volume->ever_mounted = volume->is_mounted;

    volume->set_info();

    // ztd::logger::debug("====devnum={}:{}", gnu_dev_major(volume->devnum), gnu_dev_minor(volume->devnum));
    // ztd::logger::debug("    device_file={}", volume->device_file);
    // ztd::logger::debug("    udi={}", volume->udi);
    // ztd::logger::debug("    label={}", volume->label);
    // ztd::logger::debug("    icon={}", volume->icon);
    // ztd::logger::debug("    is_mounted={}", volume->is_mounted);
    // ztd::logger::debug("    is_mountable={}", volume->is_mountable);
    // ztd::logger::debug("    is_optical={}", volume->is_optical);
    // ztd::logger::debug("    is_removable={}", volume->is_removable);
    // ztd::logger::debug("    requires_eject={}", volume->requires_eject);
    // ztd::logger::debug("    is_user_visible={}", volume->is_user_visible);
    // ztd::logger::debug("    mount_point={}", volume->mount_point);
    // ztd::logger::debug("    size={}", volume->size);
    // ztd::logger::debug("    disp_name={}", volume->disp_name);

    return volume;
}

bool
is_path_mountpoint(const std::string_view path)
{
    if (!std::filesystem::exists(path))
    {
        return false;
    }

    const auto path_stat_dev = ztd::stat(path).dev();
    const auto path_statvfs_fsid = ztd::statvfs(path).fsid();

    return (path_stat_dev == path_statvfs_fsid);
}

const std::optional<std::string>
VFSVolume::device_mount_cmd() noexcept
{
    const std::string path = Glib::find_program_in_path("udiskie-mount");
    if (path.empty())
    {
        return std::nullopt;
    }
    return fmt::format("{} {}", path, ztd::shell::quote(this->device_file));
}

const std::optional<std::string>
VFSVolume::device_unmount_cmd() noexcept
{
    const std::string path = Glib::find_program_in_path("udiskie-umount");
    if (path.empty())
    {
        return std::nullopt;
    }
    return fmt::format("{} {}", path, ztd::shell::quote(this->mount_point));
}

void
VFSVolume::device_added() noexcept
{ // frees volume if needed
    if (this->udi.empty() || this->device_file.empty())
    {
        return;
    }

    // check if we already have this volume device file
    for (const vfs::volume volume : vfs_volume_get_all_volumes())
    {
        if (volume->devnum == this->devnum)
        {
            // update existing volume
            const bool was_mounted = volume->is_mounted;

            // detect changed mount point
            std::string changed_mount_point;
            if (!was_mounted && this->is_mounted)
            {
                changed_mount_point = this->mount_point;
            }
            else if (was_mounted && !this->is_mounted)
            {
                changed_mount_point = volume->mount_point;
            }

            volume->udi = this->udi;
            volume->device_file = this->device_file;
            volume->label = this->label;
            volume->mount_point = this->mount_point;
            volume->icon = this->icon;
            volume->disp_name = this->disp_name;
            volume->is_mounted = this->is_mounted;
            volume->is_mountable = this->is_mountable;
            volume->is_optical = this->is_optical;
            volume->requires_eject = this->requires_eject;
            volume->is_removable = this->is_removable;
            volume->is_user_visible = this->is_user_visible;
            volume->size = this->size;
            volume->fs_type = this->fs_type;

            // Mount and ejection detect for automount
            if (this->is_mounted)
            {
                volume->ever_mounted = true;
            }
            else
            {
                if (this->is_removable && !this->is_mountable)
                { // ejected
                    volume->ever_mounted = false;
                }
            }

            call_callbacks(volume, VFSVolumeState::CHANGED);

            // refresh tabs containing changed mount point
            if (!changed_mount_point.empty())
            {
                main_window_refresh_all_tabs_matching(changed_mount_point);
            }

            volume->set_info();

            return;
        }
    }

    // add as new volume
    volumes.emplace_back(this);
    call_callbacks(this, VFSVolumeState::ADDED);

    // refresh tabs containing changed mount point
    if (this->is_mounted && !this->mount_point.empty())
    {
        main_window_refresh_all_tabs_matching(this->mount_point);
    }
}

static void
vfs_volume_device_removed(const libudev::device& udevice)
{
    if (!udevice.is_initialized())
    {
        return;
    }

    const dev_t devnum = udevice.get_devnum();

    for (const vfs::volume volume : vfs_volume_get_all_volumes())
    {
        if (volume->device_type == VFSVolumeDeviceType::BLOCK && volume->devnum == devnum)
        { // remove volume
            // ztd::logger::debug("remove volume {}", volume->device_file);
            volumes.erase(std::remove(volumes.begin(), volumes.end(), volume), volumes.end());
            call_callbacks(volume, VFSVolumeState::REMOVED);
            if (volume->is_mounted && !volume->mount_point.empty())
            {
                main_window_refresh_all_tabs_matching(volume->mount_point);
            }
            delete volume;

            break;
        }
    }
}

bool
vfs_volume_init()
{
    // create udev
    if (!udev.is_initialized())
    {
        ztd::logger::warn("unable to initialize udev");
        return false;
    }

    // read all block mount points
    parse_mounts(false);

    // enumerate devices
    const auto enumerate = udev.enumerate_new();
    if (enumerate.is_initialized())
    {
        enumerate.add_match_subsystem("block");
        enumerate.scan_devices();
        const auto devices = enumerate.enumerate_devices();
        for (const auto& device : devices)
        {
            const auto syspath = device.get_syspath();
            const auto udevice = udev.device_from_syspath(syspath.value());
            if (udevice)
            {
                vfs::volume volume = vfs_volume_read_by_device(udevice.value());
                if (volume != nullptr)
                { // frees volume if needed
                    volume->device_added();
                }
            }
        }
    }

    // enumerate non-block
    parse_mounts(true);

    // start udev monitor
    const auto check_umonitor = udev.monitor_new_from_netlink("udev");
    if (!check_umonitor)
    {
        ztd::logger::warn("cannot create udev from netlink");
        return false;
    }
    umonitor = check_umonitor.value();
    if (!umonitor.is_initialized())
    {
        ztd::logger::warn("cannot create udev monitor");
        return false;
    }
    if (!umonitor.enable_receiving())
    {
        ztd::logger::warn("cannot enable udev monitor receiving");
        return false;
    }
    if (!umonitor.filter_add_match_subsystem_devtype("block"))
    {
        ztd::logger::warn("cannot set udev filter");
        return false;
    }

    const i32 ufd = umonitor.get_fd();
    if (ufd == 0)
    {
        ztd::logger::warn("cannot get udev monitor socket file descriptor");
        return false;
    }

    uchannel = Glib::IOChannel::create_from_fd(ufd);
    uchannel->set_flags(Glib::IOFlags::NONBLOCK);
    uchannel->set_close_on_unref(true);

    Glib::signal_io().connect(sigc::ptr_fun(cb_udev_monitor_watch),
                              uchannel,
                              Glib::IOCondition::IO_IN | Glib::IOCondition::IO_HUP);

    // start mount monitor
    mchannel = Glib::IOChannel::create_from_file(MOUNTINFO, "r");
    mchannel->set_close_on_unref(true);

    Glib::signal_io().connect(sigc::ptr_fun(cb_mount_monitor_watch),
                              mchannel,
                              Glib::IOCondition::IO_ERR);

    return true;
}

void
vfs_volume_finalize()
{
    // stop global mount monitor
    mchannel = nullptr;

    // stop global udev monitor
    uchannel = nullptr;

    // free all devmounts
    devmounts.clear();

    // free callbacks
    callbacks.clear(); // free all shared_ptr

    // free volumes, uses raw pointers has to be this way
    while (true)
    {
        if (volumes.empty())
        {
            break;
        }

        vfs::volume volume = volumes.back();
        volumes.pop_back();

        delete volume;
    }
}

const std::vector<vfs::volume>&
vfs_volume_get_all_volumes()
{
    return volumes;
}

vfs::volume
vfs_volume_get_by_device(const std::string_view device_file)
{
    for (const vfs::volume volume : vfs_volume_get_all_volumes())
    {
        if (ztd::same(device_file, volume->device_file))
        {
            return volume;
        }
    }
    return nullptr;
}

static void
call_callbacks(vfs::volume vol, VFSVolumeState state)
{
    for (const auto& callback : callbacks)
    {
        callback->cb(vol, state, callback->user_data);
    }

    if (event_handler->device->s || event_handler->device->ob2_data)
    {
        main_window_event(nullptr,
                          nullptr,
                          XSetName::EVT_DEVICE,
                          0,
                          0,
                          vol->device_file.data(),
                          0,
                          0,
                          state,
                          false);
    }
}

void
vfs_volume_add_callback(VFSVolumeCallback cb, void* user_data)
{
    if (cb == nullptr)
    {
        return;
    }

    const volume_callback_data_t data = std::make_shared<VFSVolumeCallbackData>(cb, user_data);

    callbacks.emplace_back(data);
}

void
vfs_volume_remove_callback(VFSVolumeCallback cb, void* user_data)
{
    for (const auto& callback : callbacks)
    {
        if (callback->cb == cb && callback->user_data == user_data)
        {
            ztd::remove(callbacks, callback);
            break;
        }
    }
}

const std::string
VFSVolume::get_disp_name() const noexcept
{
    return this->disp_name;
}

const std::string
VFSVolume::get_mount_point() const noexcept
{
    return this->mount_point;
}

const std::string
VFSVolume::get_device_file() const noexcept
{
    return this->device_file;
}
const std::string
VFSVolume::get_fstype() const noexcept
{
    return this->fs_type;
}

const std::string
VFSVolume::get_icon() const noexcept
{
    return this->icon;
}

bool
vfs_volume_dir_avoid_changes(const std::string_view dir)
{
    // determines if file change detection should be disabled for this
    // dir (eg nfs stat calls block when a write is in progress so file
    // change detection is unwanted)
    // return false to detect changes in this dir, true to avoid change detection

    // ztd::logger::debug("vfs_volume_dir_avoid_changes({})", dir);
    if (!std::filesystem::exists(dir) || !udev.is_initialized())
    {
        return false;
    }

    const std::string canon = std::filesystem::canonical(dir);
    const auto stat = ztd::stat(canon);
    if (!stat.is_valid() || stat.is_block_file())
    {
        return false;
    }
    // ztd::logger::debug("    stat.dev() = {}:{}", gnu_dev_major(stat.dev()), gnu_dev_minor(stat.dev()));

    const auto check_fstype = get_devmount_fstype(stat.dev());
    if (!check_fstype)
    {
        return false;
    }
    const std::string& fstype = check_fstype.value();
    // ztd::logger::debug("    fstype={}", fstype);

    const auto blacklisted = ztd::split(xset_get_s(XSetName::DEV_CHANGE), " ");
    for (const auto& blacklist : blacklisted)
    {
        if (ztd::contains(fstype, blacklist))
        {
            // ztd::logger::debug("    fstype matches blacklisted filesystem {}", blacklist);
            return true;
        }
    }
    // ztd::logger::debug("    fstype does not match any blacklisted filesystems");
    return false;
}
