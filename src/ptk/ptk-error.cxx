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

#include <gtk/gtk.h>

#include <glibmm.h>

// #include "settings.hxx"

#include "ptk/ptk-error.hxx"

void
ptk_show_error(GtkWindow* parent, const std::string_view title, const std::string_view message)
{
    // const Glib::ustring msg = Glib::Markup::escape_text(message.data());
    GtkWidget* dlg = gtk_message_dialog_new(parent,
                                            GtkDialogFlags::GTK_DIALOG_MODAL,
                                            GtkMessageType::GTK_MESSAGE_ERROR,
                                            GtkButtonsType::GTK_BUTTONS_OK,
                                            message.data(),
                                            nullptr);

    gtk_window_set_title(GTK_WINDOW(dlg), title.empty() ? "Error" : title.data());
    // xset_set_window_icon(GTK_WINDOW(dlg));
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}
