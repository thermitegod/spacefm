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

#include <gtkmm.h>
#include <gdkmm.h>

#include <ztd/ztd.hxx>

#include "xset/xset.hxx"

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
