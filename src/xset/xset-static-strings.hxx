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

#include <string_view>

inline constexpr std::string_view enter_command_line =
    "Enter program or bash command line:\n\nUse:\n\t%%F\tselected files  or  %%f first selected "
    "file\n\t%%N\tselected filenames  or  %%n first selected filename\n\t%%d\tcurrent "
    "directory\n\t%%v\tselected device (eg /dev/sda1)\n\t%%m\tdevice mount point (eg "
    "/media/dvd);  %%l device label\n\t%%b\tselected bookmark\n\t%%t\tselected task directory;  "
    "%%p task pid\n\t%%a\tmenu item value\n\t$fm_panel, $fm_tab, etc";

inline constexpr std::string_view icon_desc =
    "Enter an icon name, icon file path, or stock item name:\n\nOr click Choose to select an "
    "icon.  Not all icons may work properly due to various issues.";

inline constexpr std::string_view enter_menu_name_new =
    "Enter new item name:\n\nPrecede a character with an underscore (_) to underline that "
    "character as a shortcut key if desired.\n\nTIP: To change this item later, right-click on "
    "the item to open the Design Menu.";
