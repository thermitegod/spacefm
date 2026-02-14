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

#include <limits>

#include <gdkmm.h>
#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "gui/utils/utils.hxx"

void
gui::utils::set_window_icon(GtkWindow* window) noexcept
{
    gtk_window_set_icon_name(GTK_WINDOW(window), "spacefm");
}

u32
gui::utils::get_keymod(GdkModifierType event) noexcept
{
    return (event & (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK |
                     GdkModifierType::GDK_MOD1_MASK | GdkModifierType::GDK_SUPER_MASK |
                     GdkModifierType::GDK_HYPER_MASK | GdkModifierType::GDK_META_MASK));
}

i32
gui::utils::stamp() noexcept
{
    return i32::random(0, std::numeric_limits<i32>::max());
}
