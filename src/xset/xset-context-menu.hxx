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

#include <gdkmm.h>
#include <gtkmm.h>

#include "xset/xset.hxx"

void xset_add_menu(gui::browser* browser, GtkWidget* menu, GtkAccelGroup* accel_group,
                   const std::vector<xset::name>& submenu_entries) noexcept;

GtkWidget* xset_add_menuitem(gui::browser* browser, GtkWidget* menu, GtkAccelGroup* accel_group,
                             const xset_t& set) noexcept;

void xset_menu_cb(GtkWidget* item, const xset_t& set) noexcept;
