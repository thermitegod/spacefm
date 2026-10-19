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

#include <algorithm>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <sys/types.h>

#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "vfs/volume/device-mount.hxx"
#include "vfs/volume/device.hxx"
#include "vfs/volume/manager.hxx"
#include "vfs/volume/volume.hxx"

#include "vfs/linux/sysfs.hxx"

#include "libudevpp/libudevpp.hxx"
#include "logger.hxx"

#undef major
#undef minor
#undef makedev

vfs::volume_manager::volume_manager()
{
    if (!udev_.is_initialized())
    {
        logger::warn<logger::vfs>("unable to initialize udev");
        return;
    }

    parse_mounts(false);

    const auto enumerate = udev_.enumerate_new();
    if (enumerate.is_initialized())
    {
        enumerate.add_match_subsystem("block");
        enumerate.scan_devices();
        const auto devices = enumerate.enumerate_devices();
        for (const auto& device : devices)
        {
            const auto syspath = device.get_syspath();
            const auto udevice = udev_.device_from_syspath(syspath.value());
            if (udevice)
            {
                const auto volume = read_by_device(udevice.value());
                if (volume != nullptr)
                {
                    volumes_.push_back(volume);
                }
            }
        }
    }

    parse_mounts(true);

    const auto check_umonitor = udev_.monitor_new_from_netlink("udev");
    if (!check_umonitor)
    {
        logger::warn<logger::vfs>("cannot create udev from netlink");
        return;
    }
    umonitor_ = check_umonitor.value();
    if (!umonitor_.is_initialized())
    {
        logger::warn<logger::vfs>("cannot create udev monitor");
        return;
    }
    if (!umonitor_.enable_receiving())
    {
        logger::warn<logger::vfs>("cannot enable udev monitor receiving");
        return;
    }
    if (!umonitor_.filter_add_match_subsystem_devtype("block"))
    {
        logger::warn<logger::vfs>("cannot set udev filter");
        return;
    }

    const auto ufd = umonitor_.get_fd();
    if (ufd == 0)
    {
        logger::warn<logger::vfs>("cannot get udev monitor socket file descriptor");
        return;
    }

    uchannel_ = Glib::IOChannel::create_from_fd(ufd);
    uchannel_->set_flags(Glib::IOFlags::NONBLOCK);
    uchannel_->set_close_on_unref(false);

    signal_uchannel_ = Glib::signal_io().connect(
        [this](const Glib::IOCondition condition)
        {
            if (condition == Glib::IOCondition::IO_NVAL)
            {
                return false;
            }
            else if (condition != Glib::IOCondition::IO_IN)
            {
                return condition != Glib::IOCondition::IO_HUP;
            }

            const auto udevice = umonitor_.receive_device();
            if (udevice)
            {
                const auto action = udevice->get_action().value();
                if (action.empty())
                {
                    return false;
                }

                const auto devnode = udevice->get_devnode().value_or("unknown");
                const auto devnum = udevice->get_devnum();

                logger::info_if<logger::vfs>(action == "add", "udev added:   {}", devnode);
                logger::info_if<logger::vfs>(action == "remove", "udev removed: {}", devnode);
                logger::info_if<logger::vfs>(action == "change", "udev changed: {}", devnode);
                logger::info_if<logger::vfs>(action == "move", "udev moved:   {}", devnode);

                if (action == "add")
                {
                    const auto device_data = vfs::device::create(udevice.value());
                    if (device_data && device_data->is_valid())
                    {
                        auto new_volume = vfs::volume::create(device_data);
                        volumes_.push_back(new_volume);
                        signal_added().emit(devnode);
                    }
                }
                if (action == "change")
                {
                    const auto device_data = vfs::device::create(udevice.value());
                    if (device_data && device_data->is_valid())
                    {
                        auto it = std::ranges::find_if(volumes_,
                                                       [devnum](const auto& v)
                                                       { return v->devnum() == devnum; });

                        if (it != volumes_.cend())
                        {
                            (*it)->update_from_device(device_data);
                            signal_changed().emit(devnode);
                        }
                    }
                }
                else if (action == "remove")
                {
                    if (!udevice->is_initialized())
                    {
                        return false;
                    }

                    auto it = std::ranges::find_if(volumes_,
                                                   [devnum](const auto& v)
                                                   { return v->devnum() == devnum; });

                    if (it != volumes_.cend())
                    {
                        auto remove = *it;
                        if (remove->is_mounted() && !remove->mount_point().empty())
                        {
                            // TODO: main_window_refresh_all_tabs_matching(removeremove->mount_point());
                        }

                        volumes_.erase(it);

                        signal_removed().emit(devnode);
                    }
                }
                else if (action == "remove")
                {
                    // TODO what to do for move action?
                }

                parse_mounts(true);
            }

            return true;
        },
        uchannel_,
        Glib::IOCondition::IO_IN | Glib::IOCondition::IO_HUP);

    mchannel_ = Glib::IOChannel::create_from_file("/proc/self/mountinfo", "r");
    mchannel_->set_close_on_unref(true);

    signal_mchannel_ = Glib::signal_io().connect(
        [this](const Glib::IOCondition condition)
        {
            if ((condition & Glib::IOCondition::IO_PRI) != Glib::IOCondition{})
            {
                parse_mounts(true);
            }

            if ((condition & (Glib::IOCondition::IO_ERR | Glib::IOCondition::IO_HUP |
                              Glib::IOCondition::IO_NVAL)) != Glib::IOCondition{})
            {
                return false;
            }

            return true;
        },
        mchannel_,
        Glib::IOCondition::IO_PRI | Glib::IOCondition::IO_ERR | Glib::IOCondition::IO_HUP |
            Glib::IOCondition::IO_NVAL);
}

vfs::volume_manager::~volume_manager()
{
    signal_uchannel_.disconnect();
    signal_mchannel_.disconnect();

    uchannel_.reset();

    if (mchannel_)
    {
        try
        {
            mchannel_->close();
        }
        catch (const Glib::Error& ex)
        {
            logger::warn<logger::vfs>("Failed to close mchannel: {}", ex.what());
        }
        mchannel_.reset();
    }
}

std::shared_ptr<vfs::volume_manager>
vfs::volume_manager::create() noexcept
{
    struct hack : public vfs::volume_manager
    {
        hack() : volume_manager() {}
    };

    return std::make_shared<hack>();
}

std::span<const std::shared_ptr<vfs::volume>>
vfs::volume_manager::get_all() noexcept
{
    return volumes_;
}

std::optional<std::string>
vfs::volume_manager::devmount_fstype(const dev_t device) const noexcept
{
    const auto major = gnu_dev_major(device);
    const auto minor = gnu_dev_minor(device);

    for (const auto& devmount : devmounts_)
    {
        if (devmount->major == major && devmount->minor == minor)
        {
            return devmount->fstype;
        }
    }
    return std::nullopt;
}

std::shared_ptr<vfs::volume>
vfs::volume_manager::read_by_device(const libudev::device& udevice) const noexcept
{
    if (!udevice.is_initialized())
    {
        return nullptr;
    }

    const auto device = vfs::device::create(udevice);
    if (!device->is_valid() || device->devnode().empty() || device->devnum() == 0 ||
        !device->devnode().starts_with("/dev/"))
    {
        return nullptr;
    }

    return vfs::volume::create(device);
}

void
vfs::volume_manager::parse_mounts(const bool report) noexcept
{
    // logger::debug<logger::vfs>("parse_mounts() report={}", report);

    std::vector<std::shared_ptr<device_mount>> newmounts;
    std::unordered_map<dev_t, std::shared_ptr<device_mount>> device_map;

    const auto listmounts = ztd::listmount::create();
    for (const auto& mount : listmounts->statmounts())
    {
        // mount where only a subtree of a filesystem is mounted?
        const bool subdir_mount = mount.root() != "/";

        if (mount.mount_point().empty())
        {
            continue;
        }

        // logger::debug<logger::vfs>("mount_point({}:{})={}", mount.dev_major(), mount.dev_minor(), mount.mount_point());
        const dev_t devnum = gnu_dev_makedev(static_cast<std::uint32_t>(mount.dev_major()),
                                             static_cast<std::uint32_t>(mount.dev_minor()));

        auto& devmount = device_map[devnum];

        if (!devmount)
        {
            // logger::debug<logger::vfs>("     new devmount {}", mount.mount_point());
            const auto udevice = udev_.device_from_devnum('b', devnum);
            const bool is_block = udevice && udevice->is_initialized();

            if (subdir_mount && is_block)
            {
                // is block device with subdir mount - ignore
                device_map.erase(devnum);
                continue;
            }

            if (report || is_block)
            {
                devmount = device_mount::create(mount.dev_major(), mount.dev_minor());
                devmount->fstype = mount.fs_type();
                newmounts.push_back(devmount);
            }
            else
            {
                device_map.erase(devnum);
                continue;
            }
        }

        if (devmount && !std::ranges::contains(devmount->mounts, mount.mount_point()))
        {
            // logger::debug<logger::vfs>("    prepended");
            if (devmount && !std::ranges::contains(devmount->mounts, mount.mount_point()))
            {
                devmount->mounts.push_back(std::string(mount.mount_point()));
                devmount->mount_points = ztd::join(devmount->mounts, ",");
            }
        }
    }

    std::vector<std::shared_ptr<device_mount>> changed;
    if (report)
    {
        for (const auto& devmount : newmounts)
        {
            // logger::debug<logger::vfs>("finding {}:{}", devmount->major, devmount->minor);

            auto it = std::ranges::find_if(devmounts_,
                                           [&](const auto& search)
                                           {
                                               return search && devmount->major == search->major &&
                                                      devmount->minor == search->minor;
                                           });

            if (it != devmounts_.cend())
            {
                // logger::debug<logger::vfs>("    found");
                if ((*it)->mount_points == devmount->mount_points)
                {
                    // logger::debug<logger::vfs>("    freed");
                    // no change to mount points, so remove from old list
                    devmounts_.erase(it);
                }
            }
            else
            {
                // new mount
                // logger::debug<logger::vfs>("    new mount {}:{} {}", devmount->major, devmount->minor, devmount->mount_points);
                const auto devcopy = device_mount::create(devmount->major, devmount->minor);
                devcopy->mount_points = devmount->mount_points;
                devcopy->mounts = devmount->mounts;
                devcopy->fstype = devmount->fstype;

                for (const auto& mount : devcopy->mounts)
                {
                    signal_mounted().emit(mount);
                }

                changed.push_back(devcopy);
            }
        }

        // logger::debug<logger::vfs>("REMAINING");
        // any remaining devices in old list have changed mount status
        for (const auto& devmount : devmounts_)
        {
            // logger::debug<logger::vfs>("remain {}:{}", devmount->major, devmount->minor);
            if (devmount)
            {
                changed.push_back(devmount);
            }
        }
    }

    devmounts_ = newmounts;

    if (report && !changed.empty())
    {
        for (const auto& devmount : changed)
        {
            const dev_t devnum = gnu_dev_makedev(static_cast<std::uint32_t>(devmount->major),
                                                 static_cast<std::uint32_t>(devmount->minor));
            const auto udevice = udev_.device_from_devnum('b', devnum);

            if (!udevice || !udevice->is_initialized())
            {
                continue;
            }

            const auto devnode = udevice->get_devnode().value_or("");
            if (devnode.empty())
            {
                continue;
            }

            logger::info<logger::vfs>("mount changed: {}", devnode);

            const auto device_data = vfs::device::create(udevice.value());
            if (device_data && device_data->is_valid())
            {
                auto it =
                    std::ranges::find_if(volumes_,
                                         [devnum](const auto& v) { return v->devnum() == devnum; });

                if (it != volumes_.cend())
                {
                    (*it)->update_from_device(device_data);
                    signal_changed().emit(devnode);
                }
                else
                {
                    auto new_volume = vfs::volume::create(device_data);
                    volumes_.push_back(new_volume);
                    signal_added().emit(devnode);
                }
            }
        }
    }
    // logger::debug<logger::vfs>("END PARSE");
}

bool
vfs::volume_manager::avoid_changes(const std::filesystem::path& dir) const noexcept
{
    // determines if file change detection should be disabled for this
    // dir (eg nfs stat calls block when a write is in progress so file
    // change detection is unwanted)
    // return false to detect changes in this dir, true to avoid change detection

    // logger::debug<logger::vfs>("vfs::volume_dir_avoid_changes({})", dir);
    if (!std::filesystem::exists(dir) || !udev_.is_initialized())
    {
        return false;
    }

    const auto canon = std::filesystem::canonical(dir);

    const auto stat = ztd::stat::create(canon);
    if (!stat || (stat && stat->is_block_file()))
    {
        return false;
    }
    // logger::debug<logger::vfs>("    stat.dev() = {}:{}", gnu_dev_major(stat.dev()), gnu_dev_minor(stat.dev()));

    const auto fstype = devmount_fstype(stat->dev().data());
    if (!fstype)
    {
        return false;
    }
    // logger::debug<logger::vfs>("    fstype={}", fstype);

    static constexpr auto blacklisted = std::array<std::string_view, 6>{
        "cifs",
        "curlftpfs",
        "ftpfs",
        "fuse.sshfs",
        "nfs",
        "smbfs",
    };

    const auto has_blacklisted = [&fstype](std::string_view blacklisted)
    { return fstype->contains(blacklisted); };

    const auto is_blacklisted = std::ranges::any_of(blacklisted, has_blacklisted);

    // logger::debug_if<logger::vfs>(is_blacklisted, "    fstype '{}' matches blacklisted filesystem", fstype.value());

    return is_blacklisted;
}
