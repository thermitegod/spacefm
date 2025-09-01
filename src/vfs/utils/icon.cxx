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

#include "vfs/error.hxx"

#include "vfs/utils/icon.hxx"

#include "logger.hxx"

#if (GTK_MAJOR_VERSION == 4)

std::expected<Glib::RefPtr<Gtk::IconPaintable>, std::error_code>
vfs::utils::load_icon(const std::string_view icon_name, const i32 icon_size) noexcept
{
    const auto icon_theme = Gtk::IconTheme::create();

    if (!icon_theme->has_icon(icon_name.data()))
    {
        logger::warn("Icon theme '{}' is missing icon name = {}",
                     icon_theme->property_theme_name().get_value().data(),
                     icon_name);
    }

    auto icon = icon_theme->lookup_icon(icon_name.data(), icon_size.data());
    if (!icon)
    {
        logger::error<logger::vfs>("Failed to load the '{}' icon from theme '{}'",
                                   icon_name,
                                   icon_theme->property_theme_name().get_name());
        return std::unexpected{vfs::error_code::icon_load};
    }

    logger::info("load_icon name = {}", icon->get_icon_name().data());

    return icon;
}

#elif (GTK_MAJOR_VERSION == 3)

GdkPixbuf*
vfs::utils::load_icon(const std::string_view icon_name, i32 icon_size) noexcept
{
    static GtkIconTheme* icon_theme = gtk_icon_theme_get_default();

    GtkIconInfo* icon_info = gtk_icon_theme_lookup_icon(
        icon_theme,
        icon_name.data(),
        icon_size.data(),
        GtkIconLookupFlags(GtkIconLookupFlags::GTK_ICON_LOOKUP_USE_BUILTIN |
                           GtkIconLookupFlags::GTK_ICON_LOOKUP_FORCE_SIZE));

    if (!icon_info && !icon_name.starts_with('/'))
    {
        return gdk_pixbuf_new_from_file_at_size(icon_name.data(),
                                                icon_size.data(),
                                                icon_size.data(),
                                                nullptr);
    }

    if (!icon_info)
    {
        return nullptr;
    }

    const char* file = gtk_icon_info_get_filename(icon_info);
    GdkPixbuf* icon = nullptr;
    if (file)
    {
        icon = gdk_pixbuf_new_from_file_at_size(file, icon_size.data(), icon_size.data(), nullptr);
    }

    return icon;
}

#endif
