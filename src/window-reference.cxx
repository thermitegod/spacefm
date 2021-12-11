/*
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 */

#include <gtk/gtk.h>

#include "window-reference.hxx"

WindowRef* window_ref = window_ref->getInstance();

WindowRef* WindowRef::s_instance = 0;

void
WindowRef::ref_inc()
{
    this->ref_count = ref_count + 1;
};
void
WindowRef::ref_dec()
{
    this->ref_count = ref_count - 1;
    if (this->ref_count == 0 && !this->daemon_mode)
        gtk_main_quit();
};

void
WindowRef::set_daemon_mode(bool is_daemon)
{
    this->daemon_mode = is_daemon;
};

namespace WindowReference
{
    void
    increase()
    {
        window_ref->ref_inc();
    };
    void
    decrease()
    {
        window_ref->ref_dec();
    };

    void
    set_daemon(bool is_daemon)
    {
        window_ref->set_daemon_mode(is_daemon);
    }

} // namespace WindowReference
