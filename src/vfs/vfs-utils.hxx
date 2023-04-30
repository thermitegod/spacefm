/**
 * Copyright 2008 PCMan <pcman.tw@gmail.com>
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

#include <ztd/ztd.hxx>

GdkPixbuf* vfs_load_icon(std::string_view icon_name, i32 icon_size);

const std::string vfs_file_size_format(u64 size_in_bytes, bool decimal = true);

const std::string vfs_get_unique_name(std::string_view dest_dir, std::string_view base_name,
                                      std::string_view ext);
