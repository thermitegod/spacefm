/**
 * Copyright (C) 2013-2014 OmegaPhil <OmegaPhil@startmail.com>
 * Copyright (C) 2014 IgnorantGuru <ignorantguru@gmx.com>
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

#include "ptk/ptk-file-browser.hxx"

// Archive operations enum
enum PTKFileArchiverArc
{
    ARC_COMPRESS,
    ARC_EXTRACT,
    ARC_LIST
};

// Pass file_browser or desktop depending on where you are calling from
void ptk_file_archiver_create(PtkFileBrowser* file_browser, std::vector<VFSFileInfo*>& sel_files,
                              const char* cwd);
void ptk_file_archiver_extract(PtkFileBrowser* file_browser, std::vector<VFSFileInfo*>& sel_files,
                               const char* cwd, const char* dest_dir, int job,
                               bool archive_presence_checked);
