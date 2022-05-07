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

#include <gtk/gtk.h>
#include <glib.h>
#include <glib-object.h>

#include <sys/types.h>

#include "vfs/vfs-dir.hxx"

#define PTK_FILE_LIST(obj)             (static_cast<PtkFileList*>(obj))
#define PTK_FILE_LIST_REINTERPRET(obj) (reinterpret_cast<PtkFileList*>(obj))

#define PTK_TYPE_FILE_LIST    (ptk_file_list_get_type())
#define PTK_IS_FILE_LIST(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PTK_TYPE_FILE_LIST))

/* Columns of directory view */
enum PTKFileListCol
{
    COL_FILE_BIG_ICON,
    COL_FILE_SMALL_ICON,
    COL_FILE_NAME,
    COL_FILE_SIZE,
    COL_FILE_DESC,
    COL_FILE_PERM,
    COL_FILE_OWNER,
    COL_FILE_MTIME,
    COL_FILE_INFO,
    N_FILE_LIST_COLS
};

// sort_dir of directory view - do not change order, saved
// see also: main-window.c main_window_socket_command() get sort_first
enum PTKFileListSortDir
{
    PTK_LIST_SORT_DIR_MIXED,
    PTK_LIST_SORT_DIR_FIRST,
    PTK_LIST_SORT_DIR_LAST
};

struct PtkFileList
{
    GObject parent;
    /* <private> */
    VFSDir* dir;
    GList* files;
    unsigned int n_files;

    bool show_hidden : 1;
    bool big_thumbnail : 1;
    int max_thumbnail;

    int sort_col;
    GtkSortType sort_order;
    bool sort_alphanum;
    bool sort_natural;
    bool sort_case;
    bool sort_hidden_first;
    char sort_dir;

    // Random integer to check whether an iter belongs to our model
    int stamp;
};

struct PtkFileListClass
{
    GObjectClass parent;
    /* Default signal handlers */
    void (*file_created)(VFSDir* dir, const char* file_name);
    void (*file_deleted)(VFSDir* dir, const char* file_name);
    void (*file_changed)(VFSDir* dir, const char* file_name);
    void (*load_complete)(VFSDir* dir);
};

GType ptk_file_list_get_type();

PtkFileList* ptk_file_list_new(VFSDir* dir, bool show_hidden);

void ptk_file_list_set_dir(PtkFileList* list, VFSDir* dir);

bool ptk_file_list_find_iter(PtkFileList* list, GtkTreeIter* it, VFSFileInfo* fi);

void ptk_file_list_file_created(VFSDir* dir, VFSFileInfo* file, PtkFileList* list);

void ptk_file_list_file_deleted(VFSDir* dir, VFSFileInfo* file, PtkFileList* list);

void ptk_file_list_file_changed(VFSDir* dir, VFSFileInfo* file, PtkFileList* list);

void ptk_file_list_show_thumbnails(PtkFileList* list, bool is_big, int max_file_size);
void ptk_file_list_sort(PtkFileList* list); // sfm
