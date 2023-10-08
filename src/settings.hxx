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

#include <gtkmm.h>
#include <gdkmm.h>

#include <ztd/ztd.hxx>

#include "xset/xset.hxx"

void load_settings();
void save_settings();
void free_settings();

///////////////////////////////////////////////////////////////////////////////
// MOD extra settings below

GtkWidget* xset_design_show_menu(GtkWidget* menu, const xset_t& set, const xset_t& book_insert,
                                 u32 button, std::time_t time);

GtkWidget* xset_get_image(const std::string_view icon, GtkIconSize icon_size);

#if (GTK_MAJOR_VERSION == 4)
void xset_add_menu(PtkFileBrowser* file_browser, GtkWidget* menu, GtkEventController* accel_group,
                   const std::vector<xset::name>& submenu_entries);
#elif (GTK_MAJOR_VERSION == 3)
void xset_add_menu(PtkFileBrowser* file_browser, GtkWidget* menu, GtkAccelGroup* accel_group,
                   const std::vector<xset::name>& submenu_entries);
#endif

#if (GTK_MAJOR_VERSION == 4)
GtkWidget* xset_add_menuitem(PtkFileBrowser* file_browser, GtkWidget* menu,
                             GtkEventController* accel_group, const xset_t& set);
#elif (GTK_MAJOR_VERSION == 3)
GtkWidget* xset_add_menuitem(PtkFileBrowser* file_browser, GtkWidget* menu,
                             GtkAccelGroup* accel_group, const xset_t& set);
#endif

void xset_menu_cb(GtkWidget* item, const xset_t& set);

void xset_edit(GtkWidget* parent, const std::filesystem::path& path);
void xset_fill_toolbar(GtkWidget* parent, PtkFileBrowser* file_browser, GtkToolbar* toolbar,
                       const xset_t& set_parent, bool show_tooltips);

const std::string xset_get_builtin_toolitem_label(xset::tool tool_type);

////////////////////////////////////////

void xset_custom_insert_after(const xset_t& target, const xset_t& set);
const xset_t xset_new_builtin_toolitem(xset::tool tool_type);
