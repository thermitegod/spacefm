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

#include <string>
#include <string_view>

#include <gtk/gtk.h>

#include "vfs/vfs-volume.hxx"

// Location View
GtkWidget* ptk_location_view_new(PtkFileBrowser* file_browser);
bool ptk_location_view_chdir(GtkTreeView* location_view, const char* path);
void ptk_location_view_on_action(GtkWidget* view, xset_t set);
vfs::volume ptk_location_view_get_selected_vol(GtkTreeView* location_view);
void update_volume_icons();
void ptk_location_view_mount_network(PtkFileBrowser* file_browser, const char* url, bool new_tab,
                                     bool force_new_mount);
void ptk_location_view_dev_menu(GtkWidget* parent, PtkFileBrowser* file_browser, GtkWidget* menu);
char* ptk_location_view_create_mount_point(i32 mode, vfs::volume vol, netmount_t* netmount,
                                           const char* path);
char* ptk_location_view_get_mount_point_dir(const char* name);
void ptk_location_view_clean_mount_points();
bool ptk_location_view_open_block(const char* block, bool new_tab);
