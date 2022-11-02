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

#include <gtk/gtk.h>
#include "ptk/ptk-file-browser.hxx"

#include "vfs/vfs-file-info.hxx"

struct AutoOpenCreate
{
    AutoOpenCreate();
    ~AutoOpenCreate();

    char* path;
    PtkFileBrowser* file_browser;
    GFunc callback;
    bool open_file;
};

enum PtkRenameMode
{
    PTK_RENAME,
    PTK_RENAME_NEW_FILE,
    PTK_RENAME_NEW_DIR,
    PTK_RENAME_NEW_LINK
};

void ptk_trash_files(GtkWindow* parent_win, const char* cwd,
                     const std::vector<VFSFileInfo*>& sel_files, GtkTreeView* task_view);
void ptk_delete_files(GtkWindow* parent_win, const char* cwd,
                      const std::vector<VFSFileInfo*>& sel_files, GtkTreeView* task_view);

int ptk_rename_file(PtkFileBrowser* file_browser, const char* file_dir, VFSFileInfo* file,
                    const char* dest_dir, bool clip_copy, PtkRenameMode create_new,
                    AutoOpenCreate* auto_open);

void ptk_show_file_properties(GtkWindow* parent_win, const char* cwd,
                              std::vector<VFSFileInfo*>& sel_files, int page);

/* sel_files is a list of VFSFileInfo
 * app_desktop is the application used to open the files.
 * If app_desktop == nullptr, each file will be opened with its
 * default application. */
void ptk_open_files_with_app(const char* cwd, const std::vector<VFSFileInfo*>& sel_files,
                             const char* app_desktop, PtkFileBrowser* file_browser, bool xforce,
                             bool xnever);

void ptk_file_misc_paste_as(PtkFileBrowser* file_browser, const char* cwd, GFunc callback);
void ptk_file_misc_rootcmd(PtkFileBrowser* file_browser, const std::vector<VFSFileInfo*>& sel_files,
                           const char* cwd, const char* setname);

char* get_real_link_target(const char* link_path);
