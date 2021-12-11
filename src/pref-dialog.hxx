/*
 *
 * License: See COPYING file
 *
 */

#pragma once

#include <gtk/gtk.h>
#include <glib.h>

enum PrefDlgPage
{
    PREF_GENERAL,
    PREF_ADVANCED
};

bool fm_edit_preference(GtkWindow* parent, int page);
