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

#include <expected>
#include <string_view>
#include <system_error>

#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "vfs/utils/icon.hxx"

#include "logger.hxx"

Glib::RefPtr<Gtk::IconPaintable>
vfs::utils::load_icon(const std::string_view icon_name, const i32 icon_size,
                      const std::string_view fallback) noexcept
{
    auto display = Gdk::Display::get_default();
    if (!display)
    {
        return nullptr;
    }

    const auto icon_theme = Gtk::IconTheme::get_for_display(display);

    if (!icon_theme->has_icon(icon_name.data()))
    {
        logger::warn("Icon theme '{}' is missing icon name = {}",
                     icon_theme->property_theme_name().get_value().data(),
                     icon_name);
        return icon_theme->lookup_icon(fallback.data(), icon_size.data());
    }

    return icon_theme->lookup_icon(icon_name.data(), icon_size.data());
}
