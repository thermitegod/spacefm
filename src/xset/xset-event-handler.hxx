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

#include "xset/xset.hxx"

// cache these for speed in event handlers
struct EventHandler
{
    xset_t win_focus{nullptr};
    xset_t win_move{nullptr};
    xset_t win_click{nullptr};
    xset_t win_key{nullptr};
    xset_t win_close{nullptr};
    xset_t pnl_show{nullptr};
    xset_t pnl_focus{nullptr};
    xset_t pnl_sel{nullptr};
    xset_t tab_new{nullptr};
    xset_t tab_chdir{nullptr};
    xset_t tab_focus{nullptr};
    xset_t tab_close{nullptr};
    xset_t device{nullptr};
};

extern EventHandler event_handler;