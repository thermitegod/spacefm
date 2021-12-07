/*
 *
 * License: See COPYING file
 *
 */

#include <gtk/gtk.h>

#include "spacefm.hxx"

typedef struct WindowRef
{
    size_t n_pcmanfm_ref;
    bool daemon_mode;
} WindowRef;

WindowRef window_ref_counter;

void init_window_ref_counter(bool daemon_mode)
{
    window_ref_counter.n_pcmanfm_ref = 0;
    window_ref_counter.daemon_mode = daemon_mode;
}

/* After opening any window/dialog/tool, this should be called. */
void pcmanfm_ref()
{
    ++window_ref_counter.n_pcmanfm_ref;
}

/* After closing any window/dialog/tool, this should be called.
 * If the last window is closed and we are not a deamon, pcmanfm will quit.
 */
void pcmanfm_unref()
{
    --window_ref_counter.n_pcmanfm_ref;
    if (window_ref_counter.n_pcmanfm_ref == 0 && !window_ref_counter.daemon_mode)
        gtk_main_quit();
}
