/**
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "window-reference.hxx"

WindowRef* window_ref = window_ref->getInstance();

WindowRef* WindowRef::s_instance = nullptr;

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
