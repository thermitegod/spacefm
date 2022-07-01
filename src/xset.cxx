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

#include <map>

#include <fmt/format.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "types.hxx"

#include "xset.hxx"

// clang-format off

// All builtin XSet's, custom XSet's are not added to this lookup map
const std::map<XSetName, const std::string> xset_name_map{
    // separator
    {XSetName::SEPARATOR, "separator"},

    // dev menu
    {XSetName::DEV_MENU_REMOVE, "dev_menu_remove"},
    {XSetName::DEV_MENU_UNMOUNT, "dev_menu_unmount"},
    {XSetName::DEV_MENU_OPEN, "dev_menu_open"},
    {XSetName::DEV_MENU_TAB, "dev_menu_tab"},
    {XSetName::DEV_MENU_MOUNT, "dev_menu_mount"},
    {XSetName::DEV_MENU_MARK, "dev_menu_mark"},
    {XSetName::DEV_PROP, "dev_prop"},
    {XSetName::DEV_MENU_SETTINGS, "dev_menu_settings"},

    // dev settings
    {XSetName::DEV_SHOW, "dev_show"},
    {XSetName::DEV_SHOW_INTERNAL_DRIVES, "dev_show_internal_drives"},
    {XSetName::DEV_SHOW_EMPTY, "dev_show_empty"},
    {XSetName::DEV_SHOW_PARTITION_TABLES, "dev_show_partition_tables"},
    {XSetName::DEV_SHOW_NET, "dev_show_net"},
    {XSetName::DEV_SHOW_FILE, "dev_show_file"},
    {XSetName::DEV_IGNORE_UDISKS_HIDE, "dev_ignore_udisks_hide"},
    {XSetName::DEV_SHOW_HIDE_VOLUMES, "dev_show_hide_volumes"},
    {XSetName::DEV_DISPNAME, "dev_dispname"},

    {XSetName::DEV_MENU_AUTO, "dev_menu_auto"},
    {XSetName::DEV_AUTOMOUNT_OPTICAL, "dev_automount_optical"},
    {XSetName::DEV_AUTOMOUNT_REMOVABLE, "dev_automount_removable"},
    {XSetName::DEV_IGNORE_UDISKS_NOPOLICY, "dev_ignore_udisks_nopolicy"},
    {XSetName::DEV_AUTOMOUNT_VOLUMES, "dev_automount_volumes"},
    {XSetName::DEV_AUTOMOUNT_DIRS, "dev_automount_dirs"},
    {XSetName::DEV_AUTO_OPEN, "dev_auto_open"},
    {XSetName::DEV_UNMOUNT_QUIT, "dev_unmount_quit"},

    {XSetName::DEV_EXEC, "dev_exec"},
    {XSetName::DEV_EXEC_FS, "dev_exec_fs"},
    {XSetName::DEV_EXEC_AUDIO, "dev_exec_audio"},
    {XSetName::DEV_EXEC_VIDEO, "dev_exec_video"},
    {XSetName::DEV_EXEC_INSERT, "dev_exec_insert"},
    {XSetName::DEV_EXEC_UNMOUNT, "dev_exec_unmount"},
    {XSetName::DEV_EXEC_REMOVE, "dev_exec_remove"},

    {XSetName::DEV_MOUNT_OPTIONS, "dev_mount_options"},
    {XSetName::DEV_CHANGE, "dev_change"},
    {XSetName::DEV_FS_CNF, "dev_fs_cnf"},
    {XSetName::DEV_NET_CNF, "dev_net_cnf"},

    // dev icons
    {XSetName::DEV_ICON, "dev_icon"},
    {XSetName::DEV_ICON_INTERNAL_MOUNTED, "dev_icon_internal_mounted"},
    {XSetName::DEV_ICON_INTERNAL_UNMOUNTED, "dev_icon_internal_unmounted"},
    {XSetName::DEV_ICON_REMOVE_MOUNTED, "dev_icon_remove_mounted"},
    {XSetName::DEV_ICON_REMOVE_UNMOUNTED, "dev_icon_remove_unmounted"},
    {XSetName::DEV_ICON_OPTICAL_MOUNTED, "dev_icon_optical_mounted"},
    {XSetName::DEV_ICON_OPTICAL_MEDIA, "dev_icon_optical_media"},
    {XSetName::DEV_ICON_OPTICAL_NOMEDIA, "dev_icon_optical_nomedia"},
    {XSetName::DEV_ICON_AUDIOCD, "dev_icon_audiocd"},
    {XSetName::DEV_ICON_FLOPPY_MOUNTED, "dev_icon_floppy_mounted"},
    {XSetName::DEV_ICON_FLOPPY_UNMOUNTED, "dev_icon_floppy_unmounted"},
    {XSetName::DEV_ICON_NETWORK, "dev_icon_network"},
    {XSetName::DEV_ICON_FILE, "dev_icon_file"},

    {XSetName::BOOK_OPEN, "book_open"},
    {XSetName::BOOK_SETTINGS, "book_settings"},
    {XSetName::BOOK_ICON, "book_icon"},
    {XSetName::BOOK_MENU_ICON, "book_menu_icon"},
    {XSetName::BOOK_SHOW, "book_show"},
    {XSetName::BOOK_ADD, "book_add"},
    {XSetName::MAIN_BOOK, "main_book"},

    // Rename/Move Dialog
    {XSetName::MOVE_NAME, "move_name"},
    {XSetName::MOVE_FILENAME, "move_filename"},
    {XSetName::MOVE_PARENT, "move_parent"},
    {XSetName::MOVE_PATH, "move_path"},
    {XSetName::MOVE_TYPE, "move_type"},
    {XSetName::MOVE_TARGET, "move_target"},
    {XSetName::MOVE_TEMPLATE, "move_template"},
    {XSetName::MOVE_OPTION, "move_option"},
    {XSetName::MOVE_COPY, "move_copy"},
    {XSetName::MOVE_LINK, "move_link"},
    {XSetName::MOVE_COPYT, "move_copyt"},
    {XSetName::MOVE_LINKT, "move_linkt"},
    {XSetName::MOVE_AS_ROOT, "move_as_root"},
    {XSetName::MOVE_DLG_HELP, "move_dlg_help"},
    {XSetName::MOVE_DLG_CONFIRM_CREATE, "move_dlg_confirm_create"},

    // status bar
    {XSetName::STATUS_MIDDLE, "status_middle"},
    {XSetName::STATUS_NAME, "status_name"},
    {XSetName::STATUS_PATH, "status_path"},
    {XSetName::STATUS_INFO, "status_info"},
    {XSetName::STATUS_HIDE, "status_hide"},

    // MAIN WINDOW MENUS //

    // File //
    {XSetName::MAIN_NEW_WINDOW, "main_new_window"},
    {XSetName::MAIN_ROOT_WINDOW, "main_root_window"},
    {XSetName::MAIN_SEARCH, "main_search"},
    {XSetName::MAIN_TERMINAL, "main_terminal"},
    {XSetName::MAIN_ROOT_TERMINAL, "main_root_terminal"},
    {XSetName::MAIN_SAVE_SESSION, "main_save_session"},
    {XSetName::MAIN_SAVE_TABS, "main_save_tabs"},
    {XSetName::MAIN_EXIT, "main_exit"},

    // VIEW //
    {XSetName::PANEL1_SHOW, "panel1_show"},
    {XSetName::PANEL2_SHOW, "panel2_show"},
    {XSetName::PANEL3_SHOW, "panel3_show"},
    {XSetName::PANEL4_SHOW, "panel4_show"},
    {XSetName::MAIN_PBAR, "main_pbar"},
    {XSetName::MAIN_FOCUS_PANEL, "main_focus_panel"},
    {XSetName::PANEL_PREV, "panel_prev"},
    {XSetName::PANEL_NEXT, "panel_next"},
    {XSetName::PANEL_HIDE, "panel_hide"},
    {XSetName::PANEL_1, "panel_1"},
    {XSetName::PANEL_2, "panel_2"},
    {XSetName::PANEL_3, "panel_3"},
    {XSetName::PANEL_4, "panel_4"},

    {XSetName::MAIN_AUTO, "main_auto"},
    {XSetName::AUTO_INST, "auto_inst"},
    {XSetName::EVT_START, "evt_start"},
    {XSetName::EVT_EXIT, "evt_exit"},
    {XSetName::AUTO_WIN, "auto_win"},

    {XSetName::EVT_WIN_NEW, "evt_win_new"},
    {XSetName::EVT_WIN_FOCUS, "evt_win_focus"},
    {XSetName::EVT_WIN_MOVE, "evt_win_move"},
    {XSetName::EVT_WIN_CLICK, "evt_win_click"},
    {XSetName::EVT_WIN_KEY, "evt_win_key"},
    {XSetName::EVT_WIN_CLOSE, "evt_win_close"},

    {XSetName::AUTO_PNL, "auto_pnl"},
    {XSetName::EVT_PNL_FOCUS, "evt_pnl_focus"},
    {XSetName::EVT_PNL_SHOW, "evt_pnl_show"},
    {XSetName::EVT_PNL_SEL, "evt_pnl_sel"},

    {XSetName::AUTO_TAB, "auto_tab"},
    {XSetName::EVT_TAB_NEW, "evt_tab_new"},
    {XSetName::EVT_TAB_CHDIR, "evt_tab_chdir"},
    {XSetName::EVT_TAB_FOCUS, "evt_tab_focus"},
    {XSetName::EVT_TAB_CLOSE, "evt_tab_close"},

    {XSetName::EVT_DEVICE, "evt_device"},
    {XSetName::MAIN_TITLE, "main_title"},
    {XSetName::MAIN_ICON, "main_icon"},
    {XSetName::MAIN_FULL, "main_full"},
    {XSetName::MAIN_DESIGN_MODE, "main_design_mode"},
    {XSetName::MAIN_PREFS, "main_prefs"},
    {XSetName::MAIN_TOOL, "main_tool"},
    {XSetName::ROOT_BAR, "root_bar"},
    {XSetName::VIEW_THUMB, "view_thumb"},

    // Plugins //
    {XSetName::PLUG_INSTALL, "plug_install"},
    {XSetName::PLUG_IFILE, "plug_ifile"},
    {XSetName::PLUG_COPY, "plug_copy"},
    {XSetName::PLUG_CFILE, "plug_cfile"},
    {XSetName::PLUG_CVERB, "plug_cverb"},

    // Help //
    {XSetName::MAIN_ABOUT, "main_about"},
    {XSetName::MAIN_DEV, "main_dev"},

    // Tasks //
    {XSetName::MAIN_TASKS, "main_tasks"},

    {XSetName::TASK_MANAGER, "task_manager"},

    {XSetName::TASK_COLUMNS, "task_columns"},
    {XSetName::TASK_COL_STATUS, "task_col_status"},
    {XSetName::TASK_COL_COUNT, "task_col_count"},
    {XSetName::TASK_COL_PATH, "task_col_path"},
    {XSetName::TASK_COL_FILE, "task_col_file"},
    {XSetName::TASK_COL_TO, "task_col_to"},
    {XSetName::TASK_COL_PROGRESS, "task_col_progress"},
    {XSetName::TASK_COL_TOTAL, "task_col_total"},
    {XSetName::TASK_COL_STARTED, "task_col_started"},
    {XSetName::TASK_COL_ELAPSED, "task_col_elapsed"},
    {XSetName::TASK_COL_CURSPEED, "task_col_curspeed"},
    {XSetName::TASK_COL_CUREST, "task_col_curest"},
    {XSetName::TASK_COL_AVGSPEED, "task_col_avgspeed"},
    {XSetName::TASK_COL_AVGEST, "task_col_avgest"},
    {XSetName::TASK_COL_REORDER, "task_col_reorder"},

    {XSetName::TASK_STOP, "task_stop"},
    {XSetName::TASK_PAUSE, "task_pause"},
    {XSetName::TASK_QUE, "task_que"},
    {XSetName::TASK_RESUME, "task_resume"},
    {XSetName::TASK_SHOWOUT, "task_showout"},

    {XSetName::TASK_ALL, "task_all"},
    {XSetName::TASK_STOP_ALL, "task_stop_all"},
    {XSetName::TASK_PAUSE_ALL, "task_pause_all"},
    {XSetName::TASK_QUE_ALL, "task_que_all"},
    {XSetName::TASK_RESUME_ALL, "task_resume_all"},

    {XSetName::TASK_SHOW_MANAGER, "task_show_manager"},
    {XSetName::TASK_HIDE_MANAGER, "task_hide_manager"},

    {XSetName::TASK_POPUPS, "task_popups"},
    {XSetName::TASK_POP_ALL, "task_pop_all"},
    {XSetName::TASK_POP_TOP, "task_pop_top"},
    {XSetName::TASK_POP_ABOVE, "task_pop_above"},
    {XSetName::TASK_POP_STICK, "task_pop_stick"},
    {XSetName::TASK_POP_DETAIL, "task_pop_detail"},
    {XSetName::TASK_POP_OVER, "task_pop_over"},
    {XSetName::TASK_POP_ERR, "task_pop_err"},

    {XSetName::TASK_ERRORS, "task_errors"},
    {XSetName::TASK_ERR_FIRST, "task_err_first"},
    {XSetName::TASK_ERR_ANY, "task_err_any"},
    {XSetName::TASK_ERR_CONT, "task_err_cont"},

    {XSetName::TASK_QUEUE, "task_queue"},
    {XSetName::TASK_Q_NEW, "task_q_new"},
    {XSetName::TASK_Q_SMART, "task_q_smart"},
    {XSetName::TASK_Q_PAUSE, "task_q_pause"},

    // PANELS COMMON //
    {XSetName::DATE_FORMAT, "date_format"},
    {XSetName::CON_OPEN, "con_open"},
    {XSetName::OPEN_EXECUTE, "open_execute"},
    {XSetName::OPEN_EDIT, "open_edit"},
    {XSetName::OPEN_EDIT_ROOT, "open_edit_root"},
    {XSetName::OPEN_OTHER, "open_other"},
    {XSetName::OPEN_HAND, "open_hand"},
    {XSetName::OPEN_ALL, "open_all"},
    {XSetName::OPEN_IN_TAB, "open_in_tab"},

    {XSetName::OPENTAB_NEW, "opentab_new"},
    {XSetName::OPENTAB_PREV, "opentab_prev"},
    {XSetName::OPENTAB_NEXT, "opentab_next"},
    {XSetName::OPENTAB_1, "opentab_1"},
    {XSetName::OPENTAB_2, "opentab_2"},
    {XSetName::OPENTAB_3, "opentab_3"},
    {XSetName::OPENTAB_4, "opentab_4"},
    {XSetName::OPENTAB_5, "opentab_5"},
    {XSetName::OPENTAB_6, "opentab_6"},
    {XSetName::OPENTAB_7, "opentab_7"},
    {XSetName::OPENTAB_8, "opentab_8"},
    {XSetName::OPENTAB_9, "opentab_9"},
    {XSetName::OPENTAB_10, "opentab_10"},

    {XSetName::OPEN_IN_PANEL, "open_in_panel"},
    {XSetName::OPEN_IN_PANELPREV, "open_in_panelprev"},
    {XSetName::OPEN_IN_PANELNEXT, "open_in_panelnext"},
    {XSetName::OPEN_IN_PANEL1, "open_in_panel1"},
    {XSetName::OPEN_IN_PANEL2, "open_in_panel2"},
    {XSetName::OPEN_IN_PANEL3, "open_in_panel3"},
    {XSetName::OPEN_IN_PANEL4, "open_in_panel4"},

    {XSetName::ARC_EXTRACT, "arc_extract"},
    {XSetName::ARC_EXTRACTTO, "arc_extractto"},
    {XSetName::ARC_LIST, "arc_list"},

    {XSetName::ARC_DEFAULT, "arc_default"},
    {XSetName::ARC_DEF_OPEN, "arc_def_open"},
    {XSetName::ARC_DEF_EX, "arc_def_ex"},
    {XSetName::ARC_DEF_EXTO, "arc_def_exto"},
    {XSetName::ARC_DEF_LIST, "arc_def_list"},
    {XSetName::ARC_DEF_PARENT, "arc_def_parent"},
    {XSetName::ARC_DEF_WRITE, "arc_def_write"},
    {XSetName::ARC_CONF2, "arc_conf2"},

    {XSetName::OPEN_NEW, "open_new"},
    {XSetName::NEW_FILE, "new_file"},
    {XSetName::NEW_DIRECTORY, "new_directory"},
    {XSetName::NEW_LINK, "new_link"},
    {XSetName::NEW_ARCHIVE, "new_archive"},
    {XSetName::TAB_NEW, "tab_new"},
    {XSetName::TAB_NEW_HERE, "tab_new_here"},
    {XSetName::NEW_BOOKMARK, "new_bookmark"},
    {XSetName::ARC_DLG, "arc_dlg"},

    {XSetName::NEW_APP, "new_app"},
    {XSetName::CON_GO, "con_go"},

    {XSetName::GO_REFRESH, "go_refresh"},
    {XSetName::GO_BACK, "go_back"},
    {XSetName::GO_FORWARD, "go_forward"},
    {XSetName::GO_UP, "go_up"},
    {XSetName::GO_HOME, "go_home"},
    {XSetName::GO_DEFAULT, "go_default"},
    {XSetName::GO_SET_DEFAULT, "go_set_default"},
    {XSetName::EDIT_CANON, "edit_canon"},

    {XSetName::GO_FOCUS, "go_focus"},
    {XSetName::FOCUS_PATH_BAR, "focus_path_bar"},
    {XSetName::FOCUS_FILELIST, "focus_filelist"},
    {XSetName::FOCUS_DIRTREE, "focus_dirtree"},
    {XSetName::FOCUS_BOOK, "focus_book"},
    {XSetName::FOCUS_DEVICE, "focus_device"},

    {XSetName::GO_TAB, "go_tab"},
    {XSetName::TAB_PREV, "tab_prev"},
    {XSetName::TAB_NEXT, "tab_next"},
    {XSetName::TAB_RESTORE, "tab_restore"},
    {XSetName::TAB_CLOSE, "tab_close"},
    {XSetName::TAB_1, "tab_1"},
    {XSetName::TAB_2, "tab_2"},
    {XSetName::TAB_3, "tab_3"},
    {XSetName::TAB_4, "tab_4"},
    {XSetName::TAB_5, "tab_5"},
    {XSetName::TAB_6, "tab_6"},
    {XSetName::TAB_7, "tab_7"},
    {XSetName::TAB_8, "tab_8"},
    {XSetName::TAB_9, "tab_9"},
    {XSetName::TAB_10, "tab_10"},

    {XSetName::CON_VIEW, "con_view"},
    {XSetName::VIEW_LIST_STYLE, "view_list_style"},
    {XSetName::VIEW_COLUMNS, "view_columns"},
    {XSetName::VIEW_REORDER_COL, "view_reorder_col"},
    {XSetName::RUBBERBAND, "rubberband"},

    {XSetName::VIEW_SORTBY, "view_sortby"},
    {XSetName::SORTBY_NAME, "sortby_name"},
    {XSetName::SORTBY_SIZE, "sortby_size"},
    {XSetName::SORTBY_TYPE, "sortby_type"},
    {XSetName::SORTBY_PERM, "sortby_perm"},
    {XSetName::SORTBY_OWNER, "sortby_owner"},
    {XSetName::SORTBY_DATE, "sortby_date"},
    {XSetName::SORTBY_ASCEND, "sortby_ascend"},
    {XSetName::SORTBY_DESCEND, "sortby_descend"},
    {XSetName::SORTX_ALPHANUM, "sortx_alphanum"},
    //{XSetName::SORTX_NATURAL, "sortx_natural"},
    {XSetName::SORTX_CASE, "sortx_case"},
    {XSetName::SORTX_DIRECTORIES, "sortx_directories"},
    {XSetName::SORTX_FILES, "sortx_files"},
    {XSetName::SORTX_MIX, "sortx_mix"},
    {XSetName::SORTX_HIDFIRST, "sortx_hidfirst"},
    {XSetName::SORTX_HIDLAST, "sortx_hidlast"},

    {XSetName::VIEW_REFRESH, "view_refresh"},
    {XSetName::PATH_SEEK, "path_seek"},
    {XSetName::PATH_HAND, "path_hand"},
    {XSetName::PATH_HELP, "path_help"},
    {XSetName::EDIT_CUT, "edit_cut"},
    {XSetName::EDIT_COPY, "edit_copy"},
    {XSetName::EDIT_PASTE, "edit_paste"},
    {XSetName::EDIT_RENAME, "edit_rename"},
    {XSetName::EDIT_DELETE, "edit_delete"},
    {XSetName::EDIT_TRASH, "edit_trash"},

    {XSetName::EDIT_SUBMENU, "edit_submenu"},
    {XSetName::COPY_NAME, "copy_name"},
    {XSetName::COPY_PARENT, "copy_parent"},
    {XSetName::COPY_PATH, "copy_path"},
    {XSetName::PASTE_LINK, "paste_link"},
    {XSetName::PASTE_TARGET, "paste_target"},
    {XSetName::PASTE_AS, "paste_as"},
    {XSetName::COPY_TO, "copy_to"},
    {XSetName::COPY_LOC, "copy_loc"},
    {XSetName::COPY_LOC_LAST, "copy_loc_last"},

    {XSetName::COPY_TAB, "copy_tab"},
    {XSetName::COPY_TAB_PREV, "copy_tab_prev"},
    {XSetName::COPY_TAB_NEXT, "copy_tab_next"},
    {XSetName::COPY_TAB_1, "copy_tab_1"},
    {XSetName::COPY_TAB_2, "copy_tab_2"},
    {XSetName::COPY_TAB_3, "copy_tab_3"},
    {XSetName::COPY_TAB_4, "copy_tab_4"},
    {XSetName::COPY_TAB_5, "copy_tab_5"},
    {XSetName::COPY_TAB_6, "copy_tab_6"},
    {XSetName::COPY_TAB_7, "copy_tab_7"},
    {XSetName::COPY_TAB_8, "copy_tab_8"},
    {XSetName::COPY_TAB_9, "copy_tab_9"},
    {XSetName::COPY_TAB_10, "copy_tab_10"},

    {XSetName::COPY_PANEL, "copy_panel"},
    {XSetName::COPY_PANEL_PREV, "copy_panel_prev"},
    {XSetName::COPY_PANEL_NEXT, "copy_panel_next"},
    {XSetName::COPY_PANEL_1, "copy_panel_1"},
    {XSetName::COPY_PANEL_2, "copy_panel_2"},
    {XSetName::COPY_PANEL_3, "copy_panel_3"},
    {XSetName::COPY_PANEL_4, "copy_panel_4"},

    {XSetName::MOVE_TO, "move_to"},
    {XSetName::MOVE_LOC, "move_loc"},
    {XSetName::MOVE_LOC_LAST, "move_loc_last"},

    {XSetName::MOVE_TAB, "move_tab"},
    {XSetName::MOVE_TAB_PREV, "move_tab_prev"},
    {XSetName::MOVE_TAB_NEXT, "move_tab_next"},
    {XSetName::MOVE_TAB_1, "move_tab_1"},
    {XSetName::MOVE_TAB_2, "move_tab_2"},
    {XSetName::MOVE_TAB_3, "move_tab_3"},
    {XSetName::MOVE_TAB_4, "move_tab_4"},
    {XSetName::MOVE_TAB_5, "move_tab_5"},
    {XSetName::MOVE_TAB_6, "move_tab_6"},
    {XSetName::MOVE_TAB_7, "move_tab_7"},
    {XSetName::MOVE_TAB_8, "move_tab_8"},
    {XSetName::MOVE_TAB_9, "move_tab_9"},
    {XSetName::MOVE_TAB_10, "move_tab_10"},

    {XSetName::MOVE_PANEL, "move_panel"},
    {XSetName::MOVE_PANEL_PREV, "move_panel_prev"},
    {XSetName::MOVE_PANEL_NEXT, "move_panel_next"},
    {XSetName::MOVE_PANEL_1, "move_panel_1"},
    {XSetName::MOVE_PANEL_2, "move_panel_2"},
    {XSetName::MOVE_PANEL_3, "move_panel_3"},
    {XSetName::MOVE_PANEL_4, "move_panel_4"},

    {XSetName::EDIT_HIDE, "edit_hide"},
    {XSetName::SELECT_ALL, "select_all"},
    {XSetName::SELECT_UN, "select_un"},
    {XSetName::SELECT_INVERT, "select_invert"},
    {XSetName::SELECT_PATT, "select_patt"},
    {XSetName::EDIT_ROOT, "edit_root"},
    {XSetName::ROOT_COPY_LOC, "root_copy_loc"},
    {XSetName::ROOT_MOVE2, "root_move2"},
    {XSetName::ROOT_TRASH, "root_trash"},
    {XSetName::ROOT_DELETE, "root_delete"},

    // Properties //
    {XSetName::CON_PROP, "con_prop"},
    {XSetName::PROP_INFO, "prop_info"},
    {XSetName::PROP_PERM, "prop_perm"},
    {XSetName::PROP_QUICK, "prop_quick"},

    {XSetName::PERM_R, "perm_r"},
    {XSetName::PERM_RW, "perm_rw"},
    {XSetName::PERM_RWX, "perm_rwx"},
    {XSetName::PERM_R_R, "perm_r_r"},
    {XSetName::PERM_RW_R, "perm_rw_r"},
    {XSetName::PERM_RW_RW, "perm_rw_rw"},
    {XSetName::PERM_RWXR_X, "perm_rwxr_x"},
    {XSetName::PERM_RWXRWX, "perm_rwxrwx"},
    {XSetName::PERM_R_R_R, "perm_r_r_r"},
    {XSetName::PERM_RW_R_R, "perm_rw_r_r"},
    {XSetName::PERM_RW_RW_RW, "perm_rw_rw_rw"},
    {XSetName::PERM_RWXR_R, "perm_rwxr_r"},
    {XSetName::PERM_RWXR_XR_X, "perm_rwxr_xr_x"},
    {XSetName::PERM_RWXRWXRWX, "perm_rwxrwxrwx"},
    {XSetName::PERM_RWXRWXRWT, "perm_rwxrwxrwt"},
    {XSetName::PERM_UNSTICK, "perm_unstick"},
    {XSetName::PERM_STICK, "perm_stick"},

    {XSetName::PERM_RECURS, "perm_recurs"},
    {XSetName::PERM_GO_W, "perm_go_w"},
    {XSetName::PERM_GO_RWX, "perm_go_rwx"},
    {XSetName::PERM_UGO_W, "perm_ugo_w"},
    {XSetName::PERM_UGO_RX, "perm_ugo_rx"},
    {XSetName::PERM_UGO_RWX, "perm_ugo_rwx"},

    {XSetName::PROP_ROOT, "prop_root"},
    {XSetName::RPERM_RW, "rperm_rw"},
    {XSetName::RPERM_RWX, "rperm_rwx"},
    {XSetName::RPERM_RW_R, "rperm_rw_r"},
    {XSetName::RPERM_RW_RW, "rperm_rw_rw"},
    {XSetName::RPERM_RWXR_X, "rperm_rwxr_x"},
    {XSetName::RPERM_RWXRWX, "rperm_rwxrwx"},
    {XSetName::RPERM_RW_R_R, "rperm_rw_r_r"},
    {XSetName::RPERM_RW_RW_RW, "rperm_rw_rw_rw"},
    {XSetName::RPERM_RWXR_R, "rperm_rwxr_r"},
    {XSetName::RPERM_RWXR_XR_X, "rperm_rwxr_xr_x"},
    {XSetName::RPERM_RWXRWXRWX, "rperm_rwxrwxrwx"},
    {XSetName::RPERM_RWXRWXRWT, "rperm_rwxrwxrwt"},
    {XSetName::RPERM_UNSTICK, "rperm_unstick"},
    {XSetName::RPERM_STICK, "rperm_stick"},

    {XSetName::RPERM_RECURS, "rperm_recurs"},
    {XSetName::RPERM_GO_W, "rperm_go_w"},
    {XSetName::RPERM_GO_RWX, "rperm_go_rwx"},
    {XSetName::RPERM_UGO_W, "rperm_ugo_w"},
    {XSetName::RPERM_UGO_RX, "rperm_ugo_rx"},
    {XSetName::RPERM_UGO_RWX, "rperm_ugo_rwx"},

    {XSetName::RPERM_OWN, "rperm_own"},
    {XSetName::OWN_MYUSER, "own_myuser"},
    {XSetName::OWN_MYUSER_USERS, "own_myuser_users"},
    {XSetName::OWN_USER1, "own_user1"},
    {XSetName::OWN_USER1_USERS, "own_user1_users"},
    {XSetName::OWN_USER2, "own_user2"},
    {XSetName::OWN_USER2_USERS, "own_user2_users"},
    {XSetName::OWN_ROOT, "own_root"},
    {XSetName::OWN_ROOT_USERS, "own_root_users"},
    {XSetName::OWN_ROOT_MYUSER, "own_root_myuser"},
    {XSetName::OWN_ROOT_USER1, "own_root_user1"},
    {XSetName::OWN_ROOT_USER2, "own_root_user2"},

    {XSetName::OWN_RECURS, "own_recurs"},
    {XSetName::ROWN_MYUSER, "rown_myuser"},
    {XSetName::ROWN_MYUSER_USERS, "rown_myuser_users"},
    {XSetName::ROWN_USER1, "rown_user1"},
    {XSetName::ROWN_USER1_USERS, "rown_user1_users"},
    {XSetName::ROWN_USER2, "rown_user2"},
    {XSetName::ROWN_USER2_USERS, "rown_user2_users"},
    {XSetName::ROWN_ROOT, "rown_root"},
    {XSetName::ROWN_ROOT_USERS, "rown_root_users"},
    {XSetName::ROWN_ROOT_MYUSER, "rown_root_myuser"},
    {XSetName::ROWN_ROOT_USER1, "rown_root_user1"},
    {XSetName::ROWN_ROOT_USER2, "rown_root_user2"},

    // PANELS //
    {XSetName::PANEL_SLIDERS, "panel_sliders"},

    // panel1
    {XSetName::PANEL1_SHOW_TOOLBOX, "panel1_show_toolbox"},
    {XSetName::PANEL1_SHOW_DEVMON, "panel1_show_devmon"},
    {XSetName::PANEL1_SHOW_DIRTREE, "panel1_show_dirtree"},
    {XSetName::PANEL1_SHOW_BOOK, "panel1_show_book"},
    {XSetName::PANEL1_SHOW_SIDEBAR, "panel1_show_sidebar"},
    {XSetName::PANEL1_LIST_DETAILED, "panel1_list_detailed"},
    {XSetName::PANEL1_LIST_ICONS, "panel1_list_icons"},
    {XSetName::PANEL1_LIST_COMPACT, "panel1_list_compact"},
    {XSetName::PANEL1_LIST_LARGE, "panel1_list_large"},
    {XSetName::PANEL1_SHOW_HIDDEN, "panel1_show_hidden"},
    {XSetName::PANEL1_ICON_TAB, "panel1_icon_tab"},
    {XSetName::PANEL1_ICON_STATUS, "panel1_icon_status"},
    {XSetName::PANEL1_DETCOL_NAME, "panel1_detcol_name"},
    {XSetName::PANEL1_DETCOL_SIZE, "panel1_detcol_size"},
    {XSetName::PANEL1_DETCOL_TYPE, "panel1_detcol_type"},
    {XSetName::PANEL1_DETCOL_PERM, "panel1_detcol_perm"},
    {XSetName::PANEL1_DETCOL_OWNER, "panel1_detcol_owner"},
    {XSetName::PANEL1_DETCOL_DATE, "panel1_detcol_date"},
    {XSetName::PANEL1_SORT_EXTRA, "panel1_sort_extra"},
    {XSetName::PANEL1_BOOK_FOL, "panel1_book_fol"},
    {XSetName::PANEL1_TOOL_L, "panel1_tool_l"},
    {XSetName::PANEL1_TOOL_R, "panel1_tool_r"},
    {XSetName::PANEL1_TOOL_S, "panel1_tool_s"},

    // panel2
    {XSetName::PANEL2_SHOW_TOOLBOX, "panel2_show_toolbox"},
    {XSetName::PANEL2_SHOW_DEVMON, "panel2_show_devmon"},
    {XSetName::PANEL2_SHOW_DIRTREE, "panel2_show_dirtree"},
    {XSetName::PANEL2_SHOW_BOOK, "panel2_show_book"},
    {XSetName::PANEL2_SHOW_SIDEBAR, "panel2_show_sidebar"},
    {XSetName::PANEL2_LIST_DETAILED, "panel2_list_detailed"},
    {XSetName::PANEL2_LIST_ICONS, "panel2_list_icons"},
    {XSetName::PANEL2_LIST_COMPACT, "panel2_list_compact"},
    {XSetName::PANEL2_LIST_LARGE, "panel2_list_large"},
    {XSetName::PANEL2_SHOW_HIDDEN, "panel2_show_hidden"},
    {XSetName::PANEL2_ICON_TAB, "panel2_icon_tab"},
    {XSetName::PANEL2_ICON_STATUS, "panel2_icon_status"},
    {XSetName::PANEL2_DETCOL_NAME, "panel2_detcol_name"},
    {XSetName::PANEL2_DETCOL_SIZE, "panel2_detcol_size"},
    {XSetName::PANEL2_DETCOL_TYPE, "panel2_detcol_type"},
    {XSetName::PANEL2_DETCOL_PERM, "panel2_detcol_perm"},
    {XSetName::PANEL2_DETCOL_OWNER, "panel2_detcol_owner"},
    {XSetName::PANEL2_DETCOL_DATE, "panel2_detcol_date"},
    {XSetName::PANEL2_SORT_EXTRA, "panel2_sort_extra"},
    {XSetName::PANEL2_BOOK_FOL, "panel2_book_fol"},
    {XSetName::PANEL2_TOOL_L, "panel2_tool_l"},
    {XSetName::PANEL2_TOOL_R, "panel2_tool_r"},
    {XSetName::PANEL2_TOOL_S, "panel2_tool_s"},

    // panel3
    {XSetName::PANEL3_SHOW_TOOLBOX, "panel3_show_toolbox"},
    {XSetName::PANEL3_SHOW_DEVMON, "panel3_show_devmon"},
    {XSetName::PANEL3_SHOW_DIRTREE, "panel3_show_dirtree"},
    {XSetName::PANEL3_SHOW_BOOK, "panel3_show_book"},
    {XSetName::PANEL3_SHOW_SIDEBAR, "panel3_show_sidebar"},
    {XSetName::PANEL3_LIST_DETAILED, "panel3_list_detailed"},
    {XSetName::PANEL3_LIST_ICONS, "panel3_list_icons"},
    {XSetName::PANEL3_LIST_COMPACT, "panel3_list_compact"},
    {XSetName::PANEL3_LIST_LARGE, "panel3_list_large"},
    {XSetName::PANEL3_SHOW_HIDDEN, "panel3_show_hidden"},
    {XSetName::PANEL3_ICON_TAB, "panel3_icon_tab"},
    {XSetName::PANEL3_ICON_STATUS, "panel3_icon_status"},
    {XSetName::PANEL3_DETCOL_NAME, "panel3_detcol_name"},
    {XSetName::PANEL3_DETCOL_SIZE, "panel3_detcol_size"},
    {XSetName::PANEL3_DETCOL_TYPE, "panel3_detcol_type"},
    {XSetName::PANEL3_DETCOL_PERM, "panel3_detcol_perm"},
    {XSetName::PANEL3_DETCOL_OWNER, "panel3_detcol_owner"},
    {XSetName::PANEL3_DETCOL_DATE, "panel3_detcol_date"},
    {XSetName::PANEL3_SORT_EXTRA, "panel3_sort_extra"},
    {XSetName::PANEL3_BOOK_FOL, "panel3_book_fol"},
    {XSetName::PANEL3_TOOL_L, "panel3_tool_l"},
    {XSetName::PANEL3_TOOL_R, "panel3_tool_r"},
    {XSetName::PANEL3_TOOL_S, "panel3_tool_s"},

    // panel4
    {XSetName::PANEL4_SHOW_TOOLBOX, "panel4_show_toolbox"},
    {XSetName::PANEL4_SHOW_DEVMON, "panel4_show_devmon"},
    {XSetName::PANEL4_SHOW_DIRTREE, "panel4_show_dirtree"},
    {XSetName::PANEL4_SHOW_BOOK, "panel4_show_book"},
    {XSetName::PANEL4_SHOW_SIDEBAR, "panel4_show_sidebar"},
    {XSetName::PANEL4_LIST_DETAILED, "panel4_list_detailed"},
    {XSetName::PANEL4_LIST_ICONS, "panel4_list_icons"},
    {XSetName::PANEL4_LIST_COMPACT, "panel4_list_compact"},
    {XSetName::PANEL4_LIST_LARGE, "panel4_list_large"},
    {XSetName::PANEL4_SHOW_HIDDEN, "panel4_show_hidden"},
    {XSetName::PANEL4_ICON_TAB, "panel4_icon_tab"},
    {XSetName::PANEL4_ICON_STATUS, "panel4_icon_status"},
    {XSetName::PANEL4_DETCOL_NAME, "panel4_detcol_name"},
    {XSetName::PANEL4_DETCOL_SIZE, "panel4_detcol_size"},
    {XSetName::PANEL4_DETCOL_TYPE, "panel4_detcol_type"},
    {XSetName::PANEL4_DETCOL_PERM, "panel4_detcol_perm"},
    {XSetName::PANEL4_DETCOL_OWNER, "panel4_detcol_owner"},
    {XSetName::PANEL4_DETCOL_DATE, "panel4_detcol_date"},
    {XSetName::PANEL4_SORT_EXTRA, "panel4_sort_extra"},
    {XSetName::PANEL4_BOOK_FOL, "panel4_book_fol"},
    {XSetName::PANEL4_TOOL_L, "panel4_tool_l"},
    {XSetName::PANEL4_TOOL_R, "panel4_tool_r"},
    {XSetName::PANEL4_TOOL_S, "panel4_tool_s"},

    // speed
    {XSetName::BOOK_NEWTAB, "book_newtab"},
    {XSetName::BOOK_SINGLE, "book_single"},
    {XSetName::DEV_NEWTAB, "dev_newtab"},
    {XSetName::DEV_SINGLE, "dev_single"},

    // dialog
    {XSetName::APP_DLG, "app_dlg"},
    {XSetName::CONTEXT_DLG, "context_dlg"},
    {XSetName::FILE_DLG, "file_dlg"},
    {XSetName::TEXT_DLG, "text_dlg"},

    // other
    {XSetName::CONFIG_VERSION, "config_version"},
    {XSetName::BOOK_NEW, "book_new"},
    {XSetName::DRAG_ACTION, "drag_action"},
    {XSetName::EDITOR, "editor"},
    {XSetName::ROOT_EDITOR, "root_editor"},
    {XSetName::SU_COMMAND, "su_command"},

    // HANDLERS //

    // handlers arc
    {XSetName::HAND_ARC_7Z, "hand_arc_+7z"},
    {XSetName::HAND_ARC_GZ, "hand_arc_+gz"},
    {XSetName::HAND_ARC_RAR, "hand_arc_+rar"},
    {XSetName::HAND_ARC_TAR, "hand_arc_+tar"},
    {XSetName::HAND_ARC_TAR_BZ2, "hand_arc_+tar_bz2"},
    {XSetName::HAND_ARC_TAR_GZ, "hand_arc_+tar_gz"},
    {XSetName::HAND_ARC_TAR_LZ4, "hand_arc_+tar_lz4"},
    {XSetName::HAND_ARC_TAR_XZ, "hand_arc_+tar_xz"},
    {XSetName::HAND_ARC_TAR_ZST, "hand_arc_+tar_zst"},
    {XSetName::HAND_ARC_LZ4, "hand_arc_+lz4"},
    {XSetName::HAND_ARC_XZ, "hand_arc_+xz"},
    {XSetName::HAND_ARC_ZIP, "hand_arc_+zip"},
    {XSetName::HAND_ARC_ZST, "hand_arc_+zst"},

    // handlers file
    {XSetName::HAND_F_ISO, "hand_f_+iso"},

    // handlers fs
    {XSetName::HAND_FS_DEF, "hand_fs_+def"},
    {XSetName::HAND_FS_FUSEISO, "hand_fs_+fuseiso"},
    {XSetName::HAND_FS_UDISO, "hand_fs_+udiso"},

    // handlers net
    {XSetName::HAND_NET_FTP, "hand_net_+ftp"},
    {XSetName::HAND_NET_FUSE, "hand_net_+fuse"},
    {XSetName::HAND_NET_FUSESMB, "hand_net_+fusesmb"},
    {XSetName::HAND_NET_GPHOTO, "hand_net_+gphoto"},
    {XSetName::HAND_NET_HTTP, "hand_net_+http"},
    {XSetName::HAND_NET_IFUSE, "hand_net_+ifuse"},
    {XSetName::HAND_NET_MTP, "hand_net_+mtp"},
    {XSetName::HAND_NET_SSH, "hand_net_+ssh"},
    {XSetName::HAND_NET_UDEVIL, "hand_net_+udevil"},
    {XSetName::HAND_NET_UDEVILSMB, "hand_net_+udevilsmb"},

    // other
    {XSetName::TOOL_L, "tool_l"},
    {XSetName::TOOL_R, "tool_r"},
    {XSetName::TOOL_S, "tool_s"},
};

// clang-format on

#ifdef XSET_MAP_TEST
bool
is_in_xset_map_test(const std::string& name)
{
    for (auto it = xset_name_map.begin(); it != xset_name_map.end(); ++it)
    {
        if (ztd::same(name, it->second))
            return true;
    }
    return false;
}

bool
is_in_xset_map_test(XSetName name)
{
    try
    {
        xset_name_map.at(name);
    }
    catch (std::out_of_range)
    {
        return false;
    }
    return true;
}
#endif

XSetName
translate_xset_name_to(const std::string& name)
{
    for (auto it = xset_name_map.begin(); it != xset_name_map.end(); ++it)
    {
        if (ztd::same(name, it->second))
            return it->first;
    }
    return XSetName::CUSTOM;
}

const std::string&
translate_xset_name_from(XSetName name)
{
    try
    {
        return xset_name_map.at(name);
    }
    catch (std::out_of_range)
    {
        std::string msg = fmt::format("XSetName:: Not Implemented: {}", INT(name));
        throw InvalidXSetName(msg);
    }
}
