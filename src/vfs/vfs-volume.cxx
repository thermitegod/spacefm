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

#include <format>

#include <filesystem>

#include <array>
#include <utility>
#include <vector>

#include <optional>

#include <algorithm>
#include <ranges>

#include <memory>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "xset/xset.hxx"
#include "xset/xset-event-handler.hxx"

#include "main-window.hxx"

#include "vfs/vfs-utils.hxx"

#include "vfs/vfs-device.hxx"

#include "vfs/libudevpp/libudevpp.hxx"
#include "vfs/linux/procfs.hxx"
#include "vfs/linux/sysfs.hxx"

#include "vfs/vfs-volume.hxx"

static vfs::volume vfs_volume_read_by_device(const libudev::device& udevice);
static void vfs_volume_device_removed(const libudev::device& udevice);
static void call_callbacks(vfs::volume vol, vfs::volume_state state);

struct VFSVolumeCallbackData
{
    VFSVolumeCallbackData() = delete;
    VFSVolumeCallbackData(vfs::volume_callback callback, void* callback_data);

    vfs::volume_callback cb{nullptr};
    void* user_data{nullptr};
};

VFSVolumeCallbackData::VFSVolumeCallbackData(vfs::volume_callback callback, void* callback_data)
{
    this->cb = callback;
    this->user_data = callback_data;
}

using volume_callback_data_t = std::shared_ptr<VFSVolumeCallbackData>;

static std::vector<vfs::volume> volumes;
static std::vector<volume_callback_data_t> callbacks;

static libudev::udev udev;
static libudev::monitor umonitor;

static Glib::RefPtr<Glib::IOChannel> uchannel = nullptr;
static Glib::RefPtr<Glib::IOChannel> mchannel = nullptr;

/*
 * DeviceMount
 */

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

static vfs::volume
vfs_volume_read_by_device(const libudev::device& udevice)
{ // uses udev to read device parameters into returned volume
    if (!udevice.is_initialized())
    {
        return nullptr;
    }

    const vfs::device_t device = std::make_shared<VFSDevice>(udevice);
    if (!device->is_valid() || device->devnode().empty() || device->devnum() == 0 ||
        !ztd::startswith(device->devnode(), "/dev/"))
    {
        return nullptr;
    }

    // translate device info to VFSVolume
    const auto volume = new VFSVolume(device);
    return volume;
}

bool
is_path_mountpoint(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path))
    {
        return false;
    }

    const auto path_stat_dev = ztd::stat(path).dev();
    const auto path_statvfs_fsid = ztd::statvfs(path).fsid();

    return (path_stat_dev == path_statvfs_fsid);
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
        if (volume->is_device_type(vfs::volume_device_type::block) && volume->devnum() == devnum)
        { // remove volume
            // ztd::logger::debug("remove volume {}", volume->device_file);
            volumes.erase(std::remove(volumes.begin(), volumes.end(), volume), volumes.end());
            call_callbacks(volume, vfs::volume_state::removed);
            if (volume->is_mounted() && !volume->mount_point().empty())
            {
                main_window_refresh_all_tabs_matching(volume->mount_point());
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
        if (ztd::same(device_file, volume->device_file()))
        {
            return volume;
        }
    }
    return nullptr;
}

static void
call_callbacks(vfs::volume vol, vfs::volume_state state)
{
    for (const auto& callback : callbacks)
    {
        callback->cb(vol, state, callback->user_data);
    }

    if (event_handler->device->s || event_handler->device->ob2_data)
    {
        main_window_event(nullptr,
                          nullptr,
                          xset::name::evt_device,
                          0,
                          0,
                          vol->device_file().data(),
                          0,
                          0,
                          state,
                          false);
    }
}

void
vfs_volume_add_callback(vfs::volume_callback cb, void* user_data)
{
    if (cb == nullptr)
    {
        return;
    }

    const volume_callback_data_t data = std::make_shared<VFSVolumeCallbackData>(cb, user_data);

    callbacks.emplace_back(data);
}

void
vfs_volume_remove_callback(vfs::volume_callback cb, void* user_data)
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

bool
vfs_volume_dir_avoid_changes(const std::filesystem::path& dir)
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

    const auto canon = std::filesystem::canonical(dir);
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

    const auto dev_change = xset_get_s(xset::name::dev_change).value_or("");
    const auto blacklisted = ztd::split(dev_change, " ");
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

/*
 * VFSVolume
 */

VFSVolume::VFSVolume(const vfs::device_t& device)
{
    this->devnum_ = device->devnum();
    this->device_type_ = vfs::volume_device_type::block;
    this->device_file_ = device->devnode();
    this->udi_ = device->id();
    this->is_optical_ = device->is_optical_disc();
    this->is_removable_ = !device->is_system_internal();
    this->requires_eject_ = device->is_media_ejectable();
    this->is_mountable_ = device->is_media_available();
    this->is_mounted_ = device->is_mounted();
    this->is_user_visible_ = device->udevice.is_partition() ||
                             (device->udevice.is_removable() && !device->udevice.is_disk());
    this->ever_mounted_ = false;
    if (!device->mount_points().empty())
    {
        if (ztd::contains(device->mount_points(), ","))
        {
            this->mount_point_ = ztd::partition(device->mount_points(), ",")[0];
        }
        else
        {
            this->mount_point_ = device->mount_points();
        }
    }
    this->size_ = device->size();
    this->label_ = device->id_label();
    this->fstype_ = device->fstype();

    // adjustments
    this->ever_mounted_ = this->is_mounted();

    this->set_info();

    // ztd::logger::debug("====devnum={}:{}", gnu_dev_major(this->devnum_), gnu_dev_minor(this->devnum_));
    // ztd::logger::debug("    device_file={}", this->device_file_);
    // ztd::logger::debug("    udi={}", this->udi_);
    // ztd::logger::debug("    label={}", this->label_);
    // ztd::logger::debug("    icon={}", this->icon_);
    // ztd::logger::debug("    is_mounted={}", this->is_mounted_);
    // ztd::logger::debug("    is_mountable={}", this->is_mountable_);
    // ztd::logger::debug("    is_optical={}", this->is_optical_);
    // ztd::logger::debug("    is_removable={}", this->is_removable_);
    // ztd::logger::debug("    requires_eject={}", this->requires_eject_);
    // ztd::logger::debug("    is_user_visible={}", this->is_user_visible_);
    // ztd::logger::debug("    mount_point={}", this->mount_point_);
    // ztd::logger::debug("    size={}", this->size_);
    // ztd::logger::debug("    disp_name={}", this->disp_name_);
}

const std::optional<std::string>
VFSVolume::device_mount_cmd() noexcept
{
    const std::filesystem::path path = Glib::find_program_in_path("udiskie-mount");
    if (path.empty())
    {
        return std::nullopt;
    }
    return std::format("{} {}", path.string(), ztd::shell::quote(this->device_file_));
}

const std::optional<std::string>
VFSVolume::device_unmount_cmd() noexcept
{
    const std::filesystem::path path = Glib::find_program_in_path("udiskie-umount");
    if (path.empty())
    {
        return std::nullopt;
    }
    return std::format("{} {}", path.string(), ztd::shell::quote(this->mount_point_));
}

const std::string_view
VFSVolume::display_name() const noexcept
{
    return this->disp_name_;
}

const std::string_view
VFSVolume::mount_point() const noexcept
{
    return this->mount_point_;
}

const std::string_view
VFSVolume::device_file() const noexcept
{
    return this->device_file_;
}
const std::string_view
VFSVolume::fstype() const noexcept
{
    return this->fstype_;
}

const std::string_view
VFSVolume::icon() const noexcept
{
    return this->icon_;
}

const std::string_view
VFSVolume::udi() const noexcept
{
    return this->udi_;
}

const std::string_view
VFSVolume::label() const noexcept
{
    return this->label_;
}

dev_t
VFSVolume::devnum() const noexcept
{
    return this->devnum_;
}

dev_t
VFSVolume::size() const noexcept
{
    return this->size_;
}

bool
VFSVolume::is_device_type(vfs::volume_device_type type) const noexcept
{
    return type == this->device_type_;
}

bool
VFSVolume::is_mounted() const noexcept
{
    return this->is_mounted_;
}

bool
VFSVolume::is_removable() const noexcept
{
    return this->is_removable_;
}

bool
VFSVolume::is_mountable() const noexcept
{
    return this->is_mountable_;
}

bool
VFSVolume::is_user_visible() const noexcept
{
    return this->is_user_visible_;
}

bool
VFSVolume::is_optical() const noexcept
{
    return this->is_optical_;
}

bool
VFSVolume::requires_eject() const noexcept
{
    return this->requires_eject_;
}

bool
VFSVolume::ever_mounted() const noexcept
{
    return this->ever_mounted_;
}

void
VFSVolume::device_added() noexcept
{ // frees volume if needed
    if (this->udi_.empty() || this->device_file_.empty())
    {
        return;
    }

    // check if we already have this volume device file
    for (const vfs::volume volume : vfs_volume_get_all_volumes())
    {
        if (volume->devnum() == this->devnum_)
        {
            // update existing volume
            const bool was_mounted = volume->is_mounted();

            // detect changed mount point
            std::string changed_mount_point;
            if (!was_mounted && this->is_mounted())
            {
                changed_mount_point = this->mount_point_;
            }
            else if (was_mounted && !this->is_mounted())
            {
                changed_mount_point = volume->mount_point();
            }

            volume->udi_ = this->udi_;
            volume->device_file_ = this->device_file_;
            volume->label_ = this->label_;
            volume->mount_point_ = this->mount_point_;
            volume->icon_ = this->icon_;
            volume->disp_name_ = this->disp_name_;
            volume->is_mounted_ = this->is_mounted_;
            volume->is_mountable_ = this->is_mountable_;
            volume->is_optical_ = this->is_optical_;
            volume->requires_eject_ = this->requires_eject_;
            volume->is_removable_ = this->is_removable_;
            volume->is_user_visible_ = this->is_user_visible_;
            volume->size_ = this->size_;
            volume->fstype_ = this->fstype_;

            // Mount and ejection detect for automount
            if (this->is_mounted())
            {
                volume->ever_mounted_ = true;
            }
            else
            {
                if (this->is_removable() && !this->is_mountable())
                { // ejected
                    volume->ever_mounted_ = false;
                }
            }

            call_callbacks(volume, vfs::volume_state::changed);

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
    call_callbacks(this, vfs::volume_state::added);

    // refresh tabs containing changed mount point
    if (this->is_mounted() && !this->mount_point_.empty())
    {
        main_window_refresh_all_tabs_matching(this->mount_point_);
    }
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
    if (this->is_mounted())
    {
        disp_label = this->label_;

        if (this->size_ > 0)
        {
            disp_size = vfs_file_size_format(this->size_, false);
        }

        if (!this->mount_point_.empty())
        {
            disp_mount = std::format("{}", this->mount_point_);
        }
        else
        {
            disp_mount = "???";
        }
    }
    else if (this->is_mountable())
    { // has_media
        disp_label = this->label_;

        if (this->size_ > 0)
        {
            disp_size = vfs_file_size_format(this->size_, false);
        }
        disp_mount = "---";
    }
    else
    {
        disp_label = "[no media]";
    }

    disp_device = this->device_file_;
    disp_fstype = this->fstype_;
    disp_devnum = std::format("{}:{}", gnu_dev_major(this->devnum_), gnu_dev_minor(this->devnum_));

    std::string parameter;
    const auto user_format = xset_get_s(xset::name::dev_dispname);
    if (user_format)
    {
        parameter = user_format.value();
        parameter = ztd::replace(parameter, "%v", disp_device);
        parameter = ztd::replace(parameter, "%s", disp_size);
        parameter = ztd::replace(parameter, "%t", disp_fstype);
        parameter = ztd::replace(parameter, "%l", disp_label);
        parameter = ztd::replace(parameter, "%m", disp_mount);
        parameter = ztd::replace(parameter, "%n", disp_devnum);
    }
    else
    {
        parameter = std::format("{} {} {} {} {}",
                                disp_device,
                                disp_size,
                                disp_fstype,
                                disp_label,
                                disp_mount);
    }

    parameter = ztd::replace(parameter, "  ", " ");

    this->disp_name_ = parameter;
    if (this->udi_.empty())
    {
        this->udi_ = this->device_file_;
    }
}
