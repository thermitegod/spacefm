/**
 * Copyright 2008 PCMan <pcman.tw@gmail.com>
 *
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

#include <string>
#include <string_view>

#include <fmt/format.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "settings/app.hxx"

#include "vfs/vfs-utils.hxx"

GdkPixbuf*
vfs_load_icon(std::string_view icon_name, i32 icon_size)
{
    GtkIconTheme* icon_theme = gtk_icon_theme_get_default();

    GtkIconInfo* icon_info = gtk_icon_theme_lookup_icon(
        icon_theme,
        icon_name.data(),
        icon_size,
        GtkIconLookupFlags(GtkIconLookupFlags::GTK_ICON_LOOKUP_USE_BUILTIN |
                           GtkIconLookupFlags::GTK_ICON_LOOKUP_FORCE_SIZE));

    if (!icon_info && !ztd::startswith(icon_name, "/"))
    {
        return gdk_pixbuf_new_from_file_at_size(icon_name.data(), icon_size, icon_size, nullptr);
    }

    if (!icon_info)
    {
        return nullptr;
    }

    const char* file = gtk_icon_info_get_filename(icon_info);
    GdkPixbuf* icon = nullptr;
    if (file)
    {
        icon = gdk_pixbuf_new_from_file_at_size(file, icon_size, icon_size, nullptr);
    }

    return icon;
}

const std::string
vfs_file_size_format(u64 size_in_bytes, bool decimal)
{
    if (app_settings.get_use_si_prefix())
    {
        return ztd::format_filesize(size_in_bytes, ztd::format_base::SI, decimal ? 1 : 0);
    }
    else
    {
        return ztd::format_filesize(size_in_bytes, ztd::format_base::IEC, decimal ? 1 : 0);
    }
}
