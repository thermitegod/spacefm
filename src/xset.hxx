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

enum XSetCMD
{
    XSET_CMD_LINE,
    XSET_CMD_SCRIPT,
    XSET_CMD_APP,
    XSET_CMD_BOOKMARK
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
    INVALID // keep this always last
};

enum XSetJob
{
    XSET_JOB_KEY,
    XSET_JOB_ICON,
    XSET_JOB_LABEL,
    XSET_JOB_EDIT,
    XSET_JOB_EDIT_ROOT,
    XSET_JOB_LINE,
    XSET_JOB_SCRIPT,
    XSET_JOB_CUSTOM,
    XSET_JOB_TERM,
    XSET_JOB_KEEP,
    XSET_JOB_USER,
    XSET_JOB_TASK,
    XSET_JOB_POP,
    XSET_JOB_ERR,
    XSET_JOB_OUT,
    XSET_JOB_BOOKMARK,
    XSET_JOB_APP,
    XSET_JOB_COMMAND,
    XSET_JOB_SUBMENU,
    XSET_JOB_SUBMENU_BOOK,
    XSET_JOB_SEP,
    XSET_JOB_ADD_TOOL,
    XSET_JOB_IMPORT_FILE,
    XSET_JOB_IMPORT_GTK,
    XSET_JOB_CUT,
    XSET_JOB_COPY,
    XSET_JOB_PASTE,
    XSET_JOB_REMOVE,
    XSET_JOB_REMOVE_BOOK,
    XSET_JOB_NORMAL,
    XSET_JOB_CHECK,
    XSET_JOB_CONFIRM,
    XSET_JOB_DIALOG,
    XSET_JOB_MESSAGE,
    XSET_JOB_COPYNAME,
    XSET_JOB_PROP,
    XSET_JOB_PROP_CMD,
    XSET_JOB_IGNORE_CONTEXT,
    XSET_JOB_SCROLL,
    XSET_JOB_EXPORT,
    XSET_JOB_BROWSE_FILES,
    XSET_JOB_BROWSE_DATA,
    XSET_JOB_BROWSE_PLUGIN,
    XSET_JOB_HELP,
    XSET_JOB_HELP_NEW,
    XSET_JOB_HELP_ADD,
    XSET_JOB_HELP_BROWSE,
    XSET_JOB_HELP_STYLE,
    XSET_JOB_HELP_BOOK,
    XSET_JOB_TOOLTIPS
};
