/*
 *
 * License: See COPYING file
 *
 */

#include "settings.hxx"

#include "autosave.hxx"

static bool on_autosave_timer(void* main_window);

struct AutoSave
{
    size_t timer;
    bool request;
};

AutoSave autosave = AutoSave();

static void
idle_save_settings(void* main_window)
{
    // printf("AUTOSAVE *** idle_save_settings\n" );
    save_settings(nullptr);
}

static void
autosave_start(bool delay)
{
    // printf("AUTOSAVE autosave_start\n" );
    if (!delay)
    {
        g_idle_add((GSourceFunc)idle_save_settings, nullptr);
        autosave.request = false;
    }
    else
        autosave.request = true;
    if (!autosave.timer)
    {
        autosave.timer = g_timeout_add_seconds(10, (GSourceFunc)on_autosave_timer, nullptr);
        // printf("AUTOSAVE timer started\n" );
    }
}

static bool
on_autosave_timer(void* main_window)
{
    // printf("AUTOSAVE timeout\n" );
    if (autosave.timer)
    {
        g_source_remove(autosave.timer);
        autosave.timer = 0;
    }
    if (autosave.request)
        autosave_start(false);
    return false;
}

void
xset_autosave(bool force, bool delay)
{
    if (autosave.timer && !force)
    {
        // autosave timer is running, so request save on timeout to prevent
        // saving too frequently, unless force
        autosave.request = true;
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

void
xset_autosave_cancel()
{
    // printf("AUTOSAVE cancel\n" );
    autosave.request = false;
    if (autosave.timer)
    {
        g_source_remove(autosave.timer);
        autosave.timer = 0;
    }
}
