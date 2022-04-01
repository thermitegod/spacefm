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

#include <vector>

#include <atomic>

#include <glib.h>

#include <unistd.h>
#include <sys/inotify.h>

#define VFS_FILE_MONITOR(obj)               (static_cast<VFSFileMonitor*>(obj))
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
    VFSFileMonitor(const char* real_path);
    ~VFSFileMonitor();

    const char* path;

    // TODO private
    int wd;
    std::vector<VFSFileMonitorCallbackEntry*> callbacks;

    void ref_inc();
    void ref_dec();
    unsigned int ref_count();

  private:
    std::atomic<unsigned int> n_ref{0};
};

/* Callback function which will be called when monitored events happen
 *  NOTE: GDK_THREADS_ENTER and GDK_THREADS_LEAVE might be needed
 *  if gtk+ APIs are called in this callback, since the callback is called from
 *  IO channel handler.
 */
typedef void (*VFSFileMonitorCallback)(VFSFileMonitor* fm, VFSFileMonitorEvent event,
                                       const char* file_name, void* user_data);

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
VFSFileMonitor* vfs_file_monitor_add(const char* path, VFSFileMonitorCallback cb, void* user_data);

/*
 * Remove previously installed monitor.
 */
void vfs_file_monitor_remove(VFSFileMonitor* fm, VFSFileMonitorCallback cb, void* user_data);

/*
 * Clean up and shutdown file alteration monitor.
 */
void vfs_file_monitor_clean();
