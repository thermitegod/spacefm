/**
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

#include <vector>

#include <gtk/gtk.h>

#include "ptk/ptk-file-browser.hxx"

#include "vfs/vfs-file-info.hxx"

/* sel_files is a list of VFSFileInfo
 * app_desktop is the application used to open the files.
 * If app_desktop == nullptr, each file will be opened with its
 * default application. */
void ptk_open_files_with_app(const char* cwd, const std::vector<vfs::file_info>& sel_files,
                             const char* app_desktop, PtkFileBrowser* file_browser, bool xforce,
                             bool xnever);
