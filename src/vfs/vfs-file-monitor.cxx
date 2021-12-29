/*
 *  C Implementation: vfs-monitor
 *
 * Description: File alteration monitor
 *
 *
 * Copyright (C) 2014 IgnorantGuru <ignorantguru@gmx.com>
 * Original Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 * Most of the inotify parts are taken from "menu-monitor-inotify.c" of
 * gnome-menus, which is licensed under GNU Lesser General Public License.
 *
 * Copyright (C) 2005 Red Hat, Inc.
 * Copyright (C) 2006 Mark McLoughlin
 */

#include <sys/stat.h>

#include <linux/limits.h>

#include "logger.hxx"

#include "vfs/vfs-file-monitor.hxx"

struct VFSFileMonitorCallbackEntry
{
    VFSFileMonitorCallback callback;
    void* user_data;
};

static GHashTable* monitor_hash = nullptr;
static GIOChannel* fam_io_channel = nullptr;
static unsigned int fam_io_watch = 0;
static int inotify_fd = -1;

/* event handler of all FAM events */
static bool on_fam_event(GIOChannel* channel, GIOCondition cond, void* user_data);

static bool
connect_to_fam()
{
    inotify_fd = inotify_init();
    if (inotify_fd < 0)
    {
        fam_io_channel = nullptr;
        LOG_WARN("failed to initialize inotify.");
        return false;
    }
    fam_io_channel = g_io_channel_unix_new(inotify_fd);

    /* set fam socket to non-blocking */
    /* fcntl( FAMCONNECTION_GETFD( &fam ),F_SETFL,O_NONBLOCK); */

    g_io_channel_set_encoding(fam_io_channel, nullptr, nullptr);
    g_io_channel_set_buffered(fam_io_channel, false);
    g_io_channel_set_flags(fam_io_channel, G_IO_FLAG_NONBLOCK, nullptr);

    fam_io_watch = g_io_add_watch(fam_io_channel,
                                  GIOCondition(G_IO_IN | G_IO_PRI | G_IO_HUP | G_IO_ERR),
                                  (GIOFunc)on_fam_event,
                                  nullptr);
    return true;
}

static void
disconnect_from_fam()
{
    if (fam_io_channel)
    {
        g_io_channel_unref(fam_io_channel);
        fam_io_channel = nullptr;
        g_source_remove(fam_io_watch);
        close(inotify_fd);
        inotify_fd = -1;
    }
}

/* final cleanup */
void
vfs_file_monitor_clean()
{
    disconnect_from_fam();
    if (monitor_hash)
    {
        g_hash_table_destroy(monitor_hash);
        monitor_hash = nullptr;
    }
}

/*
 * Init monitor:
 * Establish connection with gamin/fam.
 */
bool
vfs_file_monitor_init()
{
    monitor_hash = g_hash_table_new(g_str_hash, g_str_equal);
    if (!connect_to_fam())
        return false;
    return true;
}

VFSFileMonitor*
vfs_file_monitor_add(char* path, bool is_dir, VFSFileMonitorCallback cb, void* user_data)
{
    char resolved_path[PATH_MAX];
    char* real_path;

    // LOG_INFO("vfs_file_monitor_add  {}", path);

    if (!monitor_hash)
        return nullptr;

    // Since gamin, FAM and inotify don't follow symlinks, need to get real path
    if (strlen(path) > PATH_MAX - 1)
    {
        LOG_WARN("PATH_MAX exceeded on {}", path);
        real_path = path; // fallback
    }
    else if (realpath(path, resolved_path) == nullptr)
    {
        LOG_WARN("realpath failed on {}", path);
        real_path = path; // fallback
    }
    else
        real_path = resolved_path;

    VFSFileMonitor* monitor =
        static_cast<VFSFileMonitor*>(g_hash_table_lookup(monitor_hash, real_path));
    if (!monitor)
    {
        monitor = g_slice_new0(VFSFileMonitor);
        monitor->path = g_strdup(real_path);

        monitor->callbacks = g_array_new(false, false, sizeof(VFSFileMonitorCallbackEntry));
        g_hash_table_insert(monitor_hash, monitor->path, monitor);

        monitor->wd = inotify_add_watch(inotify_fd,
                                        real_path,
                                        IN_MODIFY | IN_CREATE | IN_DELETE | IN_DELETE_SELF |
                                            IN_MOVE | IN_MOVE_SELF | IN_UNMOUNT | IN_ATTRIB);
        if (monitor->wd < 0)
        {
            const char* msg;
            switch (errno)
            {
                case EACCES:
                    msg = g_strdup("EACCES Read access to the given directory is not permitted.");
                    break;
                case EBADF:
                    msg = g_strdup("EBADF The given file descriptor is not valid.");
                    break;
                case EFAULT:
                    msg = g_strdup("EFAULT Pathname points outside of the process's accessible "
                                   "address space.");
                    break;
                case EINVAL:
                    msg = g_strdup("EINVAL The given event mask contains no valid events; "
                                   "or fd is not an inotify file descriptor.");
                    break;
                case ENOENT:
                    msg = g_strdup("ENOENT A directory component in pathname does not exist "
                                   "or is a dangling symbolic link.");
                    break;
                case ENOMEM:
                    msg = g_strdup("ENOMEM Insufficient kernel memory was available.");
                    break;
                case ENOSPC:
                    msg = g_strdup(
                        "ENOSPC The user limit on the total number of inotify watches (cat "
                        "/proc/sys/fs/inotify/max_user_watches) was reached or the kernel failed "
                        "to allocate a needed resource.");
                    break;
                default:
                    msg = g_strdup("??? Unknown error.");
                    break;
            }
            LOG_WARN("Failed to add watch on '{}' ('{}'): inotify_add_watch errno {} {}",
                     real_path,
                     path,
                     errno,
                     msg);
            return nullptr;
        }
        // LOG_INFO("vfs_file_monitor_add  {} ({}) {}", real_path, path, monitor->wd);
    }

    if (G_LIKELY(monitor))
    {
        // LOG_DEBUG("monitor installed: {}, {:p}", path, monitor);
        if (cb)
        { /* Install a callback */
            VFSFileMonitorCallbackEntry cb_ent;
            cb_ent.callback = cb;
            cb_ent.user_data = user_data;
            monitor->callbacks = g_array_append_val(monitor->callbacks, cb_ent);
        }
        monitor->ref_inc();
    }
    return monitor;
}

void
vfs_file_monitor_remove(VFSFileMonitor* fm, VFSFileMonitorCallback cb, void* user_data)
{
    if (!fm)
        return;

    // LOG_INFO("vfs_file_monitor_remove");
    if (cb && fm->callbacks)
    {
        VFSFileMonitorCallbackEntry* callbacks =
            VFS_FILE_MONITOR_CALLBACK_DATA(fm->callbacks->data);
        unsigned int i;
        for (i = 0; i < fm->callbacks->len; ++i)
        {
            if (callbacks[i].callback == cb && callbacks[i].user_data == user_data)
            {
                fm->callbacks = g_array_remove_index_fast(fm->callbacks, i);
                break;
            }
        }
    }

    fm->ref_dec();
    if (fm->ref_count() == 0)
    {
        // LOG_INFO("vfs_file_monitor_remove  {}", fm->wd);
        inotify_rm_watch(inotify_fd, fm->wd);

        g_hash_table_remove(monitor_hash, fm->path);
        g_free(fm->path);
        g_array_free(fm->callbacks, true);
        g_slice_free(VFSFileMonitor, fm);
    }
    // LOG_INFO("vfs_file_monitor_remove   DONE");
}

static void
reconnect_fam(void* key, void* value, void* user_data)
{
    struct stat file_stat; // skip stat
    VFSFileMonitor* monitor = static_cast<VFSFileMonitor*>(value);
    const char* path = (const char*)key;
    if (lstat(path, &file_stat) != -1)
    {
        monitor->wd =
            inotify_add_watch(inotify_fd, path, IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVE);
        if (monitor->wd < 0)
        {
            /*
             * FIXME: add monitor to an ancestor which does actually exist,
             *        or do the equivalent of inotify-missing.c by maintaining
             *        a list of monitors on non-existent files/directories
             *        which you retry in a timeout.
             */
            LOG_WARN("Failed to add monitor on '{}': {}", path, g_strerror(errno));
            return;
        }
    }
}

static bool
find_monitor(void* key, void* value, void* user_data)
{
    int wd = GPOINTER_TO_INT(user_data);
    VFSFileMonitor* monitor = static_cast<VFSFileMonitor*>(value);
    return (monitor->wd == wd);
}

static VFSFileMonitorEvent
translate_inotify_event(int inotify_mask)
{
    if (inotify_mask & (IN_CREATE | IN_MOVED_TO))
        return VFS_FILE_MONITOR_CREATE;
    else if (inotify_mask & (IN_DELETE | IN_MOVED_FROM | IN_DELETE_SELF | IN_UNMOUNT))
        return VFS_FILE_MONITOR_DELETE;
    else if (inotify_mask & (IN_MODIFY | IN_ATTRIB))
        return VFS_FILE_MONITOR_CHANGE;
    else
    {
        // IN_IGNORED not handled
        // LOG_WARN("translate_inotify_event mask not handled {}", inotify_mask);
        return VFS_FILE_MONITOR_CHANGE;
    }
}

static void
dispatch_event(VFSFileMonitor* monitor, VFSFileMonitorEvent evt, const char* file_name)
{
    /* Call the callback functions */
    if (monitor->callbacks && monitor->callbacks->len)
    {
        VFSFileMonitorCallbackEntry* cb = VFS_FILE_MONITOR_CALLBACK_DATA(monitor->callbacks->data);
        unsigned int i;
        for (i = 0; i < monitor->callbacks->len; ++i)
        {
            VFSFileMonitorCallback func = cb[i].callback;
            func(monitor, evt, file_name, cb[i].user_data);
        }
    }
}

/* event handler of all FAM events */
static bool
on_fam_event(GIOChannel* channel, GIOCondition cond, void* user_data)
{
#define BUF_LEN (1024 * (sizeof(struct inotify_event) + 16))
    char buf[BUF_LEN];

    if (cond & (G_IO_HUP | G_IO_ERR))
    {
        disconnect_from_fam();
        if (g_hash_table_size(monitor_hash) > 0)
        {
            /*
              Disconnected from FAM server, but there are still monitors.
              This may be caused by crash of FAM server.
              So we have to reconnect to FAM server.
            */
            if (connect_to_fam())
                g_hash_table_foreach(monitor_hash, (GHFunc)reconnect_fam, nullptr);
        }
        return true; /* don't need to remove the event source since
                                    it has been removed by disconnect_from_fam(). */
    }

    int len;
    while ((len = read(inotify_fd, buf, BUF_LEN)) < 0 && errno == EINTR)
        ;
    if (len < 0)
    {
        LOG_WARN("Error reading inotify event: {}", g_strerror(errno));
        /* goto error_cancel; */
        return false;
    }

    if (len == 0)
    {
        /*
         * FIXME: handle this better?
         */
        LOG_WARN("Error reading inotify event: supplied buffer was too small");
        /* goto error_cancel; */
        return false;
    }
    int i = 0;
    while (i < len)
    {
        struct inotify_event* ievent = (struct inotify_event*)&buf[i];
        /* FIXME: 2 different paths can have the same wd because of link
         *        This was fixed in spacefm 0.8.7 ?? */
        VFSFileMonitor* monitor = static_cast<VFSFileMonitor*>(
            g_hash_table_find(monitor_hash, (GHRFunc)find_monitor, GINT_TO_POINTER(ievent->wd)));
        if (G_LIKELY(monitor))
        {
            const char* file_name;
            file_name = ievent->len > 0 ? ievent->name : monitor->path;
            /*
            //MOD for debug output only
            char* desc;
            if ( ievent->mask & ( IN_CREATE | IN_MOVED_TO ) )
                desc = "CREATE";
            else if ( ievent->mask & ( IN_DELETE | IN_MOVED_FROM | IN_DELETE_SELF | IN_UNMOUNT ) )
                desc = "DELETE";
            else if ( ievent->mask & ( IN_MODIFY | IN_ATTRIB ) )
                desc = "CHANGE";
            if ( !strcmp( monitor->path, "/tmp" ) && g_str_has_prefix( file_name, "vte" ) )
            { } // due to current vte scroll problems creating and deleting massive numbers of
            // /tmp/vte8CBO7V types of files, ignore these (creates feedback loop when
            // spacefm is run in terminal because each printf triggers a scroll,
            // which triggers another printf below, which triggers another file change)
            // https://bugs.launchpad.net/ubuntu/+source/vte/+bug/778872
            else
                LOG_INFO("inotify-event {}: {}///{}", desc, monitor->path, file_name);
            //LOG_DEBUG("inotify ({}) :{}", ievent->mask, file_name);
            */
            dispatch_event(monitor, translate_inotify_event(ievent->mask), file_name);
        }
        i += sizeof(struct inotify_event) + ievent->len;
    }
    return true;
}

void
VFSFileMonitor::ref_inc()
{
    ++n_ref;
}

void
VFSFileMonitor::ref_dec()
{
    --n_ref;
}

unsigned int
VFSFileMonitor::ref_count()
{
    return n_ref;
}
