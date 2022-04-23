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

#include <glib.h>
#include <gtk/gtk.h>

void ptk_clipboard_cut_or_copy_files(const char* working_dir, GList* files, bool copy);

void ptk_clipboard_copy_as_text(const char* working_dir,
                                GList* files); // MOD added

void ptk_clipboard_copy_name(const char* working_dir,
                             GList* files); // MOD added

void ptk_clipboard_paste_files(GtkWindow* parent_win, const char* dest_dir, GtkTreeView* task_view,
                               GFunc callback, GtkWindow* callback_win);

void ptk_clipboard_paste_links(GtkWindow* parent_win, const char* dest_dir, GtkTreeView* task_view,
                               GFunc callback, GtkWindow* callback_win);

void ptk_clipboard_paste_targets(GtkWindow* parent_win, const char* dest_dir,
                                 GtkTreeView* task_view, GFunc callback, GtkWindow* callback_win);

void ptk_clipboard_copy_text(const char* text); // MOD added

void ptk_clipboard_copy_file_list(char** path, bool copy); // sfm

std::vector<std::string> ptk_clipboard_get_file_paths(const char* cwd, bool* is_cut,
                                                      int* missing_targets);
