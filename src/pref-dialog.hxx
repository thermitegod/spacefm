/*
 *
 * License: See COPYING file
 *
 */

#pragma once

#include <gtk/gtk.h>
#include <glib.h>

typedef enum PrefDlgPage
{
    PREF_GENERAL,
    PREF_ADVANCED
} PrefDlgPage;

bool fm_edit_preference(GtkWindow* parent, int page);
