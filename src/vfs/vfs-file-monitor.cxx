/**
 * Copyright (C) 2014 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 * Copyright (C) 2005 Red Hat, Inc.
 * Copyright (C) 2006 Mark McLoughlin
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

// Most of the inotify parts are taken from "menu-monitor-inotify.c" of
// gnome-menus, which is licensed under GNU Lesser General Public License.

#include <string>
#include <string_view>

#include <format>

#include <filesystem>

#include <map>

#include <memory>

#include <algorithm>
#include <ranges>

#include <sys/inotify.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-file-monitor.hxx"

// #define VFS_FILE_MONITOR_DEBUG

// clang-format off
inline constexpr u32 EVENT_SIZE = (sizeof(struct inotify_event));
inline constexpr u32 BUF_LEN    = (1024 * (EVENT_SIZE + 16));
// clang-format on

static std::map<std::filesystem::path, vfs::file_monitor> monitor_map;
static std::mutex monitor_mutex;

static i32 inotify_fd = -1;
static Glib::RefPtr<Glib::IOChannel> inotify_io_channel = nullptr;

// event handler of all inotify events
static bool vfs_file_monitor_on_inotify_event(Glib::IOCondition condition);

struct VFSFileMonitorCallbackEntry
{
    VFSFileMonitorCallbackEntry() = delete;
    ~VFSFileMonitorCallbackEntry() = default;
    // ~VFSFileMonitorCallbackEntry(){ztd::logger::info("VFSFileMonitorCallbackEntry Destructor");};

    VFSFileMonitorCallbackEntry(vfs::file_monitor_callback callback, void* user_data);

    vfs::file_monitor_callback callback() const noexcept;
    void* user_data() const noexcept;

  private:
    vfs::file_monitor_callback callback_{nullptr};
    void* user_data_{nullptr};
};

VFSFileMonitorCallbackEntry::VFSFileMonitorCallbackEntry(vfs::file_monitor_callback callback,
                                                         void* user_data)
    : callback_(callback), user_data_(user_data)
{
    // ztd::logger::info("VFSFileMonitorCallbackEntry Constructor");
}

vfs::file_monitor_callback
VFSFileMonitorCallbackEntry::callback() const noexcept
{
    return this->callback_;
}

void*
VFSFileMonitorCallbackEntry::user_data() const noexcept
{
    return this->user_data_;
}

VFSFileMonitor::VFSFileMonitor(const std::filesystem::path& real_path, i32 wd)
    : path_(real_path), wd_(wd)
{
    // ztd::logger::info("VFSFileMonitor Constructor {}", real_path);
}

VFSFileMonitor::~VFSFileMonitor()
{
    // ztd::logger::info("VFSFileMonitor Destructor {}", this->wd);
    inotify_rm_watch(inotify_fd, this->wd_);
}

void
VFSFileMonitor::add_user() noexcept
{
    this->user_count += 1;
}

void
VFSFileMonitor::remove_user() noexcept
{
    this->user_count -= 1;
}

bool
VFSFileMonitor::has_users() const noexcept
{
    return (this->user_count != 0);
}

const std::filesystem::path&
VFSFileMonitor::path() const noexcept
{
    return this->path_;
}

i32
VFSFileMonitor::wd() const noexcept
{
    return this->wd_;
}

void
VFSFileMonitor::add_callback(vfs::file_monitor_callback callback, void* user_data) noexcept
{
    const auto cb_ent = std::make_shared<VFSFileMonitorCallbackEntry>(callback, user_data);

    this->callbacks_.emplace_back(cb_ent);
}

void
VFSFileMonitor::remove_callback(vfs::file_monitor_callback callback, void* user_data) noexcept
{
    for (const auto& installed_callback : this->callbacks_)
    {
        if (installed_callback->callback() == callback &&
            installed_callback->user_data() == user_data)
        {
            ztd::remove(this->callbacks_, installed_callback);
            return;
        }
    }
}

const std::vector<vfs::file_monitor_callback_entry>
VFSFileMonitor::callbacks() const noexcept
{
    return this->callbacks_;
}

static bool
vfs_file_monitor_connect_to_inotify()
{
    inotify_fd = inotify_init();
    if (inotify_fd == -1)
    {
        ztd::logger::error("failed to initialize inotify");
        return false;
    }

    inotify_io_channel = Glib::IOChannel::create_from_fd(inotify_fd);
    inotify_io_channel->set_buffered(true);
    inotify_io_channel->set_flags(Glib::IOFlags::NONBLOCK);

    Glib::signal_io().connect(sigc::ptr_fun(vfs_file_monitor_on_inotify_event),
                              inotify_io_channel,
                              Glib::IOCondition::IO_IN | Glib::IOCondition::IO_PRI |
                                  Glib::IOCondition::IO_HUP | Glib::IOCondition::IO_ERR);

    return true;
}

static void
vfs_file_monitor_disconnect_from_inotify()
{
    if (!inotify_io_channel)
    {
        return;
    }

    close(inotify_fd);
    inotify_fd = -1;
}

void
vfs_file_monitor_clean()
{
    std::lock_guard<std::mutex> lock(monitor_mutex);

    vfs_file_monitor_disconnect_from_inotify();

    monitor_map.clear();
}

bool
vfs_file_monitor_init()
{
    if (!vfs_file_monitor_connect_to_inotify())
    {
        return false;
    }
    return true;
}

vfs::file_monitor
vfs_file_monitor_add(const std::filesystem::path& path, vfs::file_monitor_callback callback,
                     void* user_data)
{
    std::lock_guard<std::mutex> lock(monitor_mutex);

    // inotify does not follow symlinks, need to get real path
    const auto real_path = std::filesystem::absolute(path);

    vfs::file_monitor monitor;

    if (monitor_map.contains(real_path))
    {
        monitor = monitor_map.at(real_path);
        monitor->add_user();
    }
    else
    {
        const i32 wd = inotify_add_watch(inotify_fd,
                                         real_path.c_str(),
                                         IN_MODIFY | IN_CREATE | IN_DELETE | IN_DELETE_SELF |
                                             IN_MOVE | IN_MOVE_SELF | IN_UNMOUNT | IN_ATTRIB);
        if (wd < 0)
        {
            ztd::logger::error("Failed to add watch on '{}' ({})", real_path, path.string());
            // const std::string errno_msg = std::strerror(errno);
            // ztd::logger::error("inotify_add_watch: {}", errno_msg);
            return nullptr;
        }
        // ztd::logger::info("vfs_file_monitor_add  {} ({}) {}", real_path, path, wd);

        monitor = std::make_shared<VFSFileMonitor>(real_path, wd);
        monitor_map.insert({monitor->path(), monitor});
    }

    // ztd::logger::debug("monitor installed for: {}", path);
    if (callback)
    { // Install a callback
        monitor->add_callback(callback, user_data);
    }

    return monitor;
}

void
vfs_file_monitor_remove(const vfs::file_monitor& monitor, vfs::file_monitor_callback callback,
                        void* user_data)
{
    std::lock_guard<std::mutex> lock(monitor_mutex);

    if (!monitor || !callback)
    {
        return;
    }

    monitor->remove_callback(callback, user_data);

    if (monitor_map.contains(monitor->path()))
    {
        monitor->remove_user();
        if (!monitor->has_users())
        {
            monitor_map.erase(monitor->path());
        }
    }
}

static void
vfs_file_monitor_dispatch_event(const vfs::file_monitor& monitor, vfs::file_monitor_event event,
                                const std::filesystem::path& file_name)
{
    // Call the callback functions
    for (const vfs::file_monitor_callback_entry& cb : monitor->callbacks())
    {
        vfs::file_monitor_callback func = cb->callback();
        func(monitor, event, file_name, cb->user_data());
    }
}

static bool
vfs_file_monitor_on_inotify_event(Glib::IOCondition condition)
{
    if (condition == Glib::IOCondition::IO_HUP || condition == Glib::IOCondition::IO_ERR)
    {
        ztd::logger::error("Disconnected from inotify server");
        return false;
    }

    char buffer[BUF_LEN];
    const auto length = read(inotify_fd, buffer, BUF_LEN);
    if (length < 0)
    {
        ztd::logger::warn("Error reading inotify event: {}", std::strerror(errno));
        return false;
    }

    std::lock_guard<std::mutex> lock(monitor_mutex);

    u32 i = 0;
    while (i < length)
    {
        const auto event = (struct inotify_event*)&buffer[i];
        // FIXME: 2 different paths can have the same wd because of link
        // This was fixed in spacefm 0.8.7 ??
        if (event->len)
        {
            vfs::file_monitor monitor;

            for (const auto& fm : monitor_map)
            {
                if (fm.second->wd() != event->wd)
                {
                    continue;
                }

                monitor = fm.second;
                break;
            }

            if (monitor)
            {
                const std::filesystem::path file_name = (const char*)event->name;

                vfs::file_monitor_event monitor_event;
                if (event->mask & (IN_CREATE | IN_MOVED_TO))
                {
                    monitor_event = vfs::file_monitor_event::created;
#if defined(VFS_FILE_MONITOR_DEBUG)
                    const auto path = monitor->path() / file_name;
                    ztd::logger::debug("inotify-event MASK={} CREATE={}", event->mask, path);
#endif
                }
                else if (event->mask & (IN_DELETE | IN_MOVED_FROM | IN_DELETE_SELF | IN_UNMOUNT))
                {
                    monitor_event = vfs::file_monitor_event::deleted;
#if defined(VFS_FILE_MONITOR_DEBUG)
                    const auto path = monitor->path() / file_name;
                    ztd::logger::debug("inotify-event MASK={} DELETE={}", event->mask, path);
#endif
                }
                else if (event->mask & (IN_MODIFY | IN_ATTRIB))
                {
                    monitor_event = vfs::file_monitor_event::changed;
#if defined(VFS_FILE_MONITOR_DEBUG)
                    const auto path = monitor->path() / file_name;
                    ztd::logger::debug("inotify-event MASK={} CHANGE={}", event->mask, path);
#endif
                }
                else
                { // IN_IGNORED not handled
                    monitor_event = vfs::file_monitor_event::other;
#if defined(VFS_FILE_MONITOR_DEBUG)
                    const auto path = monitor->path() / file_name;
                    ztd::logger::debug("inotify-event MASK={} OTHER={}", event->mask, path);
#endif
                }

                vfs_file_monitor_dispatch_event(monitor, monitor_event, file_name);
            }
        }
        i += EVENT_SIZE + event->len;
    }

    return true;
}
