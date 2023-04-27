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

#include <string>
#include <string_view>

#include <unordered_map>

#include <fmt/format.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "types.hxx"

#include "xset/xset-lookup.hxx"

using namespace std::literals::string_view_literals;

// clang-format off

// All builtin XSet's, custom XSet's are not added to this lookup map
static const std::unordered_map<XSetName, std::string_view> xset_name_map{
    // separator
    {XSetName::SEPARATOR,                   "separator"sv},

    // dev menu
    {XSetName::DEV_MENU_REMOVE,             "dev_menu_remove"sv},
    {XSetName::DEV_MENU_UNMOUNT,            "dev_menu_unmount"sv},
    {XSetName::DEV_MENU_OPEN,               "dev_menu_open"sv},
    {XSetName::DEV_MENU_TAB,                "dev_menu_tab"sv},
    {XSetName::DEV_MENU_MOUNT,              "dev_menu_mount"sv},
    {XSetName::DEV_MENU_MARK,               "dev_menu_mark"sv},
    {XSetName::DEV_MENU_SETTINGS,           "dev_menu_settings"sv},

    // dev settings
    {XSetName::DEV_SHOW,                    "dev_show"sv},
    {XSetName::DEV_SHOW_INTERNAL_DRIVES,    "dev_show_internal_drives"sv},
    {XSetName::DEV_SHOW_EMPTY,              "dev_show_empty"sv},
    {XSetName::DEV_SHOW_PARTITION_TABLES,   "dev_show_partition_tables"sv},
    {XSetName::DEV_SHOW_NET,                "dev_show_net"sv},
    {XSetName::DEV_SHOW_FILE,               "dev_show_file"sv},
    {XSetName::DEV_IGNORE_UDISKS_HIDE,      "dev_ignore_udisks_hide"sv},
    {XSetName::DEV_SHOW_HIDE_VOLUMES,       "dev_show_hide_volumes"sv},
    {XSetName::DEV_DISPNAME,                "dev_dispname"sv},

    {XSetName::DEV_MENU_AUTO,               "dev_menu_auto"sv},
    {XSetName::DEV_AUTOMOUNT_OPTICAL,       "dev_automount_optical"sv},
    {XSetName::DEV_AUTOMOUNT_REMOVABLE,     "dev_automount_removable"sv},
    {XSetName::DEV_IGNORE_UDISKS_NOPOLICY,  "dev_ignore_udisks_nopolicy"sv},
    {XSetName::DEV_AUTOMOUNT_VOLUMES,       "dev_automount_volumes"sv},
    {XSetName::DEV_AUTOMOUNT_DIRS,          "dev_automount_dirs"sv},
    {XSetName::DEV_AUTO_OPEN,               "dev_auto_open"sv},
    {XSetName::DEV_UNMOUNT_QUIT,            "dev_unmount_quit"sv},

    {XSetName::DEV_EXEC,                    "dev_exec"sv},
    {XSetName::DEV_EXEC_FS,                 "dev_exec_fs"sv},
    {XSetName::DEV_EXEC_AUDIO,              "dev_exec_audio"sv},
    {XSetName::DEV_EXEC_VIDEO,              "dev_exec_video"sv},
    {XSetName::DEV_EXEC_INSERT,             "dev_exec_insert"sv},
    {XSetName::DEV_EXEC_UNMOUNT,            "dev_exec_unmount"sv},
    {XSetName::DEV_EXEC_REMOVE,             "dev_exec_remove"sv},

    {XSetName::DEV_MOUNT_OPTIONS,           "dev_mount_options"sv},
    {XSetName::DEV_CHANGE,                  "dev_change"sv},
    {XSetName::DEV_FS_CNF,                  "dev_fs_cnf"sv},
    {XSetName::DEV_NET_CNF,                 "dev_net_cnf"sv},

    // dev icons
    {XSetName::DEV_ICON,                    "dev_icon"sv},
    {XSetName::DEV_ICON_INTERNAL_MOUNTED,   "dev_icon_internal_mounted"sv},
    {XSetName::DEV_ICON_INTERNAL_UNMOUNTED, "dev_icon_internal_unmounted"sv},
    {XSetName::DEV_ICON_REMOVE_MOUNTED,     "dev_icon_remove_mounted"sv},
    {XSetName::DEV_ICON_REMOVE_UNMOUNTED,   "dev_icon_remove_unmounted"sv},
    {XSetName::DEV_ICON_OPTICAL_MOUNTED,    "dev_icon_optical_mounted"sv},
    {XSetName::DEV_ICON_OPTICAL_MEDIA,      "dev_icon_optical_media"sv},
    {XSetName::DEV_ICON_OPTICAL_NOMEDIA,    "dev_icon_optical_nomedia"sv},
    {XSetName::DEV_ICON_AUDIOCD,            "dev_icon_audiocd"sv},
    {XSetName::DEV_ICON_FLOPPY_MOUNTED,     "dev_icon_floppy_mounted"sv},
    {XSetName::DEV_ICON_FLOPPY_UNMOUNTED,   "dev_icon_floppy_unmounted"sv},
    {XSetName::DEV_ICON_NETWORK,            "dev_icon_network"sv},
    {XSetName::DEV_ICON_FILE,               "dev_icon_file"sv},

    {XSetName::BOOK_OPEN,                   "book_open"sv},
    {XSetName::BOOK_SETTINGS,               "book_settings"sv},
    {XSetName::BOOK_ICON,                   "book_icon"sv},
    {XSetName::BOOK_MENU_ICON,              "book_menu_icon"sv},
    {XSetName::BOOK_SHOW,                   "book_show"sv},
    {XSetName::BOOK_ADD,                    "book_add"sv},
    {XSetName::MAIN_BOOK,                   "main_book"sv},

    // Fonts
    {XSetName::FONT_GENERAL,                "font_general"sv},
    {XSetName::FONT_VIEW_ICON,              "font_view_icon"sv},
    {XSetName::FONT_VIEW_COMPACT,           "font_view_compact"sv},

    // Rename/Move Dialog
    {XSetName::MOVE_NAME,                   "move_name"sv},
    {XSetName::MOVE_FILENAME,               "move_filename"sv},
    {XSetName::MOVE_PARENT,                 "move_parent"sv},
    {XSetName::MOVE_PATH,                   "move_path"sv},
    {XSetName::MOVE_TYPE,                   "move_type"sv},
    {XSetName::MOVE_TARGET,                 "move_target"sv},
    {XSetName::MOVE_TEMPLATE,               "move_template"sv},
    {XSetName::MOVE_OPTION,                 "move_option"sv},
    {XSetName::MOVE_COPY,                   "move_copy"sv},
    {XSetName::MOVE_LINK,                   "move_link"sv},
    {XSetName::MOVE_COPYT,                  "move_copyt"sv},
    {XSetName::MOVE_LINKT,                  "move_linkt"sv},
    {XSetName::MOVE_AS_ROOT,                "move_as_root"sv},
    {XSetName::MOVE_DLG_HELP,               "move_dlg_help"sv},
    {XSetName::MOVE_DLG_CONFIRM_CREATE,     "move_dlg_confirm_create"sv},

    // status bar
    {XSetName::STATUS_MIDDLE,               "status_middle"sv},
    {XSetName::STATUS_NAME,                 "status_name"sv},
    {XSetName::STATUS_PATH,                 "status_path"sv},
    {XSetName::STATUS_INFO,                 "status_info"sv},
    {XSetName::STATUS_HIDE,                 "status_hide"sv},

    // MAIN WINDOW MENUS //

    // File //
    {XSetName::MAIN_NEW_WINDOW,             "main_new_window"sv},
    {XSetName::MAIN_ROOT_WINDOW,            "main_root_window"sv},
    {XSetName::MAIN_SEARCH,                 "main_search"sv},
    {XSetName::MAIN_TERMINAL,               "main_terminal"sv},
    {XSetName::MAIN_ROOT_TERMINAL,          "main_root_terminal"sv},
    {XSetName::MAIN_SAVE_SESSION,           "main_save_session"sv},
    {XSetName::MAIN_SAVE_TABS,              "main_save_tabs"sv},
    {XSetName::MAIN_EXIT,                   "main_exit"sv},

    // VIEW //
    {XSetName::PANEL1_SHOW,                 "panel1_show"sv},
    {XSetName::PANEL2_SHOW,                 "panel2_show"sv},
    {XSetName::PANEL3_SHOW,                 "panel3_show"sv},
    {XSetName::PANEL4_SHOW,                 "panel4_show"sv},
    {XSetName::MAIN_FOCUS_PANEL,            "main_focus_panel"sv},
    {XSetName::PANEL_PREV,                  "panel_prev"sv},
    {XSetName::PANEL_NEXT,                  "panel_next"sv},
    {XSetName::PANEL_HIDE,                  "panel_hide"sv},
    {XSetName::PANEL_1,                     "panel_1"sv},
    {XSetName::PANEL_2,                     "panel_2"sv},
    {XSetName::PANEL_3,                     "panel_3"sv},
    {XSetName::PANEL_4,                     "panel_4"sv},

    {XSetName::MAIN_AUTO,                   "main_auto"sv},
    {XSetName::AUTO_INST,                   "auto_inst"sv},
    {XSetName::EVT_START,                   "evt_start"sv},
    {XSetName::EVT_EXIT,                    "evt_exit"sv},
    {XSetName::AUTO_WIN,                    "auto_win"sv},

    {XSetName::EVT_WIN_NEW,                 "evt_win_new"sv},
    {XSetName::EVT_WIN_FOCUS,               "evt_win_focus"sv},
    {XSetName::EVT_WIN_MOVE,                "evt_win_move"sv},
    {XSetName::EVT_WIN_CLICK,               "evt_win_click"sv},
    {XSetName::EVT_WIN_KEY,                 "evt_win_key"sv},
    {XSetName::EVT_WIN_CLOSE,               "evt_win_close"sv},

    {XSetName::AUTO_PNL,                    "auto_pnl"sv},
    {XSetName::EVT_PNL_FOCUS,               "evt_pnl_focus"sv},
    {XSetName::EVT_PNL_SHOW,                "evt_pnl_show"sv},
    {XSetName::EVT_PNL_SEL,                 "evt_pnl_sel"sv},

    {XSetName::AUTO_TAB,                    "auto_tab"sv},
    {XSetName::EVT_TAB_NEW,                 "evt_tab_new"sv},
    {XSetName::EVT_TAB_CHDIR,               "evt_tab_chdir"sv},
    {XSetName::EVT_TAB_FOCUS,               "evt_tab_focus"sv},
    {XSetName::EVT_TAB_CLOSE,               "evt_tab_close"sv},

    {XSetName::EVT_DEVICE,                  "evt_device"sv},
    {XSetName::MAIN_TITLE,                  "main_title"sv},
    {XSetName::MAIN_ICON,                   "main_icon"sv},
    {XSetName::MAIN_FULL,                   "main_full"sv},
    {XSetName::MAIN_DESIGN_MODE,            "main_design_mode"sv},
    {XSetName::MAIN_PREFS,                  "main_prefs"sv},
    {XSetName::MAIN_TOOL,                   "main_tool"sv},
    {XSetName::ROOT_BAR,                    "root_bar"sv},
    {XSetName::VIEW_THUMB,                  "view_thumb"sv},

    // Plugins //
    {XSetName::PLUG_INSTALL,                "plug_install"sv},
    {XSetName::PLUG_IFILE,                  "plug_ifile"sv},
    {XSetName::PLUG_COPY,                   "plug_copy"sv},
    {XSetName::PLUG_CFILE,                  "plug_cfile"sv},
    {XSetName::PLUG_CVERB,                  "plug_cverb"sv},

    // Help //
    {XSetName::MAIN_ABOUT,                  "main_about"sv},
    {XSetName::MAIN_DEV,                    "main_dev"sv},

    // Tasks //
    {XSetName::MAIN_TASKS,                  "main_tasks"sv},

    {XSetName::TASK_MANAGER,                "task_manager"sv},

    {XSetName::TASK_COLUMNS,                "task_columns"sv},
    {XSetName::TASK_COL_STATUS,             "task_col_status"sv},
    {XSetName::TASK_COL_COUNT,              "task_col_count"sv},
    {XSetName::TASK_COL_PATH,               "task_col_path"sv},
    {XSetName::TASK_COL_FILE,               "task_col_file"sv},
    {XSetName::TASK_COL_TO,                 "task_col_to"sv},
    {XSetName::TASK_COL_PROGRESS,           "task_col_progress"sv},
    {XSetName::TASK_COL_TOTAL,              "task_col_total"sv},
    {XSetName::TASK_COL_STARTED,            "task_col_started"sv},
    {XSetName::TASK_COL_ELAPSED,            "task_col_elapsed"sv},
    {XSetName::TASK_COL_CURSPEED,           "task_col_curspeed"sv},
    {XSetName::TASK_COL_CUREST,             "task_col_curest"sv},
    {XSetName::TASK_COL_AVGSPEED,           "task_col_avgspeed"sv},
    {XSetName::TASK_COL_AVGEST,             "task_col_avgest"sv},
    {XSetName::TASK_COL_REORDER,            "task_col_reorder"sv},

    {XSetName::TASK_STOP,                   "task_stop"sv},
    {XSetName::TASK_PAUSE,                  "task_pause"sv},
    {XSetName::TASK_QUE,                    "task_que"sv},
    {XSetName::TASK_RESUME,                 "task_resume"sv},
    {XSetName::TASK_SHOWOUT,                "task_showout"sv},

    {XSetName::TASK_ALL,                    "task_all"sv},
    {XSetName::TASK_STOP_ALL,               "task_stop_all"sv},
    {XSetName::TASK_PAUSE_ALL,              "task_pause_all"sv},
    {XSetName::TASK_QUE_ALL,                "task_que_all"sv},
    {XSetName::TASK_RESUME_ALL,             "task_resume_all"sv},

    {XSetName::TASK_SHOW_MANAGER,           "task_show_manager"sv},
    {XSetName::TASK_HIDE_MANAGER,           "task_hide_manager"sv},

    {XSetName::TASK_POPUPS,                 "task_popups"sv},
    {XSetName::TASK_POP_ALL,                "task_pop_all"sv},
    {XSetName::TASK_POP_TOP,                "task_pop_top"sv},
    {XSetName::TASK_POP_ABOVE,              "task_pop_above"sv},
    {XSetName::TASK_POP_STICK,              "task_pop_stick"sv},
    {XSetName::TASK_POP_DETAIL,             "task_pop_detail"sv},
    {XSetName::TASK_POP_OVER,               "task_pop_over"sv},
    {XSetName::TASK_POP_ERR,                "task_pop_err"sv},

    {XSetName::TASK_ERRORS,                 "task_errors"sv},
    {XSetName::TASK_ERR_FIRST,              "task_err_first"sv},
    {XSetName::TASK_ERR_ANY,                "task_err_any"sv},
    {XSetName::TASK_ERR_CONT,               "task_err_cont"sv},

    {XSetName::TASK_QUEUE,                  "task_queue"sv},
    {XSetName::TASK_Q_NEW,                  "task_q_new"sv},
    {XSetName::TASK_Q_SMART,                "task_q_smart"sv},
    {XSetName::TASK_Q_PAUSE,                "task_q_pause"sv},

    // PANELS COMMON //
    {XSetName::DATE_FORMAT,                 "date_format"sv},
    {XSetName::CON_OPEN,                    "con_open"sv},
    {XSetName::OPEN_EXECUTE,                "open_execute"sv},
    {XSetName::OPEN_EDIT,                   "open_edit"sv},
    {XSetName::OPEN_EDIT_ROOT,              "open_edit_root"sv},
    {XSetName::OPEN_OTHER,                  "open_other"sv},
    {XSetName::OPEN_HAND,                   "open_hand"sv},
    {XSetName::OPEN_ALL,                    "open_all"sv},
    {XSetName::OPEN_IN_TAB,                 "open_in_tab"sv},

    {XSetName::OPENTAB_NEW,                 "opentab_new"sv},
    {XSetName::OPENTAB_PREV,                "opentab_prev"sv},
    {XSetName::OPENTAB_NEXT,                "opentab_next"sv},
    {XSetName::OPENTAB_1,                   "opentab_1"sv},
    {XSetName::OPENTAB_2,                   "opentab_2"sv},
    {XSetName::OPENTAB_3,                   "opentab_3"sv},
    {XSetName::OPENTAB_4,                   "opentab_4"sv},
    {XSetName::OPENTAB_5,                   "opentab_5"sv},
    {XSetName::OPENTAB_6,                   "opentab_6"sv},
    {XSetName::OPENTAB_7,                   "opentab_7"sv},
    {XSetName::OPENTAB_8,                   "opentab_8"sv},
    {XSetName::OPENTAB_9,                   "opentab_9"sv},
    {XSetName::OPENTAB_10,                  "opentab_10"sv},

    {XSetName::OPEN_IN_PANEL,               "open_in_panel"sv},
    {XSetName::OPEN_IN_PANELPREV,           "open_in_panelprev"sv},
    {XSetName::OPEN_IN_PANELNEXT,           "open_in_panelnext"sv},
    {XSetName::OPEN_IN_PANEL1,              "open_in_panel1"sv},
    {XSetName::OPEN_IN_PANEL2,              "open_in_panel2"sv},
    {XSetName::OPEN_IN_PANEL3,              "open_in_panel3"sv},
    {XSetName::OPEN_IN_PANEL4,              "open_in_panel4"sv},

    {XSetName::ARC_EXTRACT,                 "arc_extract"sv},
    {XSetName::ARC_EXTRACTTO,               "arc_extractto"sv},
    {XSetName::ARC_LIST,                    "arc_list"sv},

    {XSetName::ARC_DEFAULT,                 "arc_default"sv},
    {XSetName::ARC_DEF_OPEN,                "arc_def_open"sv},
    {XSetName::ARC_DEF_EX,                  "arc_def_ex"sv},
    {XSetName::ARC_DEF_EXTO,                "arc_def_exto"sv},
    {XSetName::ARC_DEF_LIST,                "arc_def_list"sv},
    {XSetName::ARC_DEF_PARENT,              "arc_def_parent"sv},
    {XSetName::ARC_DEF_WRITE,               "arc_def_write"sv},
    {XSetName::ARC_CONF2,                   "arc_conf2"sv},

    {XSetName::OPEN_NEW,                    "open_new"sv},
    {XSetName::NEW_FILE,                    "new_file"sv},
    {XSetName::NEW_DIRECTORY,               "new_directory"sv},
    {XSetName::NEW_LINK,                    "new_link"sv},
    {XSetName::NEW_ARCHIVE,                 "new_archive"sv},
    {XSetName::TAB_NEW,                     "tab_new"sv},
    {XSetName::TAB_NEW_HERE,                "tab_new_here"sv},
    {XSetName::NEW_BOOKMARK,                "new_bookmark"sv},
    {XSetName::ARC_DLG,                     "arc_dlg"sv},

    {XSetName::NEW_APP,                     "new_app"sv},
    {XSetName::CON_GO,                      "con_go"sv},

    {XSetName::GO_REFRESH,                  "go_refresh"sv},
    {XSetName::GO_BACK,                     "go_back"sv},
    {XSetName::GO_FORWARD,                  "go_forward"sv},
    {XSetName::GO_UP,                       "go_up"sv},
    {XSetName::GO_HOME,                     "go_home"sv},
    {XSetName::GO_DEFAULT,                  "go_default"sv},
    {XSetName::GO_SET_DEFAULT,              "go_set_default"sv},
    {XSetName::EDIT_CANON,                  "edit_canon"sv},

    {XSetName::GO_FOCUS,                    "go_focus"sv},
    {XSetName::FOCUS_PATH_BAR,              "focus_path_bar"sv},
    {XSetName::FOCUS_FILELIST,              "focus_filelist"sv},
    {XSetName::FOCUS_DIRTREE,               "focus_dirtree"sv},
    {XSetName::FOCUS_BOOK,                  "focus_book"sv},
    {XSetName::FOCUS_DEVICE,                "focus_device"sv},

    {XSetName::GO_TAB,                      "go_tab"sv},
    {XSetName::TAB_PREV,                    "tab_prev"sv},
    {XSetName::TAB_NEXT,                    "tab_next"sv},
    {XSetName::TAB_RESTORE,                 "tab_restore"sv},
    {XSetName::TAB_CLOSE,                   "tab_close"sv},
    {XSetName::TAB_1,                       "tab_1"sv},
    {XSetName::TAB_2,                       "tab_2"sv},
    {XSetName::TAB_3,                       "tab_3"sv},
    {XSetName::TAB_4,                       "tab_4"sv},
    {XSetName::TAB_5,                       "tab_5"sv},
    {XSetName::TAB_6,                       "tab_6"sv},
    {XSetName::TAB_7,                       "tab_7"sv},
    {XSetName::TAB_8,                       "tab_8"sv},
    {XSetName::TAB_9,                       "tab_9"sv},
    {XSetName::TAB_10,                      "tab_10"sv},

    {XSetName::CON_VIEW,                    "con_view"sv},
    {XSetName::VIEW_LIST_STYLE,             "view_list_style"sv},
    {XSetName::VIEW_COLUMNS,                "view_columns"sv},
    {XSetName::VIEW_REORDER_COL,            "view_reorder_col"sv},
    {XSetName::RUBBERBAND,                  "rubberband"sv},

    {XSetName::VIEW_SORTBY,                 "view_sortby"sv},
    {XSetName::SORTBY_NAME,                 "sortby_name"sv},
    {XSetName::SORTBY_SIZE,                 "sortby_size"sv},
    {XSetName::SORTBY_TYPE,                 "sortby_type"sv},
    {XSetName::SORTBY_PERM,                 "sortby_perm"sv},
    {XSetName::SORTBY_OWNER,                "sortby_owner"sv},
    {XSetName::SORTBY_DATE,                 "sortby_date"sv},
    {XSetName::SORTBY_ASCEND,               "sortby_ascend"sv},
    {XSetName::SORTBY_DESCEND,              "sortby_descend"sv},
    {XSetName::SORTX_ALPHANUM,              "sortx_alphanum"sv},
    // {XSetName::SORTX_NATURAL,            "sortx_natural"sv},
    {XSetName::SORTX_CASE,                  "sortx_case"sv},
    {XSetName::SORTX_DIRECTORIES,           "sortx_directories"sv},
    {XSetName::SORTX_FILES,                 "sortx_files"sv},
    {XSetName::SORTX_MIX,                   "sortx_mix"sv},
    {XSetName::SORTX_HIDFIRST,              "sortx_hidfirst"sv},
    {XSetName::SORTX_HIDLAST,               "sortx_hidlast"sv},

    {XSetName::VIEW_REFRESH,                "view_refresh"sv},
    {XSetName::PATH_SEEK,                   "path_seek"sv},
    {XSetName::PATH_HAND,                   "path_hand"sv},
    {XSetName::EDIT_CUT,                    "edit_cut"sv},
    {XSetName::EDIT_COPY,                   "edit_copy"sv},
    {XSetName::EDIT_PASTE,                  "edit_paste"sv},
    {XSetName::EDIT_RENAME,                 "edit_rename"sv},
    {XSetName::EDIT_DELETE,                 "edit_delete"sv},
    {XSetName::EDIT_TRASH,                  "edit_trash"sv},

    {XSetName::EDIT_SUBMENU,                "edit_submenu"sv},
    {XSetName::COPY_NAME,                   "copy_name"sv},
    {XSetName::COPY_PARENT,                 "copy_parent"sv},
    {XSetName::COPY_PATH,                   "copy_path"sv},
    {XSetName::PASTE_LINK,                  "paste_link"sv},
    {XSetName::PASTE_TARGET,                "paste_target"sv},
    {XSetName::PASTE_AS,                    "paste_as"sv},
    {XSetName::COPY_TO,                     "copy_to"sv},
    {XSetName::COPY_LOC,                    "copy_loc"sv},
    {XSetName::COPY_LOC_LAST,               "copy_loc_last"sv},

    {XSetName::COPY_TAB,                    "copy_tab"sv},
    {XSetName::COPY_TAB_PREV,               "copy_tab_prev"sv},
    {XSetName::COPY_TAB_NEXT,               "copy_tab_next"sv},
    {XSetName::COPY_TAB_1,                  "copy_tab_1"sv},
    {XSetName::COPY_TAB_2,                  "copy_tab_2"sv},
    {XSetName::COPY_TAB_3,                  "copy_tab_3"sv},
    {XSetName::COPY_TAB_4,                  "copy_tab_4"sv},
    {XSetName::COPY_TAB_5,                  "copy_tab_5"sv},
    {XSetName::COPY_TAB_6,                  "copy_tab_6"sv},
    {XSetName::COPY_TAB_7,                  "copy_tab_7"sv},
    {XSetName::COPY_TAB_8,                  "copy_tab_8"sv},
    {XSetName::COPY_TAB_9,                  "copy_tab_9"sv},
    {XSetName::COPY_TAB_10,                 "copy_tab_10"sv},

    {XSetName::COPY_PANEL,                  "copy_panel"sv},
    {XSetName::COPY_PANEL_PREV,             "copy_panel_prev"sv},
    {XSetName::COPY_PANEL_NEXT,             "copy_panel_next"sv},
    {XSetName::COPY_PANEL_1,                "copy_panel_1"sv},
    {XSetName::COPY_PANEL_2,                "copy_panel_2"sv},
    {XSetName::COPY_PANEL_3,                "copy_panel_3"sv},
    {XSetName::COPY_PANEL_4,                "copy_panel_4"sv},

    {XSetName::MOVE_TO,                     "move_to"sv},
    {XSetName::MOVE_LOC,                    "move_loc"sv},
    {XSetName::MOVE_LOC_LAST,               "move_loc_last"sv},

    {XSetName::MOVE_TAB,                    "move_tab"sv},
    {XSetName::MOVE_TAB_PREV,               "move_tab_prev"sv},
    {XSetName::MOVE_TAB_NEXT,               "move_tab_next"sv},
    {XSetName::MOVE_TAB_1,                  "move_tab_1"sv},
    {XSetName::MOVE_TAB_2,                  "move_tab_2"sv},
    {XSetName::MOVE_TAB_3,                  "move_tab_3"sv},
    {XSetName::MOVE_TAB_4,                  "move_tab_4"sv},
    {XSetName::MOVE_TAB_5,                  "move_tab_5"sv},
    {XSetName::MOVE_TAB_6,                  "move_tab_6"sv},
    {XSetName::MOVE_TAB_7,                  "move_tab_7"sv},
    {XSetName::MOVE_TAB_8,                  "move_tab_8"sv},
    {XSetName::MOVE_TAB_9,                  "move_tab_9"sv},
    {XSetName::MOVE_TAB_10,                 "move_tab_10"sv},

    {XSetName::MOVE_PANEL,                  "move_panel"sv},
    {XSetName::MOVE_PANEL_PREV,             "move_panel_prev"sv},
    {XSetName::MOVE_PANEL_NEXT,             "move_panel_next"sv},
    {XSetName::MOVE_PANEL_1,                "move_panel_1"sv},
    {XSetName::MOVE_PANEL_2,                "move_panel_2"sv},
    {XSetName::MOVE_PANEL_3,                "move_panel_3"sv},
    {XSetName::MOVE_PANEL_4,                "move_panel_4"sv},

    {XSetName::EDIT_HIDE,                   "edit_hide"sv},
    {XSetName::SELECT_ALL,                  "select_all"sv},
    {XSetName::SELECT_UN,                   "select_un"sv},
    {XSetName::SELECT_INVERT,               "select_invert"sv},
    {XSetName::SELECT_PATT,                 "select_patt"sv},
    {XSetName::EDIT_ROOT,                   "edit_root"sv},
    {XSetName::ROOT_COPY_LOC,               "root_copy_loc"sv},
    {XSetName::ROOT_MOVE2,                  "root_move2"sv},
    {XSetName::ROOT_TRASH,                  "root_trash"sv},
    {XSetName::ROOT_DELETE,                 "root_delete"sv},

    // Properties //
    {XSetName::CON_PROP,                    "con_prop"sv},
    {XSetName::PROP_INFO,                   "prop_info"sv},
    {XSetName::PROP_PERM,                   "prop_perm"sv},
    {XSetName::PROP_QUICK,                  "prop_quick"sv},

    {XSetName::PERM_R,                      "perm_r"sv},
    {XSetName::PERM_RW,                     "perm_rw"sv},
    {XSetName::PERM_RWX,                    "perm_rwx"sv},
    {XSetName::PERM_R_R,                    "perm_r_r"sv},
    {XSetName::PERM_RW_R,                   "perm_rw_r"sv},
    {XSetName::PERM_RW_RW,                  "perm_rw_rw"sv},
    {XSetName::PERM_RWXR_X,                 "perm_rwxr_x"sv},
    {XSetName::PERM_RWXRWX,                 "perm_rwxrwx"sv},
    {XSetName::PERM_R_R_R,                  "perm_r_r_r"sv},
    {XSetName::PERM_RW_R_R,                 "perm_rw_r_r"sv},
    {XSetName::PERM_RW_RW_RW,               "perm_rw_rw_rw"sv},
    {XSetName::PERM_RWXR_R,                 "perm_rwxr_r"sv},
    {XSetName::PERM_RWXR_XR_X,              "perm_rwxr_xr_x"sv},
    {XSetName::PERM_RWXRWXRWX,              "perm_rwxrwxrwx"sv},
    {XSetName::PERM_RWXRWXRWT,              "perm_rwxrwxrwt"sv},
    {XSetName::PERM_UNSTICK,                "perm_unstick"sv},
    {XSetName::PERM_STICK,                  "perm_stick"sv},

    {XSetName::PERM_RECURS,                 "perm_recurs"sv},
    {XSetName::PERM_GO_W,                   "perm_go_w"sv},
    {XSetName::PERM_GO_RWX,                 "perm_go_rwx"sv},
    {XSetName::PERM_UGO_W,                  "perm_ugo_w"sv},
    {XSetName::PERM_UGO_RX,                 "perm_ugo_rx"sv},
    {XSetName::PERM_UGO_RWX,                "perm_ugo_rwx"sv},

    {XSetName::PROP_ROOT,                   "prop_root"sv},
    {XSetName::RPERM_RW,                    "rperm_rw"sv},
    {XSetName::RPERM_RWX,                   "rperm_rwx"sv},
    {XSetName::RPERM_RW_R,                  "rperm_rw_r"sv},
    {XSetName::RPERM_RW_RW,                 "rperm_rw_rw"sv},
    {XSetName::RPERM_RWXR_X,                "rperm_rwxr_x"sv},
    {XSetName::RPERM_RWXRWX,                "rperm_rwxrwx"sv},
    {XSetName::RPERM_RW_R_R,                "rperm_rw_r_r"sv},
    {XSetName::RPERM_RW_RW_RW,              "rperm_rw_rw_rw"sv},
    {XSetName::RPERM_RWXR_R,                "rperm_rwxr_r"sv},
    {XSetName::RPERM_RWXR_XR_X,             "rperm_rwxr_xr_x"sv},
    {XSetName::RPERM_RWXRWXRWX,             "rperm_rwxrwxrwx"sv},
    {XSetName::RPERM_RWXRWXRWT,             "rperm_rwxrwxrwt"sv},
    {XSetName::RPERM_UNSTICK,               "rperm_unstick"sv},
    {XSetName::RPERM_STICK,                 "rperm_stick"sv},

    {XSetName::RPERM_RECURS,                "rperm_recurs"sv},
    {XSetName::RPERM_GO_W,                  "rperm_go_w"sv},
    {XSetName::RPERM_GO_RWX,                "rperm_go_rwx"sv},
    {XSetName::RPERM_UGO_W,                 "rperm_ugo_w"sv},
    {XSetName::RPERM_UGO_RX,                "rperm_ugo_rx"sv},
    {XSetName::RPERM_UGO_RWX,               "rperm_ugo_rwx"sv},

    {XSetName::RPERM_OWN,                   "rperm_own"sv},
    {XSetName::OWN_MYUSER,                  "own_myuser"sv},
    {XSetName::OWN_MYUSER_USERS,            "own_myuser_users"sv},
    {XSetName::OWN_USER1,                   "own_user1"sv},
    {XSetName::OWN_USER1_USERS,             "own_user1_users"sv},
    {XSetName::OWN_USER2,                   "own_user2"sv},
    {XSetName::OWN_USER2_USERS,             "own_user2_users"sv},
    {XSetName::OWN_ROOT,                    "own_root"sv},
    {XSetName::OWN_ROOT_USERS,              "own_root_users"sv},
    {XSetName::OWN_ROOT_MYUSER,             "own_root_myuser"sv},
    {XSetName::OWN_ROOT_USER1,              "own_root_user1"sv},
    {XSetName::OWN_ROOT_USER2,              "own_root_user2"sv},

    {XSetName::OWN_RECURS,                  "own_recurs"sv},
    {XSetName::ROWN_MYUSER,                 "rown_myuser"sv},
    {XSetName::ROWN_MYUSER_USERS,           "rown_myuser_users"sv},
    {XSetName::ROWN_USER1,                  "rown_user1"sv},
    {XSetName::ROWN_USER1_USERS,            "rown_user1_users"sv},
    {XSetName::ROWN_USER2,                  "rown_user2"sv},
    {XSetName::ROWN_USER2_USERS,            "rown_user2_users"sv},
    {XSetName::ROWN_ROOT,                   "rown_root"sv},
    {XSetName::ROWN_ROOT_USERS,             "rown_root_users"sv},
    {XSetName::ROWN_ROOT_MYUSER,            "rown_root_myuser"sv},
    {XSetName::ROWN_ROOT_USER1,             "rown_root_user1"sv},
    {XSetName::ROWN_ROOT_USER2,             "rown_root_user2"sv},

    // PANELS //
    {XSetName::PANEL_SLIDERS,               "panel_sliders"sv},

    // panel1
    {XSetName::PANEL1_SHOW_TOOLBOX,         "panel1_show_toolbox"sv},
    {XSetName::PANEL1_SHOW_DEVMON,          "panel1_show_devmon"sv},
    {XSetName::PANEL1_SHOW_DIRTREE,         "panel1_show_dirtree"sv},
    {XSetName::PANEL1_SHOW_SIDEBAR,         "panel1_show_sidebar"sv},
    {XSetName::PANEL1_SLIDER_POSITIONS,     "panel1_slider_positions"sv},
    {XSetName::PANEL1_LIST_DETAILED,        "panel1_list_detailed"sv},
    {XSetName::PANEL1_LIST_ICONS,           "panel1_list_icons"sv},
    {XSetName::PANEL1_LIST_COMPACT,         "panel1_list_compact"sv},
    {XSetName::PANEL1_LIST_LARGE,           "panel1_list_large"sv},
    {XSetName::PANEL1_SHOW_HIDDEN,          "panel1_show_hidden"sv},
    {XSetName::PANEL1_ICON_TAB,             "panel1_icon_tab"sv},
    {XSetName::PANEL1_ICON_STATUS,          "panel1_icon_status"sv},
    {XSetName::PANEL1_DETCOL_NAME,          "panel1_detcol_name"sv},
    {XSetName::PANEL1_DETCOL_SIZE,          "panel1_detcol_size"sv},
    {XSetName::PANEL1_DETCOL_TYPE,          "panel1_detcol_type"sv},
    {XSetName::PANEL1_DETCOL_PERM,          "panel1_detcol_perm"sv},
    {XSetName::PANEL1_DETCOL_OWNER,         "panel1_detcol_owner"sv},
    {XSetName::PANEL1_DETCOL_DATE,          "panel1_detcol_date"sv},
    {XSetName::PANEL1_SORT_EXTRA,           "panel1_sort_extra"sv},
    {XSetName::PANEL1_BOOK_FOL,             "panel1_book_fol"sv},
    {XSetName::PANEL1_TOOL_L,               "panel1_tool_l"sv},
    {XSetName::PANEL1_TOOL_R,               "panel1_tool_r"sv},
    {XSetName::PANEL1_TOOL_S,               "panel1_tool_s"sv},

    // panel2
    {XSetName::PANEL2_SHOW_TOOLBOX,         "panel2_show_toolbox"sv},
    {XSetName::PANEL2_SHOW_DEVMON,          "panel2_show_devmon"sv},
    {XSetName::PANEL2_SHOW_DIRTREE,         "panel2_show_dirtree"sv},
    {XSetName::PANEL2_SHOW_SIDEBAR,         "panel2_show_sidebar"sv},
    {XSetName::PANEL2_SLIDER_POSITIONS,     "panel2_slider_positions"sv},
    {XSetName::PANEL2_LIST_DETAILED,        "panel2_list_detailed"sv},
    {XSetName::PANEL2_LIST_ICONS,           "panel2_list_icons"sv},
    {XSetName::PANEL2_LIST_COMPACT,         "panel2_list_compact"sv},
    {XSetName::PANEL2_LIST_LARGE,           "panel2_list_large"sv},
    {XSetName::PANEL2_SHOW_HIDDEN,          "panel2_show_hidden"sv},
    {XSetName::PANEL2_ICON_TAB,             "panel2_icon_tab"sv},
    {XSetName::PANEL2_ICON_STATUS,          "panel2_icon_status"sv},
    {XSetName::PANEL2_DETCOL_NAME,          "panel2_detcol_name"sv},
    {XSetName::PANEL2_DETCOL_SIZE,          "panel2_detcol_size"sv},
    {XSetName::PANEL2_DETCOL_TYPE,          "panel2_detcol_type"sv},
    {XSetName::PANEL2_DETCOL_PERM,          "panel2_detcol_perm"sv},
    {XSetName::PANEL2_DETCOL_OWNER,         "panel2_detcol_owner"sv},
    {XSetName::PANEL2_DETCOL_DATE,          "panel2_detcol_date"sv},
    {XSetName::PANEL2_SORT_EXTRA,           "panel2_sort_extra"sv},
    {XSetName::PANEL2_BOOK_FOL,             "panel2_book_fol"sv},
    {XSetName::PANEL2_TOOL_L,               "panel2_tool_l"sv},
    {XSetName::PANEL2_TOOL_R,               "panel2_tool_r"sv},
    {XSetName::PANEL2_TOOL_S,               "panel2_tool_s"sv},

    // panel3
    {XSetName::PANEL3_SHOW_TOOLBOX,         "panel3_show_toolbox"sv},
    {XSetName::PANEL3_SHOW_DEVMON,          "panel3_show_devmon"sv},
    {XSetName::PANEL3_SHOW_DIRTREE,         "panel3_show_dirtree"sv},
    {XSetName::PANEL3_SHOW_SIDEBAR,         "panel3_show_sidebar"sv},
    {XSetName::PANEL3_SLIDER_POSITIONS,     "panel3_slider_positions"sv},
    {XSetName::PANEL3_LIST_DETAILED,        "panel3_list_detailed"sv},
    {XSetName::PANEL3_LIST_ICONS,           "panel3_list_icons"sv},
    {XSetName::PANEL3_LIST_COMPACT,         "panel3_list_compact"sv},
    {XSetName::PANEL3_LIST_LARGE,           "panel3_list_large"sv},
    {XSetName::PANEL3_SHOW_HIDDEN,          "panel3_show_hidden"sv},
    {XSetName::PANEL3_ICON_TAB,             "panel3_icon_tab"sv},
    {XSetName::PANEL3_ICON_STATUS,          "panel3_icon_status"sv},
    {XSetName::PANEL3_DETCOL_NAME,          "panel3_detcol_name"sv},
    {XSetName::PANEL3_DETCOL_SIZE,          "panel3_detcol_size"sv},
    {XSetName::PANEL3_DETCOL_TYPE,          "panel3_detcol_type"sv},
    {XSetName::PANEL3_DETCOL_PERM,          "panel3_detcol_perm"sv},
    {XSetName::PANEL3_DETCOL_OWNER,         "panel3_detcol_owner"sv},
    {XSetName::PANEL3_DETCOL_DATE,          "panel3_detcol_date"sv},
    {XSetName::PANEL3_SORT_EXTRA,           "panel3_sort_extra"sv},
    {XSetName::PANEL3_BOOK_FOL,             "panel3_book_fol"sv},
    {XSetName::PANEL3_TOOL_L,               "panel3_tool_l"sv},
    {XSetName::PANEL3_TOOL_R,               "panel3_tool_r"sv},
    {XSetName::PANEL3_TOOL_S,               "panel3_tool_s"sv},

    // panel4
    {XSetName::PANEL4_SHOW_TOOLBOX,         "panel4_show_toolbox"sv},
    {XSetName::PANEL4_SHOW_DEVMON,          "panel4_show_devmon"sv},
    {XSetName::PANEL4_SHOW_DIRTREE,         "panel4_show_dirtree"sv},
    {XSetName::PANEL4_SHOW_SIDEBAR,         "panel4_show_sidebar"sv},
    {XSetName::PANEL4_SLIDER_POSITIONS,     "panel4_slider_positions"sv},
    {XSetName::PANEL4_LIST_DETAILED,        "panel4_list_detailed"sv},
    {XSetName::PANEL4_LIST_ICONS,           "panel4_list_icons"sv},
    {XSetName::PANEL4_LIST_COMPACT,         "panel4_list_compact"sv},
    {XSetName::PANEL4_LIST_LARGE,           "panel4_list_large"sv},
    {XSetName::PANEL4_SHOW_HIDDEN,          "panel4_show_hidden"sv},
    {XSetName::PANEL4_ICON_TAB,             "panel4_icon_tab"sv},
    {XSetName::PANEL4_ICON_STATUS,          "panel4_icon_status"sv},
    {XSetName::PANEL4_DETCOL_NAME,          "panel4_detcol_name"sv},
    {XSetName::PANEL4_DETCOL_SIZE,          "panel4_detcol_size"sv},
    {XSetName::PANEL4_DETCOL_TYPE,          "panel4_detcol_type"sv},
    {XSetName::PANEL4_DETCOL_PERM,          "panel4_detcol_perm"sv},
    {XSetName::PANEL4_DETCOL_OWNER,         "panel4_detcol_owner"sv},
    {XSetName::PANEL4_DETCOL_DATE,          "panel4_detcol_date"sv},
    {XSetName::PANEL4_SORT_EXTRA,           "panel4_sort_extra"sv},
    {XSetName::PANEL4_BOOK_FOL,             "panel4_book_fol"sv},
    {XSetName::PANEL4_TOOL_L,               "panel4_tool_l"sv},
    {XSetName::PANEL4_TOOL_R,               "panel4_tool_r"sv},
    {XSetName::PANEL4_TOOL_S,               "panel4_tool_s"sv},

    // panel modes

    // panel1

    // panel1 mode 0
    {XSetName::PANEL1_SHOW_TOOLBOX_0,       "panel1_show_toolbox0"sv},
    {XSetName::PANEL1_SHOW_DEVMON_0,        "panel1_show_devmon0"sv},
    {XSetName::PANEL1_SHOW_DIRTREE_0,       "panel1_show_dirtree0"sv},
    {XSetName::PANEL1_SHOW_SIDEBAR_0,       "panel1_show_sidebar0"sv},
    {XSetName::PANEL1_SLIDER_POSITIONS_0,   "panel1_slider_positions0"sv},
    {XSetName::PANEL1_LIST_DETAILED_0,      "panel1_list_detailed0"sv},
    {XSetName::PANEL1_LIST_ICONS_0,         "panel1_list_icons0"sv},
    {XSetName::PANEL1_LIST_COMPACT_0,       "panel1_list_compact0"sv},
    {XSetName::PANEL1_LIST_LARGE_0,         "panel1_list_large0"sv},
    {XSetName::PANEL1_SHOW_HIDDEN_0,        "panel1_show_hidden0"sv},
    {XSetName::PANEL1_ICON_TAB_0,           "panel1_icon_tab0"sv},
    {XSetName::PANEL1_ICON_STATUS_0,        "panel1_icon_status0"sv},
    {XSetName::PANEL1_DETCOL_NAME_0,        "panel1_detcol_name0"sv},
    {XSetName::PANEL1_DETCOL_SIZE_0,        "panel1_detcol_size0"sv},
    {XSetName::PANEL1_DETCOL_TYPE_0,        "panel1_detcol_type0"sv},
    {XSetName::PANEL1_DETCOL_PERM_0,        "panel1_detcol_perm0"sv},
    {XSetName::PANEL1_DETCOL_OWNER_0,       "panel1_detcol_owner0"sv},
    {XSetName::PANEL1_DETCOL_DATE_0,        "panel1_detcol_date0"sv},
    {XSetName::PANEL1_SORT_EXTRA_0,         "panel1_sort_extra0"sv},
    {XSetName::PANEL1_BOOK_FOL_0,           "panel1_book_fol0"sv},
    {XSetName::PANEL1_TOOL_L_0,             "panel1_tool_l0"sv},
    {XSetName::PANEL1_TOOL_R_0,             "panel1_tool_r0"sv},
    {XSetName::PANEL1_TOOL_S_0,             "panel1_tool_s0"sv},

    // panel1 mode 1
    {XSetName::PANEL1_SHOW_TOOLBOX_1,       "panel1_show_toolbox1"sv},
    {XSetName::PANEL1_SHOW_DEVMON_1,        "panel1_show_devmon1"sv},
    {XSetName::PANEL1_SHOW_DIRTREE_1,       "panel1_show_dirtree1"sv},
    {XSetName::PANEL1_SHOW_SIDEBAR_1,       "panel1_show_sidebar1"sv},
    {XSetName::PANEL1_SLIDER_POSITIONS_1,   "panel1_slider_positions1"sv},
    {XSetName::PANEL1_LIST_DETAILED_1,      "panel1_list_detailed1"sv},
    {XSetName::PANEL1_LIST_ICONS_1,         "panel1_list_icons1"sv},
    {XSetName::PANEL1_LIST_COMPACT_1,       "panel1_list_compact1"sv},
    {XSetName::PANEL1_LIST_LARGE_1,         "panel1_list_large1"sv},
    {XSetName::PANEL1_SHOW_HIDDEN_1,        "panel1_show_hidden1"sv},
    {XSetName::PANEL1_ICON_TAB_1,           "panel1_icon_tab1"sv},
    {XSetName::PANEL1_ICON_STATUS_1,        "panel1_icon_status1"sv},
    {XSetName::PANEL1_DETCOL_NAME_1,        "panel1_detcol_name1"sv},
    {XSetName::PANEL1_DETCOL_SIZE_1,        "panel1_detcol_size1"sv},
    {XSetName::PANEL1_DETCOL_TYPE_1,        "panel1_detcol_type1"sv},
    {XSetName::PANEL1_DETCOL_PERM_1,        "panel1_detcol_perm1"sv},
    {XSetName::PANEL1_DETCOL_OWNER_1,       "panel1_detcol_owner1"sv},
    {XSetName::PANEL1_DETCOL_DATE_1,        "panel1_detcol_date1"sv},
    {XSetName::PANEL1_SORT_EXTRA_1,         "panel1_sort_extra1"sv},
    {XSetName::PANEL1_BOOK_FOL_1,           "panel1_book_fol1"sv},
    {XSetName::PANEL1_TOOL_L_1,             "panel1_tool_l1"sv},
    {XSetName::PANEL1_TOOL_R_1,             "panel1_tool_r1"sv},
    {XSetName::PANEL1_TOOL_S_1,             "panel1_tool_s1"sv},

    // panel1 mode 2
    {XSetName::PANEL1_SHOW_TOOLBOX_2,       "panel1_show_toolbox2"sv},
    {XSetName::PANEL1_SHOW_DEVMON_2,        "panel1_show_devmon2"sv},
    {XSetName::PANEL1_SHOW_DIRTREE_2,       "panel1_show_dirtree2"sv},
    {XSetName::PANEL1_SHOW_SIDEBAR_2,       "panel1_show_sidebar2"sv},
    {XSetName::PANEL1_SLIDER_POSITIONS_2,   "panel1_slider_positions2"sv},
    {XSetName::PANEL1_LIST_DETAILED_2,      "panel1_list_detailed2"sv},
    {XSetName::PANEL1_LIST_ICONS_2,         "panel1_list_icons2"sv},
    {XSetName::PANEL1_LIST_COMPACT_2,       "panel1_list_compact2"sv},
    {XSetName::PANEL1_LIST_LARGE_2,         "panel1_list_large2"sv},
    {XSetName::PANEL1_SHOW_HIDDEN_2,        "panel1_show_hidden2"sv},
    {XSetName::PANEL1_ICON_TAB_2,           "panel1_icon_tab2"sv},
    {XSetName::PANEL1_ICON_STATUS_2,        "panel1_icon_status2"sv},
    {XSetName::PANEL1_DETCOL_NAME_2,        "panel1_detcol_name2"sv},
    {XSetName::PANEL1_DETCOL_SIZE_2,        "panel1_detcol_size2"sv},
    {XSetName::PANEL1_DETCOL_TYPE_2,        "panel1_detcol_type2"sv},
    {XSetName::PANEL1_DETCOL_PERM_2,        "panel1_detcol_perm2"sv},
    {XSetName::PANEL1_DETCOL_OWNER_2,       "panel1_detcol_owner2"sv},
    {XSetName::PANEL1_DETCOL_DATE_2,        "panel1_detcol_date2"sv},
    {XSetName::PANEL1_SORT_EXTRA_2,         "panel1_sort_extra2"sv},
    {XSetName::PANEL1_BOOK_FOL_2,           "panel1_book_fol2"sv},
    {XSetName::PANEL1_TOOL_L_2,             "panel1_tool_l2"sv},
    {XSetName::PANEL1_TOOL_R_2,             "panel1_tool_r2"sv},
    {XSetName::PANEL1_TOOL_S_2,             "panel1_tool_s2"sv},

    // panel1 mode 3
    {XSetName::PANEL1_SHOW_TOOLBOX_3,       "panel1_show_toolbox3"sv},
    {XSetName::PANEL1_SHOW_DEVMON_3,        "panel1_show_devmon3"sv},
    {XSetName::PANEL1_SHOW_DIRTREE_3,       "panel1_show_dirtree3"sv},
    {XSetName::PANEL1_SHOW_SIDEBAR_3,       "panel1_show_sidebar3"sv},
    {XSetName::PANEL1_SLIDER_POSITIONS_3,   "panel1_slider_positions3"sv},
    {XSetName::PANEL1_LIST_DETAILED_3,      "panel1_list_detailed3"sv},
    {XSetName::PANEL1_LIST_ICONS_3,         "panel1_list_icons3"sv},
    {XSetName::PANEL1_LIST_COMPACT_3,       "panel1_list_compact3"sv},
    {XSetName::PANEL1_LIST_LARGE_3,         "panel1_list_large3"sv},
    {XSetName::PANEL1_SHOW_HIDDEN_3,        "panel1_show_hidden3"sv},
    {XSetName::PANEL1_ICON_TAB_3,           "panel1_icon_tab3"sv},
    {XSetName::PANEL1_ICON_STATUS_3,        "panel1_icon_status3"sv},
    {XSetName::PANEL1_DETCOL_NAME_3,        "panel1_detcol_name3"sv},
    {XSetName::PANEL1_DETCOL_SIZE_3,        "panel1_detcol_size3"sv},
    {XSetName::PANEL1_DETCOL_TYPE_3,        "panel1_detcol_type3"sv},
    {XSetName::PANEL1_DETCOL_PERM_3,        "panel1_detcol_perm3"sv},
    {XSetName::PANEL1_DETCOL_OWNER_3,       "panel1_detcol_owner3"sv},
    {XSetName::PANEL1_DETCOL_DATE_3,        "panel1_detcol_date3"sv},
    {XSetName::PANEL1_SORT_EXTRA_3,         "panel1_sort_extra3"sv},
    {XSetName::PANEL1_BOOK_FOL_3,           "panel1_book_fol3"sv},
    {XSetName::PANEL1_TOOL_L_3,             "panel1_tool_l3"sv},
    {XSetName::PANEL1_TOOL_R_3,             "panel1_tool_r3"sv},
    {XSetName::PANEL1_TOOL_S_3,             "panel1_tool_s3"sv},

    // panel2

    // panel2 mode 0
    {XSetName::PANEL2_SHOW_TOOLBOX_0,       "panel2_show_toolbox0"sv},
    {XSetName::PANEL2_SHOW_DEVMON_0,        "panel2_show_devmon0"sv},
    {XSetName::PANEL2_SHOW_DIRTREE_0,       "panel2_show_dirtree0"sv},
    {XSetName::PANEL2_SHOW_SIDEBAR_0,       "panel2_show_sidebar0"sv},
    {XSetName::PANEL2_SLIDER_POSITIONS_0,   "panel2_slider_positions0"sv},
    {XSetName::PANEL2_LIST_DETAILED_0,      "panel2_list_detailed0"sv},
    {XSetName::PANEL2_LIST_ICONS_0,         "panel2_list_icons0"sv},
    {XSetName::PANEL2_LIST_COMPACT_0,       "panel2_list_compact0"sv},
    {XSetName::PANEL2_LIST_LARGE_0,         "panel2_list_large0"sv},
    {XSetName::PANEL2_SHOW_HIDDEN_0,        "panel2_show_hidden0"sv},
    {XSetName::PANEL2_ICON_TAB_0,           "panel2_icon_tab0"sv},
    {XSetName::PANEL2_ICON_STATUS_0,        "panel2_icon_status0"sv},
    {XSetName::PANEL2_DETCOL_NAME_0,        "panel2_detcol_name0"sv},
    {XSetName::PANEL2_DETCOL_SIZE_0,        "panel2_detcol_size0"sv},
    {XSetName::PANEL2_DETCOL_TYPE_0,        "panel2_detcol_type0"sv},
    {XSetName::PANEL2_DETCOL_PERM_0,        "panel2_detcol_perm0"sv},
    {XSetName::PANEL2_DETCOL_OWNER_0,       "panel2_detcol_owner0"sv},
    {XSetName::PANEL2_DETCOL_DATE_0,        "panel2_detcol_date0"sv},
    {XSetName::PANEL2_SORT_EXTRA_0,         "panel2_sort_extra0"sv},
    {XSetName::PANEL2_BOOK_FOL_0,           "panel2_book_fol0"sv},
    {XSetName::PANEL2_TOOL_L_0,             "panel2_tool_l0"sv},
    {XSetName::PANEL2_TOOL_R_0,             "panel2_tool_r0"sv},
    {XSetName::PANEL2_TOOL_S_0,             "panel2_tool_s0"sv},

    // panel2 mode 1
    {XSetName::PANEL2_SHOW_TOOLBOX_1,       "panel2_show_toolbox1"sv},
    {XSetName::PANEL2_SHOW_DEVMON_1,        "panel2_show_devmon1"sv},
    {XSetName::PANEL2_SHOW_DIRTREE_1,       "panel2_show_dirtree1"sv},
    {XSetName::PANEL2_SHOW_SIDEBAR_1,       "panel2_show_sidebar1"sv},
    {XSetName::PANEL2_SLIDER_POSITIONS_1,   "panel2_slider_positions1"sv},
    {XSetName::PANEL2_LIST_DETAILED_1,      "panel2_list_detailed1"sv},
    {XSetName::PANEL2_LIST_ICONS_1,         "panel2_list_icons1"sv},
    {XSetName::PANEL2_LIST_COMPACT_1,       "panel2_list_compact1"sv},
    {XSetName::PANEL2_LIST_LARGE_1,         "panel2_list_large1"sv},
    {XSetName::PANEL2_SHOW_HIDDEN_1,        "panel2_show_hidden1"sv},
    {XSetName::PANEL2_ICON_TAB_1,           "panel2_icon_tab1"sv},
    {XSetName::PANEL2_ICON_STATUS_1,        "panel2_icon_status1"sv},
    {XSetName::PANEL2_DETCOL_NAME_1,        "panel2_detcol_name1"sv},
    {XSetName::PANEL2_DETCOL_SIZE_1,        "panel2_detcol_size1"sv},
    {XSetName::PANEL2_DETCOL_TYPE_1,        "panel2_detcol_type1"sv},
    {XSetName::PANEL2_DETCOL_PERM_1,        "panel2_detcol_perm1"sv},
    {XSetName::PANEL2_DETCOL_OWNER_1,       "panel2_detcol_owner1"sv},
    {XSetName::PANEL2_DETCOL_DATE_1,        "panel2_detcol_date1"sv},
    {XSetName::PANEL2_SORT_EXTRA_1,         "panel2_sort_extra1"sv},
    {XSetName::PANEL2_BOOK_FOL_1,           "panel2_book_fol1"sv},
    {XSetName::PANEL2_TOOL_L_1,             "panel2_tool_l1"sv},
    {XSetName::PANEL2_TOOL_R_1,             "panel2_tool_r1"sv},
    {XSetName::PANEL2_TOOL_S_1,             "panel2_tool_s1"sv},

    // panel2 mode 2
    {XSetName::PANEL2_SHOW_TOOLBOX_2,       "panel2_show_toolbox2"sv},
    {XSetName::PANEL2_SHOW_DEVMON_2,        "panel2_show_devmon2"sv},
    {XSetName::PANEL2_SHOW_DIRTREE_2,       "panel2_show_dirtree2"sv},
    {XSetName::PANEL2_SHOW_SIDEBAR_2,       "panel2_show_sidebar2"sv},
    {XSetName::PANEL2_SLIDER_POSITIONS_2,   "panel2_slider_positions2"sv},
    {XSetName::PANEL2_LIST_DETAILED_2,      "panel2_list_detailed2"sv},
    {XSetName::PANEL2_LIST_ICONS_2,         "panel2_list_icons2"sv},
    {XSetName::PANEL2_LIST_COMPACT_2,       "panel2_list_compact2"sv},
    {XSetName::PANEL2_LIST_LARGE_2,         "panel2_list_large2"sv},
    {XSetName::PANEL2_SHOW_HIDDEN_2,        "panel2_show_hidden2"sv},
    {XSetName::PANEL2_ICON_TAB_2,           "panel2_icon_tab2"sv},
    {XSetName::PANEL2_ICON_STATUS_2,        "panel2_icon_status2"sv},
    {XSetName::PANEL2_DETCOL_NAME_2,        "panel2_detcol_name2"sv},
    {XSetName::PANEL2_DETCOL_SIZE_2,        "panel2_detcol_size2"sv},
    {XSetName::PANEL2_DETCOL_TYPE_2,        "panel2_detcol_type2"sv},
    {XSetName::PANEL2_DETCOL_PERM_2,        "panel2_detcol_perm2"sv},
    {XSetName::PANEL2_DETCOL_OWNER_2,       "panel2_detcol_owner2"sv},
    {XSetName::PANEL2_DETCOL_DATE_2,        "panel2_detcol_date2"sv},
    {XSetName::PANEL2_SORT_EXTRA_2,         "panel2_sort_extra2"sv},
    {XSetName::PANEL2_BOOK_FOL_2,           "panel2_book_fol2"sv},
    {XSetName::PANEL2_TOOL_L_2,             "panel2_tool_l2"sv},
    {XSetName::PANEL2_TOOL_R_2,             "panel2_tool_r2"}  ,
    {XSetName::PANEL2_TOOL_S_2,             "panel2_tool_s2"}  ,

    // panel2 mode 3
    {XSetName::PANEL2_SHOW_TOOLBOX_3,       "panel2_show_toolbox3"sv},
    {XSetName::PANEL2_SHOW_DEVMON_3,        "panel2_show_devmon3"sv},
    {XSetName::PANEL2_SHOW_DIRTREE_3,       "panel2_show_dirtree3"sv},
    {XSetName::PANEL2_SHOW_SIDEBAR_3,       "panel2_show_sidebar3"sv},
    {XSetName::PANEL2_SLIDER_POSITIONS_3,   "panel2_slider_positions3"sv},
    {XSetName::PANEL2_LIST_DETAILED_3,      "panel2_list_detailed3"sv},
    {XSetName::PANEL2_LIST_ICONS_3,         "panel2_list_icons3"sv},
    {XSetName::PANEL2_LIST_COMPACT_3,       "panel2_list_compact3"sv},
    {XSetName::PANEL2_LIST_LARGE_3,         "panel2_list_large3"sv},
    {XSetName::PANEL2_SHOW_HIDDEN_3,        "panel2_show_hidden3"sv},
    {XSetName::PANEL2_ICON_TAB_3,           "panel2_icon_tab3"sv},
    {XSetName::PANEL2_ICON_STATUS_3,        "panel2_icon_status3"sv},
    {XSetName::PANEL2_DETCOL_NAME_3,        "panel2_detcol_name3"sv},
    {XSetName::PANEL2_DETCOL_SIZE_3,        "panel2_detcol_size3"sv},
    {XSetName::PANEL2_DETCOL_TYPE_3,        "panel2_detcol_type3"sv},
    {XSetName::PANEL2_DETCOL_PERM_3,        "panel2_detcol_perm3"sv},
    {XSetName::PANEL2_DETCOL_OWNER_3,       "panel2_detcol_owner3"sv},
    {XSetName::PANEL2_DETCOL_DATE_3,        "panel2_detcol_date3"sv},
    {XSetName::PANEL2_SORT_EXTRA_3,         "panel2_sort_extra3"sv},
    {XSetName::PANEL2_BOOK_FOL_3,           "panel2_book_fol3"sv},
    {XSetName::PANEL2_TOOL_L_3,             "panel2_tool_l3"sv},
    {XSetName::PANEL2_TOOL_R_3,             "panel2_tool_r3"sv},
    {XSetName::PANEL2_TOOL_S_3,             "panel2_tool_s3"sv},

    // panel3

    // panel3 mode 0
    {XSetName::PANEL3_SHOW_TOOLBOX_0,       "panel3_show_toolbox0"sv},
    {XSetName::PANEL3_SHOW_DEVMON_0,        "panel3_show_devmon0"sv},
    {XSetName::PANEL3_SHOW_DIRTREE_0,       "panel3_show_dirtree0"sv},
    {XSetName::PANEL3_SHOW_SIDEBAR_0,       "panel3_show_sidebar0"sv},
    {XSetName::PANEL3_SLIDER_POSITIONS_0,   "panel3_slider_positions0"sv},
    {XSetName::PANEL3_LIST_DETAILED_0,      "panel3_list_detailed0"sv},
    {XSetName::PANEL3_LIST_ICONS_0,         "panel3_list_icons0"sv},
    {XSetName::PANEL3_LIST_COMPACT_0,       "panel3_list_compact0"sv},
    {XSetName::PANEL3_LIST_LARGE_0,         "panel3_list_large0"sv},
    {XSetName::PANEL3_SHOW_HIDDEN_0,        "panel3_show_hidden0"sv},
    {XSetName::PANEL3_ICON_TAB_0,           "panel3_icon_tab0"sv},
    {XSetName::PANEL3_ICON_STATUS_0,        "panel3_icon_status0"sv},
    {XSetName::PANEL3_DETCOL_NAME_0,        "panel3_detcol_name0"sv},
    {XSetName::PANEL3_DETCOL_SIZE_0,        "panel3_detcol_size0"sv},
    {XSetName::PANEL3_DETCOL_TYPE_0,        "panel3_detcol_type0"sv},
    {XSetName::PANEL3_DETCOL_PERM_0,        "panel3_detcol_perm0"sv},
    {XSetName::PANEL3_DETCOL_OWNER_0,       "panel3_detcol_owner0"sv},
    {XSetName::PANEL3_DETCOL_DATE_0,        "panel3_detcol_date0"sv},
    {XSetName::PANEL3_SORT_EXTRA_0,         "panel3_sort_extra0"sv},
    {XSetName::PANEL3_BOOK_FOL_0,           "panel3_book_fol0"sv},
    {XSetName::PANEL3_TOOL_L_0,             "panel3_tool_l0"sv},
    {XSetName::PANEL3_TOOL_R_0,             "panel3_tool_r0"sv},
    {XSetName::PANEL3_TOOL_S_0,             "panel3_tool_s0"sv},

    // panel3 mode 1
    {XSetName::PANEL3_SHOW_TOOLBOX_1,       "panel3_show_toolbox1"sv},
    {XSetName::PANEL3_SHOW_DEVMON_1,        "panel3_show_devmon1"sv},
    {XSetName::PANEL3_SHOW_DIRTREE_1,       "panel3_show_dirtree1"sv},
    {XSetName::PANEL3_SHOW_SIDEBAR_1,       "panel3_show_sidebar1"sv},
    {XSetName::PANEL3_SLIDER_POSITIONS_1,   "panel3_slider_positions1"sv},
    {XSetName::PANEL3_LIST_DETAILED_1,      "panel3_list_detailed1"sv},
    {XSetName::PANEL3_LIST_ICONS_1,         "panel3_list_icons1"sv},
    {XSetName::PANEL3_LIST_COMPACT_1,       "panel3_list_compact1"sv},
    {XSetName::PANEL3_LIST_LARGE_1,         "panel3_list_large1"sv},
    {XSetName::PANEL3_SHOW_HIDDEN_1,        "panel3_show_hidden1"sv},
    {XSetName::PANEL3_ICON_TAB_1,           "panel3_icon_tab1"sv},
    {XSetName::PANEL3_ICON_STATUS_1,        "panel3_icon_status1"sv},
    {XSetName::PANEL3_DETCOL_NAME_1,        "panel3_detcol_name1"sv},
    {XSetName::PANEL3_DETCOL_SIZE_1,        "panel3_detcol_size1"sv},
    {XSetName::PANEL3_DETCOL_TYPE_1,        "panel3_detcol_type1"sv},
    {XSetName::PANEL3_DETCOL_PERM_1,        "panel3_detcol_perm1"sv},
    {XSetName::PANEL3_DETCOL_OWNER_1,       "panel3_detcol_owner1"sv},
    {XSetName::PANEL3_DETCOL_DATE_1,        "panel3_detcol_date1"sv},
    {XSetName::PANEL3_SORT_EXTRA_1,         "panel3_sort_extra1"sv},
    {XSetName::PANEL3_BOOK_FOL_1,           "panel3_book_fol1"sv},
    {XSetName::PANEL3_TOOL_L_1,             "panel3_tool_l1"sv},
    {XSetName::PANEL3_TOOL_R_1,             "panel3_tool_r1"sv},
    {XSetName::PANEL3_TOOL_S_1,             "panel3_tool_s1"sv},

    // panel3 mode 2
    {XSetName::PANEL3_SHOW_TOOLBOX_2,       "panel3_show_toolbox2"sv},
    {XSetName::PANEL3_SHOW_DEVMON_2,        "panel3_show_devmon2"sv},
    {XSetName::PANEL3_SHOW_DIRTREE_2,       "panel3_show_dirtree2"sv},
    {XSetName::PANEL3_SHOW_SIDEBAR_2,       "panel3_show_sidebar2"sv},
    {XSetName::PANEL3_SLIDER_POSITIONS_2,   "panel3_slider_positions2"sv},
    {XSetName::PANEL3_LIST_DETAILED_2,      "panel3_list_detailed2"sv},
    {XSetName::PANEL3_LIST_ICONS_2,         "panel3_list_icons2"sv},
    {XSetName::PANEL3_LIST_COMPACT_2,       "panel3_list_compact2"sv},
    {XSetName::PANEL3_LIST_LARGE_2,         "panel3_list_large2"sv},
    {XSetName::PANEL3_SHOW_HIDDEN_2,        "panel3_show_hidden2"sv},
    {XSetName::PANEL3_ICON_TAB_2,           "panel3_icon_tab2"sv},
    {XSetName::PANEL3_ICON_STATUS_2,        "panel3_icon_status2"sv},
    {XSetName::PANEL3_DETCOL_NAME_2,        "panel3_detcol_name2"sv},
    {XSetName::PANEL3_DETCOL_SIZE_2,        "panel3_detcol_size2"sv},
    {XSetName::PANEL3_DETCOL_TYPE_2,        "panel3_detcol_type2"sv},
    {XSetName::PANEL3_DETCOL_PERM_2,        "panel3_detcol_perm2"sv},
    {XSetName::PANEL3_DETCOL_OWNER_2,       "panel3_detcol_owner2"sv},
    {XSetName::PANEL3_DETCOL_DATE_2,        "panel3_detcol_date2"sv},
    {XSetName::PANEL3_SORT_EXTRA_2,         "panel3_sort_extra2"sv},
    {XSetName::PANEL3_BOOK_FOL_2,           "panel3_book_fol2"sv},
    {XSetName::PANEL3_TOOL_L_2,             "panel3_tool_l2"sv},
    {XSetName::PANEL3_TOOL_R_2,             "panel3_tool_r2"sv},
    {XSetName::PANEL3_TOOL_S_2,             "panel3_tool_s2"sv},

    // panel13 mode 3
    {XSetName::PANEL3_SHOW_TOOLBOX_3,       "panel3_show_toolbox3"sv},
    {XSetName::PANEL3_SHOW_DEVMON_3,        "panel3_show_devmon3"sv},
    {XSetName::PANEL3_SHOW_DIRTREE_3,       "panel3_show_dirtree3"sv},
    {XSetName::PANEL3_SHOW_SIDEBAR_3,       "panel3_show_sidebar3"sv},
    {XSetName::PANEL3_SLIDER_POSITIONS_3,   "panel3_slider_positions3"sv},
    {XSetName::PANEL3_LIST_DETAILED_3,      "panel3_list_detailed3"sv},
    {XSetName::PANEL3_LIST_ICONS_3,         "panel3_list_icons3"sv},
    {XSetName::PANEL3_LIST_COMPACT_3,       "panel3_list_compact3"sv},
    {XSetName::PANEL3_LIST_LARGE_3,         "panel3_list_large3"sv},
    {XSetName::PANEL3_SHOW_HIDDEN_3,        "panel3_show_hidden3"sv},
    {XSetName::PANEL3_ICON_TAB_3,           "panel3_icon_tab3"sv},
    {XSetName::PANEL3_ICON_STATUS_3,        "panel3_icon_status3"sv},
    {XSetName::PANEL3_DETCOL_NAME_3,        "panel3_detcol_name3"sv},
    {XSetName::PANEL3_DETCOL_SIZE_3,        "panel3_detcol_size3"sv},
    {XSetName::PANEL3_DETCOL_TYPE_3,        "panel3_detcol_type3"sv},
    {XSetName::PANEL3_DETCOL_PERM_3,        "panel3_detcol_perm3"sv},
    {XSetName::PANEL3_DETCOL_OWNER_3,       "panel3_detcol_owner3"sv},
    {XSetName::PANEL3_DETCOL_DATE_3,        "panel3_detcol_date3"sv},
    {XSetName::PANEL3_SORT_EXTRA_3,         "panel3_sort_extra3"sv},
    {XSetName::PANEL3_BOOK_FOL_3,           "panel3_book_fol3"sv},
    {XSetName::PANEL3_TOOL_L_3,             "panel3_tool_l3"sv},
    {XSetName::PANEL3_TOOL_R_3,             "panel3_tool_r3"sv},
    {XSetName::PANEL3_TOOL_S_3,             "panel3_tool_s3"sv},

    // panel4

    // panel4 mode 0
    {XSetName::PANEL4_SHOW_TOOLBOX_0,       "panel4_show_toolbox0"sv},
    {XSetName::PANEL4_SHOW_DEVMON_0,        "panel4_show_devmon0"sv},
    {XSetName::PANEL4_SHOW_DIRTREE_0,       "panel4_show_dirtree0"sv},
    {XSetName::PANEL4_SHOW_SIDEBAR_0,       "panel4_show_sidebar0"sv},
    {XSetName::PANEL4_SLIDER_POSITIONS_0,   "panel4_slider_positions0"sv},
    {XSetName::PANEL4_LIST_DETAILED_0,      "panel4_list_detailed0"sv},
    {XSetName::PANEL4_LIST_ICONS_0,         "panel4_list_icons0"sv},
    {XSetName::PANEL4_LIST_COMPACT_0,       "panel4_list_compact0"sv},
    {XSetName::PANEL4_LIST_LARGE_0,         "panel4_list_large0"sv},
    {XSetName::PANEL4_SHOW_HIDDEN_0,        "panel4_show_hidden0"sv},
    {XSetName::PANEL4_ICON_TAB_0,           "panel4_icon_tab0"sv},
    {XSetName::PANEL4_ICON_STATUS_0,        "panel4_icon_status0"sv},
    {XSetName::PANEL4_DETCOL_NAME_0,        "panel4_detcol_name0"sv},
    {XSetName::PANEL4_DETCOL_SIZE_0,        "panel4_detcol_size0"sv},
    {XSetName::PANEL4_DETCOL_TYPE_0,        "panel4_detcol_type0"sv},
    {XSetName::PANEL4_DETCOL_PERM_0,        "panel4_detcol_perm0"sv},
    {XSetName::PANEL4_DETCOL_OWNER_0,       "panel4_detcol_owner0"sv},
    {XSetName::PANEL4_DETCOL_DATE_0,        "panel4_detcol_date0"sv},
    {XSetName::PANEL4_SORT_EXTRA_0,         "panel4_sort_extra0"sv},
    {XSetName::PANEL4_BOOK_FOL_0,           "panel4_book_fol0"sv},
    {XSetName::PANEL4_TOOL_L_0,             "panel4_tool_l0"sv},
    {XSetName::PANEL4_TOOL_R_0,             "panel4_tool_r0"sv},
    {XSetName::PANEL4_TOOL_S_0,             "panel4_tool_s0"sv},

    // panel4 mode 1
    {XSetName::PANEL4_SHOW_TOOLBOX_1,       "panel4_show_toolbox1"sv},
    {XSetName::PANEL4_SHOW_DEVMON_1,        "panel4_show_devmon1"sv},
    {XSetName::PANEL4_SHOW_DIRTREE_1,       "panel4_show_dirtree1"sv},
    {XSetName::PANEL4_SHOW_SIDEBAR_1,       "panel4_show_sidebar1"sv},
    {XSetName::PANEL4_SLIDER_POSITIONS_1,   "panel4_slider_positions1"sv},
    {XSetName::PANEL4_LIST_DETAILED_1,      "panel4_list_detailed1"sv},
    {XSetName::PANEL4_LIST_ICONS_1,         "panel4_list_icons1"sv},
    {XSetName::PANEL4_LIST_COMPACT_1,       "panel4_list_compact1"sv},
    {XSetName::PANEL4_LIST_LARGE_1,         "panel4_list_large1"sv},
    {XSetName::PANEL4_SHOW_HIDDEN_1,        "panel4_show_hidden1"sv},
    {XSetName::PANEL4_ICON_TAB_1,           "panel4_icon_tab1"sv},
    {XSetName::PANEL4_ICON_STATUS_1,        "panel4_icon_status1"sv},
    {XSetName::PANEL4_DETCOL_NAME_1,        "panel4_detcol_name1"sv},
    {XSetName::PANEL4_DETCOL_SIZE_1,        "panel4_detcol_size1"sv},
    {XSetName::PANEL4_DETCOL_TYPE_1,        "panel4_detcol_type1"sv},
    {XSetName::PANEL4_DETCOL_PERM_1,        "panel4_detcol_perm1"sv},
    {XSetName::PANEL4_DETCOL_OWNER_1,       "panel4_detcol_owner1"sv},
    {XSetName::PANEL4_DETCOL_DATE_1,        "panel4_detcol_date1"sv},
    {XSetName::PANEL4_SORT_EXTRA_1,         "panel4_sort_extra1"sv},
    {XSetName::PANEL4_BOOK_FOL_1,           "panel4_book_fol1"sv},
    {XSetName::PANEL4_TOOL_L_1,             "panel4_tool_l1"sv},
    {XSetName::PANEL4_TOOL_R_1,             "panel4_tool_r1"sv},
    {XSetName::PANEL4_TOOL_S_1,             "panel4_tool_s1"sv},

    // panel4 mode 2
    {XSetName::PANEL4_SHOW_TOOLBOX_2,       "panel4_show_toolbox2"sv},
    {XSetName::PANEL4_SHOW_DEVMON_2,        "panel4_show_devmon2"sv},
    {XSetName::PANEL4_SHOW_DIRTREE_2,       "panel4_show_dirtree2"sv},
    {XSetName::PANEL4_SHOW_SIDEBAR_2,       "panel4_show_sidebar2"sv},
    {XSetName::PANEL4_SLIDER_POSITIONS_2,   "panel4_slider_positions2"sv},
    {XSetName::PANEL4_LIST_DETAILED_2,      "panel4_list_detailed2"sv},
    {XSetName::PANEL4_LIST_ICONS_2,         "panel4_list_icons2"sv},
    {XSetName::PANEL4_LIST_COMPACT_2,       "panel4_list_compact2"sv},
    {XSetName::PANEL4_LIST_LARGE_2,         "panel4_list_large2"sv},
    {XSetName::PANEL4_SHOW_HIDDEN_2,        "panel4_show_hidden2"sv},
    {XSetName::PANEL4_ICON_TAB_2,           "panel4_icon_tab2"sv},
    {XSetName::PANEL4_ICON_STATUS_2,        "panel4_icon_status2"sv},
    {XSetName::PANEL4_DETCOL_NAME_2,        "panel4_detcol_name2"sv},
    {XSetName::PANEL4_DETCOL_SIZE_2,        "panel4_detcol_size2"sv},
    {XSetName::PANEL4_DETCOL_TYPE_2,        "panel4_detcol_type2"sv},
    {XSetName::PANEL4_DETCOL_PERM_2,        "panel4_detcol_perm2"sv},
    {XSetName::PANEL4_DETCOL_OWNER_2,       "panel4_detcol_owner2"sv},
    {XSetName::PANEL4_DETCOL_DATE_2,        "panel4_detcol_date2"sv},
    {XSetName::PANEL4_SORT_EXTRA_2,         "panel4_sort_extra2"sv},
    {XSetName::PANEL4_BOOK_FOL_2,           "panel4_book_fol2"sv},
    {XSetName::PANEL4_TOOL_L_2,             "panel4_tool_l2"sv},
    {XSetName::PANEL4_TOOL_R_2,             "panel4_tool_r2"sv},
    {XSetName::PANEL4_TOOL_S_2,             "panel4_tool_s2"sv},

    // panel4 mode 3
    {XSetName::PANEL4_SHOW_TOOLBOX_3,       "panel4_show_toolbox3"sv},
    {XSetName::PANEL4_SHOW_DEVMON_3,        "panel4_show_devmon3"sv},
    {XSetName::PANEL4_SHOW_DIRTREE_3,       "panel4_show_dirtree3"sv},
    {XSetName::PANEL4_SHOW_SIDEBAR_3,       "panel4_show_sidebar3"sv},
    {XSetName::PANEL4_SLIDER_POSITIONS_3,   "panel4_slider_positions3"sv},
    {XSetName::PANEL4_LIST_DETAILED_3,      "panel4_list_detailed3"sv},
    {XSetName::PANEL4_LIST_ICONS_3,         "panel4_list_icons3"sv},
    {XSetName::PANEL4_LIST_COMPACT_3,       "panel4_list_compact3"sv},
    {XSetName::PANEL4_LIST_LARGE_3,         "panel4_list_large3"sv},
    {XSetName::PANEL4_SHOW_HIDDEN_3,        "panel4_show_hidden3"sv},
    {XSetName::PANEL4_ICON_TAB_3,           "panel4_icon_tab3"sv},
    {XSetName::PANEL4_ICON_STATUS_3,        "panel4_icon_status3"sv},
    {XSetName::PANEL4_DETCOL_NAME_3,        "panel4_detcol_name3"sv},
    {XSetName::PANEL4_DETCOL_SIZE_3,        "panel4_detcol_size3"sv},
    {XSetName::PANEL4_DETCOL_TYPE_3,        "panel4_detcol_type3"sv},
    {XSetName::PANEL4_DETCOL_PERM_3,        "panel4_detcol_perm3"sv},
    {XSetName::PANEL4_DETCOL_OWNER_3,       "panel4_detcol_owner3"sv},
    {XSetName::PANEL4_DETCOL_DATE_3,        "panel4_detcol_date3"sv},
    {XSetName::PANEL4_SORT_EXTRA_3,         "panel4_sort_extra3"sv},
    {XSetName::PANEL4_BOOK_FOL_3,           "panel4_book_fol3"sv},
    {XSetName::PANEL4_TOOL_L_3,             "panel4_tool_l3"sv},
    {XSetName::PANEL4_TOOL_R_3,             "panel4_tool_r3"sv},
    {XSetName::PANEL4_TOOL_S_3,             "panel4_tool_s3"sv},

    // speed
    {XSetName::BOOK_NEWTAB,                 "book_newtab"sv},
    {XSetName::BOOK_SINGLE,                 "book_single"sv},
    {XSetName::DEV_NEWTAB,                  "dev_newtab"sv},
    {XSetName::DEV_SINGLE,                  "dev_single"sv},

    // dialog
    {XSetName::APP_DLG,                     "app_dlg"sv},
    {XSetName::CONTEXT_DLG,                 "context_dlg"sv},
    {XSetName::FILE_DLG,                    "file_dlg"sv},
    {XSetName::TEXT_DLG,                    "text_dlg"sv},

    // other
    {XSetName::BOOK_NEW,                    "book_new"sv},
    {XSetName::DRAG_ACTION,                 "drag_action"sv},
    {XSetName::EDITOR,                      "editor"sv},
    {XSetName::ROOT_EDITOR,                 "root_editor"sv},
    {XSetName::SU_COMMAND,                  "su_command"sv},

    // HANDLERS //

    // handlers arc
    {XSetName::HAND_ARC_7Z,                 "hand_arc_+7z"sv},
    {XSetName::HAND_ARC_GZ,                 "hand_arc_+gz"sv},
    {XSetName::HAND_ARC_RAR,                "hand_arc_+rar"sv},
    {XSetName::HAND_ARC_TAR,                "hand_arc_+tar"sv},
    {XSetName::HAND_ARC_TAR_BZ2,            "hand_arc_+tar_bz2"sv},
    {XSetName::HAND_ARC_TAR_GZ,             "hand_arc_+tar_gz"sv},
    {XSetName::HAND_ARC_TAR_LZ4,            "hand_arc_+tar_lz4"sv},
    {XSetName::HAND_ARC_TAR_XZ,             "hand_arc_+tar_xz"sv},
    {XSetName::HAND_ARC_TAR_ZST,            "hand_arc_+tar_zst"sv},
    {XSetName::HAND_ARC_LZ4,                "hand_arc_+lz4"sv},
    {XSetName::HAND_ARC_XZ,                 "hand_arc_+xz"sv},
    {XSetName::HAND_ARC_ZIP,                "hand_arc_+zip"sv},
    {XSetName::HAND_ARC_ZST,                "hand_arc_+zst"sv},

    // handlers file
    {XSetName::HAND_F_ISO,                  "hand_f_+iso"sv},

    // handlers fs
    {XSetName::HAND_FS_DEF,                 "hand_fs_+def"sv},
    {XSetName::HAND_FS_FUSEISO,             "hand_fs_+fuseiso"sv},
    {XSetName::HAND_FS_UDISO,               "hand_fs_+udiso"sv},

    // handlers net
    {XSetName::HAND_NET_FTP,                "hand_net_+ftp"sv},
    {XSetName::HAND_NET_FUSE,               "hand_net_+fuse"sv},
    {XSetName::HAND_NET_FUSESMB,            "hand_net_+fusesmb"sv},
    {XSetName::HAND_NET_GPHOTO,             "hand_net_+gphoto"sv},
    {XSetName::HAND_NET_HTTP,               "hand_net_+http"sv},
    {XSetName::HAND_NET_IFUSE,              "hand_net_+ifuse"sv},
    {XSetName::HAND_NET_MTP,                "hand_net_+mtp"sv},
    {XSetName::HAND_NET_SSH,                "hand_net_+ssh"sv},
    {XSetName::HAND_NET_UDEVIL,             "hand_net_+udevil"sv},
    {XSetName::HAND_NET_UDEVILSMB,          "hand_net_+udevilsmb"sv},

    // other
    {XSetName::TOOL_L,                      "tool_l"sv},
    {XSetName::TOOL_R,                      "tool_r"sv},
    {XSetName::TOOL_S,                      "tool_s"sv},
};

// panels

// panel1
static const std::unordered_map<XSetPanel, XSetName> xset_panel1_map{
    // panel1
    {XSetPanel::SHOW,             XSetName::PANEL1_SHOW},
    {XSetPanel::SHOW_TOOLBOX,     XSetName::PANEL1_SHOW_TOOLBOX},
    {XSetPanel::SHOW_DEVMON,      XSetName::PANEL1_SHOW_DEVMON},
    {XSetPanel::SHOW_DIRTREE,     XSetName::PANEL1_SHOW_DIRTREE},
    {XSetPanel::SHOW_SIDEBAR,     XSetName::PANEL1_SHOW_SIDEBAR},
    {XSetPanel::SLIDER_POSITIONS, XSetName::PANEL1_SLIDER_POSITIONS},
    {XSetPanel::LIST_DETAILED,    XSetName::PANEL1_LIST_DETAILED},
    {XSetPanel::LIST_ICONS,       XSetName::PANEL1_LIST_ICONS},
    {XSetPanel::LIST_COMPACT,     XSetName::PANEL1_LIST_COMPACT},
    {XSetPanel::LIST_LARGE,       XSetName::PANEL1_LIST_LARGE},
    {XSetPanel::SHOW_HIDDEN,      XSetName::PANEL1_SHOW_HIDDEN},
    {XSetPanel::ICON_TAB,         XSetName::PANEL1_ICON_TAB},
    {XSetPanel::ICON_STATUS,      XSetName::PANEL1_ICON_STATUS},
    {XSetPanel::DETCOL_NAME,      XSetName::PANEL1_DETCOL_NAME},
    {XSetPanel::DETCOL_SIZE,      XSetName::PANEL1_DETCOL_SIZE},
    {XSetPanel::DETCOL_TYPE,      XSetName::PANEL1_DETCOL_TYPE},
    {XSetPanel::DETCOL_PERM,      XSetName::PANEL1_DETCOL_PERM},
    {XSetPanel::DETCOL_OWNER,     XSetName::PANEL1_DETCOL_OWNER},
    {XSetPanel::DETCOL_DATE,      XSetName::PANEL1_DETCOL_DATE},
    {XSetPanel::SORT_EXTRA,       XSetName::PANEL1_SORT_EXTRA},
    {XSetPanel::BOOK_FOL,         XSetName::PANEL1_BOOK_FOL},
    {XSetPanel::TOOL_L,           XSetName::PANEL1_TOOL_L},
    {XSetPanel::TOOL_R,           XSetName::PANEL1_TOOL_R},
    {XSetPanel::TOOL_S,           XSetName::PANEL1_TOOL_S},
};

// panel2
static const std::unordered_map<XSetPanel, XSetName> xset_panel2_map{
    // panel2
    {XSetPanel::SHOW,             XSetName::PANEL2_SHOW},
    {XSetPanel::SHOW_TOOLBOX,     XSetName::PANEL2_SHOW_TOOLBOX},
    {XSetPanel::SHOW_DEVMON,      XSetName::PANEL2_SHOW_DEVMON},
    {XSetPanel::SHOW_DIRTREE,     XSetName::PANEL2_SHOW_DIRTREE},
    {XSetPanel::SHOW_SIDEBAR,     XSetName::PANEL2_SHOW_SIDEBAR},
    {XSetPanel::SLIDER_POSITIONS, XSetName::PANEL2_SLIDER_POSITIONS},
    {XSetPanel::LIST_DETAILED,    XSetName::PANEL2_LIST_DETAILED},
    {XSetPanel::LIST_ICONS,       XSetName::PANEL2_LIST_ICONS},
    {XSetPanel::LIST_COMPACT,     XSetName::PANEL2_LIST_COMPACT},
    {XSetPanel::LIST_LARGE,       XSetName::PANEL2_LIST_LARGE},
    {XSetPanel::SHOW_HIDDEN,      XSetName::PANEL2_SHOW_HIDDEN},
    {XSetPanel::ICON_TAB,         XSetName::PANEL2_ICON_TAB},
    {XSetPanel::ICON_STATUS,      XSetName::PANEL2_ICON_STATUS},
    {XSetPanel::DETCOL_NAME,      XSetName::PANEL2_DETCOL_NAME},
    {XSetPanel::DETCOL_SIZE,      XSetName::PANEL2_DETCOL_SIZE},
    {XSetPanel::DETCOL_TYPE,      XSetName::PANEL2_DETCOL_TYPE},
    {XSetPanel::DETCOL_PERM,      XSetName::PANEL2_DETCOL_PERM},
    {XSetPanel::DETCOL_OWNER,     XSetName::PANEL2_DETCOL_OWNER},
    {XSetPanel::DETCOL_DATE,      XSetName::PANEL2_DETCOL_DATE},
    {XSetPanel::SORT_EXTRA,       XSetName::PANEL2_SORT_EXTRA},
    {XSetPanel::BOOK_FOL,         XSetName::PANEL2_BOOK_FOL},
    {XSetPanel::TOOL_L,           XSetName::PANEL2_TOOL_L},
    {XSetPanel::TOOL_R,           XSetName::PANEL2_TOOL_R},
    {XSetPanel::TOOL_S,           XSetName::PANEL2_TOOL_S},
};

// panel3
static const std::unordered_map<XSetPanel, XSetName> xset_panel3_map{
    // panel3
    {XSetPanel::SHOW,             XSetName::PANEL3_SHOW},
    {XSetPanel::SHOW_TOOLBOX,     XSetName::PANEL3_SHOW_TOOLBOX},
    {XSetPanel::SHOW_DEVMON,      XSetName::PANEL3_SHOW_DEVMON},
    {XSetPanel::SHOW_DIRTREE,     XSetName::PANEL3_SHOW_DIRTREE},
    {XSetPanel::SHOW_SIDEBAR,     XSetName::PANEL3_SHOW_SIDEBAR},
    {XSetPanel::SLIDER_POSITIONS, XSetName::PANEL3_SLIDER_POSITIONS},
    {XSetPanel::LIST_DETAILED,    XSetName::PANEL3_LIST_DETAILED},
    {XSetPanel::LIST_ICONS,       XSetName::PANEL3_LIST_ICONS},
    {XSetPanel::LIST_COMPACT,     XSetName::PANEL3_LIST_COMPACT},
    {XSetPanel::LIST_LARGE,       XSetName::PANEL3_LIST_LARGE},
    {XSetPanel::SHOW_HIDDEN,      XSetName::PANEL3_SHOW_HIDDEN},
    {XSetPanel::ICON_TAB,         XSetName::PANEL3_ICON_TAB},
    {XSetPanel::ICON_STATUS,      XSetName::PANEL3_ICON_STATUS},
    {XSetPanel::DETCOL_NAME,      XSetName::PANEL3_DETCOL_NAME},
    {XSetPanel::DETCOL_SIZE,      XSetName::PANEL3_DETCOL_SIZE},
    {XSetPanel::DETCOL_TYPE,      XSetName::PANEL3_DETCOL_TYPE},
    {XSetPanel::DETCOL_PERM,      XSetName::PANEL3_DETCOL_PERM},
    {XSetPanel::DETCOL_OWNER,     XSetName::PANEL3_DETCOL_OWNER},
    {XSetPanel::DETCOL_DATE,      XSetName::PANEL3_DETCOL_DATE},
    {XSetPanel::SORT_EXTRA,       XSetName::PANEL3_SORT_EXTRA},
    {XSetPanel::BOOK_FOL,         XSetName::PANEL3_BOOK_FOL},
    {XSetPanel::TOOL_L,           XSetName::PANEL3_TOOL_L},
    {XSetPanel::TOOL_R,           XSetName::PANEL3_TOOL_R},
    {XSetPanel::TOOL_S,           XSetName::PANEL3_TOOL_S},
};

// panel4
static const std::unordered_map<XSetPanel, XSetName> xset_panel4_map{
    // panel4
    {XSetPanel::SHOW,             XSetName::PANEL4_SHOW},
    {XSetPanel::SHOW_TOOLBOX,     XSetName::PANEL4_SHOW_TOOLBOX},
    {XSetPanel::SHOW_DEVMON,      XSetName::PANEL4_SHOW_DEVMON},
    {XSetPanel::SHOW_DIRTREE,     XSetName::PANEL4_SHOW_DIRTREE},
    {XSetPanel::SHOW_SIDEBAR,     XSetName::PANEL4_SHOW_SIDEBAR},
    {XSetPanel::SLIDER_POSITIONS, XSetName::PANEL4_SLIDER_POSITIONS},
    {XSetPanel::LIST_DETAILED,    XSetName::PANEL4_LIST_DETAILED},
    {XSetPanel::LIST_ICONS,       XSetName::PANEL4_LIST_ICONS},
    {XSetPanel::LIST_COMPACT,     XSetName::PANEL4_LIST_COMPACT},
    {XSetPanel::LIST_LARGE,       XSetName::PANEL4_LIST_LARGE},
    {XSetPanel::SHOW_HIDDEN,      XSetName::PANEL4_SHOW_HIDDEN},
    {XSetPanel::ICON_TAB,         XSetName::PANEL4_ICON_TAB},
    {XSetPanel::ICON_STATUS,      XSetName::PANEL4_ICON_STATUS},
    {XSetPanel::DETCOL_NAME,      XSetName::PANEL4_DETCOL_NAME},
    {XSetPanel::DETCOL_SIZE,      XSetName::PANEL4_DETCOL_SIZE},
    {XSetPanel::DETCOL_TYPE,      XSetName::PANEL4_DETCOL_TYPE},
    {XSetPanel::DETCOL_PERM,      XSetName::PANEL4_DETCOL_PERM},
    {XSetPanel::DETCOL_OWNER,     XSetName::PANEL4_DETCOL_OWNER},
    {XSetPanel::DETCOL_DATE,      XSetName::PANEL4_DETCOL_DATE},
    {XSetPanel::SORT_EXTRA,       XSetName::PANEL4_SORT_EXTRA},
    {XSetPanel::BOOK_FOL,         XSetName::PANEL4_BOOK_FOL},
    {XSetPanel::TOOL_L,           XSetName::PANEL4_TOOL_L},
    {XSetPanel::TOOL_R,           XSetName::PANEL4_TOOL_R},
    {XSetPanel::TOOL_S,           XSetName::PANEL4_TOOL_S},
};

// panel modes

// panel1

// panel1 mode 0
static const std::unordered_map<XSetPanel, XSetName> xset_panel1_mode0_map{
    {XSetPanel::SHOW_TOOLBOX,     XSetName::PANEL1_SHOW_TOOLBOX_0},
    {XSetPanel::SHOW_DEVMON,      XSetName::PANEL1_SHOW_DEVMON_0},
    {XSetPanel::SHOW_DIRTREE,     XSetName::PANEL1_SHOW_DIRTREE_0},
    {XSetPanel::SHOW_SIDEBAR,     XSetName::PANEL1_SHOW_SIDEBAR_0},
    {XSetPanel::SLIDER_POSITIONS, XSetName::PANEL1_SLIDER_POSITIONS_0},
    {XSetPanel::LIST_DETAILED,    XSetName::PANEL1_LIST_DETAILED_0},
    {XSetPanel::LIST_ICONS,       XSetName::PANEL1_LIST_ICONS_0},
    {XSetPanel::LIST_COMPACT,     XSetName::PANEL1_LIST_COMPACT_0},
    {XSetPanel::LIST_LARGE,       XSetName::PANEL1_LIST_LARGE_0},
    {XSetPanel::SHOW_HIDDEN,      XSetName::PANEL1_SHOW_HIDDEN_0},
    {XSetPanel::ICON_TAB,         XSetName::PANEL1_ICON_TAB_0},
    {XSetPanel::ICON_STATUS,      XSetName::PANEL1_ICON_STATUS_0},
    {XSetPanel::DETCOL_NAME,      XSetName::PANEL1_DETCOL_NAME_0},
    {XSetPanel::DETCOL_SIZE,      XSetName::PANEL1_DETCOL_SIZE_0},
    {XSetPanel::DETCOL_TYPE,      XSetName::PANEL1_DETCOL_TYPE_0},
    {XSetPanel::DETCOL_PERM,      XSetName::PANEL1_DETCOL_PERM_0},
    {XSetPanel::DETCOL_OWNER,     XSetName::PANEL1_DETCOL_OWNER_0},
    {XSetPanel::DETCOL_DATE,      XSetName::PANEL1_DETCOL_DATE_0},
    {XSetPanel::SORT_EXTRA,       XSetName::PANEL1_SORT_EXTRA_0},
    {XSetPanel::BOOK_FOL,         XSetName::PANEL1_BOOK_FOL_0},
    {XSetPanel::TOOL_L,           XSetName::PANEL1_TOOL_L_0},
    {XSetPanel::TOOL_R,           XSetName::PANEL1_TOOL_R_0},
    {XSetPanel::TOOL_S,           XSetName::PANEL1_TOOL_S_0},
};

// panel1 mode 1
static const std::unordered_map<XSetPanel, XSetName> xset_panel1_mode1_map{
    {XSetPanel::SHOW_TOOLBOX,     XSetName::PANEL1_SHOW_TOOLBOX_1},
    {XSetPanel::SHOW_DEVMON,      XSetName::PANEL1_SHOW_DEVMON_1},
    {XSetPanel::SHOW_DIRTREE,     XSetName::PANEL1_SHOW_DIRTREE_1},
    {XSetPanel::SHOW_SIDEBAR,     XSetName::PANEL1_SHOW_SIDEBAR_1},
    {XSetPanel::SLIDER_POSITIONS, XSetName::PANEL1_SLIDER_POSITIONS_1},
    {XSetPanel::LIST_DETAILED,    XSetName::PANEL1_LIST_DETAILED_1},
    {XSetPanel::LIST_ICONS,       XSetName::PANEL1_LIST_ICONS_1},
    {XSetPanel::LIST_COMPACT,     XSetName::PANEL1_LIST_COMPACT_1},
    {XSetPanel::LIST_LARGE,       XSetName::PANEL1_LIST_LARGE_1},
    {XSetPanel::SHOW_HIDDEN,      XSetName::PANEL1_SHOW_HIDDEN_1},
    {XSetPanel::ICON_TAB,         XSetName::PANEL1_ICON_TAB_1},
    {XSetPanel::ICON_STATUS,      XSetName::PANEL1_ICON_STATUS_1},
    {XSetPanel::DETCOL_NAME,      XSetName::PANEL1_DETCOL_NAME_1},
    {XSetPanel::DETCOL_SIZE,      XSetName::PANEL1_DETCOL_SIZE_1},
    {XSetPanel::DETCOL_TYPE,      XSetName::PANEL1_DETCOL_TYPE_1},
    {XSetPanel::DETCOL_PERM,      XSetName::PANEL1_DETCOL_PERM_1},
    {XSetPanel::DETCOL_OWNER,     XSetName::PANEL1_DETCOL_OWNER_1},
    {XSetPanel::DETCOL_DATE,      XSetName::PANEL1_DETCOL_DATE_1},
    {XSetPanel::SORT_EXTRA,       XSetName::PANEL1_SORT_EXTRA_1},
    {XSetPanel::BOOK_FOL,         XSetName::PANEL1_BOOK_FOL_1},
    {XSetPanel::TOOL_L,           XSetName::PANEL1_TOOL_L_1},
    {XSetPanel::TOOL_R,           XSetName::PANEL1_TOOL_R_1},
    {XSetPanel::TOOL_S,           XSetName::PANEL1_TOOL_S_1},
};

// panel1 mode 2
static const std::unordered_map<XSetPanel, XSetName> xset_panel1_mode2_map{
    {XSetPanel::SHOW_TOOLBOX,     XSetName::PANEL1_SHOW_TOOLBOX_2},
    {XSetPanel::SHOW_DEVMON,      XSetName::PANEL1_SHOW_DEVMON_2},
    {XSetPanel::SHOW_DIRTREE,     XSetName::PANEL1_SHOW_DIRTREE_2},
    {XSetPanel::SHOW_SIDEBAR,     XSetName::PANEL1_SHOW_SIDEBAR_2},
    {XSetPanel::SLIDER_POSITIONS, XSetName::PANEL1_SLIDER_POSITIONS_2},
    {XSetPanel::LIST_DETAILED,    XSetName::PANEL1_LIST_DETAILED_2},
    {XSetPanel::LIST_ICONS,       XSetName::PANEL1_LIST_ICONS_2},
    {XSetPanel::LIST_COMPACT,     XSetName::PANEL1_LIST_COMPACT_2},
    {XSetPanel::LIST_LARGE,       XSetName::PANEL1_LIST_LARGE_2},
    {XSetPanel::SHOW_HIDDEN,      XSetName::PANEL1_SHOW_HIDDEN_2},
    {XSetPanel::ICON_TAB,         XSetName::PANEL1_ICON_TAB_2},
    {XSetPanel::ICON_STATUS,      XSetName::PANEL1_ICON_STATUS_2},
    {XSetPanel::DETCOL_NAME,      XSetName::PANEL1_DETCOL_NAME_2},
    {XSetPanel::DETCOL_SIZE,      XSetName::PANEL1_DETCOL_SIZE_2},
    {XSetPanel::DETCOL_TYPE,      XSetName::PANEL1_DETCOL_TYPE_2},
    {XSetPanel::DETCOL_PERM,      XSetName::PANEL1_DETCOL_PERM_2},
    {XSetPanel::DETCOL_OWNER,     XSetName::PANEL1_DETCOL_OWNER_2},
    {XSetPanel::DETCOL_DATE,      XSetName::PANEL1_DETCOL_DATE_2},
    {XSetPanel::SORT_EXTRA,       XSetName::PANEL1_SORT_EXTRA_2},
    {XSetPanel::BOOK_FOL,         XSetName::PANEL1_BOOK_FOL_2},
    {XSetPanel::TOOL_L,           XSetName::PANEL1_TOOL_L_2},
    {XSetPanel::TOOL_R,           XSetName::PANEL1_TOOL_R_2},
    {XSetPanel::TOOL_S,           XSetName::PANEL1_TOOL_S_2},
};

// panel1 mode 3
static const std::unordered_map<XSetPanel, XSetName> xset_panel1_mode3_map{
    {XSetPanel::SHOW_TOOLBOX,     XSetName::PANEL1_SHOW_TOOLBOX_3},
    {XSetPanel::SHOW_DEVMON,      XSetName::PANEL1_SHOW_DEVMON_3},
    {XSetPanel::SHOW_DIRTREE,     XSetName::PANEL1_SHOW_DIRTREE_3},
    {XSetPanel::SHOW_SIDEBAR,     XSetName::PANEL1_SHOW_SIDEBAR_3},
    {XSetPanel::SLIDER_POSITIONS, XSetName::PANEL1_SLIDER_POSITIONS_3},
    {XSetPanel::LIST_DETAILED,    XSetName::PANEL1_LIST_DETAILED_3},
    {XSetPanel::LIST_ICONS,       XSetName::PANEL1_LIST_ICONS_3},
    {XSetPanel::LIST_COMPACT,     XSetName::PANEL1_LIST_COMPACT_3},
    {XSetPanel::LIST_LARGE,       XSetName::PANEL1_LIST_LARGE_3},
    {XSetPanel::SHOW_HIDDEN,      XSetName::PANEL1_SHOW_HIDDEN_3},
    {XSetPanel::ICON_TAB,         XSetName::PANEL1_ICON_TAB_3},
    {XSetPanel::ICON_STATUS,      XSetName::PANEL1_ICON_STATUS_3},
    {XSetPanel::DETCOL_NAME,      XSetName::PANEL1_DETCOL_NAME_3},
    {XSetPanel::DETCOL_SIZE,      XSetName::PANEL1_DETCOL_SIZE_3},
    {XSetPanel::DETCOL_TYPE,      XSetName::PANEL1_DETCOL_TYPE_3},
    {XSetPanel::DETCOL_PERM,      XSetName::PANEL1_DETCOL_PERM_3},
    {XSetPanel::DETCOL_OWNER,     XSetName::PANEL1_DETCOL_OWNER_3},
    {XSetPanel::DETCOL_DATE,      XSetName::PANEL1_DETCOL_DATE_3},
    {XSetPanel::SORT_EXTRA,       XSetName::PANEL1_SORT_EXTRA_3},
    {XSetPanel::BOOK_FOL,         XSetName::PANEL1_BOOK_FOL_3},
    {XSetPanel::TOOL_L,           XSetName::PANEL1_TOOL_L_3},
    {XSetPanel::TOOL_R,           XSetName::PANEL1_TOOL_R_3},
    {XSetPanel::TOOL_S,           XSetName::PANEL1_TOOL_S_3},
};

// panel2

// panel2 mode 0
static const std::unordered_map<XSetPanel, XSetName> xset_panel2_mode0_map{
    {XSetPanel::SHOW_TOOLBOX,     XSetName::PANEL2_SHOW_TOOLBOX_0},
    {XSetPanel::SHOW_DEVMON,      XSetName::PANEL2_SHOW_DEVMON_0},
    {XSetPanel::SHOW_DIRTREE,     XSetName::PANEL2_SHOW_DIRTREE_0},
    {XSetPanel::SHOW_SIDEBAR,     XSetName::PANEL2_SHOW_SIDEBAR_0},
    {XSetPanel::SLIDER_POSITIONS, XSetName::PANEL2_SLIDER_POSITIONS_0},
    {XSetPanel::LIST_DETAILED,    XSetName::PANEL2_LIST_DETAILED_0},
    {XSetPanel::LIST_ICONS,       XSetName::PANEL2_LIST_ICONS_0},
    {XSetPanel::LIST_COMPACT,     XSetName::PANEL2_LIST_COMPACT_0},
    {XSetPanel::LIST_LARGE,       XSetName::PANEL2_LIST_LARGE_0},
    {XSetPanel::SHOW_HIDDEN,      XSetName::PANEL2_SHOW_HIDDEN_0},
    {XSetPanel::ICON_TAB,         XSetName::PANEL2_ICON_TAB_0},
    {XSetPanel::ICON_STATUS,      XSetName::PANEL2_ICON_STATUS_0},
    {XSetPanel::DETCOL_NAME,      XSetName::PANEL2_DETCOL_NAME_0},
    {XSetPanel::DETCOL_SIZE,      XSetName::PANEL2_DETCOL_SIZE_0},
    {XSetPanel::DETCOL_TYPE,      XSetName::PANEL2_DETCOL_TYPE_0},
    {XSetPanel::DETCOL_PERM,      XSetName::PANEL2_DETCOL_PERM_0},
    {XSetPanel::DETCOL_OWNER,     XSetName::PANEL2_DETCOL_OWNER_0},
    {XSetPanel::DETCOL_DATE,      XSetName::PANEL2_DETCOL_DATE_0},
    {XSetPanel::SORT_EXTRA,       XSetName::PANEL2_SORT_EXTRA_0},
    {XSetPanel::BOOK_FOL,         XSetName::PANEL2_BOOK_FOL_0},
    {XSetPanel::TOOL_L,           XSetName::PANEL2_TOOL_L_0},
    {XSetPanel::TOOL_R,           XSetName::PANEL2_TOOL_R_0},
    {XSetPanel::TOOL_S,           XSetName::PANEL2_TOOL_S_0},
};

// panel2 mode 1
static const std::unordered_map<XSetPanel, XSetName> xset_panel2_mode1_map{
    {XSetPanel::SHOW_TOOLBOX,     XSetName::PANEL2_SHOW_TOOLBOX_1},
    {XSetPanel::SHOW_DEVMON,      XSetName::PANEL2_SHOW_DEVMON_1},
    {XSetPanel::SHOW_DIRTREE,     XSetName::PANEL2_SHOW_DIRTREE_1},
    {XSetPanel::SHOW_SIDEBAR,     XSetName::PANEL2_SHOW_SIDEBAR_1},
    {XSetPanel::SLIDER_POSITIONS, XSetName::PANEL2_SLIDER_POSITIONS_1},
    {XSetPanel::LIST_DETAILED,    XSetName::PANEL2_LIST_DETAILED_1},
    {XSetPanel::LIST_ICONS,       XSetName::PANEL2_LIST_ICONS_1},
    {XSetPanel::LIST_COMPACT,     XSetName::PANEL2_LIST_COMPACT_1},
    {XSetPanel::LIST_LARGE,       XSetName::PANEL2_LIST_LARGE_1},
    {XSetPanel::SHOW_HIDDEN,      XSetName::PANEL2_SHOW_HIDDEN_1},
    {XSetPanel::ICON_TAB,         XSetName::PANEL2_ICON_TAB_1},
    {XSetPanel::ICON_STATUS,      XSetName::PANEL2_ICON_STATUS_1},
    {XSetPanel::DETCOL_NAME,      XSetName::PANEL2_DETCOL_NAME_1},
    {XSetPanel::DETCOL_SIZE,      XSetName::PANEL2_DETCOL_SIZE_1},
    {XSetPanel::DETCOL_TYPE,      XSetName::PANEL2_DETCOL_TYPE_1},
    {XSetPanel::DETCOL_PERM,      XSetName::PANEL2_DETCOL_PERM_1},
    {XSetPanel::DETCOL_OWNER,     XSetName::PANEL2_DETCOL_OWNER_1},
    {XSetPanel::DETCOL_DATE,      XSetName::PANEL2_DETCOL_DATE_1},
    {XSetPanel::SORT_EXTRA,       XSetName::PANEL2_SORT_EXTRA_1},
    {XSetPanel::BOOK_FOL,         XSetName::PANEL2_BOOK_FOL_1},
    {XSetPanel::TOOL_L,           XSetName::PANEL2_TOOL_L_1},
    {XSetPanel::TOOL_R,           XSetName::PANEL2_TOOL_R_1},
    {XSetPanel::TOOL_S,           XSetName::PANEL2_TOOL_S_1},
};

// panel2 mode 2
static const std::unordered_map<XSetPanel, XSetName> xset_panel2_mode2_map{
    {XSetPanel::SHOW_TOOLBOX,     XSetName::PANEL2_SHOW_TOOLBOX_2},
    {XSetPanel::SHOW_DEVMON,      XSetName::PANEL2_SHOW_DEVMON_2},
    {XSetPanel::SHOW_DIRTREE,     XSetName::PANEL2_SHOW_DIRTREE_2},
    {XSetPanel::SHOW_SIDEBAR,     XSetName::PANEL2_SHOW_SIDEBAR_2},
    {XSetPanel::SLIDER_POSITIONS, XSetName::PANEL2_SLIDER_POSITIONS_2},
    {XSetPanel::LIST_DETAILED,    XSetName::PANEL2_LIST_DETAILED_2},
    {XSetPanel::LIST_ICONS,       XSetName::PANEL2_LIST_ICONS_2},
    {XSetPanel::LIST_COMPACT,     XSetName::PANEL2_LIST_COMPACT_2},
    {XSetPanel::LIST_LARGE,       XSetName::PANEL2_LIST_LARGE_2},
    {XSetPanel::SHOW_HIDDEN,      XSetName::PANEL2_SHOW_HIDDEN_2},
    {XSetPanel::ICON_TAB,         XSetName::PANEL2_ICON_TAB_2},
    {XSetPanel::ICON_STATUS,      XSetName::PANEL2_ICON_STATUS_2},
    {XSetPanel::DETCOL_NAME,      XSetName::PANEL2_DETCOL_NAME_2},
    {XSetPanel::DETCOL_SIZE,      XSetName::PANEL2_DETCOL_SIZE_2},
    {XSetPanel::DETCOL_TYPE,      XSetName::PANEL2_DETCOL_TYPE_2},
    {XSetPanel::DETCOL_PERM,      XSetName::PANEL2_DETCOL_PERM_2},
    {XSetPanel::DETCOL_OWNER,     XSetName::PANEL2_DETCOL_OWNER_2},
    {XSetPanel::DETCOL_DATE,      XSetName::PANEL2_DETCOL_DATE_2},
    {XSetPanel::SORT_EXTRA,       XSetName::PANEL2_SORT_EXTRA_2},
    {XSetPanel::BOOK_FOL,         XSetName::PANEL2_BOOK_FOL_2},
    {XSetPanel::TOOL_L,           XSetName::PANEL2_TOOL_L_2},
    {XSetPanel::TOOL_R,           XSetName::PANEL2_TOOL_R_2},
    {XSetPanel::TOOL_S,           XSetName::PANEL2_TOOL_S_2},
};

// panel2 mode 3
static const std::unordered_map<XSetPanel, XSetName> xset_panel2_mode3_map{
    {XSetPanel::SHOW_TOOLBOX,     XSetName::PANEL2_SHOW_TOOLBOX_3},
    {XSetPanel::SHOW_DEVMON,      XSetName::PANEL2_SHOW_DEVMON_3},
    {XSetPanel::SHOW_DIRTREE,     XSetName::PANEL2_SHOW_DIRTREE_3},
    {XSetPanel::SHOW_SIDEBAR,     XSetName::PANEL2_SHOW_SIDEBAR_3},
    {XSetPanel::SLIDER_POSITIONS, XSetName::PANEL2_SLIDER_POSITIONS_3},
    {XSetPanel::LIST_DETAILED,    XSetName::PANEL2_LIST_DETAILED_3},
    {XSetPanel::LIST_ICONS,       XSetName::PANEL2_LIST_ICONS_3},
    {XSetPanel::LIST_COMPACT,     XSetName::PANEL2_LIST_COMPACT_3},
    {XSetPanel::LIST_LARGE,       XSetName::PANEL2_LIST_LARGE_3},
    {XSetPanel::SHOW_HIDDEN,      XSetName::PANEL2_SHOW_HIDDEN_3},
    {XSetPanel::ICON_TAB,         XSetName::PANEL2_ICON_TAB_3},
    {XSetPanel::ICON_STATUS,      XSetName::PANEL2_ICON_STATUS_3},
    {XSetPanel::DETCOL_NAME,      XSetName::PANEL2_DETCOL_NAME_3},
    {XSetPanel::DETCOL_SIZE,      XSetName::PANEL2_DETCOL_SIZE_3},
    {XSetPanel::DETCOL_TYPE,      XSetName::PANEL2_DETCOL_TYPE_3},
    {XSetPanel::DETCOL_PERM,      XSetName::PANEL2_DETCOL_PERM_3},
    {XSetPanel::DETCOL_OWNER,     XSetName::PANEL2_DETCOL_OWNER_3},
    {XSetPanel::DETCOL_DATE,      XSetName::PANEL2_DETCOL_DATE_3},
    {XSetPanel::SORT_EXTRA,       XSetName::PANEL2_SORT_EXTRA_3},
    {XSetPanel::BOOK_FOL,         XSetName::PANEL2_BOOK_FOL_3},
    {XSetPanel::TOOL_L,           XSetName::PANEL2_TOOL_L_3},
    {XSetPanel::TOOL_R,           XSetName::PANEL2_TOOL_R_3},
    {XSetPanel::TOOL_S,           XSetName::PANEL2_TOOL_S_3},
};

// panel3

// panel3 mode 0
static const std::unordered_map<XSetPanel, XSetName> xset_panel3_mode0_map{
    {XSetPanel::SHOW_TOOLBOX,     XSetName::PANEL3_SHOW_TOOLBOX_0},
    {XSetPanel::SHOW_DEVMON,      XSetName::PANEL3_SHOW_DEVMON_0},
    {XSetPanel::SHOW_DIRTREE,     XSetName::PANEL3_SHOW_DIRTREE_0},
    {XSetPanel::SHOW_SIDEBAR,     XSetName::PANEL3_SHOW_SIDEBAR_0},
    {XSetPanel::SLIDER_POSITIONS, XSetName::PANEL3_SLIDER_POSITIONS_0},
    {XSetPanel::LIST_DETAILED,    XSetName::PANEL3_LIST_DETAILED_0},
    {XSetPanel::LIST_ICONS,       XSetName::PANEL3_LIST_ICONS_0},
    {XSetPanel::LIST_COMPACT,     XSetName::PANEL3_LIST_COMPACT_0},
    {XSetPanel::LIST_LARGE,       XSetName::PANEL3_LIST_LARGE_0},
    {XSetPanel::SHOW_HIDDEN,      XSetName::PANEL3_SHOW_HIDDEN_0},
    {XSetPanel::ICON_TAB,         XSetName::PANEL3_ICON_TAB_0},
    {XSetPanel::ICON_STATUS,      XSetName::PANEL3_ICON_STATUS_0},
    {XSetPanel::DETCOL_NAME,      XSetName::PANEL3_DETCOL_NAME_0},
    {XSetPanel::DETCOL_SIZE,      XSetName::PANEL3_DETCOL_SIZE_0},
    {XSetPanel::DETCOL_TYPE,      XSetName::PANEL3_DETCOL_TYPE_0},
    {XSetPanel::DETCOL_PERM,      XSetName::PANEL3_DETCOL_PERM_0},
    {XSetPanel::DETCOL_OWNER,     XSetName::PANEL3_DETCOL_OWNER_0},
    {XSetPanel::DETCOL_DATE,      XSetName::PANEL3_DETCOL_DATE_0},
    {XSetPanel::SORT_EXTRA,       XSetName::PANEL3_SORT_EXTRA_0},
    {XSetPanel::BOOK_FOL,         XSetName::PANEL3_BOOK_FOL_0},
    {XSetPanel::TOOL_L,           XSetName::PANEL3_TOOL_L_0},
    {XSetPanel::TOOL_R,           XSetName::PANEL3_TOOL_R_0},
    {XSetPanel::TOOL_S,           XSetName::PANEL3_TOOL_S_0},
};

// panel3 mode 1
static const std::unordered_map<XSetPanel, XSetName> xset_panel3_mode1_map{
    {XSetPanel::SHOW_TOOLBOX,     XSetName::PANEL3_SHOW_TOOLBOX_1},
    {XSetPanel::SHOW_DEVMON,      XSetName::PANEL3_SHOW_DEVMON_1},
    {XSetPanel::SHOW_DIRTREE,     XSetName::PANEL3_SHOW_DIRTREE_1},
    {XSetPanel::SHOW_SIDEBAR,     XSetName::PANEL3_SHOW_SIDEBAR_1},
    {XSetPanel::SLIDER_POSITIONS, XSetName::PANEL3_SLIDER_POSITIONS_1},
    {XSetPanel::LIST_DETAILED,    XSetName::PANEL3_LIST_DETAILED_1},
    {XSetPanel::LIST_ICONS,       XSetName::PANEL3_LIST_ICONS_1},
    {XSetPanel::LIST_COMPACT,     XSetName::PANEL3_LIST_COMPACT_1},
    {XSetPanel::LIST_LARGE,       XSetName::PANEL3_LIST_LARGE_1},
    {XSetPanel::SHOW_HIDDEN,      XSetName::PANEL3_SHOW_HIDDEN_1},
    {XSetPanel::ICON_TAB,         XSetName::PANEL3_ICON_TAB_1},
    {XSetPanel::ICON_STATUS,      XSetName::PANEL3_ICON_STATUS_1},
    {XSetPanel::DETCOL_NAME,      XSetName::PANEL3_DETCOL_NAME_1},
    {XSetPanel::DETCOL_SIZE,      XSetName::PANEL3_DETCOL_SIZE_1},
    {XSetPanel::DETCOL_TYPE,      XSetName::PANEL3_DETCOL_TYPE_1},
    {XSetPanel::DETCOL_PERM,      XSetName::PANEL3_DETCOL_PERM_1},
    {XSetPanel::DETCOL_OWNER,     XSetName::PANEL3_DETCOL_OWNER_1},
    {XSetPanel::DETCOL_DATE,      XSetName::PANEL3_DETCOL_DATE_1},
    {XSetPanel::SORT_EXTRA,       XSetName::PANEL3_SORT_EXTRA_1},
    {XSetPanel::BOOK_FOL,         XSetName::PANEL3_BOOK_FOL_1},
    {XSetPanel::TOOL_L,           XSetName::PANEL3_TOOL_L_1},
    {XSetPanel::TOOL_R,           XSetName::PANEL3_TOOL_R_1},
    {XSetPanel::TOOL_S,           XSetName::PANEL3_TOOL_S_1},
};

// panel3 mode 2
static const std::unordered_map<XSetPanel, XSetName> xset_panel3_mode2_map{
    {XSetPanel::SHOW_TOOLBOX,     XSetName::PANEL3_SHOW_TOOLBOX_2},
    {XSetPanel::SHOW_DEVMON,      XSetName::PANEL3_SHOW_DEVMON_2},
    {XSetPanel::SHOW_DIRTREE,     XSetName::PANEL3_SHOW_DIRTREE_2},
    {XSetPanel::SHOW_SIDEBAR,     XSetName::PANEL3_SHOW_SIDEBAR_2},
    {XSetPanel::SLIDER_POSITIONS, XSetName::PANEL3_SLIDER_POSITIONS_2},
    {XSetPanel::LIST_DETAILED,    XSetName::PANEL3_LIST_DETAILED_2},
    {XSetPanel::LIST_ICONS,       XSetName::PANEL3_LIST_ICONS_2},
    {XSetPanel::LIST_COMPACT,     XSetName::PANEL3_LIST_COMPACT_2},
    {XSetPanel::LIST_LARGE,       XSetName::PANEL3_LIST_LARGE_2},
    {XSetPanel::SHOW_HIDDEN,      XSetName::PANEL3_SHOW_HIDDEN_2},
    {XSetPanel::ICON_TAB,         XSetName::PANEL3_ICON_TAB_2},
    {XSetPanel::ICON_STATUS,      XSetName::PANEL3_ICON_STATUS_2},
    {XSetPanel::DETCOL_NAME,      XSetName::PANEL3_DETCOL_NAME_2},
    {XSetPanel::DETCOL_SIZE,      XSetName::PANEL3_DETCOL_SIZE_2},
    {XSetPanel::DETCOL_TYPE,      XSetName::PANEL3_DETCOL_TYPE_2},
    {XSetPanel::DETCOL_PERM,      XSetName::PANEL3_DETCOL_PERM_2},
    {XSetPanel::DETCOL_OWNER,     XSetName::PANEL3_DETCOL_OWNER_2},
    {XSetPanel::DETCOL_DATE,      XSetName::PANEL3_DETCOL_DATE_2},
    {XSetPanel::SORT_EXTRA,       XSetName::PANEL3_SORT_EXTRA_2},
    {XSetPanel::BOOK_FOL,         XSetName::PANEL3_BOOK_FOL_2},
    {XSetPanel::TOOL_L,           XSetName::PANEL3_TOOL_L_2},
    {XSetPanel::TOOL_R,           XSetName::PANEL3_TOOL_R_2},
    {XSetPanel::TOOL_S,           XSetName::PANEL3_TOOL_S_2},
};

// panel3 mode 3
static const std::unordered_map<XSetPanel, XSetName> xset_panel3_mode3_map{
    {XSetPanel::SHOW_TOOLBOX,     XSetName::PANEL3_SHOW_TOOLBOX_3},
    {XSetPanel::SHOW_DEVMON,      XSetName::PANEL3_SHOW_DEVMON_3},
    {XSetPanel::SHOW_DIRTREE,     XSetName::PANEL3_SHOW_DIRTREE_3},
    {XSetPanel::SHOW_SIDEBAR,     XSetName::PANEL3_SHOW_SIDEBAR_3},
    {XSetPanel::SLIDER_POSITIONS, XSetName::PANEL3_SLIDER_POSITIONS_3},
    {XSetPanel::LIST_DETAILED,    XSetName::PANEL3_LIST_DETAILED_3},
    {XSetPanel::LIST_ICONS,       XSetName::PANEL3_LIST_ICONS_3},
    {XSetPanel::LIST_COMPACT,     XSetName::PANEL3_LIST_COMPACT_3},
    {XSetPanel::LIST_LARGE,       XSetName::PANEL3_LIST_LARGE_3},
    {XSetPanel::SHOW_HIDDEN,      XSetName::PANEL3_SHOW_HIDDEN_3},
    {XSetPanel::ICON_TAB,         XSetName::PANEL3_ICON_TAB_3},
    {XSetPanel::ICON_STATUS,      XSetName::PANEL3_ICON_STATUS_3},
    {XSetPanel::DETCOL_NAME,      XSetName::PANEL3_DETCOL_NAME_3},
    {XSetPanel::DETCOL_SIZE,      XSetName::PANEL3_DETCOL_SIZE_3},
    {XSetPanel::DETCOL_TYPE,      XSetName::PANEL3_DETCOL_TYPE_3},
    {XSetPanel::DETCOL_PERM,      XSetName::PANEL3_DETCOL_PERM_3},
    {XSetPanel::DETCOL_OWNER,     XSetName::PANEL3_DETCOL_OWNER_3},
    {XSetPanel::DETCOL_DATE,      XSetName::PANEL3_DETCOL_DATE_3},
    {XSetPanel::SORT_EXTRA,       XSetName::PANEL3_SORT_EXTRA_3},
    {XSetPanel::BOOK_FOL,         XSetName::PANEL3_BOOK_FOL_3},
    {XSetPanel::TOOL_L,           XSetName::PANEL3_TOOL_L_3},
    {XSetPanel::TOOL_R,           XSetName::PANEL3_TOOL_R_3},
    {XSetPanel::TOOL_S,           XSetName::PANEL3_TOOL_S_3},
};

// panel4

// panel4 mode 0
static const std::unordered_map<XSetPanel, XSetName> xset_panel4_mode0_map{
    {XSetPanel::SHOW_TOOLBOX,     XSetName::PANEL4_SHOW_TOOLBOX_0},
    {XSetPanel::SHOW_DEVMON,      XSetName::PANEL4_SHOW_DEVMON_0},
    {XSetPanel::SHOW_DIRTREE,     XSetName::PANEL4_SHOW_DIRTREE_0},
    {XSetPanel::SHOW_SIDEBAR,     XSetName::PANEL4_SHOW_SIDEBAR_0},
    {XSetPanel::SLIDER_POSITIONS, XSetName::PANEL4_SLIDER_POSITIONS_0},
    {XSetPanel::LIST_DETAILED,    XSetName::PANEL4_LIST_DETAILED_0},
    {XSetPanel::LIST_ICONS,       XSetName::PANEL4_LIST_ICONS_0},
    {XSetPanel::LIST_COMPACT,     XSetName::PANEL4_LIST_COMPACT_0},
    {XSetPanel::LIST_LARGE,       XSetName::PANEL4_LIST_LARGE_0},
    {XSetPanel::SHOW_HIDDEN,      XSetName::PANEL4_SHOW_HIDDEN_0},
    {XSetPanel::ICON_TAB,         XSetName::PANEL4_ICON_TAB_0},
    {XSetPanel::ICON_STATUS,      XSetName::PANEL4_ICON_STATUS_0},
    {XSetPanel::DETCOL_NAME,      XSetName::PANEL4_DETCOL_NAME_0},
    {XSetPanel::DETCOL_SIZE,      XSetName::PANEL4_DETCOL_SIZE_0},
    {XSetPanel::DETCOL_TYPE,      XSetName::PANEL4_DETCOL_TYPE_0},
    {XSetPanel::DETCOL_PERM,      XSetName::PANEL4_DETCOL_PERM_0},
    {XSetPanel::DETCOL_OWNER,     XSetName::PANEL4_DETCOL_OWNER_0},
    {XSetPanel::DETCOL_DATE,      XSetName::PANEL4_DETCOL_DATE_0},
    {XSetPanel::SORT_EXTRA,       XSetName::PANEL4_SORT_EXTRA_0},
    {XSetPanel::BOOK_FOL,         XSetName::PANEL4_BOOK_FOL_0},
    {XSetPanel::TOOL_L,           XSetName::PANEL4_TOOL_L_0},
    {XSetPanel::TOOL_R,           XSetName::PANEL4_TOOL_R_0},
    {XSetPanel::TOOL_S,           XSetName::PANEL4_TOOL_S_0},
};

// panel4 mode 1
static const std::unordered_map<XSetPanel, XSetName> xset_panel4_mode1_map{
    {XSetPanel::SHOW_TOOLBOX,     XSetName::PANEL4_SHOW_TOOLBOX_1},
    {XSetPanel::SHOW_DEVMON,      XSetName::PANEL4_SHOW_DEVMON_1},
    {XSetPanel::SHOW_DIRTREE,     XSetName::PANEL4_SHOW_DIRTREE_1},
    {XSetPanel::SHOW_SIDEBAR,     XSetName::PANEL4_SHOW_SIDEBAR_1},
    {XSetPanel::SLIDER_POSITIONS, XSetName::PANEL4_SLIDER_POSITIONS_1},
    {XSetPanel::LIST_DETAILED,    XSetName::PANEL4_LIST_DETAILED_1},
    {XSetPanel::LIST_ICONS,       XSetName::PANEL4_LIST_ICONS_1},
    {XSetPanel::LIST_COMPACT,     XSetName::PANEL4_LIST_COMPACT_1},
    {XSetPanel::LIST_LARGE,       XSetName::PANEL4_LIST_LARGE_1},
    {XSetPanel::SHOW_HIDDEN,      XSetName::PANEL4_SHOW_HIDDEN_1},
    {XSetPanel::ICON_TAB,         XSetName::PANEL4_ICON_TAB_1},
    {XSetPanel::ICON_STATUS,      XSetName::PANEL4_ICON_STATUS_1},
    {XSetPanel::DETCOL_NAME,      XSetName::PANEL4_DETCOL_NAME_1},
    {XSetPanel::DETCOL_SIZE,      XSetName::PANEL4_DETCOL_SIZE_1},
    {XSetPanel::DETCOL_TYPE,      XSetName::PANEL4_DETCOL_TYPE_1},
    {XSetPanel::DETCOL_PERM,      XSetName::PANEL4_DETCOL_PERM_1},
    {XSetPanel::DETCOL_OWNER,     XSetName::PANEL4_DETCOL_OWNER_1},
    {XSetPanel::DETCOL_DATE,      XSetName::PANEL4_DETCOL_DATE_1},
    {XSetPanel::SORT_EXTRA,       XSetName::PANEL4_SORT_EXTRA_1},
    {XSetPanel::BOOK_FOL,         XSetName::PANEL4_BOOK_FOL_1},
    {XSetPanel::TOOL_L,           XSetName::PANEL4_TOOL_L_1},
    {XSetPanel::TOOL_R,           XSetName::PANEL4_TOOL_R_1},
    {XSetPanel::TOOL_S,           XSetName::PANEL4_TOOL_S_1},
};

// panel4 mode 2
static const std::unordered_map<XSetPanel, XSetName> xset_panel4_mode2_map{
    {XSetPanel::SHOW_TOOLBOX,     XSetName::PANEL4_SHOW_TOOLBOX_2},
    {XSetPanel::SHOW_DEVMON,      XSetName::PANEL4_SHOW_DEVMON_2},
    {XSetPanel::SHOW_DIRTREE,     XSetName::PANEL4_SHOW_DIRTREE_2},
    {XSetPanel::SHOW_SIDEBAR,     XSetName::PANEL4_SHOW_SIDEBAR_2},
    {XSetPanel::SLIDER_POSITIONS, XSetName::PANEL4_SLIDER_POSITIONS_2},
    {XSetPanel::LIST_DETAILED,    XSetName::PANEL4_LIST_DETAILED_2},
    {XSetPanel::LIST_ICONS,       XSetName::PANEL4_LIST_ICONS_2},
    {XSetPanel::LIST_COMPACT,     XSetName::PANEL4_LIST_COMPACT_2},
    {XSetPanel::LIST_LARGE,       XSetName::PANEL4_LIST_LARGE_2},
    {XSetPanel::SHOW_HIDDEN,      XSetName::PANEL4_SHOW_HIDDEN_2},
    {XSetPanel::ICON_TAB,         XSetName::PANEL4_ICON_TAB_2},
    {XSetPanel::ICON_STATUS,      XSetName::PANEL4_ICON_STATUS_2},
    {XSetPanel::DETCOL_NAME,      XSetName::PANEL4_DETCOL_NAME_2},
    {XSetPanel::DETCOL_SIZE,      XSetName::PANEL4_DETCOL_SIZE_2},
    {XSetPanel::DETCOL_TYPE,      XSetName::PANEL4_DETCOL_TYPE_2},
    {XSetPanel::DETCOL_PERM,      XSetName::PANEL4_DETCOL_PERM_2},
    {XSetPanel::DETCOL_OWNER,     XSetName::PANEL4_DETCOL_OWNER_2},
    {XSetPanel::DETCOL_DATE,      XSetName::PANEL4_DETCOL_DATE_2},
    {XSetPanel::SORT_EXTRA,       XSetName::PANEL4_SORT_EXTRA_2},
    {XSetPanel::BOOK_FOL,         XSetName::PANEL4_BOOK_FOL_2},
    {XSetPanel::TOOL_L,           XSetName::PANEL4_TOOL_L_2},
    {XSetPanel::TOOL_R,           XSetName::PANEL4_TOOL_R_2},
    {XSetPanel::TOOL_S,           XSetName::PANEL4_TOOL_S_2},
};

// panel4 mode 3
static const std::unordered_map<XSetPanel, XSetName> xset_panel4_mode3_map{
    {XSetPanel::SHOW_TOOLBOX,     XSetName::PANEL4_SHOW_TOOLBOX_3},
    {XSetPanel::SHOW_DEVMON,      XSetName::PANEL4_SHOW_DEVMON_3},
    {XSetPanel::SHOW_DIRTREE,     XSetName::PANEL4_SHOW_DIRTREE_3},
    {XSetPanel::SHOW_SIDEBAR,     XSetName::PANEL4_SHOW_SIDEBAR_3},
    {XSetPanel::SLIDER_POSITIONS, XSetName::PANEL4_SLIDER_POSITIONS_3},
    {XSetPanel::LIST_DETAILED,    XSetName::PANEL4_LIST_DETAILED_3},
    {XSetPanel::LIST_ICONS,       XSetName::PANEL4_LIST_ICONS_3},
    {XSetPanel::LIST_COMPACT,     XSetName::PANEL4_LIST_COMPACT_3},
    {XSetPanel::LIST_LARGE,       XSetName::PANEL4_LIST_LARGE_3},
    {XSetPanel::SHOW_HIDDEN,      XSetName::PANEL4_SHOW_HIDDEN_3},
    {XSetPanel::ICON_TAB,         XSetName::PANEL4_ICON_TAB_3},
    {XSetPanel::ICON_STATUS,      XSetName::PANEL4_ICON_STATUS_3},
    {XSetPanel::DETCOL_NAME,      XSetName::PANEL4_DETCOL_NAME_3},
    {XSetPanel::DETCOL_SIZE,      XSetName::PANEL4_DETCOL_SIZE_3},
    {XSetPanel::DETCOL_TYPE,      XSetName::PANEL4_DETCOL_TYPE_3},
    {XSetPanel::DETCOL_PERM,      XSetName::PANEL4_DETCOL_PERM_3},
    {XSetPanel::DETCOL_OWNER,     XSetName::PANEL4_DETCOL_OWNER_3},
    {XSetPanel::DETCOL_DATE,      XSetName::PANEL4_DETCOL_DATE_3},
    {XSetPanel::SORT_EXTRA,       XSetName::PANEL4_SORT_EXTRA_3},
    {XSetPanel::BOOK_FOL,         XSetName::PANEL4_BOOK_FOL_3},
    {XSetPanel::TOOL_L,           XSetName::PANEL4_TOOL_L_3},
    {XSetPanel::TOOL_R,           XSetName::PANEL4_TOOL_R_3},
    {XSetPanel::TOOL_S,           XSetName::PANEL4_TOOL_S_3},
};

static const std::unordered_map<std::string_view, XSetVar> xset_var_map{
    {"s"sv,                 XSetVar::S},
    {"b"sv,                 XSetVar::B},
    {"x"sv,                 XSetVar::X},
    {"y"sv,                 XSetVar::Y},
    {"z"sv,                 XSetVar::Z},
    {"key"sv,               XSetVar::KEY},
    {"keymod"sv,            XSetVar::KEYMOD},
    {"style"sv,             XSetVar::STYLE},
    {"desc"sv,              XSetVar::DESC},
    {"title"sv,             XSetVar::TITLE},
    {"menu_label"sv,        XSetVar::MENU_LABEL},
    {"menu_label_custom"sv, XSetVar::MENU_LABEL_CUSTOM},
    {"icn"sv,               XSetVar::ICN},
    {"icon"sv,              XSetVar::ICON},
    {"shared_key"sv,        XSetVar::SHARED_KEY},
    {"next"sv,              XSetVar::NEXT},
    {"prev"sv,              XSetVar::PREV},
    {"parent"sv,            XSetVar::PARENT},
    {"child"sv,             XSetVar::CHILD},
    {"context"sv,           XSetVar::CONTEXT},
    {"line"sv,              XSetVar::LINE},
    {"tool"sv,              XSetVar::TOOL},
    {"task"sv,              XSetVar::TASK},
    {"task_pop"sv,          XSetVar::TASK_POP},
    {"task_err"sv,          XSetVar::TASK_OUT},
    {"task_out"sv,          XSetVar::TASK_OUT},
    {"run_in_terminal"sv,   XSetVar::RUN_IN_TERMINAL},
    {"keep_terminal"sv,     XSetVar::KEEP_TERMINAL},
    {"scroll_lock"sv,       XSetVar::SCROLL_LOCK},
    {"disable"sv,           XSetVar::DISABLE},
    {"opener"sv,            XSetVar::OPENER},

    // Deprecated ini only config keys
#if defined(HAVE_DEPRECATED_INI_CONFIG_LOADING)
    {"lbl"sv,               XSetVar::MENU_LABEL},
    {"label"sv,             XSetVar::MENU_LABEL_CUSTOM},
    {"cxt"sv,               XSetVar::CONTEXT},
    {"term"sv,              XSetVar::RUN_IN_TERMINAL},
    {"keep"sv,              XSetVar::KEEP_TERMINAL},
    {"scroll"sv,            XSetVar::SCROLL_LOCK},
    {"op"sv,                XSetVar::OPENER},
#endif
};

static const std::unordered_map<MainWindowPanel, std::string_view> main_window_panel_mode_map{
    {MainWindowPanel::PANEL_NEITHER, "0"sv},
    {MainWindowPanel::PANEL_HORIZ,   "1"sv},
    {MainWindowPanel::PANEL_VERT,    "2"sv},
    {MainWindowPanel::PANEL_BOTH,    "3"sv},
};

// clang-format on

XSetName
xset_get_xsetname_from_name(std::string_view name)
{
    for (const auto& it : xset_name_map)
    {
        if (ztd::same(name, it.second))
        {
            return it.first;
        }
    }
    return XSetName::CUSTOM;
}

const std::string
xset_get_name_from_xsetname(XSetName name)
{
    try
    {
        return xset_name_map.at(name).data();
    }
    catch (std::out_of_range)
    {
        const std::string msg = fmt::format("XSetName:: Not Implemented: {}", INT(name));
        throw InvalidXSetName(msg);
    }
}

// panel

XSetName
xset_get_xsetname_from_panel(panel_t panel, XSetPanel name)
{
    switch (panel)
    {
        case 1:
            return xset_panel1_map.at(name);
        case 2:
            return xset_panel2_map.at(name);
        case 3:
            return xset_panel3_map.at(name);
        case 4:
            return xset_panel4_map.at(name);
        default:
            // ztd::logger::warn("Panel out of range, using panel 1");
            return xset_panel1_map.at(name);
    }
}

const std::string
xset_get_name_from_panel(panel_t panel, XSetPanel name)
{
    return xset_name_map.at(xset_get_xsetname_from_panel(panel, name)).data();
}

// panel mode

XSetName
xset_get_xsetname_from_panel_mode(panel_t panel, XSetPanel name, MainWindowPanel mode)
{
    switch (panel)
    {
        case 1:
            switch (mode)
            {
                case MainWindowPanel::PANEL_NEITHER:
                    return xset_panel1_mode0_map.at(name);
                case MainWindowPanel::PANEL_HORIZ:
                    return xset_panel1_mode1_map.at(name);
                case MainWindowPanel::PANEL_VERT:
                    return xset_panel1_mode2_map.at(name);
                case MainWindowPanel::PANEL_BOTH:
                    return xset_panel1_mode3_map.at(name);
            }
        case 2:
            switch (mode)
            {
                case MainWindowPanel::PANEL_NEITHER:
                    return xset_panel2_mode0_map.at(name);
                case MainWindowPanel::PANEL_HORIZ:
                    return xset_panel2_mode1_map.at(name);
                case MainWindowPanel::PANEL_VERT:
                    return xset_panel2_mode2_map.at(name);
                case MainWindowPanel::PANEL_BOTH:
                    return xset_panel2_mode3_map.at(name);
            }
        case 3:
            switch (mode)
            {
                case MainWindowPanel::PANEL_NEITHER:
                    return xset_panel3_mode0_map.at(name);
                case MainWindowPanel::PANEL_HORIZ:
                    return xset_panel3_mode1_map.at(name);
                case MainWindowPanel::PANEL_VERT:
                    return xset_panel3_mode2_map.at(name);
                case MainWindowPanel::PANEL_BOTH:
                    return xset_panel3_mode3_map.at(name);
            }
        case 4:
            switch (mode)
            {
                case MainWindowPanel::PANEL_NEITHER:
                    return xset_panel4_mode0_map.at(name);
                case MainWindowPanel::PANEL_HORIZ:
                    return xset_panel4_mode1_map.at(name);
                case MainWindowPanel::PANEL_VERT:
                    return xset_panel4_mode2_map.at(name);
                case MainWindowPanel::PANEL_BOTH:
                    return xset_panel4_mode3_map.at(name);
            }
        default:
            // ztd::logger::warn("Panel Mode out of range: {}, using panel 1",
            // std::to_string(mode));
            return xset_get_xsetname_from_panel_mode(1, name, mode);
    }
}

const std::string
xset_get_name_from_panel_mode(panel_t panel, XSetPanel name, MainWindowPanel mode)
{
    return xset_name_map.at(xset_get_xsetname_from_panel_mode(panel, name, mode)).data();
}

// xset var

XSetVar
xset_get_xsetvar_from_name(std::string_view name)
{
    try
    {
        return xset_var_map.at(name);
    }
    catch (std::out_of_range)
    {
        const std::string err_msg = fmt::format("Unknown XSet var {}", name);
        throw std::logic_error(err_msg);
    }
}

const std::string
xset_get_name_from_xsetvar(XSetVar name)
{
    for (const auto& it : xset_var_map)
    {
        if (name == it.second)
        {
            return it.first.data();
        }
    }

    const std::string err_msg = fmt::format("NOT implemented XSetVar: {}", INT(name));
    throw std::logic_error(err_msg);
}

// main window panel mode

const std::string
xset_get_window_panel_mode(MainWindowPanel mode)
{
    try
    {
        return main_window_panel_mode_map.at(mode).data();
    }
    catch (std::out_of_range)
    {
        const std::string msg = fmt::format("MainWindowPanel:: Not Implemented: {}", INT(mode));
        throw InvalidXSetName(msg);
    }
}
