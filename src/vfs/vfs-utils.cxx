/*
 *      vfs-utils.c
 *
 *      Copyright 2008 PCMan <pcman.tw@gmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 3 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#include "vfs-utils.hxx"

GdkPixbuf* vfs_load_icon(GtkIconTheme* theme, const char* icon_name, int size)
{
    if (!icon_name)
        return nullptr;

    GtkIconInfo* inf = gtk_icon_theme_lookup_icon(
        theme,
        icon_name,
        size,
        GtkIconLookupFlags(GTK_ICON_LOOKUP_USE_BUILTIN | GTK_ICON_LOOKUP_FORCE_SIZE));

    if (!inf && icon_name[0] == '/')
        return gdk_pixbuf_new_from_file_at_size(icon_name, size, size, nullptr);

    if (G_UNLIKELY(!inf))
        return nullptr;

    const char* file = gtk_icon_info_get_filename(inf);
    GdkPixbuf* icon = nullptr;
    if (G_LIKELY(file))
        icon = gdk_pixbuf_new_from_file_at_size(file, size, size, nullptr);
    else
    {
        icon = gtk_icon_info_get_builtin_pixbuf(inf);
        g_object_ref(icon);
    }
    g_object_unref(inf);

    return icon;
}
