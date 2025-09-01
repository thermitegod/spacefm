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

#include <glibmm.h>
#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "icon.hxx"

Gtk::Image
vfs::utils::load_icon(const std::string_view icon_name, const std::int32_t icon_size) noexcept
{
    Gtk::Image icon;

    const auto theme = Gtk::IconTheme::create();
    if (theme)
    {
        const auto theme_icon = theme->lookup_icon(icon_name.data(), icon_size);

        if (theme_icon)
        {
            icon = Gtk::Image(theme_icon);
        }
        else
        {
            // logger::error<logger::vfs>("Failed to load the '{}' icon from theme '{}'", icon_name, theme->property_theme_name().get_name());
        }
    }
    else
    {
        // logger::error<logger::vfs>("Failed to load a theme");
    }

    return icon;
}
