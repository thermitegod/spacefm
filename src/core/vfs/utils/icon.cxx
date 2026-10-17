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
vfs::utils::load_icon(std::string_view icon, const i32 size, std::string_view fallback) noexcept
{
    auto display = Gdk::Display::get_default();
    if (!display)
    {
        return nullptr;
    }

    const auto theme = Gtk::IconTheme::get_for_display(display);

    if (!theme->has_icon(icon.data()))
    {
        logger::warn("Icon theme '{}' is missing icon: {}",
                     theme->property_theme_name().get_value().data(),
                     icon);
        return theme->lookup_icon(fallback.data(), size.data());
    }

    return theme->lookup_icon(icon.data(), size.data());
}

Glib::RefPtr<Gtk::IconPaintable>
vfs::utils::load_icon(const Glib::RefPtr<Gio::Icon>& icon, const i32 size) noexcept
{
    auto display = Gdk::Display::get_default();
    if (!display)
    {
        return nullptr;
    }

    const auto theme = Gtk::IconTheme::get_for_display(display);

    return theme->lookup_icon(icon,
                              size.data(),
                              1,
                              Gtk::TextDirection::NONE,
                              Gtk::IconLookupFlags::NONE);
}
