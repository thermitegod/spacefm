/*
 *  C Interface: vfs-execute
 *
 * Description:
 *
 *
 * Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 */

#pragma once

#include <glib.h>
#include <gdk/gdk.h>

#define VFS_EXEC_DEFAULT_FLAGS \
    (G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL)

bool vfs_exec_on_screen(GdkScreen* screen, const char* work_dir, char** argv, char** envp,
                        const char* disp_name, GSpawnFlags flags, bool use_startup_notify,
                        GError** err);
