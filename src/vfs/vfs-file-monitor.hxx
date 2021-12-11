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

/*
  FIXME: VFSFileMonitor can support at most 1024 monitored files.
         This is caused by the limit of FAM/gamin itself.
         Maybe using inotify directly can solve this?
*/

#pragma once

#include <glib.h>

#include <unistd.h>
#include <sys/inotify.h>

enum VFSFileMonitorEvent
{
    VFS_FILE_MONITOR_CREATE,
    VFS_FILE_MONITOR_DELETE,
    VFS_FILE_MONITOR_CHANGE
};

typedef struct VFSFileMonitor
{
    char* path;
    /*<private>*/
    int n_ref;
    int wd;
    GArray* callbacks;
} VFSFileMonitor;

/* Callback function which will be called when monitored events happen
 *  NOTE: GDK_THREADS_ENTER and GDK_THREADS_LEAVE might be needed
 *  if gtk+ APIs are called in this callback, since the callback is called from
 *  IO channel handler.
 */
typedef void (*VFSFileMonitorCallback)(VFSFileMonitor* fm, VFSFileMonitorEvent event,
                                       const char* file_name, void* user_data);

/*
 * Init monitor:
 * Establish connection with gamin/fam.
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
VFSFileMonitor* vfs_file_monitor_add(char* path, bool is_dir, VFSFileMonitorCallback cb,
                                     void* user_data);

/*
 * Monitor changes of a file.
 *
 * Parameters:
 * path: the file/dir to be monitored
 * cb: callback function to be called when file event happens.
 * user_data: user data to be passed to callback function.
 */
#define vfs_file_monitor_add_file(path, cb, user_data) \
    vfs_file_monitor_add(path, false, cb, user_data)

/*
 * Monitor changes of a directory.
 *
 * Parameters:
 * path: the file/dir to be monitored
 * cb: callback function to be called when file event happens.
 * user_data: user data to be passed to callback function.
 */
#define vfs_file_monitor_add_dir(path, cb, user_data) \
    vfs_file_monitor_add(path, true, cb, user_data)

/*
 * Remove previously installed monitor.
 */
void vfs_file_monitor_remove(VFSFileMonitor* fm, VFSFileMonitorCallback cb, void* user_data);

/*
 * Clearn up and shutdown file alteration monitor.
 */
void vfs_file_monitor_clean();
