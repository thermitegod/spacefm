/*
 *  C Interface: ptk-path-entry
 *
 * Description: A custom entry widget with auto-completion
 *
 *
 * Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 */

#pragma once

#include <gtk/gtk.h>
#include "ptk-file-browser.hxx"

typedef struct EntryData
{
    PtkFileBrowser* browser;
    unsigned int seek_timer;
} EntryData;

GtkWidget* ptk_path_entry_new(PtkFileBrowser* file_browser);
void ptk_path_entry_help(GtkWidget* widget, GtkWidget* parent);
