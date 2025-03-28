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
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include <glibmm.h>
#include <gtkmm.h>

#include "vfs/vfs-file.hxx"

namespace ptk::clipboard
{
void cut_or_copy_files(const std::span<const std::shared_ptr<vfs::file>> selected_files,
                       bool copy) noexcept;
void copy_as_text(const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept;
void copy_name(const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept;
void paste_files(GtkWindow* parent_win, const std::filesystem::path& dest_dir,
                 GtkTreeView* task_view, GFunc callback, GtkWindow* callback_win) noexcept;
void paste_links(GtkWindow* parent_win, const std::filesystem::path& dest_dir,
                 GtkTreeView* task_view, GFunc callback, GtkWindow* callback_win) noexcept;
void paste_targets(GtkWindow* parent_win, const std::filesystem::path& dest_dir,
                   GtkTreeView* task_view, GFunc callback, GtkWindow* callback_win) noexcept;
void copy_text(const std::string_view text) noexcept;
void cut_or_copy_file_list(const std::span<const std::string> selected_files, bool copy) noexcept;
std::vector<std::filesystem::path> get_file_paths(const std::filesystem::path& cwd, bool* is_cut,
                                                  i32* missing_targets) noexcept;
} // namespace ptk::clipboard
