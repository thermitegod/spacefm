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

#include <cassert>

#include <gtkmm.h>
#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-utils.hxx"

void
ptk_set_window_icon(GtkWindow* window)
{
    assert(GTK_IS_WINDOW(window));

    GdkPixbuf* icon = vfs_load_icon("spacefm", 48);
    if (icon)
    {
        gtk_window_set_icon(GTK_WINDOW(window), icon);
        g_object_unref(icon);
    }
}
