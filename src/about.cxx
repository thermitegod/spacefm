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

#include <gtk/gtk.h>

#include <ztd/ztd.hxx>

#include "about.hxx"

void
show_about_dialog(GtkWindow* parent)
{
    GtkWidget* dialog = gtk_about_dialog_new();
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));

    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), PACKAGE_NAME_FANCY);
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), PACKAGE_VERSION);
    gtk_about_dialog_set_logo_icon_name(GTK_ABOUT_DIALOG(dialog), "spacefm");
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), "(unofficial)");
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog),
                                 "https://github.com/thermitegod/spacefm");
    gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(dialog), "Github");
    gtk_about_dialog_set_license_type(GTK_ABOUT_DIALOG(dialog), GtkLicense::GTK_LICENSE_GPL_3_0);

    g_signal_connect(dialog, "response", G_CALLBACK(g_object_unref), nullptr);

    gtk_widget_show(dialog);
}
