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
#include <random>

#include <gtkmm.h>
#include <gdkmm.h>
#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "ptk/utils/ptk-utils.hxx"

void
ptk::utils::set_window_icon(GtkWindow* window)
{
    gtk_window_set_icon_name(GTK_WINDOW(window), "spacefm");
}

u32
ptk::utils::get_keymod(GdkModifierType event)
{
    return (event & (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK |
#if (GTK_MAJOR_VERSION == 4)
                     GdkModifierType::GDK_ALT_MASK |
#elif (GTK_MAJOR_VERSION == 3)
                     GdkModifierType::GDK_MOD1_MASK |
#endif
                     GdkModifierType::GDK_SUPER_MASK | GdkModifierType::GDK_HYPER_MASK |
                     GdkModifierType::GDK_META_MASK));
}

i32
ptk::utils::stamp() noexcept
{
    std::mt19937 rng;
    rng.seed(std::random_device{}());
    std::uniform_int_distribution<i32> dist(0, std::numeric_limits<i32>::max());

    return dist(rng);
}
