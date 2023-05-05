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

#include <gtk/gtk.h>

#include "vfs/vfs-volume.hxx"

void ptk_bookmark_view_add_bookmark(const std::string_view book_path);
void ptk_bookmark_view_add_bookmark(PtkFileBrowser* file_browser);

void ptk_bookmark_view_add_bookmark_cb(GtkMenuItem* menuitem, PtkFileBrowser* file_browser);
