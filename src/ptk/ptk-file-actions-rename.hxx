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

i32 ptk_rename_file(PtkFileBrowser* file_browser, const char* file_dir, vfs::file_info file,
                    const char* dest_dir, bool clip_copy, PtkRenameMode create_new,
                    AutoOpenCreate* auto_open);

void ptk_file_misc_paste_as(PtkFileBrowser* file_browser, const char* cwd, GFunc callback);
void ptk_file_misc_rootcmd(PtkFileBrowser* file_browser, const std::vector<vfs::file_info>& sel_files,
                           const char* cwd, const char* setname);
