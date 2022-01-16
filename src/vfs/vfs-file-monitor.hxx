/*
 *  C Interface: vfs-monitor
 *
 * Description: File alteration monitor
 *
 *
 * Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 */

#pragma once

#include <atomic>

#include <glib.h>

#include <unistd.h>
#include <sys/inotify.h>

#define VFS_FILE_MONITOR_CALLBACK_DATA(obj) (reinterpret_cast<VFSFileMonitorCallbackEntry*>(obj))

enum VFSFileMonitorEvent
{
    VFS_FILE_MONITOR_CREATE,
    VFS_FILE_MONITOR_DELETE,
    VFS_FILE_MONITOR_CHANGE
};

struct VFSFileMonitor
{
    char* path;

    // TODO private
    int wd;
    GArray* callbacks;

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
VFSFileMonitor* vfs_file_monitor_add(char* path, VFSFileMonitorCallback cb, void* user_data);

/*
 * Remove previously installed monitor.
 */
void vfs_file_monitor_remove(VFSFileMonitor* fm, VFSFileMonitorCallback cb, void* user_data);

/*
 * Clean up and shutdown file alteration monitor.
 */
void vfs_file_monitor_clean();
