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

#pragma once

#include <ztd/ztd.hxx>

class WindowRef
{
    static WindowRef* s_instance;

    WindowRef()
    {
    }

  public:
    static WindowRef*
    getInstance()
    {
        if (!s_instance)
            s_instance = new WindowRef;
        return s_instance;
    }

    // After opening any window/dialog/tool, this should be called.
    void ref_inc();

    // After closing any window/dialog/tool, this should be called.
    // If the last window is closed and we are not a deamon, pcmanfm will quit.
    void ref_dec();

    void set_daemon_mode(bool is_daemon);

  private:
    u32 ref_count{0};
    bool daemon_mode{false};
};

namespace WindowReference
{
    void increase();
    void decrease();
    void set_daemon(bool is_daemon);
} // namespace WindowReference
