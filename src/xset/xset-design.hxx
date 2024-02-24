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

#include <chrono>

#include <gtkmm.h>

#include "xset/xset.hxx"

void xset_design_job(GtkWidget* item, const xset_t& set) noexcept;

bool xset_design_cb(GtkWidget* item, GdkEvent* event, const xset_t& set) noexcept;

GtkWidget* xset_design_show_menu(GtkWidget* menu, const xset_t& set, const xset_t& book_insert,
                                 u32 button,
                                 const std::chrono::system_clock::time_point time_point) noexcept;

bool xset_job_is_valid(const xset_t& set, xset::job job) noexcept;
