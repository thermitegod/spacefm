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
#include <optional>

#include <gtkmm.h>

#include "gui/file-browser.hxx"

namespace gui::view::dir_tree
{
GtkWidget* create(gui::browser* browser, bool show_hidden) noexcept;

bool chdir(GtkTreeView* dir_tree_view, const std::filesystem::path& path) noexcept;
void show_hidden_files(GtkTreeView* dir_tree_view, bool show_hidden) noexcept;
std::optional<std::filesystem::path> selected_dir(GtkTreeView* dir_tree_view) noexcept;
std::optional<std::filesystem::path> dir_path(GtkTreeModel* model, GtkTreeIter* it) noexcept;
} // namespace gui::view::dir_tree
