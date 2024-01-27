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
#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "ptk/utils/ptk-utils.hxx"

void
ptk::utils::set_window_icon(GtkWindow* window)
{
    gtk_window_set_icon_name(GTK_WINDOW(window), "spacefm");
}

i32
ptk::utils::stamp() noexcept
{
    std::mt19937 rng;
    rng.seed(std::random_device{}());
    std::uniform_int_distribution<i32> dist(0, std::numeric_limits<i32>::max());

    return dist(rng);
}
