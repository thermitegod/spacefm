/*
 *  C Interface: ptk-file-menu
 *
 * Description: Popup menu for files
 *
 *
 * Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 */

#pragma once

#include <gtk/gtk.h>
#include "ptk/ptk-file-browser.hxx"

#include "vfs/vfs-file-info.hxx"

/* sel_files is a list containing VFSFileInfo structures
 * The list will be freed in this function, so the caller mustn't
 * free the list after calling this function.
 */

struct PtkFileMenu
{
    PtkFileBrowser* browser;
    char* cwd;
    char* file_path;
    VFSFileInfo* info;
    GList* sel_files;
    GtkAccelGroup* accel_group;
};

GtkWidget* ptk_file_menu_new(PtkFileBrowser* browser, const char* file_path, VFSFileInfo* info,
                             const char* cwd, GList* sel_files);

void ptk_file_menu_add_panel_view_menu(PtkFileBrowser* browser, GtkWidget* menu,
                                       GtkAccelGroup* accel_group);

void on_popup_open_in_new_tab_here(GtkMenuItem* menuitem, PtkFileMenu* data);

void ptk_file_menu_action(PtkFileBrowser* browser, char* setname);

void on_popup_sortby(GtkMenuItem* menuitem, PtkFileBrowser* file_browser, int order);
void on_popup_list_detailed(GtkMenuItem* menuitem, PtkFileBrowser* browser);
void on_popup_list_icons(GtkMenuItem* menuitem, PtkFileBrowser* browser);
void on_popup_list_compact(GtkMenuItem* menuitem, PtkFileBrowser* browser);
void on_popup_list_large(GtkMenuItem* menuitem, PtkFileBrowser* browser);
