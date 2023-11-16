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

#include <string_view>

#include <gtkmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "compat/gtk4-porting.hxx"

#include "ptk/ptk-dialog.hxx"

void
ptk_show_error(GtkWindow* parent, const std::string_view title,
               const std::string_view message) noexcept
{
    GtkWidget* dialog =
        gtk_message_dialog_new(GTK_WINDOW(parent),
                               GtkDialogFlags(GtkDialogFlags::GTK_DIALOG_MODAL |
                                              GtkDialogFlags::GTK_DIALOG_DESTROY_WITH_PARENT),
                               GtkMessageType::GTK_MESSAGE_ERROR,
                               GtkButtonsType::GTK_BUTTONS_OK,
                               message.data(),
                               nullptr);

    gtk_window_set_title(GTK_WINDOW(dialog), title.empty() ? "Error" : title.data());

    // g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(g_object_unref), nullptr);
    // gtk_widget_show(GTK_WIDGET(dialog));

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

i32
ptk_show_message(GtkWindow* parent, GtkMessageType action, const std::string_view title,
                 GtkButtonsType buttons, const std::string_view message,
                 const std::string_view secondary_message) noexcept
{
    GtkWidget* dialog =
        gtk_message_dialog_new(GTK_WINDOW(parent),
                               GtkDialogFlags(GtkDialogFlags::GTK_DIALOG_MODAL |
                                              GtkDialogFlags::GTK_DIALOG_DESTROY_WITH_PARENT),
                               action,
                               buttons,
                               message.data(),
                               nullptr);

    gtk_window_set_title(GTK_WINDOW(dialog), title.data());

    if (!secondary_message.empty())
    {
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                                 secondary_message.data(),
                                                 nullptr);
    }

    gtk_widget_show_all(GTK_WIDGET(dialog));
    const auto response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    return response;
}
