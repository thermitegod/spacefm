/**
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#pragma once

#include <string>
#include <string_view>

#include <gtk/gtk.h>
#include "vfs/vfs-mime-type.hxx"

/* Let the user choose a application */
char* ptk_choose_app_for_mime_type(GtkWindow* parent, VFSMimeType* mime_type, bool focus_all_apps,
                                   bool show_command, bool show_default, bool dir_default);

/*
 * Return selected application in a ``newly allocated'' string.
 * Returned string is the file name of the *.desktop file or a command line.
 * These two can be separated by check if the returned string is ended
 * with ".desktop".
 */
// char* app_chooser_dialog_get_selected_app(GtkWidget* dlg);

/*
 * Check if the user set the selected app default handler.
 */
// bool app_chooser_dialog_get_set_default(GtkWidget* dlg);
void ptk_app_chooser_has_handler_warn(GtkWidget* parent, VFSMimeType* mime_type);

void on_notebook_switch_page(GtkNotebook* notebook, GtkWidget* page, unsigned int page_num,
                             void* user_data);

void on_browse_btn_clicked(GtkButton* button, void* user_data);
