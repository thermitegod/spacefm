/*
 *  C Interface: file_properties
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

#include <gtk/gtk.h>

GtkWidget* file_properties_dlg_new(GtkWindow* parent, const char* dir_path, GList* sel_files,
                                   int page);
