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

#include <string>
#include <string_view>

#include <format>

#include <filesystem>

#include <vector>

#include <optional>

#include <memory>

#include <gtkmm.h>
#include <glibmm.h>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-app-desktop.hxx"

#include "ptk/ptk-dialog.hxx"

#include "xset/xset.hxx"
#include "xset/xset-misc.hxx"

GtkWidget*
xset_get_image(const std::string_view icon, GtkIconSize icon_size)
{
    /*
        GtkIconSize::GTK_ICON_SIZE_MENU,
        GtkIconSize::GTK_ICON_SIZE_SMALL_TOOLBAR,
        GtkIconSize::GTK_ICON_SIZE_LARGE_TOOLBAR,
        GtkIconSize::GTK_ICON_SIZE_BUTTON,
        GtkIconSize::GTK_ICON_SIZE_DND,
        GtkIconSize::GTK_ICON_SIZE_DIALOG
    */

    if (icon.empty())
    {
        return nullptr;
    }

    if (!icon_size)
    {
        icon_size = GtkIconSize::GTK_ICON_SIZE_MENU;
    }

    return gtk_image_new_from_icon_name(icon.data(), icon_size);
}

void
xset_edit(GtkWidget* parent, const std::filesystem::path& path)
{
    GtkWidget* dlgparent = nullptr;
    if (parent)
    {
#if (GTK_MAJOR_VERSION == 4)
        dlgparent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(parent)));
#elif (GTK_MAJOR_VERSION == 3)
        dlgparent = gtk_widget_get_toplevel(GTK_WIDGET(parent));
#endif
    }

    const auto check_editor = xset_get_s(xset::name::editor);
    if (!check_editor)
    {
        ptk::dialog::error(dlgparent ? GTK_WINDOW(dlgparent) : nullptr,
                           "Editor Not Set",
                           "Please set your editor in View|Preferences|Advanced");
        return;
    }
    const auto& editor = check_editor.value();

    std::shared_ptr<vfs::desktop> desktop;
    if (editor.ends_with(".desktop"))
    {
        desktop = vfs::desktop::create(editor);
    }
    else
    { // this might work
        ztd::logger::warn("Editor is not set to a .desktop file");
        desktop = vfs::desktop::create(std::format("{}.desktop", editor));
    }

    const std::vector<std::filesystem::path> open_files{path};

    const bool opened = desktop->open_files(path.parent_path(), open_files);
    if (!opened)
    {
        ptk::dialog::error(
            nullptr,
            "Error",
            std::format("Unable to use '{}' to open file:\n{}", editor, path.string()));
    }
}
