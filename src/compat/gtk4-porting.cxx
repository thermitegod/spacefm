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

#include <cmath>

#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "compat/gtk4-porting.hxx"

#if (GTK_MAJOR_VERSION == 3)

guint
gdk_key_event_get_keyval(GdkEvent* event) noexcept
{
    guint keyval = 0;
    gdk_event_get_keyval(event, &keyval);
    return keyval;
}

GdkModifierType
gdk_event_get_modifier_state(GdkEvent* event) noexcept
{
    GdkModifierType state;
    gdk_event_get_state(event, &state);
    return state;
}

guint
gdk_button_event_get_button(GdkEvent* event) noexcept
{
    guint button = 0;
    gdk_event_get_button(event, &button);
    return button;
}

gboolean
gdk_event_get_position(GdkEvent* event, double* x, double* y) noexcept
{
    double xx = NAN;
    double yy = NAN;
    const auto ret = gdk_event_get_coords(event, &xx, &yy);
    *x = xx;
    *y = yy;
    return ret;
}

#endif
