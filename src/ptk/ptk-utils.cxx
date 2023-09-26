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

#include <gtk/gtk.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

void
ptk_set_window_icon(GtkWindow* window)
{
    assert(GTK_IS_WINDOW(window));

    GtkIconTheme* icon_theme = gtk_icon_theme_get_default();
    if (!icon_theme)
    {
        return;
    }

    GError* error = nullptr;
    GdkPixbuf* icon =
        gtk_icon_theme_load_icon(icon_theme, "spacefm", 48, (GtkIconLookupFlags)0, &error);
    if (icon)
    {
        gtk_window_set_icon(GTK_WINDOW(window), icon);
        g_object_unref(icon);
    }
    else if (!icon || error)
    {
        // An error occured on loading the icon
        ztd::logger::error("Unable to load the window icon 'spacefm' in - ptk_set_window_icon - {}",
                           error->message);
        g_error_free(error);
    }
}
