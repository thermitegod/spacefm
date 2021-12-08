/*
 * SpaceFM ptk-file-archiver.hxx
 *
 * Copyright (C) 2013-2014 OmegaPhil <OmegaPhil@startmail.com>
 * Copyright (C) 2014 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>
 *
 * License: See COPYING file
 *
 */

#pragma once

#include <gtk/gtk.h>
#include <glib.h>

#include "ptk-file-browser.hxx"

// Archive operations enum
enum
{
    ARC_COMPRESS,
    ARC_EXTRACT,
    ARC_LIST
};

// Pass file_browser or desktop depending on where you're calling from
void ptk_file_archiver_create(PtkFileBrowser* file_browser, GList* files, const char* cwd);
void ptk_file_archiver_extract(PtkFileBrowser* file_browser, GList* files, const char* cwd,
                               const char* dest_dir, int job, bool archive_presence_checked);
