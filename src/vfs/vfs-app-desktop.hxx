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

#include <string>

#include <atomic>

#include <glib.h>
#include <gtk/gtk.h>

struct VFSAppDesktop
{
    std::string file_name;
    std::string disp_name;
    char* comment;
    char* exec;
    char* icon_name;
    char* path;      // working dir
    char* full_path; // path of desktop file
    bool terminal : 1;
    bool hidden : 1;
    bool startup : 1;

    void ref_inc();
    void ref_dec();
    unsigned int ref_count();

  private:
    std::atomic<unsigned int> n_ref{0};
};

/*
 * If file_name is not a full path, this function searches default paths
 * for the desktop file.
 */
VFSAppDesktop* vfs_app_desktop_new(const char* file_name);

void vfs_app_desktop_unref(void* data);

const char* vfs_app_desktop_get_name(VFSAppDesktop* desktop);

const char* vfs_app_desktop_get_disp_name(VFSAppDesktop* desktop);

const char* vfs_app_desktop_get_exec(VFSAppDesktop* desktop);

GdkPixbuf* vfs_app_desktop_get_icon(VFSAppDesktop* desktop, int size);

const char* vfs_app_desktop_get_icon_name(VFSAppDesktop* desktop);

bool vfs_app_desktop_open_multiple_files(VFSAppDesktop* desktop);

bool vfs_app_desktop_open_in_terminal(VFSAppDesktop* desktop);

bool vfs_app_desktop_open_files(GdkScreen* screen, const char* working_dir, VFSAppDesktop* desktop,
                                GList* file_paths, GError** err);
