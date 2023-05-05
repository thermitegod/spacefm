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

#include "types.hxx"

#include "xset/xset.hxx"

const std::string xset_custom_new_name();

xset_t xset_custom_new();
void xset_custom_delete(xset_t set, bool delete_next);
xset_t xset_custom_remove(xset_t set);
char* xset_custom_get_app_name_icon(xset_t set, GdkPixbuf** icon, i32 icon_size);
void xset_custom_export(GtkWidget* parent, PtkFileBrowser* file_browser, xset_t set);
char* xset_custom_get_script(xset_t set, bool create);
xset_t xset_custom_copy(xset_t set, bool copy_next, bool delete_set);

xset_t xset_find_custom(const std::string_view search);
