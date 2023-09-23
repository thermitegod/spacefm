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

#include <string_view>

#include <filesystem>

#include <span>
#include <vector>

#include <gtk/gtk.h>

#include <glibmm.h>

#include "vfs/vfs-file-info.hxx"

void ptk_clipboard_cut_or_copy_files(const std::span<const vfs::file_info> sel_files, bool copy);

void ptk_clipboard_copy_as_text(const std::span<const vfs::file_info> sel_files);

void ptk_clipboard_copy_name(const std::span<const vfs::file_info> sel_files);

void ptk_clipboard_paste_files(GtkWindow* parent_win, const std::filesystem::path& dest_dir,
                               GtkTreeView* task_view, GFunc callback, GtkWindow* callback_win);

void ptk_clipboard_paste_links(GtkWindow* parent_win, const std::filesystem::path& dest_dir,
                               GtkTreeView* task_view, GFunc callback, GtkWindow* callback_win);

void ptk_clipboard_paste_targets(GtkWindow* parent_win, const std::filesystem::path& dest_dir,
                                 GtkTreeView* task_view, GFunc callback, GtkWindow* callback_win);

void ptk_clipboard_copy_text(const std::string_view text);

void ptk_clipboard_copy_file_list(char** path, bool copy);

const std::vector<std::filesystem::path>
ptk_clipboard_get_file_paths(const std::filesystem::path& cwd, bool* is_cut, i32* missing_targets);
