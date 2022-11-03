/**
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

#pragma once

#include <string>
#include <string_view>

#include <vector>

#include <atomic>

#include <memory>

#include <glib.h>

#define VFS_FILE_MONITOR_CALLBACK_DATA(obj) (reinterpret_cast<VFSFileMonitorCallbackEntry*>(obj))

struct VFSFileMonitorCallbackEntry;

enum class VFSFileMonitorEvent
{
    VFS_FILE_MONITOR_CREATE,
    VFS_FILE_MONITOR_DELETE,
    VFS_FILE_MONITOR_CHANGE
};

struct VFSFileMonitor
{
    VFSFileMonitor(std::string_view real_path);
    ~VFSFileMonitor();

    std::string path;

    // TODO private
    int wd;
    std::vector<VFSFileMonitorCallbackEntry*> callbacks;
};

namespace vfs
{
    using file_monitor_t = std::shared_ptr<VFSFileMonitor>;
}

/* Callback function which will be called when monitored events happen
 *  NOTE: GDK_THREADS_ENTER and GDK_THREADS_LEAVE might be needed
 *  if gtk+ APIs are called in this callback, since the callback is called from
 *  IO channel handler.
 */
using VFSFileMonitorCallback = void (*)(vfs::file_monitor_t monitor, VFSFileMonitorEvent event,
                                        std::string_view file_name, void* user_data);

/*
 * Init monitor:
 * Establish connection with inotify.
 */
bool vfs_file_monitor_init();

/*
 * Monitor changes of a file or directory.
 *
 * Parameters:
 * path: the file/dir to be monitored
 * cb: callback function to be called when file event happens.
 * user_data: user data to be passed to callback function.
 */
vfs::file_monitor_t vfs_file_monitor_add(std::string_view path, VFSFileMonitorCallback cb,
                                         void* user_data);

/*
 * Remove previously installed monitor.
 */
void vfs_file_monitor_remove(vfs::file_monitor_t monitor, VFSFileMonitorCallback cb,
                             void* user_data);

/*
 * Clean up and shutdown file alteration monitor.
 */
void vfs_file_monitor_clean();
