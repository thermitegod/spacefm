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

#include <filesystem>

#include <array>
#include <vector>

#include <memory>

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <ztd/ztd.hxx>

#include "types.hxx"

#include "vfs/vfs-file-task.hxx"

#include "xset/xset.hxx"
#include "xset/xset-lookup.hxx"

// This limits the small icon size for side panes and task list
inline constexpr i32 PANE_MAX_ICON_SIZE = 48;

void load_settings();
void autosave_settings();
void save_settings(void* main_window_ptr);
void free_settings();

///////////////////////////////////////////////////////////////////////////////
// MOD extra settings below

void xset_set_window_icon(GtkWindow* win);

void xset_set_key(GtkWidget* parent, xset_t set);
const std::string xset_get_keyname(xset_t set, i32 key_val, i32 key_mod);

GtkWidget* xset_design_show_menu(GtkWidget* menu, xset_t set, xset_t book_insert, u32 button,
                                 std::time_t time);

GtkWidget* xset_get_image(const std::string_view icon, GtkIconSize icon_size);

void xset_add_menu(PtkFileBrowser* file_browser, GtkWidget* menu, GtkAccelGroup* accel_group,
                   const std::string_view elements);
GtkWidget* xset_add_menuitem(PtkFileBrowser* file_browser, GtkWidget* menu,
                             GtkAccelGroup* accel_group, xset_t set);

void xset_menu_cb(GtkWidget* item, xset_t set);
bool xset_menu_keypress(GtkWidget* widget, GdkEventKey* event, void* user_data);

void xset_edit(GtkWidget* parent, const std::filesystem::path& path, bool force_root, bool no_root);
void xset_fill_toolbar(GtkWidget* parent, PtkFileBrowser* file_browser, GtkWidget* toolbar,
                       xset_t set_parent, bool show_tooltips);

bool xset_opener(PtkFileBrowser* file_browser, char job);
const std::string xset_get_builtin_toolitem_label(xset::tool tool_type);

////////////////////////////////////////

void xset_custom_insert_after(xset_t target, xset_t set);
xset_t xset_new_builtin_toolitem(xset::tool tool_type);
