/*
 *
 * License: See COPYING file
 *
 */

#include <stdbool.h>
#include <string.h>
//#include <stdio.h>

#include <gtk/gtk.h>

#include "autosave.h"

#include "settings.h"

static bool on_autosave_timer(void* main_window);

typedef struct AutoSave
{
    unsigned int timer;
    bool request;
} AutoSave;

AutoSave autosave = {0};

static void idle_save_settings(void* main_window)
{
    // printf("AUTOSAVE *** idle_save_settings\n" );
    save_settings(NULL);
}

static void autosave_start(bool delay)
{
    // printf("AUTOSAVE autosave_start\n" );
    if (!delay)
    {
        g_idle_add((GSourceFunc)idle_save_settings, NULL);
        autosave.request = FALSE;
    }
    else
        autosave.request = TRUE;
    if (!autosave.timer)
    {
        autosave.timer = g_timeout_add_seconds(10, (GSourceFunc)on_autosave_timer, NULL);
        // printf("AUTOSAVE timer started\n" );
    }
}

static bool on_autosave_timer(void* main_window)
{
    // printf("AUTOSAVE timeout\n" );
    if (autosave.timer)
    {
        g_source_remove(autosave.timer);
        autosave.timer = 0;
    }
    if (autosave.request)
        autosave_start(FALSE);
    return FALSE;
}

void xset_autosave(bool force, bool delay)
{
    if (autosave.timer && !force)
    {
        // autosave timer is running, so request save on timeout to prevent
        // saving too frequently, unless force
        autosave.request = TRUE;
        // printf("AUTOSAVE request\n" );
    }
    else
    {
        if (autosave.timer && force)
        {
            g_source_remove(autosave.timer);
            autosave.timer = 0;
        }

        /*
        if ( force )
            printf("AUTOSAVE force\n" );
        else if ( delay )
            printf("AUTOSAVE delay\n" );
        else
            printf("AUTOSAVE normal\n" );
        */

        autosave_start(!force && delay);
    }
}

void xset_autosave_cancel()
{
    // printf("AUTOSAVE cancel\n" );
    autosave.request = FALSE;
    if (autosave.timer)
    {
        g_source_remove(autosave.timer);
        autosave.timer = 0;
    }
}
