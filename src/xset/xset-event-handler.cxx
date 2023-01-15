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

    this->win_focus = xset_get(XSetName::EVT_WIN_FOCUS);
    this->win_move = xset_get(XSetName::EVT_WIN_MOVE);
    this->win_click = xset_get(XSetName::EVT_WIN_CLICK);
    this->win_key = xset_get(XSetName::EVT_WIN_KEY);
    this->win_close = xset_get(XSetName::EVT_WIN_CLOSE);
    this->pnl_show = xset_get(XSetName::EVT_PNL_SHOW);
    this->pnl_focus = xset_get(XSetName::EVT_PNL_FOCUS);
    this->pnl_sel = xset_get(XSetName::EVT_PNL_SEL);
    this->tab_new = xset_get(XSetName::EVT_TAB_NEW);
    this->tab_chdir = xset_get(XSetName::EVT_TAB_CHDIR);
    this->tab_focus = xset_get(XSetName::EVT_TAB_FOCUS);
    this->tab_close = xset_get(XSetName::EVT_TAB_CLOSE);
    this->device = xset_get(XSetName::EVT_DEVICE);
}
