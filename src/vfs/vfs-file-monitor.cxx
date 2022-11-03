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

#include <filesystem>

#include <map>

#include <memory>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/inotify.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-file-monitor.hxx"

// #define VFS_FILE_MONITOR_DEBUG

#define BUF_LEN (1024 * (sizeof(inotify_event) + 16))

static std::map<std::string, vfs::file_monitor_t> monitor_map;

static GIOChannel* vfs_inotify_io_channel = nullptr;
static unsigned int vfs_inotify_io_watch = 0;
static int vfs_inotify_fd = -1;

/* event handler of all inotify events */
static bool vfs_file_monitor_on_inotify_event(GIOChannel* channel, GIOCondition cond,
                                              void* user_data);

struct VFSFileMonitorCallbackEntry
{
    VFSFileMonitorCallbackEntry(VFSFileMonitorCallback callback, void* user_data);

    VFSFileMonitorCallback callback;
    void* user_data;
};

VFSFileMonitorCallbackEntry::VFSFileMonitorCallbackEntry(VFSFileMonitorCallback callback,
                                                         void* user_data)
{
    this->callback = callback;
    this->user_data = user_data;
}

VFSFileMonitor::VFSFileMonitor(std::string_view real_path)
{
    // LOG_INFO("VFSFileMonitor Constructor");
    this->path = real_path.data();
}

VFSFileMonitor::~VFSFileMonitor()
{
    // LOG_INFO("VFSFileMonitor Destructor");

    // LOG_INFO("vfs_file_monitor_remove  {}", this->wd);
    inotify_rm_watch(vfs_inotify_fd, this->wd);

    monitor_map.erase(this->path);

    this->callbacks.clear();
}

static bool
vfs_file_monitor_connect_to_inotify()
{
    vfs_inotify_fd = inotify_init();
    if (vfs_inotify_fd < 0)
    {
        vfs_inotify_io_channel = nullptr;
        LOG_WARN("failed to initialize inotify.");
        return false;
    }
    vfs_inotify_io_channel = g_io_channel_unix_new(vfs_inotify_fd);

    g_io_channel_set_encoding(vfs_inotify_io_channel, nullptr, nullptr);
    g_io_channel_set_buffered(vfs_inotify_io_channel, false);
    g_io_channel_set_flags(vfs_inotify_io_channel, G_IO_FLAG_NONBLOCK, nullptr);

    vfs_inotify_io_watch = g_io_add_watch(vfs_inotify_io_channel,
                                          GIOCondition(G_IO_IN | G_IO_PRI | G_IO_HUP | G_IO_ERR),
                                          (GIOFunc)vfs_file_monitor_on_inotify_event,
                                          nullptr);
    return true;
}

static void
vfs_file_monitor_disconnect_from_inotify()
{
    if (!vfs_inotify_io_channel)
        return;

    g_io_channel_unref(vfs_inotify_io_channel);
    vfs_inotify_io_channel = nullptr;
    g_source_remove(vfs_inotify_io_watch);
    close(vfs_inotify_fd);
    vfs_inotify_fd = -1;
}

void
vfs_file_monitor_clean()
{
    vfs_file_monitor_disconnect_from_inotify();

    // monitor_map.clear();
}

bool
vfs_file_monitor_init()
{
    if (!vfs_file_monitor_connect_to_inotify())
        return false;
    return true;
}

vfs::file_monitor_t
vfs_file_monitor_add(std::string_view path, VFSFileMonitorCallback cb, void* user_data)
{
    // inotify does not follow symlinks, need to get real path
    const std::string real_path = std::filesystem::absolute(path);

    vfs::file_monitor_t monitor;

    try
    {
        monitor = monitor_map.at(real_path);
    }
    catch (std::out_of_range)
    {
        int wd = inotify_add_watch(vfs_inotify_fd,
                                   real_path.c_str(),
                                   IN_MODIFY | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVE |
                                       IN_MOVE_SELF | IN_UNMOUNT | IN_ATTRIB);
        if (wd < 0)
        {
            LOG_ERROR("Failed to add watch on '{}' ({})", real_path, path);
            // const std::string errno_msg = std::strerror(errno);
            // LOG_ERROR("inotify_add_watch: {}", errno_msg);
            return nullptr;
        }
        // LOG_INFO("vfs_file_monitor_add  {} ({}) {}", real_path, path, wd);

        monitor = std::make_shared<VFSFileMonitor>(real_path);
        monitor->wd = wd;

        monitor_map.insert({monitor->path, monitor});
    }

    // LOG_DEBUG("monitor installed for: {}", path);
    if (cb)
    { /* Install a callback */
        VFSFileMonitorCallbackEntry* cb_ent = new VFSFileMonitorCallbackEntry(cb, user_data);
        monitor->callbacks.push_back(cb_ent);
    }

    return monitor;
}

void
vfs_file_monitor_remove(vfs::file_monitor_t monitor, VFSFileMonitorCallback cb, void* user_data)
{
    if (!monitor)
        return;

    // LOG_INFO("vfs_file_monitor_remove");
    if (cb)
    {
        for (VFSFileMonitorCallbackEntry* cb2: monitor->callbacks)
        {
            if (cb2->callback == cb && cb2->user_data == VFS_FILE_MONITOR_CALLBACK_DATA(user_data))
            {
                ztd::remove(monitor->callbacks, cb2);
                delete cb2;
                break;
            }
        }
    }
    // LOG_INFO("vfs_file_monitor_remove   DONE");
}

static void
vfs_file_monitor_reconnect_inotify(std::string_view path, vfs::file_monitor_t monitor)
{
    if (!std::filesystem::exists(path))
        return;

    monitor->wd =
        inotify_add_watch(vfs_inotify_fd, path.data(), IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVE);
    if (monitor->wd < 0)
    {
        /*
         * FIXME: add monitor to an ancestor which does actually exist,
         *        or do the equivalent of inotify-missing.c by maintaining
         *        a list of monitors on non-existent files/directories
         *        which you retry in a timeout.
         */
        const std::string errno_msg = std::strerror(errno);
        LOG_WARN("Failed to add monitor on '{}': {}", path, errno_msg);
    }
}

static VFSFileMonitorEvent
vfs_file_monitor_translate_inotify_event(int inotify_mask)
{
    if (inotify_mask & (IN_CREATE | IN_MOVED_TO))
        return VFSFileMonitorEvent::VFS_FILE_MONITOR_CREATE;
    else if (inotify_mask & (IN_DELETE | IN_MOVED_FROM | IN_DELETE_SELF | IN_UNMOUNT))
        return VFSFileMonitorEvent::VFS_FILE_MONITOR_DELETE;
    else if (inotify_mask & (IN_MODIFY | IN_ATTRIB))
        return VFSFileMonitorEvent::VFS_FILE_MONITOR_CHANGE;
    else
    {
        // IN_IGNORED not handled
        // LOG_WARN("translate_inotify_event mask not handled {}", inotify_mask);
        return VFSFileMonitorEvent::VFS_FILE_MONITOR_CHANGE;
    }
}

static void
vfs_file_monitor_dispatch_event(vfs::file_monitor_t monitor, VFSFileMonitorEvent evt,
                                std::string_view file_name)
{
    /* Call the callback functions */
    if (monitor->callbacks.empty())
        return;

    for (VFSFileMonitorCallbackEntry* cb: monitor->callbacks)
    {
        VFSFileMonitorCallback func = cb->callback;
        func(monitor, evt, file_name, cb->user_data);
    }
}

static bool
vfs_file_monitor_on_inotify_event(GIOChannel* channel, GIOCondition cond, void* user_data)
{
    (void)channel;
    (void)user_data;

    char buf[BUF_LEN];

    if (cond & (G_IO_HUP | G_IO_ERR))
    {
        vfs_file_monitor_disconnect_from_inotify();
        if (monitor_map.size() > 0)
        {
            // Disconnected from inotify server, but there are still monitors, reconnect
            if (vfs_file_monitor_connect_to_inotify())
            {
                // for (auto it = monitor_map.begin(); it != monitor_map.end(); ++it)
                for (const auto& fm: monitor_map)
                {
                    vfs_file_monitor_reconnect_inotify(fm.first, fm.second);
                }
            }
        }
        // do not need to remove the event source since
        // it has been removed by vfs_monitor_disconnect_from_inotify()
        return true;
    }

    int len;
    while ((len = read(vfs_inotify_fd, buf, BUF_LEN)) < 0 && errno == EINTR)
        ;

    if (len < 0)
    {
        const std::string errno_msg = std::strerror(errno);
        LOG_WARN("Error reading inotify event: {}", errno_msg);
        return false;
    }
    else if (len == 0)
    {
        // FIXME: handle this better?
        LOG_WARN("Error reading inotify event: supplied buffer was too small");
        return false;
    }

    int i = 0;
    while (i < len)
    {
        struct inotify_event* ievent = (struct inotify_event*)&buf[i];
        // FIXME: 2 different paths can have the same wd because of link
        // This was fixed in spacefm 0.8.7 ??

        vfs::file_monitor_t monitor;

        for (const auto& fm: monitor_map)
        {
            if (fm.second->wd != ievent->wd)
                continue;

            monitor = fm.second;
            break;
        }

        if (monitor)
        {
            const std::string file_name = ievent->len > 0 ? (char*)ievent->name : monitor->path;

#ifdef VFS_FILE_MONITOR_DEBUG
            std::stirng desc;
            if (ievent->mask & (IN_CREATE | IN_MOVED_TO))
                desc = "CREATE";
            else if (ievent->mask & (IN_DELETE | IN_MOVED_FROM | IN_DELETE_SELF | IN_UNMOUNT))
                desc = "DELETE";
            else if (ievent->mask & (IN_MODIFY | IN_ATTRIB))
                desc = "CHANGE";

            LOG_INFO("inotify-event {}: {}///{}", desc, monitor->path, file_name);
            LOG_DEBUG("inotify ({}) :{}", ievent->mask, file_name);
#endif

            vfs_file_monitor_dispatch_event(monitor,
                                            vfs_file_monitor_translate_inotify_event(ievent->mask),
                                            file_name);
        }
        i += sizeof(inotify_event) + ievent->len;
    }
    return true;
}
