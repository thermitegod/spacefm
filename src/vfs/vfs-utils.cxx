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

#include "settings.hxx"

#include "vfs/vfs-utils.hxx"

GdkPixbuf*
vfs_load_icon(const char* icon_name, i32 size)
{
    if (!icon_name)
        return nullptr;

    GtkIconInfo* inf = nullptr;
    GtkIconTheme* icon_theme = gtk_icon_theme_get_default();

    inf = gtk_icon_theme_lookup_icon(
        icon_theme,
        icon_name,
        size,
        GtkIconLookupFlags(GtkIconLookupFlags::GTK_ICON_LOOKUP_USE_BUILTIN |
                           GtkIconLookupFlags::GTK_ICON_LOOKUP_FORCE_SIZE));

    if (!inf && icon_name[0] == '/')
        return gdk_pixbuf_new_from_file_at_size(icon_name, size, size, nullptr);

    if (!inf)
        return nullptr;

    const char* file = gtk_icon_info_get_filename(inf);
    GdkPixbuf* icon = nullptr;
    if (file)
        icon = gdk_pixbuf_new_from_file_at_size(file, size, size, nullptr);
    else
    {
        icon = gtk_icon_info_get_builtin_pixbuf(inf);
        g_object_ref(icon);
    }
    // if (inf)
    //     g_object_unref(inf);
    return icon;
}

const std::string
vfs_file_size_to_string_format(u64 size_in_bytes, bool decimal)
{
    if (app_settings.get_use_si_prefix())
    {
        ztd::FileSizeSI filesize(size_in_bytes);
        return filesize.get_formated_size(decimal ? 1 : 0);
    }
    else
    {
        ztd::FileSize filesize(size_in_bytes);
        return filesize.get_formated_size(decimal ? 1 : 0);
    }
}
