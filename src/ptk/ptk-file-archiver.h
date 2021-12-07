/*
 * SpaceFM ptk-file-archiver.h
 *
 * Copyright (C) 2013-2014 OmegaPhil <OmegaPhil@startmail.com>
 * Copyright (C) 2014 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>
 *
 * License: See COPYING file
 *
 */

#pragma once

#include <stdbool.h>

#include <gtk/gtk.h>
#include <glib.h>

#include "../vfs/vfs-mime-type.h"

#include "ptk-file-browser.h"

G_BEGIN_DECLS

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

G_END_DECLS
