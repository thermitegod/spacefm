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

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "xset/xset.hxx"

#include "xset/xset-event-handler.hxx"

xset_event_handler_t event_handler;

XSetEventHandler::XSetEventHandler()
{
    // ztd::logger::info("XSetEventHandler constructor");

    this->win_focus = xset_get(xset::name::evt_win_focus);
    this->win_move = xset_get(xset::name::evt_win_move);
    this->win_click = xset_get(xset::name::evt_win_click);
    this->win_key = xset_get(xset::name::evt_win_key);
    this->win_close = xset_get(xset::name::evt_win_close);
    this->pnl_show = xset_get(xset::name::evt_pnl_show);
    this->pnl_focus = xset_get(xset::name::evt_pnl_focus);
    this->pnl_sel = xset_get(xset::name::evt_pnl_sel);
    this->tab_new = xset_get(xset::name::evt_tab_new);
    this->tab_chdir = xset_get(xset::name::evt_tab_chdir);
    this->tab_focus = xset_get(xset::name::evt_tab_focus);
    this->tab_close = xset_get(xset::name::evt_tab_close);
    this->device = xset_get(xset::name::evt_device);
}
