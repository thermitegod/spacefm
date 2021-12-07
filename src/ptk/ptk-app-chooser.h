/*
 *  C Interface: appchooserdlg
 *
 * Description:
 *
 *
 * Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 */

#pragma once

#include <stdbool.h>

#include <gtk/gtk.h>
#include "../vfs/vfs-mime-type.h"

G_BEGIN_DECLS

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

G_END_DECLS
