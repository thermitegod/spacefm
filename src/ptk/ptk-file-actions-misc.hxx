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

#include "vfs/vfs-file-info.hxx"

void ptk_trash_files(GtkWindow* parent_win, std::string_view cwd,
                     const std::vector<VFSFileInfo*>& sel_files, GtkTreeView* task_view);
void ptk_delete_files(GtkWindow* parent_win, std::string_view cwd,
                      const std::vector<VFSFileInfo*>& sel_files, GtkTreeView* task_view);
