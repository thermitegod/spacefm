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

static const char* terminal_programs[] = // for pref-dialog.c
    {"terminal",
     "xfce4-terminal",
     "aterm",
     "Eterm",
     "mlterm",
     "mrxvt",
     "rxvt",
     "sakura",
     "terminator",
     "urxvt",
     "xterm",
     "x-terminal-emulator",
     "qterminal"};

// clang-format off
static const char* su_commands[] = // order and contents must match prefdlg.ui
    {"/bin/su",
     "/usr/bin/sudo",
     "/usr/bin/doas"};
// clang-format on
