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

#include <ztd/ztd.hxx>

#define GDK_ACTION_ALL GdkDragAction(GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK)

namespace gui::utils
{
void set_window_icon(GtkWindow* win) noexcept;

u32 get_keymod(GdkModifierType event) noexcept;

/**
 *  @brief stamp
 *
 *  - Use std::mt19937 to get a random i32 between (0, INT_MAX)
 *
 * @return a random i32
 */
[[nodiscard]] i32 stamp() noexcept;
} // namespace gui::utils
