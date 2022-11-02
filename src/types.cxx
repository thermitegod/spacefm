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

#include "types.hxx"

bool
valid_panel(panel_t p)
{
    return (p != INVALID_PANEL || p >= MIN_PANELS || p <= MAX_PANELS);
}

bool
valid_panel_code(panel_t p)
{
    return (p == panel_control_code_prev || p == panel_control_code_next ||
            p == panel_control_code_hide);
}

bool
valid_tab(tab_t t)
{
    return (t != INVALID_TAB || t >= MIN_TABS);
}

bool
valid_tab_code(tab_t t)
{
    return (t == tab_control_code_prev || t == tab_control_code_next ||
            t == tab_control_code_close || t == tab_control_code_restore);
}
