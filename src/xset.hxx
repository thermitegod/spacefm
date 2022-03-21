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

#ifndef INCLUDE_XSET_ENUMS
#error "Do not need to include <xset.hxx> directly, instead include <settings.hxx>"
#endif

#pragma once

// seperate header for XSet enums to avoid
// circular header dependencies with ptk-file-browser.hxx

enum class XSetSetSet
{
    S,
    B,
    X,
    Y,
    Z,
    KEY,
    KEYMOD,
    STYLE,
    DESC,
    TITLE,
    MENU_LABEL,
    ICN,
    MENU_LABEL_CUSTOM,
    ICON,
    SHARED_KEY,
    NEXT,
    PREV,
    PARENT,
    CHILD,
    CONTEXT,
    LINE,
    TOOL,
    TASK,
    TASK_POP,
    TASK_ERR,
    TASK_OUT,
    RUN_IN_TERMINAL,
    KEEP_TERMINAL,
    SCROLL_LOCK,
    DISABLE,
    OPENER
};

enum class XSetCMD
{
    LINE,
    SCRIPT,
    APP,
    BOOKMARK,
    INVALID // Must be last
};

enum class XSetMenu
{ // do not reorder - these values are saved in session files
    NORMAL,
    CHECK,
    STRING,
    RADIO,
    FILEDLG,
    FONTDLG,
    ICON,
    COLORDLG,
    CONFIRM,
    RESERVED_03,
    RESERVED_04,
    RESERVED_05,
    RESERVED_06,
    RESERVED_07,
    RESERVED_08,
    RESERVED_09,
    RESERVED_10,
    SUBMENU, // add new before submenu
    SEP
};

enum class XSetTool
{ // do not reorder - these values are saved in session files
  // also update builtin_tool_name builtin_tool_icon in settings.c
    NOT,
    CUSTOM,
    DEVICES,
    BOOKMARKS,
    TREE,
    HOME,
    DEFAULT,
    UP,
    BACK,
    BACK_MENU,
    FWD,
    FWD_MENU,
    REFRESH,
    NEW_TAB,
    NEW_TAB_HERE,
    SHOW_HIDDEN,
    SHOW_THUMB,
    LARGE_ICONS,
    INVALID // Must be last
};

enum class XSetJob
{
    KEY,
    ICON,
    LABEL,
    EDIT,
    EDIT_ROOT,
    LINE,
    SCRIPT,
    CUSTOM,
    TERM,
    KEEP,
    USER,
    TASK,
    POP,
    ERR,
    OUT,
    BOOKMARK,
    APP,
    COMMAND,
    SUBMENU,
    SUBMENU_BOOK,
    SEP,
    ADD_TOOL,
    IMPORT_FILE,
    IMPORT_GTK,
    CUT,
    COPY,
    PASTE,
    REMOVE,
    REMOVE_BOOK,
    NORMAL,
    CHECK,
    CONFIRM,
    DIALOG,
    MESSAGE,
    COPYNAME,
    PROP,
    PROP_CMD,
    IGNORE_CONTEXT,
    SCROLL,
    EXPORT,
    BROWSE_FILES,
    BROWSE_DATA,
    BROWSE_PLUGIN,
    HELP,
    HELP_NEW,
    HELP_ADD,
    HELP_BROWSE,
    HELP_STYLE,
    HELP_BOOK,
    TOOLTIPS,
    INVALID // Must be last
};
