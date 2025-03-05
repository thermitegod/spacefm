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

#include <filesystem>
#include <optional>
#include <print>
#include <string_view>

#include <gtkmm.h>

#include <glaze/glaze.hpp>

#include <ztd/ztd.hxx>

#include "xset/xset-dialog.hxx"
#include "xset/xset.hxx"

#include "ptk/utils/ptk-utils.hxx"

#include "vfs/vfs-user-dirs.hxx"

std::optional<std::filesystem::path>
xset_file_dialog(GtkWidget* parent, GtkFileChooserAction action, const std::string_view title,
                 const std::optional<std::filesystem::path>& deffolder,
                 const std::optional<std::filesystem::path>& deffile) noexcept
{
#if (GTK_MAJOR_VERSION == 4)
    (void)parent;
    (void)action;
    (void)title;
    (void)deffolder;
    (void)deffile;
    ptk::dialog::error(nullptr,
                       "Needs Update",
                       "Gtk4 changed and then deprecated the GtkFileChooser API");
    return std::nullopt;
#elif (GTK_MAJOR_VERSION == 3)
    /*  Actions:
     *      GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_OPEN
     *      GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SAVE
     *      GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
     *      GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER
     */

#if (GTK_MAJOR_VERSION == 4)
    GtkWidget* dlgparent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(parent)));
#elif (GTK_MAJOR_VERSION == 3)
    GtkWidget* dlgparent = gtk_widget_get_toplevel(GTK_WIDGET(parent));
#endif

    GtkWidget* dlg = gtk_file_chooser_dialog_new(title.data(),
                                                 dlgparent ? GTK_WINDOW(dlgparent) : nullptr,
                                                 action,
                                                 "Cancel",
                                                 GtkResponseType::GTK_RESPONSE_CANCEL,
                                                 "OK",
                                                 GtkResponseType::GTK_RESPONSE_OK,
                                                 nullptr);
    // gtk_file_chooser_set_action(GTK_FILE_CHOOSER(dlg),
    // GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SAVE);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dlg), true);
    ptk::utils::set_window_icon(GTK_WINDOW(dlg));

    if (deffolder)
    {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), deffolder->c_str());
    }
    else
    {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), vfs::user::home().c_str());
    }
    if (deffile)
    {
        if (action == GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SAVE ||
            action == GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
        {
            gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), deffile->c_str());
        }
        else
        {
            const auto path2 = deffolder.value() / deffile.value();
            gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dlg), path2.c_str());
        }
    }

    const auto width = xset_get_int(xset::name::file_dlg, xset::var::x);
    const auto height = xset_get_int(xset::name::file_dlg, xset::var::y);
    if (width && height)
    {
        // filechooser will not honor default size or size request ?
        gtk_widget_show_all(GTK_WIDGET(dlg));
#if (GTK_MAJOR_VERSION == 3)
        gtk_window_set_position(GTK_WINDOW(dlg), GtkWindowPosition::GTK_WIN_POS_CENTER_ALWAYS);
#endif
        gtk_window_set_default_size(GTK_WINDOW(dlg), width, height);
        while (g_main_context_pending(nullptr))
        {
            g_main_context_iteration(nullptr, true);
        }
#if (GTK_MAJOR_VERSION == 3)
        gtk_window_set_position(GTK_WINDOW(dlg), GtkWindowPosition::GTK_WIN_POS_CENTER);
#endif
    }

    const auto response = gtk_dialog_run(GTK_DIALOG(dlg));

    // Saving dialog dimensions
    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(dlg), &allocation);
    xset_set(xset::name::file_dlg, xset::var::x, std::format("{}", allocation.width));
    xset_set(xset::name::file_dlg, xset::var::y, std::format("{}", allocation.height));

    if (response == GtkResponseType::GTK_RESPONSE_OK)
    {
        const char* dest = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        gtk_widget_destroy(dlg);
        if (dest != nullptr)
        {
            return dest;
        }
        return std::nullopt;
    }
    gtk_widget_destroy(dlg);
    return std::nullopt;
#endif
}
