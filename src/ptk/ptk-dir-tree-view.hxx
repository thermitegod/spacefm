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

#include <filesystem>

#include <gtk/gtk.h>

#include "ptk/ptk-file-browser.hxx"

/* Create a new dir tree view */
GtkWidget* ptk_dir_tree_view_new(PtkFileBrowser* browser, bool show_hidden);

bool ptk_dir_tree_view_chdir(GtkTreeView* dir_tree_view, const std::filesystem::path& path);

/* Return a newly allocated string containing path of current selected dir. */
char* ptk_dir_tree_view_get_selected_dir(GtkTreeView* dir_tree_view);

void ptk_dir_tree_view_show_hidden_files(GtkTreeView* dir_tree_view, bool show_hidden);

char* ptk_dir_view_get_dir_path(GtkTreeModel* model, GtkTreeIter* it);
