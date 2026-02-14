/**
 * Copyright (C) 2015 IgnorantGuru <ignorantguru@gmx.com>
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
#include <string_view>

#include <gtkmm.h>

#include "gui/file-browser.hxx"

#include "vfs/volume.hxx"

namespace gui::view::location
{
GtkWidget* create(gui::browser* browser) noexcept;

bool chdir(GtkTreeView* location_view, const std::filesystem::path& current_path) noexcept;
void on_action(GtkWidget* view, const xset_t& set) noexcept;
std::shared_ptr<vfs::volume> selected_volume(GtkTreeView* location_view) noexcept;
void update_volume_icons() noexcept;
void mount_network(gui::browser* browser, const std::string_view url, const bool new_tab,
                   const bool force_new_mount) noexcept;
void dev_menu(GtkWidget* parent, gui::browser* browser, GtkWidget* menu) noexcept;
bool open_block(const std::filesystem::path& block, const bool new_tab) noexcept;
} // namespace gui::view::location
