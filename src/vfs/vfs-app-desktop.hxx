/*
 *  C Interface: vfs-app-desktop
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
#include <gtk/gtk.h>

typedef struct VFSAppDesktop
{
    char* file_name;
    char* disp_name;
    char* comment;
    char* exec;
    char* icon_name;
    char* path;      // working dir
    char* full_path; // path of desktop file
    bool terminal : 1;
    bool hidden : 1;
    bool startup : 1;

    /* <private> */
    int n_ref;
} VFSAppDesktop;

/*
 * If file_name is not a full path, this function searches default paths
 * for the desktop file.
 */
VFSAppDesktop* vfs_app_desktop_new(const char* file_name);

void vfs_app_desktop_unref(void* data);

const char* vfs_app_desktop_get_name(VFSAppDesktop* app);

const char* vfs_app_desktop_get_disp_name(VFSAppDesktop* app);

const char* vfs_app_desktop_get_exec(VFSAppDesktop* app);

GdkPixbuf* vfs_app_desktop_get_icon(VFSAppDesktop* app, int size, bool use_fallback);

const char* vfs_app_desktop_get_icon_name(VFSAppDesktop* app);

bool vfs_app_desktop_open_multiple_files(VFSAppDesktop* app);

bool vfs_app_desktop_open_in_terminal(VFSAppDesktop* app);

bool vfs_app_desktop_open_files(GdkScreen* screen, const char* working_dir, VFSAppDesktop* app,
                                GList* file_paths, GError** err);
