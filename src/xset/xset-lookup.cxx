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

#include <format>

#include <unordered_map>

#include <cassert>

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "types.hxx"

#include "xset/xset-lookup.hxx"

// #define XSET_MAGIC_ENUM_LOOKUP

using namespace std::literals::string_view_literals;

// clang-format off

// All builtin XSet's, custom XSet's are not added to this lookup map
static const std::unordered_map<xset::name, std::string_view> xset_name_map{
    // separator
    {xset::name::separator,                   "separator"sv},

    // dev menu
    {xset::name::dev_menu_remove,             "dev_menu_remove"sv},
    {xset::name::dev_menu_unmount,            "dev_menu_unmount"sv},
    {xset::name::dev_menu_open,               "dev_menu_open"sv},
    {xset::name::dev_menu_tab,                "dev_menu_tab"sv},
    {xset::name::dev_menu_mount,              "dev_menu_mount"sv},
    {xset::name::dev_menu_mark,               "dev_menu_mark"sv},
    {xset::name::dev_menu_settings,           "dev_menu_settings"sv},

    // dev settings
    {xset::name::dev_show,                    "dev_show"sv},
    {xset::name::dev_show_internal_drives,    "dev_show_internal_drives"sv},
    {xset::name::dev_show_empty,              "dev_show_empty"sv},
    {xset::name::dev_show_partition_tables,   "dev_show_partition_tables"sv},
    {xset::name::dev_show_net,                "dev_show_net"sv},
    {xset::name::dev_show_file,               "dev_show_file"sv},
    {xset::name::dev_ignore_udisks_hide,      "dev_ignore_udisks_hide"sv},
    {xset::name::dev_show_hide_volumes,       "dev_show_hide_volumes"sv},
    {xset::name::dev_dispname,                "dev_dispname"sv},

    {xset::name::dev_menu_auto,               "dev_menu_auto"sv},
    {xset::name::dev_automount_optical,       "dev_automount_optical"sv},
    {xset::name::dev_automount_removable,     "dev_automount_removable"sv},
    {xset::name::dev_ignore_udisks_nopolicy,  "dev_ignore_udisks_nopolicy"sv},
    {xset::name::dev_automount_volumes,       "dev_automount_volumes"sv},
    {xset::name::dev_automount_dirs,          "dev_automount_dirs"sv},
    {xset::name::dev_auto_open,               "dev_auto_open"sv},
    {xset::name::dev_unmount_quit,            "dev_unmount_quit"sv},

    {xset::name::dev_exec,                    "dev_exec"sv},
    {xset::name::dev_exec_fs,                 "dev_exec_fs"sv},
    {xset::name::dev_exec_audio,              "dev_exec_audio"sv},
    {xset::name::dev_exec_video,              "dev_exec_video"sv},
    {xset::name::dev_exec_insert,             "dev_exec_insert"sv},
    {xset::name::dev_exec_unmount,            "dev_exec_unmount"sv},
    {xset::name::dev_exec_remove,             "dev_exec_remove"sv},

    {xset::name::dev_mount_options,           "dev_mount_options"sv},
    {xset::name::dev_change,                  "dev_change"sv},
    {xset::name::dev_fs_cnf,                  "dev_fs_cnf"sv},
    {xset::name::dev_net_cnf,                 "dev_net_cnf"sv},

    // dev icons
    {xset::name::dev_icon,                    "dev_icon"sv},
    {xset::name::dev_icon_internal_mounted,   "dev_icon_internal_mounted"sv},
    {xset::name::dev_icon_internal_unmounted, "dev_icon_internal_unmounted"sv},
    {xset::name::dev_icon_remove_mounted,     "dev_icon_remove_mounted"sv},
    {xset::name::dev_icon_remove_unmounted,   "dev_icon_remove_unmounted"sv},
    {xset::name::dev_icon_optical_mounted,    "dev_icon_optical_mounted"sv},
    {xset::name::dev_icon_optical_media,      "dev_icon_optical_media"sv},
    {xset::name::dev_icon_optical_nomedia,    "dev_icon_optical_nomedia"sv},
    {xset::name::dev_icon_audiocd,            "dev_icon_audiocd"sv},
    {xset::name::dev_icon_floppy_mounted,     "dev_icon_floppy_mounted"sv},
    {xset::name::dev_icon_floppy_unmounted,   "dev_icon_floppy_unmounted"sv},
    {xset::name::dev_icon_network,            "dev_icon_network"sv},
    {xset::name::dev_icon_file,               "dev_icon_file"sv},

    {xset::name::book_open,                   "book_open"sv},
    {xset::name::book_settings,               "book_settings"sv},
    {xset::name::book_icon,                   "book_icon"sv},
    {xset::name::book_menu_icon,              "book_menu_icon"sv},
    {xset::name::book_show,                   "book_show"sv},
    {xset::name::book_add,                    "book_add"sv},
    {xset::name::main_book,                   "main_book"sv},

    // fonts
    {xset::name::font_general,                "font_general"sv},
    {xset::name::font_view_icon,              "font_view_icon"sv},
    {xset::name::font_view_compact,           "font_view_compact"sv},

    // rename/move dialog
    {xset::name::move_name,                   "move_name"sv},
    {xset::name::move_filename,               "move_filename"sv},
    {xset::name::move_parent,                 "move_parent"sv},
    {xset::name::move_path,                   "move_path"sv},
    {xset::name::move_type,                   "move_type"sv},
    {xset::name::move_target,                 "move_target"sv},
    {xset::name::move_template,               "move_template"sv},
    {xset::name::move_option,                 "move_option"sv},
    {xset::name::move_copy,                   "move_copy"sv},
    {xset::name::move_link,                   "move_link"sv},
    {xset::name::move_copyt,                  "move_copyt"sv},
    {xset::name::move_linkt,                  "move_linkt"sv},
    {xset::name::move_as_root,                "move_as_root"sv},
    {xset::name::move_dlg_help,               "move_dlg_help"sv},
    {xset::name::move_dlg_confirm_create,     "move_dlg_confirm_create"sv},

    // status bar
    {xset::name::status_middle,               "status_middle"sv},
    {xset::name::status_name,                 "status_name"sv},
    {xset::name::status_path,                 "status_path"sv},
    {xset::name::status_info,                 "status_info"sv},
    {xset::name::status_hide,                 "status_hide"sv},

    // main window menus //

    // file //
    {xset::name::main_new_window,             "main_new_window"sv},
    {xset::name::main_root_window,            "main_root_window"sv},
    {xset::name::main_search,                 "main_search"sv},
    {xset::name::main_terminal,               "main_terminal"sv},
    {xset::name::main_root_terminal,          "main_root_terminal"sv},
    {xset::name::main_save_session,           "main_save_session"sv},
    {xset::name::main_save_tabs,              "main_save_tabs"sv},
    {xset::name::main_exit,                   "main_exit"sv},

    // view //
    {xset::name::panel1_show,                 "panel1_show"sv},
    {xset::name::panel2_show,                 "panel2_show"sv},
    {xset::name::panel3_show,                 "panel3_show"sv},
    {xset::name::panel4_show,                 "panel4_show"sv},
    {xset::name::main_focus_panel,            "main_focus_panel"sv},
    {xset::name::panel_prev,                  "panel_prev"sv},
    {xset::name::panel_next,                  "panel_next"sv},
    {xset::name::panel_hide,                  "panel_hide"sv},
    {xset::name::panel_1,                     "panel_1"sv},
    {xset::name::panel_2,                     "panel_2"sv},
    {xset::name::panel_3,                     "panel_3"sv},
    {xset::name::panel_4,                     "panel_4"sv},

    {xset::name::main_auto,                   "main_auto"sv},
    {xset::name::auto_inst,                   "auto_inst"sv},
    {xset::name::evt_start,                   "evt_start"sv},
    {xset::name::evt_exit,                    "evt_exit"sv},
    {xset::name::auto_win,                    "auto_win"sv},

    {xset::name::evt_win_new,                 "evt_win_new"sv},
    {xset::name::evt_win_focus,               "evt_win_focus"sv},
    {xset::name::evt_win_move,                "evt_win_move"sv},
    {xset::name::evt_win_click,               "evt_win_click"sv},
    {xset::name::evt_win_key,                 "evt_win_key"sv},
    {xset::name::evt_win_close,               "evt_win_close"sv},

    {xset::name::auto_pnl,                    "auto_pnl"sv},
    {xset::name::evt_pnl_focus,               "evt_pnl_focus"sv},
    {xset::name::evt_pnl_show,                "evt_pnl_show"sv},
    {xset::name::evt_pnl_sel,                 "evt_pnl_sel"sv},

    {xset::name::auto_tab,                    "auto_tab"sv},
    {xset::name::evt_tab_new,                 "evt_tab_new"sv},
    {xset::name::evt_tab_chdir,               "evt_tab_chdir"sv},
    {xset::name::evt_tab_focus,               "evt_tab_focus"sv},
    {xset::name::evt_tab_close,               "evt_tab_close"sv},

    {xset::name::evt_device,                  "evt_device"sv},
    {xset::name::main_title,                  "main_title"sv},
    {xset::name::main_icon,                   "main_icon"sv},
    {xset::name::main_full,                   "main_full"sv},
    {xset::name::main_design_mode,            "main_design_mode"sv},
    {xset::name::main_prefs,                  "main_prefs"sv},
    {xset::name::main_tool,                   "main_tool"sv},
    {xset::name::root_bar,                    "root_bar"sv},
    {xset::name::view_thumb,                  "view_thumb"sv},

    // plugins //
    {xset::name::plug_install,                "plug_install"sv},
    {xset::name::plug_ifile,                  "plug_ifile"sv},
    {xset::name::plug_copy,                   "plug_copy"sv},
    {xset::name::plug_cfile,                  "plug_cfile"sv},
    {xset::name::plug_cverb,                  "plug_cverb"sv},

    // help //
    {xset::name::main_about,                  "main_about"sv},
    {xset::name::main_dev,                    "main_dev"sv},

    // tasks //
    {xset::name::main_tasks,                  "main_tasks"sv},

    {xset::name::task_manager,                "task_manager"sv},

    {xset::name::task_columns,                "task_columns"sv},
    {xset::name::task_col_status,             "task_col_status"sv},
    {xset::name::task_col_count,              "task_col_count"sv},
    {xset::name::task_col_path,               "task_col_path"sv},
    {xset::name::task_col_file,               "task_col_file"sv},
    {xset::name::task_col_to,                 "task_col_to"sv},
    {xset::name::task_col_progress,           "task_col_progress"sv},
    {xset::name::task_col_total,              "task_col_total"sv},
    {xset::name::task_col_started,            "task_col_started"sv},
    {xset::name::task_col_elapsed,            "task_col_elapsed"sv},
    {xset::name::task_col_curspeed,           "task_col_curspeed"sv},
    {xset::name::task_col_curest,             "task_col_curest"sv},
    {xset::name::task_col_avgspeed,           "task_col_avgspeed"sv},
    {xset::name::task_col_avgest,             "task_col_avgest"sv},
    {xset::name::task_col_reorder,            "task_col_reorder"sv},

    {xset::name::task_stop,                   "task_stop"sv},
    {xset::name::task_pause,                  "task_pause"sv},
    {xset::name::task_que,                    "task_que"sv},
    {xset::name::task_resume,                 "task_resume"sv},
    {xset::name::task_showout,                "task_showout"sv},

    {xset::name::task_all,                    "task_all"sv},
    {xset::name::task_stop_all,               "task_stop_all"sv},
    {xset::name::task_pause_all,              "task_pause_all"sv},
    {xset::name::task_que_all,                "task_que_all"sv},
    {xset::name::task_resume_all,             "task_resume_all"sv},

    {xset::name::task_show_manager,           "task_show_manager"sv},
    {xset::name::task_hide_manager,           "task_hide_manager"sv},

    {xset::name::task_popups,                 "task_popups"sv},
    {xset::name::task_pop_all,                "task_pop_all"sv},
    {xset::name::task_pop_top,                "task_pop_top"sv},
    {xset::name::task_pop_above,              "task_pop_above"sv},
    {xset::name::task_pop_stick,              "task_pop_stick"sv},
    {xset::name::task_pop_detail,             "task_pop_detail"sv},
    {xset::name::task_pop_over,               "task_pop_over"sv},
    {xset::name::task_pop_err,                "task_pop_err"sv},

    {xset::name::task_errors,                 "task_errors"sv},
    {xset::name::task_err_first,              "task_err_first"sv},
    {xset::name::task_err_any,                "task_err_any"sv},
    {xset::name::task_err_cont,               "task_err_cont"sv},

    {xset::name::task_queue,                  "task_queue"sv},
    {xset::name::task_q_new,                  "task_q_new"sv},
    {xset::name::task_q_smart,                "task_q_smart"sv},
    {xset::name::task_q_pause,                "task_q_pause"sv},

    // panels common //
    {xset::name::date_format,                 "date_format"sv},
    {xset::name::con_open,                    "con_open"sv},
    {xset::name::open_execute,                "open_execute"sv},
    {xset::name::open_edit,                   "open_edit"sv},
    {xset::name::open_edit_root,              "open_edit_root"sv},
    {xset::name::open_other,                  "open_other"sv},
    {xset::name::open_hand,                   "open_hand"sv},
    {xset::name::open_all,                    "open_all"sv},
    {xset::name::open_in_tab,                 "open_in_tab"sv},

    {xset::name::opentab_new,                 "opentab_new"sv},
    {xset::name::opentab_prev,                "opentab_prev"sv},
    {xset::name::opentab_next,                "opentab_next"sv},
    {xset::name::opentab_1,                   "opentab_1"sv},
    {xset::name::opentab_2,                   "opentab_2"sv},
    {xset::name::opentab_3,                   "opentab_3"sv},
    {xset::name::opentab_4,                   "opentab_4"sv},
    {xset::name::opentab_5,                   "opentab_5"sv},
    {xset::name::opentab_6,                   "opentab_6"sv},
    {xset::name::opentab_7,                   "opentab_7"sv},
    {xset::name::opentab_8,                   "opentab_8"sv},
    {xset::name::opentab_9,                   "opentab_9"sv},
    {xset::name::opentab_10,                  "opentab_10"sv},

    {xset::name::open_in_panel,               "open_in_panel"sv},
    {xset::name::open_in_panelprev,           "open_in_panelprev"sv},
    {xset::name::open_in_panelnext,           "open_in_panelnext"sv},
    {xset::name::open_in_panel1,              "open_in_panel1"sv},
    {xset::name::open_in_panel2,              "open_in_panel2"sv},
    {xset::name::open_in_panel3,              "open_in_panel3"sv},
    {xset::name::open_in_panel4,              "open_in_panel4"sv},

    {xset::name::arc_extract,                 "arc_extract"sv},
    {xset::name::arc_extractto,               "arc_extractto"sv},
    {xset::name::arc_list,                    "arc_list"sv},

    {xset::name::arc_default,                 "arc_default"sv},
    {xset::name::arc_def_open,                "arc_def_open"sv},
    {xset::name::arc_def_ex,                  "arc_def_ex"sv},
    {xset::name::arc_def_exto,                "arc_def_exto"sv},
    {xset::name::arc_def_list,                "arc_def_list"sv},
    {xset::name::arc_def_parent,              "arc_def_parent"sv},
    {xset::name::arc_def_write,               "arc_def_write"sv},
    {xset::name::arc_conf2,                   "arc_conf2"sv},

    {xset::name::open_new,                    "open_new"sv},
    {xset::name::new_file,                    "new_file"sv},
    {xset::name::new_directory,               "new_directory"sv},
    {xset::name::new_link,                    "new_link"sv},
    {xset::name::new_archive,                 "new_archive"sv},
    {xset::name::tab_new,                     "tab_new"sv},
    {xset::name::tab_new_here,                "tab_new_here"sv},
    {xset::name::new_bookmark,                "new_bookmark"sv},
    {xset::name::arc_dlg,                     "arc_dlg"sv},

    {xset::name::new_app,                     "new_app"sv},
    {xset::name::con_go,                      "con_go"sv},

    {xset::name::go_refresh,                  "go_refresh"sv},
    {xset::name::go_back,                     "go_back"sv},
    {xset::name::go_forward,                  "go_forward"sv},
    {xset::name::go_up,                       "go_up"sv},
    {xset::name::go_home,                     "go_home"sv},
    {xset::name::go_default,                  "go_default"sv},
    {xset::name::go_set_default,              "go_set_default"sv},
    {xset::name::edit_canon,                  "edit_canon"sv},

    {xset::name::go_focus,                    "go_focus"sv},
    {xset::name::focus_path_bar,              "focus_path_bar"sv},
    {xset::name::focus_filelist,              "focus_filelist"sv},
    {xset::name::focus_dirtree,               "focus_dirtree"sv},
    {xset::name::focus_book,                  "focus_book"sv},
    {xset::name::focus_device,                "focus_device"sv},

    {xset::name::go_tab,                      "go_tab"sv},
    {xset::name::tab_prev,                    "tab_prev"sv},
    {xset::name::tab_next,                    "tab_next"sv},
    {xset::name::tab_restore,                 "tab_restore"sv},
    {xset::name::tab_close,                   "tab_close"sv},
    {xset::name::tab_1,                       "tab_1"sv},
    {xset::name::tab_2,                       "tab_2"sv},
    {xset::name::tab_3,                       "tab_3"sv},
    {xset::name::tab_4,                       "tab_4"sv},
    {xset::name::tab_5,                       "tab_5"sv},
    {xset::name::tab_6,                       "tab_6"sv},
    {xset::name::tab_7,                       "tab_7"sv},
    {xset::name::tab_8,                       "tab_8"sv},
    {xset::name::tab_9,                       "tab_9"sv},
    {xset::name::tab_10,                      "tab_10"sv},

    {xset::name::con_view,                    "con_view"sv},
    {xset::name::view_list_style,             "view_list_style"sv},
    {xset::name::view_columns,                "view_columns"sv},
    {xset::name::view_reorder_col,            "view_reorder_col"sv},
    {xset::name::rubberband,                  "rubberband"sv},

    {xset::name::view_sortby,                 "view_sortby"sv},
    {xset::name::sortby_name,                 "sortby_name"sv},
    {xset::name::sortby_size,                 "sortby_size"sv},
    {xset::name::sortby_type,                 "sortby_type"sv},
    {xset::name::sortby_perm,                 "sortby_perm"sv},
    {xset::name::sortby_owner,                "sortby_owner"sv},
    {xset::name::sortby_date,                 "sortby_date"sv},
    {xset::name::sortby_ascend,               "sortby_ascend"sv},
    {xset::name::sortby_descend,              "sortby_descend"sv},
    {xset::name::sortx_alphanum,              "sortx_alphanum"sv},
    // {xset::name::sortx_natural,            "sortx_natural"sv},
    {xset::name::sortx_case,                  "sortx_case"sv},
    {xset::name::sortx_directories,           "sortx_directories"sv},
    {xset::name::sortx_files,                 "sortx_files"sv},
    {xset::name::sortx_mix,                   "sortx_mix"sv},
    {xset::name::sortx_hidfirst,              "sortx_hidfirst"sv},
    {xset::name::sortx_hidlast,               "sortx_hidlast"sv},

    {xset::name::view_refresh,                "view_refresh"sv},
    {xset::name::path_seek,                   "path_seek"sv},
    {xset::name::path_hand,                   "path_hand"sv},
    {xset::name::edit_cut,                    "edit_cut"sv},
    {xset::name::edit_copy,                   "edit_copy"sv},
    {xset::name::edit_paste,                  "edit_paste"sv},
    {xset::name::edit_rename,                 "edit_rename"sv},
    {xset::name::edit_delete,                 "edit_delete"sv},
    {xset::name::edit_trash,                  "edit_trash"sv},

    {xset::name::edit_submenu,                "edit_submenu"sv},
    {xset::name::copy_name,                   "copy_name"sv},
    {xset::name::copy_parent,                 "copy_parent"sv},
    {xset::name::copy_path,                   "copy_path"sv},
    {xset::name::paste_link,                  "paste_link"sv},
    {xset::name::paste_target,                "paste_target"sv},
    {xset::name::paste_as,                    "paste_as"sv},
    {xset::name::copy_to,                     "copy_to"sv},
    {xset::name::copy_loc,                    "copy_loc"sv},
    {xset::name::copy_loc_last,               "copy_loc_last"sv},

    {xset::name::copy_tab,                    "copy_tab"sv},
    {xset::name::copy_tab_prev,               "copy_tab_prev"sv},
    {xset::name::copy_tab_next,               "copy_tab_next"sv},
    {xset::name::copy_tab_1,                  "copy_tab_1"sv},
    {xset::name::copy_tab_2,                  "copy_tab_2"sv},
    {xset::name::copy_tab_3,                  "copy_tab_3"sv},
    {xset::name::copy_tab_4,                  "copy_tab_4"sv},
    {xset::name::copy_tab_5,                  "copy_tab_5"sv},
    {xset::name::copy_tab_6,                  "copy_tab_6"sv},
    {xset::name::copy_tab_7,                  "copy_tab_7"sv},
    {xset::name::copy_tab_8,                  "copy_tab_8"sv},
    {xset::name::copy_tab_9,                  "copy_tab_9"sv},
    {xset::name::copy_tab_10,                 "copy_tab_10"sv},

    {xset::name::copy_panel,                  "copy_panel"sv},
    {xset::name::copy_panel_prev,             "copy_panel_prev"sv},
    {xset::name::copy_panel_next,             "copy_panel_next"sv},
    {xset::name::copy_panel_1,                "copy_panel_1"sv},
    {xset::name::copy_panel_2,                "copy_panel_2"sv},
    {xset::name::copy_panel_3,                "copy_panel_3"sv},
    {xset::name::copy_panel_4,                "copy_panel_4"sv},

    {xset::name::move_to,                     "move_to"sv},
    {xset::name::move_loc,                    "move_loc"sv},
    {xset::name::move_loc_last,               "move_loc_last"sv},

    {xset::name::move_tab,                    "move_tab"sv},
    {xset::name::move_tab_prev,               "move_tab_prev"sv},
    {xset::name::move_tab_next,               "move_tab_next"sv},
    {xset::name::move_tab_1,                  "move_tab_1"sv},
    {xset::name::move_tab_2,                  "move_tab_2"sv},
    {xset::name::move_tab_3,                  "move_tab_3"sv},
    {xset::name::move_tab_4,                  "move_tab_4"sv},
    {xset::name::move_tab_5,                  "move_tab_5"sv},
    {xset::name::move_tab_6,                  "move_tab_6"sv},
    {xset::name::move_tab_7,                  "move_tab_7"sv},
    {xset::name::move_tab_8,                  "move_tab_8"sv},
    {xset::name::move_tab_9,                  "move_tab_9"sv},
    {xset::name::move_tab_10,                 "move_tab_10"sv},

    {xset::name::move_panel,                  "move_panel"sv},
    {xset::name::move_panel_prev,             "move_panel_prev"sv},
    {xset::name::move_panel_next,             "move_panel_next"sv},
    {xset::name::move_panel_1,                "move_panel_1"sv},
    {xset::name::move_panel_2,                "move_panel_2"sv},
    {xset::name::move_panel_3,                "move_panel_3"sv},
    {xset::name::move_panel_4,                "move_panel_4"sv},

    {xset::name::edit_hide,                   "edit_hide"sv},
    {xset::name::select_all,                  "select_all"sv},
    {xset::name::select_un,                   "select_un"sv},
    {xset::name::select_invert,               "select_invert"sv},
    {xset::name::select_patt,                 "select_patt"sv},
    {xset::name::edit_root,                   "edit_root"sv},
    {xset::name::root_copy_loc,               "root_copy_loc"sv},
    {xset::name::root_move2,                  "root_move2"sv},
    {xset::name::root_trash,                  "root_trash"sv},
    {xset::name::root_delete,                 "root_delete"sv},

    // properties //
    {xset::name::con_prop,                    "con_prop"sv},
    {xset::name::prop_info,                   "prop_info"sv},
    {xset::name::prop_perm,                   "prop_perm"sv},
    {xset::name::prop_quick,                  "prop_quick"sv},

    {xset::name::perm_r,                      "perm_r"sv},
    {xset::name::perm_rw,                     "perm_rw"sv},
    {xset::name::perm_rwx,                    "perm_rwx"sv},
    {xset::name::perm_r_r,                    "perm_r_r"sv},
    {xset::name::perm_rw_r,                   "perm_rw_r"sv},
    {xset::name::perm_rw_rw,                  "perm_rw_rw"sv},
    {xset::name::perm_rwxr_x,                 "perm_rwxr_x"sv},
    {xset::name::perm_rwxrwx,                 "perm_rwxrwx"sv},
    {xset::name::perm_r_r_r,                  "perm_r_r_r"sv},
    {xset::name::perm_rw_r_r,                 "perm_rw_r_r"sv},
    {xset::name::perm_rw_rw_rw,               "perm_rw_rw_rw"sv},
    {xset::name::perm_rwxr_r,                 "perm_rwxr_r"sv},
    {xset::name::perm_rwxr_xr_x,              "perm_rwxr_xr_x"sv},
    {xset::name::perm_rwxrwxrwx,              "perm_rwxrwxrwx"sv},
    {xset::name::perm_rwxrwxrwt,              "perm_rwxrwxrwt"sv},
    {xset::name::perm_unstick,                "perm_unstick"sv},
    {xset::name::perm_stick,                  "perm_stick"sv},

    {xset::name::perm_recurs,                 "perm_recurs"sv},
    {xset::name::perm_go_w,                   "perm_go_w"sv},
    {xset::name::perm_go_rwx,                 "perm_go_rwx"sv},
    {xset::name::perm_ugo_w,                  "perm_ugo_w"sv},
    {xset::name::perm_ugo_rx,                 "perm_ugo_rx"sv},
    {xset::name::perm_ugo_rwx,                "perm_ugo_rwx"sv},

    {xset::name::prop_root,                   "prop_root"sv},
    {xset::name::rperm_rw,                    "rperm_rw"sv},
    {xset::name::rperm_rwx,                   "rperm_rwx"sv},
    {xset::name::rperm_rw_r,                  "rperm_rw_r"sv},
    {xset::name::rperm_rw_rw,                 "rperm_rw_rw"sv},
    {xset::name::rperm_rwxr_x,                "rperm_rwxr_x"sv},
    {xset::name::rperm_rwxrwx,                "rperm_rwxrwx"sv},
    {xset::name::rperm_rw_r_r,                "rperm_rw_r_r"sv},
    {xset::name::rperm_rw_rw_rw,              "rperm_rw_rw_rw"sv},
    {xset::name::rperm_rwxr_r,                "rperm_rwxr_r"sv},
    {xset::name::rperm_rwxr_xr_x,             "rperm_rwxr_xr_x"sv},
    {xset::name::rperm_rwxrwxrwx,             "rperm_rwxrwxrwx"sv},
    {xset::name::rperm_rwxrwxrwt,             "rperm_rwxrwxrwt"sv},
    {xset::name::rperm_unstick,               "rperm_unstick"sv},
    {xset::name::rperm_stick,                 "rperm_stick"sv},

    {xset::name::rperm_recurs,                "rperm_recurs"sv},
    {xset::name::rperm_go_w,                  "rperm_go_w"sv},
    {xset::name::rperm_go_rwx,                "rperm_go_rwx"sv},
    {xset::name::rperm_ugo_w,                 "rperm_ugo_w"sv},
    {xset::name::rperm_ugo_rx,                "rperm_ugo_rx"sv},
    {xset::name::rperm_ugo_rwx,               "rperm_ugo_rwx"sv},

    {xset::name::rperm_own,                   "rperm_own"sv},
    {xset::name::own_myuser,                  "own_myuser"sv},
    {xset::name::own_myuser_users,            "own_myuser_users"sv},
    {xset::name::own_user1,                   "own_user1"sv},
    {xset::name::own_user1_users,             "own_user1_users"sv},
    {xset::name::own_user2,                   "own_user2"sv},
    {xset::name::own_user2_users,             "own_user2_users"sv},
    {xset::name::own_root,                    "own_root"sv},
    {xset::name::own_root_users,              "own_root_users"sv},
    {xset::name::own_root_myuser,             "own_root_myuser"sv},
    {xset::name::own_root_user1,              "own_root_user1"sv},
    {xset::name::own_root_user2,              "own_root_user2"sv},

    {xset::name::own_recurs,                  "own_recurs"sv},
    {xset::name::rown_myuser,                 "rown_myuser"sv},
    {xset::name::rown_myuser_users,           "rown_myuser_users"sv},
    {xset::name::rown_user1,                  "rown_user1"sv},
    {xset::name::rown_user1_users,            "rown_user1_users"sv},
    {xset::name::rown_user2,                  "rown_user2"sv},
    {xset::name::rown_user2_users,            "rown_user2_users"sv},
    {xset::name::rown_root,                   "rown_root"sv},
    {xset::name::rown_root_users,             "rown_root_users"sv},
    {xset::name::rown_root_myuser,            "rown_root_myuser"sv},
    {xset::name::rown_root_user1,             "rown_root_user1"sv},
    {xset::name::rown_root_user2,             "rown_root_user2"sv},

    // panels //
    {xset::name::panel_sliders,               "panel_sliders"sv},

    // panel1
    {xset::name::panel1_show_toolbox,         "panel1_show_toolbox"sv},
    {xset::name::panel1_show_devmon,          "panel1_show_devmon"sv},
    {xset::name::panel1_show_dirtree,         "panel1_show_dirtree"sv},
    {xset::name::panel1_show_sidebar,         "panel1_show_sidebar"sv},
    {xset::name::panel1_slider_positions,     "panel1_slider_positions"sv},
    {xset::name::panel1_list_detailed,        "panel1_list_detailed"sv},
    {xset::name::panel1_list_icons,           "panel1_list_icons"sv},
    {xset::name::panel1_list_compact,         "panel1_list_compact"sv},
    {xset::name::panel1_list_large,           "panel1_list_large"sv},
    {xset::name::panel1_show_hidden,          "panel1_show_hidden"sv},
    {xset::name::panel1_icon_tab,             "panel1_icon_tab"sv},
    {xset::name::panel1_icon_status,          "panel1_icon_status"sv},
    {xset::name::panel1_detcol_name,          "panel1_detcol_name"sv},
    {xset::name::panel1_detcol_size,          "panel1_detcol_size"sv},
    {xset::name::panel1_detcol_type,          "panel1_detcol_type"sv},
    {xset::name::panel1_detcol_perm,          "panel1_detcol_perm"sv},
    {xset::name::panel1_detcol_owner,         "panel1_detcol_owner"sv},
    {xset::name::panel1_detcol_date,          "panel1_detcol_date"sv},
    {xset::name::panel1_sort_extra,           "panel1_sort_extra"sv},
    {xset::name::panel1_book_fol,             "panel1_book_fol"sv},
    {xset::name::panel1_tool_l,               "panel1_tool_l"sv},
    {xset::name::panel1_tool_r,               "panel1_tool_r"sv},
    {xset::name::panel1_tool_s,               "panel1_tool_s"sv},

    // panel2
    {xset::name::panel2_show_toolbox,         "panel2_show_toolbox"sv},
    {xset::name::panel2_show_devmon,          "panel2_show_devmon"sv},
    {xset::name::panel2_show_dirtree,         "panel2_show_dirtree"sv},
    {xset::name::panel2_show_sidebar,         "panel2_show_sidebar"sv},
    {xset::name::panel2_slider_positions,     "panel2_slider_positions"sv},
    {xset::name::panel2_list_detailed,        "panel2_list_detailed"sv},
    {xset::name::panel2_list_icons,           "panel2_list_icons"sv},
    {xset::name::panel2_list_compact,         "panel2_list_compact"sv},
    {xset::name::panel2_list_large,           "panel2_list_large"sv},
    {xset::name::panel2_show_hidden,          "panel2_show_hidden"sv},
    {xset::name::panel2_icon_tab,             "panel2_icon_tab"sv},
    {xset::name::panel2_icon_status,          "panel2_icon_status"sv},
    {xset::name::panel2_detcol_name,          "panel2_detcol_name"sv},
    {xset::name::panel2_detcol_size,          "panel2_detcol_size"sv},
    {xset::name::panel2_detcol_type,          "panel2_detcol_type"sv},
    {xset::name::panel2_detcol_perm,          "panel2_detcol_perm"sv},
    {xset::name::panel2_detcol_owner,         "panel2_detcol_owner"sv},
    {xset::name::panel2_detcol_date,          "panel2_detcol_date"sv},
    {xset::name::panel2_sort_extra,           "panel2_sort_extra"sv},
    {xset::name::panel2_book_fol,             "panel2_book_fol"sv},
    {xset::name::panel2_tool_l,               "panel2_tool_l"sv},
    {xset::name::panel2_tool_r,               "panel2_tool_r"sv},
    {xset::name::panel2_tool_s,               "panel2_tool_s"sv},

    // panel3
    {xset::name::panel3_show_toolbox,         "panel3_show_toolbox"sv},
    {xset::name::panel3_show_devmon,          "panel3_show_devmon"sv},
    {xset::name::panel3_show_dirtree,         "panel3_show_dirtree"sv},
    {xset::name::panel3_show_sidebar,         "panel3_show_sidebar"sv},
    {xset::name::panel3_slider_positions,     "panel3_slider_positions"sv},
    {xset::name::panel3_list_detailed,        "panel3_list_detailed"sv},
    {xset::name::panel3_list_icons,           "panel3_list_icons"sv},
    {xset::name::panel3_list_compact,         "panel3_list_compact"sv},
    {xset::name::panel3_list_large,           "panel3_list_large"sv},
    {xset::name::panel3_show_hidden,          "panel3_show_hidden"sv},
    {xset::name::panel3_icon_tab,             "panel3_icon_tab"sv},
    {xset::name::panel3_icon_status,          "panel3_icon_status"sv},
    {xset::name::panel3_detcol_name,          "panel3_detcol_name"sv},
    {xset::name::panel3_detcol_size,          "panel3_detcol_size"sv},
    {xset::name::panel3_detcol_type,          "panel3_detcol_type"sv},
    {xset::name::panel3_detcol_perm,          "panel3_detcol_perm"sv},
    {xset::name::panel3_detcol_owner,         "panel3_detcol_owner"sv},
    {xset::name::panel3_detcol_date,          "panel3_detcol_date"sv},
    {xset::name::panel3_sort_extra,           "panel3_sort_extra"sv},
    {xset::name::panel3_book_fol,             "panel3_book_fol"sv},
    {xset::name::panel3_tool_l,               "panel3_tool_l"sv},
    {xset::name::panel3_tool_r,               "panel3_tool_r"sv},
    {xset::name::panel3_tool_s,               "panel3_tool_s"sv},

    // panel4
    {xset::name::panel4_show_toolbox,         "panel4_show_toolbox"sv},
    {xset::name::panel4_show_devmon,          "panel4_show_devmon"sv},
    {xset::name::panel4_show_dirtree,         "panel4_show_dirtree"sv},
    {xset::name::panel4_show_sidebar,         "panel4_show_sidebar"sv},
    {xset::name::panel4_slider_positions,     "panel4_slider_positions"sv},
    {xset::name::panel4_list_detailed,        "panel4_list_detailed"sv},
    {xset::name::panel4_list_icons,           "panel4_list_icons"sv},
    {xset::name::panel4_list_compact,         "panel4_list_compact"sv},
    {xset::name::panel4_list_large,           "panel4_list_large"sv},
    {xset::name::panel4_show_hidden,          "panel4_show_hidden"sv},
    {xset::name::panel4_icon_tab,             "panel4_icon_tab"sv},
    {xset::name::panel4_icon_status,          "panel4_icon_status"sv},
    {xset::name::panel4_detcol_name,          "panel4_detcol_name"sv},
    {xset::name::panel4_detcol_size,          "panel4_detcol_size"sv},
    {xset::name::panel4_detcol_type,          "panel4_detcol_type"sv},
    {xset::name::panel4_detcol_perm,          "panel4_detcol_perm"sv},
    {xset::name::panel4_detcol_owner,         "panel4_detcol_owner"sv},
    {xset::name::panel4_detcol_date,          "panel4_detcol_date"sv},
    {xset::name::panel4_sort_extra,           "panel4_sort_extra"sv},
    {xset::name::panel4_book_fol,             "panel4_book_fol"sv},
    {xset::name::panel4_tool_l,               "panel4_tool_l"sv},
    {xset::name::panel4_tool_r,               "panel4_tool_r"sv},
    {xset::name::panel4_tool_s,               "panel4_tool_s"sv},

    // panel modes

    // panel1

    // panel1 mode 0
    {xset::name::panel1_show_toolbox_0,       "panel1_show_toolbox0"sv},
    {xset::name::panel1_show_devmon_0,        "panel1_show_devmon0"sv},
    {xset::name::panel1_show_dirtree_0,       "panel1_show_dirtree0"sv},
    {xset::name::panel1_show_sidebar_0,       "panel1_show_sidebar0"sv},
    {xset::name::panel1_slider_positions_0,   "panel1_slider_positions0"sv},
    {xset::name::panel1_list_detailed_0,      "panel1_list_detailed0"sv},
    {xset::name::panel1_list_icons_0,         "panel1_list_icons0"sv},
    {xset::name::panel1_list_compact_0,       "panel1_list_compact0"sv},
    {xset::name::panel1_list_large_0,         "panel1_list_large0"sv},
    {xset::name::panel1_show_hidden_0,        "panel1_show_hidden0"sv},
    {xset::name::panel1_icon_tab_0,           "panel1_icon_tab0"sv},
    {xset::name::panel1_icon_status_0,        "panel1_icon_status0"sv},
    {xset::name::panel1_detcol_name_0,        "panel1_detcol_name0"sv},
    {xset::name::panel1_detcol_size_0,        "panel1_detcol_size0"sv},
    {xset::name::panel1_detcol_type_0,        "panel1_detcol_type0"sv},
    {xset::name::panel1_detcol_perm_0,        "panel1_detcol_perm0"sv},
    {xset::name::panel1_detcol_owner_0,       "panel1_detcol_owner0"sv},
    {xset::name::panel1_detcol_date_0,        "panel1_detcol_date0"sv},
    {xset::name::panel1_sort_extra_0,         "panel1_sort_extra0"sv},
    {xset::name::panel1_book_fol_0,           "panel1_book_fol0"sv},
    {xset::name::panel1_tool_l_0,             "panel1_tool_l0"sv},
    {xset::name::panel1_tool_r_0,             "panel1_tool_r0"sv},
    {xset::name::panel1_tool_s_0,             "panel1_tool_s0"sv},

    // panel1 mode 1
    {xset::name::panel1_show_toolbox_1,       "panel1_show_toolbox1"sv},
    {xset::name::panel1_show_devmon_1,        "panel1_show_devmon1"sv},
    {xset::name::panel1_show_dirtree_1,       "panel1_show_dirtree1"sv},
    {xset::name::panel1_show_sidebar_1,       "panel1_show_sidebar1"sv},
    {xset::name::panel1_slider_positions_1,   "panel1_slider_positions1"sv},
    {xset::name::panel1_list_detailed_1,      "panel1_list_detailed1"sv},
    {xset::name::panel1_list_icons_1,         "panel1_list_icons1"sv},
    {xset::name::panel1_list_compact_1,       "panel1_list_compact1"sv},
    {xset::name::panel1_list_large_1,         "panel1_list_large1"sv},
    {xset::name::panel1_show_hidden_1,        "panel1_show_hidden1"sv},
    {xset::name::panel1_icon_tab_1,           "panel1_icon_tab1"sv},
    {xset::name::panel1_icon_status_1,        "panel1_icon_status1"sv},
    {xset::name::panel1_detcol_name_1,        "panel1_detcol_name1"sv},
    {xset::name::panel1_detcol_size_1,        "panel1_detcol_size1"sv},
    {xset::name::panel1_detcol_type_1,        "panel1_detcol_type1"sv},
    {xset::name::panel1_detcol_perm_1,        "panel1_detcol_perm1"sv},
    {xset::name::panel1_detcol_owner_1,       "panel1_detcol_owner1"sv},
    {xset::name::panel1_detcol_date_1,        "panel1_detcol_date1"sv},
    {xset::name::panel1_sort_extra_1,         "panel1_sort_extra1"sv},
    {xset::name::panel1_book_fol_1,           "panel1_book_fol1"sv},
    {xset::name::panel1_tool_l_1,             "panel1_tool_l1"sv},
    {xset::name::panel1_tool_r_1,             "panel1_tool_r1"sv},
    {xset::name::panel1_tool_s_1,             "panel1_tool_s1"sv},

    // panel1 mode 2
    {xset::name::panel1_show_toolbox_2,       "panel1_show_toolbox2"sv},
    {xset::name::panel1_show_devmon_2,        "panel1_show_devmon2"sv},
    {xset::name::panel1_show_dirtree_2,       "panel1_show_dirtree2"sv},
    {xset::name::panel1_show_sidebar_2,       "panel1_show_sidebar2"sv},
    {xset::name::panel1_slider_positions_2,   "panel1_slider_positions2"sv},
    {xset::name::panel1_list_detailed_2,      "panel1_list_detailed2"sv},
    {xset::name::panel1_list_icons_2,         "panel1_list_icons2"sv},
    {xset::name::panel1_list_compact_2,       "panel1_list_compact2"sv},
    {xset::name::panel1_list_large_2,         "panel1_list_large2"sv},
    {xset::name::panel1_show_hidden_2,        "panel1_show_hidden2"sv},
    {xset::name::panel1_icon_tab_2,           "panel1_icon_tab2"sv},
    {xset::name::panel1_icon_status_2,        "panel1_icon_status2"sv},
    {xset::name::panel1_detcol_name_2,        "panel1_detcol_name2"sv},
    {xset::name::panel1_detcol_size_2,        "panel1_detcol_size2"sv},
    {xset::name::panel1_detcol_type_2,        "panel1_detcol_type2"sv},
    {xset::name::panel1_detcol_perm_2,        "panel1_detcol_perm2"sv},
    {xset::name::panel1_detcol_owner_2,       "panel1_detcol_owner2"sv},
    {xset::name::panel1_detcol_date_2,        "panel1_detcol_date2"sv},
    {xset::name::panel1_sort_extra_2,         "panel1_sort_extra2"sv},
    {xset::name::panel1_book_fol_2,           "panel1_book_fol2"sv},
    {xset::name::panel1_tool_l_2,             "panel1_tool_l2"sv},
    {xset::name::panel1_tool_r_2,             "panel1_tool_r2"sv},
    {xset::name::panel1_tool_s_2,             "panel1_tool_s2"sv},

    // panel1 mode 3
    {xset::name::panel1_show_toolbox_3,       "panel1_show_toolbox3"sv},
    {xset::name::panel1_show_devmon_3,        "panel1_show_devmon3"sv},
    {xset::name::panel1_show_dirtree_3,       "panel1_show_dirtree3"sv},
    {xset::name::panel1_show_sidebar_3,       "panel1_show_sidebar3"sv},
    {xset::name::panel1_slider_positions_3,   "panel1_slider_positions3"sv},
    {xset::name::panel1_list_detailed_3,      "panel1_list_detailed3"sv},
    {xset::name::panel1_list_icons_3,         "panel1_list_icons3"sv},
    {xset::name::panel1_list_compact_3,       "panel1_list_compact3"sv},
    {xset::name::panel1_list_large_3,         "panel1_list_large3"sv},
    {xset::name::panel1_show_hidden_3,        "panel1_show_hidden3"sv},
    {xset::name::panel1_icon_tab_3,           "panel1_icon_tab3"sv},
    {xset::name::panel1_icon_status_3,        "panel1_icon_status3"sv},
    {xset::name::panel1_detcol_name_3,        "panel1_detcol_name3"sv},
    {xset::name::panel1_detcol_size_3,        "panel1_detcol_size3"sv},
    {xset::name::panel1_detcol_type_3,        "panel1_detcol_type3"sv},
    {xset::name::panel1_detcol_perm_3,        "panel1_detcol_perm3"sv},
    {xset::name::panel1_detcol_owner_3,       "panel1_detcol_owner3"sv},
    {xset::name::panel1_detcol_date_3,        "panel1_detcol_date3"sv},
    {xset::name::panel1_sort_extra_3,         "panel1_sort_extra3"sv},
    {xset::name::panel1_book_fol_3,           "panel1_book_fol3"sv},
    {xset::name::panel1_tool_l_3,             "panel1_tool_l3"sv},
    {xset::name::panel1_tool_r_3,             "panel1_tool_r3"sv},
    {xset::name::panel1_tool_s_3,             "panel1_tool_s3"sv},

    // panel2

    // panel2 mode 0
    {xset::name::panel2_show_toolbox_0,       "panel2_show_toolbox0"sv},
    {xset::name::panel2_show_devmon_0,        "panel2_show_devmon0"sv},
    {xset::name::panel2_show_dirtree_0,       "panel2_show_dirtree0"sv},
    {xset::name::panel2_show_sidebar_0,       "panel2_show_sidebar0"sv},
    {xset::name::panel2_slider_positions_0,   "panel2_slider_positions0"sv},
    {xset::name::panel2_list_detailed_0,      "panel2_list_detailed0"sv},
    {xset::name::panel2_list_icons_0,         "panel2_list_icons0"sv},
    {xset::name::panel2_list_compact_0,       "panel2_list_compact0"sv},
    {xset::name::panel2_list_large_0,         "panel2_list_large0"sv},
    {xset::name::panel2_show_hidden_0,        "panel2_show_hidden0"sv},
    {xset::name::panel2_icon_tab_0,           "panel2_icon_tab0"sv},
    {xset::name::panel2_icon_status_0,        "panel2_icon_status0"sv},
    {xset::name::panel2_detcol_name_0,        "panel2_detcol_name0"sv},
    {xset::name::panel2_detcol_size_0,        "panel2_detcol_size0"sv},
    {xset::name::panel2_detcol_type_0,        "panel2_detcol_type0"sv},
    {xset::name::panel2_detcol_perm_0,        "panel2_detcol_perm0"sv},
    {xset::name::panel2_detcol_owner_0,       "panel2_detcol_owner0"sv},
    {xset::name::panel2_detcol_date_0,        "panel2_detcol_date0"sv},
    {xset::name::panel2_sort_extra_0,         "panel2_sort_extra0"sv},
    {xset::name::panel2_book_fol_0,           "panel2_book_fol0"sv},
    {xset::name::panel2_tool_l_0,             "panel2_tool_l0"sv},
    {xset::name::panel2_tool_r_0,             "panel2_tool_r0"sv},
    {xset::name::panel2_tool_s_0,             "panel2_tool_s0"sv},

    // panel2 mode 1
    {xset::name::panel2_show_toolbox_1,       "panel2_show_toolbox1"sv},
    {xset::name::panel2_show_devmon_1,        "panel2_show_devmon1"sv},
    {xset::name::panel2_show_dirtree_1,       "panel2_show_dirtree1"sv},
    {xset::name::panel2_show_sidebar_1,       "panel2_show_sidebar1"sv},
    {xset::name::panel2_slider_positions_1,   "panel2_slider_positions1"sv},
    {xset::name::panel2_list_detailed_1,      "panel2_list_detailed1"sv},
    {xset::name::panel2_list_icons_1,         "panel2_list_icons1"sv},
    {xset::name::panel2_list_compact_1,       "panel2_list_compact1"sv},
    {xset::name::panel2_list_large_1,         "panel2_list_large1"sv},
    {xset::name::panel2_show_hidden_1,        "panel2_show_hidden1"sv},
    {xset::name::panel2_icon_tab_1,           "panel2_icon_tab1"sv},
    {xset::name::panel2_icon_status_1,        "panel2_icon_status1"sv},
    {xset::name::panel2_detcol_name_1,        "panel2_detcol_name1"sv},
    {xset::name::panel2_detcol_size_1,        "panel2_detcol_size1"sv},
    {xset::name::panel2_detcol_type_1,        "panel2_detcol_type1"sv},
    {xset::name::panel2_detcol_perm_1,        "panel2_detcol_perm1"sv},
    {xset::name::panel2_detcol_owner_1,       "panel2_detcol_owner1"sv},
    {xset::name::panel2_detcol_date_1,        "panel2_detcol_date1"sv},
    {xset::name::panel2_sort_extra_1,         "panel2_sort_extra1"sv},
    {xset::name::panel2_book_fol_1,           "panel2_book_fol1"sv},
    {xset::name::panel2_tool_l_1,             "panel2_tool_l1"sv},
    {xset::name::panel2_tool_r_1,             "panel2_tool_r1"sv},
    {xset::name::panel2_tool_s_1,             "panel2_tool_s1"sv},

    // panel2 mode 2
    {xset::name::panel2_show_toolbox_2,       "panel2_show_toolbox2"sv},
    {xset::name::panel2_show_devmon_2,        "panel2_show_devmon2"sv},
    {xset::name::panel2_show_dirtree_2,       "panel2_show_dirtree2"sv},
    {xset::name::panel2_show_sidebar_2,       "panel2_show_sidebar2"sv},
    {xset::name::panel2_slider_positions_2,   "panel2_slider_positions2"sv},
    {xset::name::panel2_list_detailed_2,      "panel2_list_detailed2"sv},
    {xset::name::panel2_list_icons_2,         "panel2_list_icons2"sv},
    {xset::name::panel2_list_compact_2,       "panel2_list_compact2"sv},
    {xset::name::panel2_list_large_2,         "panel2_list_large2"sv},
    {xset::name::panel2_show_hidden_2,        "panel2_show_hidden2"sv},
    {xset::name::panel2_icon_tab_2,           "panel2_icon_tab2"sv},
    {xset::name::panel2_icon_status_2,        "panel2_icon_status2"sv},
    {xset::name::panel2_detcol_name_2,        "panel2_detcol_name2"sv},
    {xset::name::panel2_detcol_size_2,        "panel2_detcol_size2"sv},
    {xset::name::panel2_detcol_type_2,        "panel2_detcol_type2"sv},
    {xset::name::panel2_detcol_perm_2,        "panel2_detcol_perm2"sv},
    {xset::name::panel2_detcol_owner_2,       "panel2_detcol_owner2"sv},
    {xset::name::panel2_detcol_date_2,        "panel2_detcol_date2"sv},
    {xset::name::panel2_sort_extra_2,         "panel2_sort_extra2"sv},
    {xset::name::panel2_book_fol_2,           "panel2_book_fol2"sv},
    {xset::name::panel2_tool_l_2,             "panel2_tool_l2"sv},
    {xset::name::panel2_tool_r_2,             "panel2_tool_r2"}  ,
    {xset::name::panel2_tool_s_2,             "panel2_tool_s2"}  ,

    // panel2 mode 3
    {xset::name::panel2_show_toolbox_3,       "panel2_show_toolbox3"sv},
    {xset::name::panel2_show_devmon_3,        "panel2_show_devmon3"sv},
    {xset::name::panel2_show_dirtree_3,       "panel2_show_dirtree3"sv},
    {xset::name::panel2_show_sidebar_3,       "panel2_show_sidebar3"sv},
    {xset::name::panel2_slider_positions_3,   "panel2_slider_positions3"sv},
    {xset::name::panel2_list_detailed_3,      "panel2_list_detailed3"sv},
    {xset::name::panel2_list_icons_3,         "panel2_list_icons3"sv},
    {xset::name::panel2_list_compact_3,       "panel2_list_compact3"sv},
    {xset::name::panel2_list_large_3,         "panel2_list_large3"sv},
    {xset::name::panel2_show_hidden_3,        "panel2_show_hidden3"sv},
    {xset::name::panel2_icon_tab_3,           "panel2_icon_tab3"sv},
    {xset::name::panel2_icon_status_3,        "panel2_icon_status3"sv},
    {xset::name::panel2_detcol_name_3,        "panel2_detcol_name3"sv},
    {xset::name::panel2_detcol_size_3,        "panel2_detcol_size3"sv},
    {xset::name::panel2_detcol_type_3,        "panel2_detcol_type3"sv},
    {xset::name::panel2_detcol_perm_3,        "panel2_detcol_perm3"sv},
    {xset::name::panel2_detcol_owner_3,       "panel2_detcol_owner3"sv},
    {xset::name::panel2_detcol_date_3,        "panel2_detcol_date3"sv},
    {xset::name::panel2_sort_extra_3,         "panel2_sort_extra3"sv},
    {xset::name::panel2_book_fol_3,           "panel2_book_fol3"sv},
    {xset::name::panel2_tool_l_3,             "panel2_tool_l3"sv},
    {xset::name::panel2_tool_r_3,             "panel2_tool_r3"sv},
    {xset::name::panel2_tool_s_3,             "panel2_tool_s3"sv},

    // panel3

    // panel3 mode 0
    {xset::name::panel3_show_toolbox_0,       "panel3_show_toolbox0"sv},
    {xset::name::panel3_show_devmon_0,        "panel3_show_devmon0"sv},
    {xset::name::panel3_show_dirtree_0,       "panel3_show_dirtree0"sv},
    {xset::name::panel3_show_sidebar_0,       "panel3_show_sidebar0"sv},
    {xset::name::panel3_slider_positions_0,   "panel3_slider_positions0"sv},
    {xset::name::panel3_list_detailed_0,      "panel3_list_detailed0"sv},
    {xset::name::panel3_list_icons_0,         "panel3_list_icons0"sv},
    {xset::name::panel3_list_compact_0,       "panel3_list_compact0"sv},
    {xset::name::panel3_list_large_0,         "panel3_list_large0"sv},
    {xset::name::panel3_show_hidden_0,        "panel3_show_hidden0"sv},
    {xset::name::panel3_icon_tab_0,           "panel3_icon_tab0"sv},
    {xset::name::panel3_icon_status_0,        "panel3_icon_status0"sv},
    {xset::name::panel3_detcol_name_0,        "panel3_detcol_name0"sv},
    {xset::name::panel3_detcol_size_0,        "panel3_detcol_size0"sv},
    {xset::name::panel3_detcol_type_0,        "panel3_detcol_type0"sv},
    {xset::name::panel3_detcol_perm_0,        "panel3_detcol_perm0"sv},
    {xset::name::panel3_detcol_owner_0,       "panel3_detcol_owner0"sv},
    {xset::name::panel3_detcol_date_0,        "panel3_detcol_date0"sv},
    {xset::name::panel3_sort_extra_0,         "panel3_sort_extra0"sv},
    {xset::name::panel3_book_fol_0,           "panel3_book_fol0"sv},
    {xset::name::panel3_tool_l_0,             "panel3_tool_l0"sv},
    {xset::name::panel3_tool_r_0,             "panel3_tool_r0"sv},
    {xset::name::panel3_tool_s_0,             "panel3_tool_s0"sv},

    // panel3 mode 1
    {xset::name::panel3_show_toolbox_1,       "panel3_show_toolbox1"sv},
    {xset::name::panel3_show_devmon_1,        "panel3_show_devmon1"sv},
    {xset::name::panel3_show_dirtree_1,       "panel3_show_dirtree1"sv},
    {xset::name::panel3_show_sidebar_1,       "panel3_show_sidebar1"sv},
    {xset::name::panel3_slider_positions_1,   "panel3_slider_positions1"sv},
    {xset::name::panel3_list_detailed_1,      "panel3_list_detailed1"sv},
    {xset::name::panel3_list_icons_1,         "panel3_list_icons1"sv},
    {xset::name::panel3_list_compact_1,       "panel3_list_compact1"sv},
    {xset::name::panel3_list_large_1,         "panel3_list_large1"sv},
    {xset::name::panel3_show_hidden_1,        "panel3_show_hidden1"sv},
    {xset::name::panel3_icon_tab_1,           "panel3_icon_tab1"sv},
    {xset::name::panel3_icon_status_1,        "panel3_icon_status1"sv},
    {xset::name::panel3_detcol_name_1,        "panel3_detcol_name1"sv},
    {xset::name::panel3_detcol_size_1,        "panel3_detcol_size1"sv},
    {xset::name::panel3_detcol_type_1,        "panel3_detcol_type1"sv},
    {xset::name::panel3_detcol_perm_1,        "panel3_detcol_perm1"sv},
    {xset::name::panel3_detcol_owner_1,       "panel3_detcol_owner1"sv},
    {xset::name::panel3_detcol_date_1,        "panel3_detcol_date1"sv},
    {xset::name::panel3_sort_extra_1,         "panel3_sort_extra1"sv},
    {xset::name::panel3_book_fol_1,           "panel3_book_fol1"sv},
    {xset::name::panel3_tool_l_1,             "panel3_tool_l1"sv},
    {xset::name::panel3_tool_r_1,             "panel3_tool_r1"sv},
    {xset::name::panel3_tool_s_1,             "panel3_tool_s1"sv},

    // panel3 mode 2
    {xset::name::panel3_show_toolbox_2,       "panel3_show_toolbox2"sv},
    {xset::name::panel3_show_devmon_2,        "panel3_show_devmon2"sv},
    {xset::name::panel3_show_dirtree_2,       "panel3_show_dirtree2"sv},
    {xset::name::panel3_show_sidebar_2,       "panel3_show_sidebar2"sv},
    {xset::name::panel3_slider_positions_2,   "panel3_slider_positions2"sv},
    {xset::name::panel3_list_detailed_2,      "panel3_list_detailed2"sv},
    {xset::name::panel3_list_icons_2,         "panel3_list_icons2"sv},
    {xset::name::panel3_list_compact_2,       "panel3_list_compact2"sv},
    {xset::name::panel3_list_large_2,         "panel3_list_large2"sv},
    {xset::name::panel3_show_hidden_2,        "panel3_show_hidden2"sv},
    {xset::name::panel3_icon_tab_2,           "panel3_icon_tab2"sv},
    {xset::name::panel3_icon_status_2,        "panel3_icon_status2"sv},
    {xset::name::panel3_detcol_name_2,        "panel3_detcol_name2"sv},
    {xset::name::panel3_detcol_size_2,        "panel3_detcol_size2"sv},
    {xset::name::panel3_detcol_type_2,        "panel3_detcol_type2"sv},
    {xset::name::panel3_detcol_perm_2,        "panel3_detcol_perm2"sv},
    {xset::name::panel3_detcol_owner_2,       "panel3_detcol_owner2"sv},
    {xset::name::panel3_detcol_date_2,        "panel3_detcol_date2"sv},
    {xset::name::panel3_sort_extra_2,         "panel3_sort_extra2"sv},
    {xset::name::panel3_book_fol_2,           "panel3_book_fol2"sv},
    {xset::name::panel3_tool_l_2,             "panel3_tool_l2"sv},
    {xset::name::panel3_tool_r_2,             "panel3_tool_r2"sv},
    {xset::name::panel3_tool_s_2,             "panel3_tool_s2"sv},

    // panel13 mode 3
    {xset::name::panel3_show_toolbox_3,       "panel3_show_toolbox3"sv},
    {xset::name::panel3_show_devmon_3,        "panel3_show_devmon3"sv},
    {xset::name::panel3_show_dirtree_3,       "panel3_show_dirtree3"sv},
    {xset::name::panel3_show_sidebar_3,       "panel3_show_sidebar3"sv},
    {xset::name::panel3_slider_positions_3,   "panel3_slider_positions3"sv},
    {xset::name::panel3_list_detailed_3,      "panel3_list_detailed3"sv},
    {xset::name::panel3_list_icons_3,         "panel3_list_icons3"sv},
    {xset::name::panel3_list_compact_3,       "panel3_list_compact3"sv},
    {xset::name::panel3_list_large_3,         "panel3_list_large3"sv},
    {xset::name::panel3_show_hidden_3,        "panel3_show_hidden3"sv},
    {xset::name::panel3_icon_tab_3,           "panel3_icon_tab3"sv},
    {xset::name::panel3_icon_status_3,        "panel3_icon_status3"sv},
    {xset::name::panel3_detcol_name_3,        "panel3_detcol_name3"sv},
    {xset::name::panel3_detcol_size_3,        "panel3_detcol_size3"sv},
    {xset::name::panel3_detcol_type_3,        "panel3_detcol_type3"sv},
    {xset::name::panel3_detcol_perm_3,        "panel3_detcol_perm3"sv},
    {xset::name::panel3_detcol_owner_3,       "panel3_detcol_owner3"sv},
    {xset::name::panel3_detcol_date_3,        "panel3_detcol_date3"sv},
    {xset::name::panel3_sort_extra_3,         "panel3_sort_extra3"sv},
    {xset::name::panel3_book_fol_3,           "panel3_book_fol3"sv},
    {xset::name::panel3_tool_l_3,             "panel3_tool_l3"sv},
    {xset::name::panel3_tool_r_3,             "panel3_tool_r3"sv},
    {xset::name::panel3_tool_s_3,             "panel3_tool_s3"sv},

    // panel4

    // panel4 mode 0
    {xset::name::panel4_show_toolbox_0,       "panel4_show_toolbox0"sv},
    {xset::name::panel4_show_devmon_0,        "panel4_show_devmon0"sv},
    {xset::name::panel4_show_dirtree_0,       "panel4_show_dirtree0"sv},
    {xset::name::panel4_show_sidebar_0,       "panel4_show_sidebar0"sv},
    {xset::name::panel4_slider_positions_0,   "panel4_slider_positions0"sv},
    {xset::name::panel4_list_detailed_0,      "panel4_list_detailed0"sv},
    {xset::name::panel4_list_icons_0,         "panel4_list_icons0"sv},
    {xset::name::panel4_list_compact_0,       "panel4_list_compact0"sv},
    {xset::name::panel4_list_large_0,         "panel4_list_large0"sv},
    {xset::name::panel4_show_hidden_0,        "panel4_show_hidden0"sv},
    {xset::name::panel4_icon_tab_0,           "panel4_icon_tab0"sv},
    {xset::name::panel4_icon_status_0,        "panel4_icon_status0"sv},
    {xset::name::panel4_detcol_name_0,        "panel4_detcol_name0"sv},
    {xset::name::panel4_detcol_size_0,        "panel4_detcol_size0"sv},
    {xset::name::panel4_detcol_type_0,        "panel4_detcol_type0"sv},
    {xset::name::panel4_detcol_perm_0,        "panel4_detcol_perm0"sv},
    {xset::name::panel4_detcol_owner_0,       "panel4_detcol_owner0"sv},
    {xset::name::panel4_detcol_date_0,        "panel4_detcol_date0"sv},
    {xset::name::panel4_sort_extra_0,         "panel4_sort_extra0"sv},
    {xset::name::panel4_book_fol_0,           "panel4_book_fol0"sv},
    {xset::name::panel4_tool_l_0,             "panel4_tool_l0"sv},
    {xset::name::panel4_tool_r_0,             "panel4_tool_r0"sv},
    {xset::name::panel4_tool_s_0,             "panel4_tool_s0"sv},

    // panel4 mode 1
    {xset::name::panel4_show_toolbox_1,       "panel4_show_toolbox1"sv},
    {xset::name::panel4_show_devmon_1,        "panel4_show_devmon1"sv},
    {xset::name::panel4_show_dirtree_1,       "panel4_show_dirtree1"sv},
    {xset::name::panel4_show_sidebar_1,       "panel4_show_sidebar1"sv},
    {xset::name::panel4_slider_positions_1,   "panel4_slider_positions1"sv},
    {xset::name::panel4_list_detailed_1,      "panel4_list_detailed1"sv},
    {xset::name::panel4_list_icons_1,         "panel4_list_icons1"sv},
    {xset::name::panel4_list_compact_1,       "panel4_list_compact1"sv},
    {xset::name::panel4_list_large_1,         "panel4_list_large1"sv},
    {xset::name::panel4_show_hidden_1,        "panel4_show_hidden1"sv},
    {xset::name::panel4_icon_tab_1,           "panel4_icon_tab1"sv},
    {xset::name::panel4_icon_status_1,        "panel4_icon_status1"sv},
    {xset::name::panel4_detcol_name_1,        "panel4_detcol_name1"sv},
    {xset::name::panel4_detcol_size_1,        "panel4_detcol_size1"sv},
    {xset::name::panel4_detcol_type_1,        "panel4_detcol_type1"sv},
    {xset::name::panel4_detcol_perm_1,        "panel4_detcol_perm1"sv},
    {xset::name::panel4_detcol_owner_1,       "panel4_detcol_owner1"sv},
    {xset::name::panel4_detcol_date_1,        "panel4_detcol_date1"sv},
    {xset::name::panel4_sort_extra_1,         "panel4_sort_extra1"sv},
    {xset::name::panel4_book_fol_1,           "panel4_book_fol1"sv},
    {xset::name::panel4_tool_l_1,             "panel4_tool_l1"sv},
    {xset::name::panel4_tool_r_1,             "panel4_tool_r1"sv},
    {xset::name::panel4_tool_s_1,             "panel4_tool_s1"sv},

    // panel4 mode 2
    {xset::name::panel4_show_toolbox_2,       "panel4_show_toolbox2"sv},
    {xset::name::panel4_show_devmon_2,        "panel4_show_devmon2"sv},
    {xset::name::panel4_show_dirtree_2,       "panel4_show_dirtree2"sv},
    {xset::name::panel4_show_sidebar_2,       "panel4_show_sidebar2"sv},
    {xset::name::panel4_slider_positions_2,   "panel4_slider_positions2"sv},
    {xset::name::panel4_list_detailed_2,      "panel4_list_detailed2"sv},
    {xset::name::panel4_list_icons_2,         "panel4_list_icons2"sv},
    {xset::name::panel4_list_compact_2,       "panel4_list_compact2"sv},
    {xset::name::panel4_list_large_2,         "panel4_list_large2"sv},
    {xset::name::panel4_show_hidden_2,        "panel4_show_hidden2"sv},
    {xset::name::panel4_icon_tab_2,           "panel4_icon_tab2"sv},
    {xset::name::panel4_icon_status_2,        "panel4_icon_status2"sv},
    {xset::name::panel4_detcol_name_2,        "panel4_detcol_name2"sv},
    {xset::name::panel4_detcol_size_2,        "panel4_detcol_size2"sv},
    {xset::name::panel4_detcol_type_2,        "panel4_detcol_type2"sv},
    {xset::name::panel4_detcol_perm_2,        "panel4_detcol_perm2"sv},
    {xset::name::panel4_detcol_owner_2,       "panel4_detcol_owner2"sv},
    {xset::name::panel4_detcol_date_2,        "panel4_detcol_date2"sv},
    {xset::name::panel4_sort_extra_2,         "panel4_sort_extra2"sv},
    {xset::name::panel4_book_fol_2,           "panel4_book_fol2"sv},
    {xset::name::panel4_tool_l_2,             "panel4_tool_l2"sv},
    {xset::name::panel4_tool_r_2,             "panel4_tool_r2"sv},
    {xset::name::panel4_tool_s_2,             "panel4_tool_s2"sv},

    // panel4 mode 3
    {xset::name::panel4_show_toolbox_3,       "panel4_show_toolbox3"sv},
    {xset::name::panel4_show_devmon_3,        "panel4_show_devmon3"sv},
    {xset::name::panel4_show_dirtree_3,       "panel4_show_dirtree3"sv},
    {xset::name::panel4_show_sidebar_3,       "panel4_show_sidebar3"sv},
    {xset::name::panel4_slider_positions_3,   "panel4_slider_positions3"sv},
    {xset::name::panel4_list_detailed_3,      "panel4_list_detailed3"sv},
    {xset::name::panel4_list_icons_3,         "panel4_list_icons3"sv},
    {xset::name::panel4_list_compact_3,       "panel4_list_compact3"sv},
    {xset::name::panel4_list_large_3,         "panel4_list_large3"sv},
    {xset::name::panel4_show_hidden_3,        "panel4_show_hidden3"sv},
    {xset::name::panel4_icon_tab_3,           "panel4_icon_tab3"sv},
    {xset::name::panel4_icon_status_3,        "panel4_icon_status3"sv},
    {xset::name::panel4_detcol_name_3,        "panel4_detcol_name3"sv},
    {xset::name::panel4_detcol_size_3,        "panel4_detcol_size3"sv},
    {xset::name::panel4_detcol_type_3,        "panel4_detcol_type3"sv},
    {xset::name::panel4_detcol_perm_3,        "panel4_detcol_perm3"sv},
    {xset::name::panel4_detcol_owner_3,       "panel4_detcol_owner3"sv},
    {xset::name::panel4_detcol_date_3,        "panel4_detcol_date3"sv},
    {xset::name::panel4_sort_extra_3,         "panel4_sort_extra3"sv},
    {xset::name::panel4_book_fol_3,           "panel4_book_fol3"sv},
    {xset::name::panel4_tool_l_3,             "panel4_tool_l3"sv},
    {xset::name::panel4_tool_r_3,             "panel4_tool_r3"sv},
    {xset::name::panel4_tool_s_3,             "panel4_tool_s3"sv},

    // speed
    {xset::name::book_newtab,                 "book_newtab"sv},
    {xset::name::book_single,                 "book_single"sv},
    {xset::name::dev_newtab,                  "dev_newtab"sv},
    {xset::name::dev_single,                  "dev_single"sv},

    // dialog
    {xset::name::app_dlg,                     "app_dlg"sv},
    {xset::name::context_dlg,                 "context_dlg"sv},
    {xset::name::file_dlg,                    "file_dlg"sv},
    {xset::name::text_dlg,                    "text_dlg"sv},

    // other
    {xset::name::book_new,                    "book_new"sv},
    {xset::name::drag_action,                 "drag_action"sv},
    {xset::name::editor,                      "editor"sv},
    {xset::name::root_editor,                 "root_editor"sv},
    {xset::name::su_command,                  "su_command"sv},

    // handlers //

    // handlers arc
    {xset::name::hand_arc_7z,                 "hand_arc_+7z"sv},
    {xset::name::hand_arc_gz,                 "hand_arc_+gz"sv},
    {xset::name::hand_arc_rar,                "hand_arc_+rar"sv},
    {xset::name::hand_arc_tar,                "hand_arc_+tar"sv},
    {xset::name::hand_arc_tar_bz2,            "hand_arc_+tar_bz2"sv},
    {xset::name::hand_arc_tar_gz,             "hand_arc_+tar_gz"sv},
    {xset::name::hand_arc_tar_lz4,            "hand_arc_+tar_lz4"sv},
    {xset::name::hand_arc_tar_xz,             "hand_arc_+tar_xz"sv},
    {xset::name::hand_arc_tar_zst,            "hand_arc_+tar_zst"sv},
    {xset::name::hand_arc_lz4,                "hand_arc_+lz4"sv},
    {xset::name::hand_arc_xz,                 "hand_arc_+xz"sv},
    {xset::name::hand_arc_zip,                "hand_arc_+zip"sv},
    {xset::name::hand_arc_zst,                "hand_arc_+zst"sv},

    // handlers file
    {xset::name::hand_f_iso,                  "hand_f_+iso"sv},

    // handlers fs
    {xset::name::hand_fs_def,                 "hand_fs_+def"sv},
    {xset::name::hand_fs_fuseiso,             "hand_fs_+fuseiso"sv},
    {xset::name::hand_fs_udiso,               "hand_fs_+udiso"sv},

    // handlers net
    {xset::name::hand_net_ftp,                "hand_net_+ftp"sv},
    {xset::name::hand_net_fuse,               "hand_net_+fuse"sv},
    {xset::name::hand_net_fusesmb,            "hand_net_+fusesmb"sv},
    {xset::name::hand_net_gphoto,             "hand_net_+gphoto"sv},
    {xset::name::hand_net_http,               "hand_net_+http"sv},
    {xset::name::hand_net_ifuse,              "hand_net_+ifuse"sv},
    {xset::name::hand_net_mtp,                "hand_net_+mtp"sv},
    {xset::name::hand_net_ssh,                "hand_net_+ssh"sv},
    {xset::name::hand_net_udevil,             "hand_net_+udevil"sv},
    {xset::name::hand_net_udevilsmb,          "hand_net_+udevilsmb"sv},

    // other
    {xset::name::tool_l,                      "tool_l"sv},
    {xset::name::tool_r,                      "tool_r"sv},
    {xset::name::tool_s,                      "tool_s"sv},
};


// panels

// panel1
static const std::unordered_map<xset::panel, xset::name> xset_panel1_map{
    // panel1
    {xset::panel::show,             xset::name::panel1_show},
    {xset::panel::show_toolbox,     xset::name::panel1_show_toolbox},
    {xset::panel::show_devmon,      xset::name::panel1_show_devmon},
    {xset::panel::show_dirtree,     xset::name::panel1_show_dirtree},
    {xset::panel::show_sidebar,     xset::name::panel1_show_sidebar},
    {xset::panel::slider_positions, xset::name::panel1_slider_positions},
    {xset::panel::list_detailed,    xset::name::panel1_list_detailed},
    {xset::panel::list_icons,       xset::name::panel1_list_icons},
    {xset::panel::list_compact,     xset::name::panel1_list_compact},
    {xset::panel::list_large,       xset::name::panel1_list_large},
    {xset::panel::show_hidden,      xset::name::panel1_show_hidden},
    {xset::panel::icon_tab,         xset::name::panel1_icon_tab},
    {xset::panel::icon_status,      xset::name::panel1_icon_status},
    {xset::panel::detcol_name,      xset::name::panel1_detcol_name},
    {xset::panel::detcol_size,      xset::name::panel1_detcol_size},
    {xset::panel::detcol_type,      xset::name::panel1_detcol_type},
    {xset::panel::detcol_perm,      xset::name::panel1_detcol_perm},
    {xset::panel::detcol_owner,     xset::name::panel1_detcol_owner},
    {xset::panel::detcol_date,      xset::name::panel1_detcol_date},
    {xset::panel::sort_extra,       xset::name::panel1_sort_extra},
    {xset::panel::book_fol,         xset::name::panel1_book_fol},
    {xset::panel::tool_l,           xset::name::panel1_tool_l},
    {xset::panel::tool_r,           xset::name::panel1_tool_r},
    {xset::panel::tool_s,           xset::name::panel1_tool_s},
};

// panel2
static const std::unordered_map<xset::panel, xset::name> xset_panel2_map{
    // panel2
    {xset::panel::show,             xset::name::panel2_show},
    {xset::panel::show_toolbox,     xset::name::panel2_show_toolbox},
    {xset::panel::show_devmon,      xset::name::panel2_show_devmon},
    {xset::panel::show_dirtree,     xset::name::panel2_show_dirtree},
    {xset::panel::show_sidebar,     xset::name::panel2_show_sidebar},
    {xset::panel::slider_positions, xset::name::panel2_slider_positions},
    {xset::panel::list_detailed,    xset::name::panel2_list_detailed},
    {xset::panel::list_icons,       xset::name::panel2_list_icons},
    {xset::panel::list_compact,     xset::name::panel2_list_compact},
    {xset::panel::list_large,       xset::name::panel2_list_large},
    {xset::panel::show_hidden,      xset::name::panel2_show_hidden},
    {xset::panel::icon_tab,         xset::name::panel2_icon_tab},
    {xset::panel::icon_status,      xset::name::panel2_icon_status},
    {xset::panel::detcol_name,      xset::name::panel2_detcol_name},
    {xset::panel::detcol_size,      xset::name::panel2_detcol_size},
    {xset::panel::detcol_type,      xset::name::panel2_detcol_type},
    {xset::panel::detcol_perm,      xset::name::panel2_detcol_perm},
    {xset::panel::detcol_owner,     xset::name::panel2_detcol_owner},
    {xset::panel::detcol_date,      xset::name::panel2_detcol_date},
    {xset::panel::sort_extra,       xset::name::panel2_sort_extra},
    {xset::panel::book_fol,         xset::name::panel2_book_fol},
    {xset::panel::tool_l,           xset::name::panel2_tool_l},
    {xset::panel::tool_r,           xset::name::panel2_tool_r},
    {xset::panel::tool_s,           xset::name::panel2_tool_s},
};

// panel3
static const std::unordered_map<xset::panel, xset::name> xset_panel3_map{
    // panel3
    {xset::panel::show,             xset::name::panel3_show},
    {xset::panel::show_toolbox,     xset::name::panel3_show_toolbox},
    {xset::panel::show_devmon,      xset::name::panel3_show_devmon},
    {xset::panel::show_dirtree,     xset::name::panel3_show_dirtree},
    {xset::panel::show_sidebar,     xset::name::panel3_show_sidebar},
    {xset::panel::slider_positions, xset::name::panel3_slider_positions},
    {xset::panel::list_detailed,    xset::name::panel3_list_detailed},
    {xset::panel::list_icons,       xset::name::panel3_list_icons},
    {xset::panel::list_compact,     xset::name::panel3_list_compact},
    {xset::panel::list_large,       xset::name::panel3_list_large},
    {xset::panel::show_hidden,      xset::name::panel3_show_hidden},
    {xset::panel::icon_tab,         xset::name::panel3_icon_tab},
    {xset::panel::icon_status,      xset::name::panel3_icon_status},
    {xset::panel::detcol_name,      xset::name::panel3_detcol_name},
    {xset::panel::detcol_size,      xset::name::panel3_detcol_size},
    {xset::panel::detcol_type,      xset::name::panel3_detcol_type},
    {xset::panel::detcol_perm,      xset::name::panel3_detcol_perm},
    {xset::panel::detcol_owner,     xset::name::panel3_detcol_owner},
    {xset::panel::detcol_date,      xset::name::panel3_detcol_date},
    {xset::panel::sort_extra,       xset::name::panel3_sort_extra},
    {xset::panel::book_fol,         xset::name::panel3_book_fol},
    {xset::panel::tool_l,           xset::name::panel3_tool_l},
    {xset::panel::tool_r,           xset::name::panel3_tool_r},
    {xset::panel::tool_s,           xset::name::panel3_tool_s},
};

// panel4
static const std::unordered_map<xset::panel, xset::name> xset_panel4_map{
    // panel4
    {xset::panel::show,             xset::name::panel4_show},
    {xset::panel::show_toolbox,     xset::name::panel4_show_toolbox},
    {xset::panel::show_devmon,      xset::name::panel4_show_devmon},
    {xset::panel::show_dirtree,     xset::name::panel4_show_dirtree},
    {xset::panel::show_sidebar,     xset::name::panel4_show_sidebar},
    {xset::panel::slider_positions, xset::name::panel4_slider_positions},
    {xset::panel::list_detailed,    xset::name::panel4_list_detailed},
    {xset::panel::list_icons,       xset::name::panel4_list_icons},
    {xset::panel::list_compact,     xset::name::panel4_list_compact},
    {xset::panel::list_large,       xset::name::panel4_list_large},
    {xset::panel::show_hidden,      xset::name::panel4_show_hidden},
    {xset::panel::icon_tab,         xset::name::panel4_icon_tab},
    {xset::panel::icon_status,      xset::name::panel4_icon_status},
    {xset::panel::detcol_name,      xset::name::panel4_detcol_name},
    {xset::panel::detcol_size,      xset::name::panel4_detcol_size},
    {xset::panel::detcol_type,      xset::name::panel4_detcol_type},
    {xset::panel::detcol_perm,      xset::name::panel4_detcol_perm},
    {xset::panel::detcol_owner,     xset::name::panel4_detcol_owner},
    {xset::panel::detcol_date,      xset::name::panel4_detcol_date},
    {xset::panel::sort_extra,       xset::name::panel4_sort_extra},
    {xset::panel::book_fol,         xset::name::panel4_book_fol},
    {xset::panel::tool_l,           xset::name::panel4_tool_l},
    {xset::panel::tool_r,           xset::name::panel4_tool_r},
    {xset::panel::tool_s,           xset::name::panel4_tool_s},
};

// panel modes

// panel1

// panel1 mode 0
static const std::unordered_map<xset::panel, xset::name> xset_panel1_mode0_map{
    {xset::panel::show_toolbox,     xset::name::panel1_show_toolbox_0},
    {xset::panel::show_devmon,      xset::name::panel1_show_devmon_0},
    {xset::panel::show_dirtree,     xset::name::panel1_show_dirtree_0},
    {xset::panel::show_sidebar,     xset::name::panel1_show_sidebar_0},
    {xset::panel::slider_positions, xset::name::panel1_slider_positions_0},
    {xset::panel::list_detailed,    xset::name::panel1_list_detailed_0},
    {xset::panel::list_icons,       xset::name::panel1_list_icons_0},
    {xset::panel::list_compact,     xset::name::panel1_list_compact_0},
    {xset::panel::list_large,       xset::name::panel1_list_large_0},
    {xset::panel::show_hidden,      xset::name::panel1_show_hidden_0},
    {xset::panel::icon_tab,         xset::name::panel1_icon_tab_0},
    {xset::panel::icon_status,      xset::name::panel1_icon_status_0},
    {xset::panel::detcol_name,      xset::name::panel1_detcol_name_0},
    {xset::panel::detcol_size,      xset::name::panel1_detcol_size_0},
    {xset::panel::detcol_type,      xset::name::panel1_detcol_type_0},
    {xset::panel::detcol_perm,      xset::name::panel1_detcol_perm_0},
    {xset::panel::detcol_owner,     xset::name::panel1_detcol_owner_0},
    {xset::panel::detcol_date,      xset::name::panel1_detcol_date_0},
    {xset::panel::sort_extra,       xset::name::panel1_sort_extra_0},
    {xset::panel::book_fol,         xset::name::panel1_book_fol_0},
    {xset::panel::tool_l,           xset::name::panel1_tool_l_0},
    {xset::panel::tool_r,           xset::name::panel1_tool_r_0},
    {xset::panel::tool_s,           xset::name::panel1_tool_s_0},
};

// panel1 mode 1
static const std::unordered_map<xset::panel, xset::name> xset_panel1_mode1_map{
    {xset::panel::show_toolbox,     xset::name::panel1_show_toolbox_1},
    {xset::panel::show_devmon,      xset::name::panel1_show_devmon_1},
    {xset::panel::show_dirtree,     xset::name::panel1_show_dirtree_1},
    {xset::panel::show_sidebar,     xset::name::panel1_show_sidebar_1},
    {xset::panel::slider_positions, xset::name::panel1_slider_positions_1},
    {xset::panel::list_detailed,    xset::name::panel1_list_detailed_1},
    {xset::panel::list_icons,       xset::name::panel1_list_icons_1},
    {xset::panel::list_compact,     xset::name::panel1_list_compact_1},
    {xset::panel::list_large,       xset::name::panel1_list_large_1},
    {xset::panel::show_hidden,      xset::name::panel1_show_hidden_1},
    {xset::panel::icon_tab,         xset::name::panel1_icon_tab_1},
    {xset::panel::icon_status,      xset::name::panel1_icon_status_1},
    {xset::panel::detcol_name,      xset::name::panel1_detcol_name_1},
    {xset::panel::detcol_size,      xset::name::panel1_detcol_size_1},
    {xset::panel::detcol_type,      xset::name::panel1_detcol_type_1},
    {xset::panel::detcol_perm,      xset::name::panel1_detcol_perm_1},
    {xset::panel::detcol_owner,     xset::name::panel1_detcol_owner_1},
    {xset::panel::detcol_date,      xset::name::panel1_detcol_date_1},
    {xset::panel::sort_extra,       xset::name::panel1_sort_extra_1},
    {xset::panel::book_fol,         xset::name::panel1_book_fol_1},
    {xset::panel::tool_l,           xset::name::panel1_tool_l_1},
    {xset::panel::tool_r,           xset::name::panel1_tool_r_1},
    {xset::panel::tool_s,           xset::name::panel1_tool_s_1},
};

// panel1 mode 2
static const std::unordered_map<xset::panel, xset::name> xset_panel1_mode2_map{
    {xset::panel::show_toolbox,     xset::name::panel1_show_toolbox_2},
    {xset::panel::show_devmon,      xset::name::panel1_show_devmon_2},
    {xset::panel::show_dirtree,     xset::name::panel1_show_dirtree_2},
    {xset::panel::show_sidebar,     xset::name::panel1_show_sidebar_2},
    {xset::panel::slider_positions, xset::name::panel1_slider_positions_2},
    {xset::panel::list_detailed,    xset::name::panel1_list_detailed_2},
    {xset::panel::list_icons,       xset::name::panel1_list_icons_2},
    {xset::panel::list_compact,     xset::name::panel1_list_compact_2},
    {xset::panel::list_large,       xset::name::panel1_list_large_2},
    {xset::panel::show_hidden,      xset::name::panel1_show_hidden_2},
    {xset::panel::icon_tab,         xset::name::panel1_icon_tab_2},
    {xset::panel::icon_status,      xset::name::panel1_icon_status_2},
    {xset::panel::detcol_name,      xset::name::panel1_detcol_name_2},
    {xset::panel::detcol_size,      xset::name::panel1_detcol_size_2},
    {xset::panel::detcol_type,      xset::name::panel1_detcol_type_2},
    {xset::panel::detcol_perm,      xset::name::panel1_detcol_perm_2},
    {xset::panel::detcol_owner,     xset::name::panel1_detcol_owner_2},
    {xset::panel::detcol_date,      xset::name::panel1_detcol_date_2},
    {xset::panel::sort_extra,       xset::name::panel1_sort_extra_2},
    {xset::panel::book_fol,         xset::name::panel1_book_fol_2},
    {xset::panel::tool_l,           xset::name::panel1_tool_l_2},
    {xset::panel::tool_r,           xset::name::panel1_tool_r_2},
    {xset::panel::tool_s,           xset::name::panel1_tool_s_2},
};

// panel1 mode 3
static const std::unordered_map<xset::panel, xset::name> xset_panel1_mode3_map{
    {xset::panel::show_toolbox,     xset::name::panel1_show_toolbox_3},
    {xset::panel::show_devmon,      xset::name::panel1_show_devmon_3},
    {xset::panel::show_dirtree,     xset::name::panel1_show_dirtree_3},
    {xset::panel::show_sidebar,     xset::name::panel1_show_sidebar_3},
    {xset::panel::slider_positions, xset::name::panel1_slider_positions_3},
    {xset::panel::list_detailed,    xset::name::panel1_list_detailed_3},
    {xset::panel::list_icons,       xset::name::panel1_list_icons_3},
    {xset::panel::list_compact,     xset::name::panel1_list_compact_3},
    {xset::panel::list_large,       xset::name::panel1_list_large_3},
    {xset::panel::show_hidden,      xset::name::panel1_show_hidden_3},
    {xset::panel::icon_tab,         xset::name::panel1_icon_tab_3},
    {xset::panel::icon_status,      xset::name::panel1_icon_status_3},
    {xset::panel::detcol_name,      xset::name::panel1_detcol_name_3},
    {xset::panel::detcol_size,      xset::name::panel1_detcol_size_3},
    {xset::panel::detcol_type,      xset::name::panel1_detcol_type_3},
    {xset::panel::detcol_perm,      xset::name::panel1_detcol_perm_3},
    {xset::panel::detcol_owner,     xset::name::panel1_detcol_owner_3},
    {xset::panel::detcol_date,      xset::name::panel1_detcol_date_3},
    {xset::panel::sort_extra,       xset::name::panel1_sort_extra_3},
    {xset::panel::book_fol,         xset::name::panel1_book_fol_3},
    {xset::panel::tool_l,           xset::name::panel1_tool_l_3},
    {xset::panel::tool_r,           xset::name::panel1_tool_r_3},
    {xset::panel::tool_s,           xset::name::panel1_tool_s_3},
};

// panel2

// panel2 mode 0
static const std::unordered_map<xset::panel, xset::name> xset_panel2_mode0_map{
    {xset::panel::show_toolbox,     xset::name::panel2_show_toolbox_0},
    {xset::panel::show_devmon,      xset::name::panel2_show_devmon_0},
    {xset::panel::show_dirtree,     xset::name::panel2_show_dirtree_0},
    {xset::panel::show_sidebar,     xset::name::panel2_show_sidebar_0},
    {xset::panel::slider_positions, xset::name::panel2_slider_positions_0},
    {xset::panel::list_detailed,    xset::name::panel2_list_detailed_0},
    {xset::panel::list_icons,       xset::name::panel2_list_icons_0},
    {xset::panel::list_compact,     xset::name::panel2_list_compact_0},
    {xset::panel::list_large,       xset::name::panel2_list_large_0},
    {xset::panel::show_hidden,      xset::name::panel2_show_hidden_0},
    {xset::panel::icon_tab,         xset::name::panel2_icon_tab_0},
    {xset::panel::icon_status,      xset::name::panel2_icon_status_0},
    {xset::panel::detcol_name,      xset::name::panel2_detcol_name_0},
    {xset::panel::detcol_size,      xset::name::panel2_detcol_size_0},
    {xset::panel::detcol_type,      xset::name::panel2_detcol_type_0},
    {xset::panel::detcol_perm,      xset::name::panel2_detcol_perm_0},
    {xset::panel::detcol_owner,     xset::name::panel2_detcol_owner_0},
    {xset::panel::detcol_date,      xset::name::panel2_detcol_date_0},
    {xset::panel::sort_extra,       xset::name::panel2_sort_extra_0},
    {xset::panel::book_fol,         xset::name::panel2_book_fol_0},
    {xset::panel::tool_l,           xset::name::panel2_tool_l_0},
    {xset::panel::tool_r,           xset::name::panel2_tool_r_0},
    {xset::panel::tool_s,           xset::name::panel2_tool_s_0},
};

// panel2 mode 1
static const std::unordered_map<xset::panel, xset::name> xset_panel2_mode1_map{
    {xset::panel::show_toolbox,     xset::name::panel2_show_toolbox_1},
    {xset::panel::show_devmon,      xset::name::panel2_show_devmon_1},
    {xset::panel::show_dirtree,     xset::name::panel2_show_dirtree_1},
    {xset::panel::show_sidebar,     xset::name::panel2_show_sidebar_1},
    {xset::panel::slider_positions, xset::name::panel2_slider_positions_1},
    {xset::panel::list_detailed,    xset::name::panel2_list_detailed_1},
    {xset::panel::list_icons,       xset::name::panel2_list_icons_1},
    {xset::panel::list_compact,     xset::name::panel2_list_compact_1},
    {xset::panel::list_large,       xset::name::panel2_list_large_1},
    {xset::panel::show_hidden,      xset::name::panel2_show_hidden_1},
    {xset::panel::icon_tab,         xset::name::panel2_icon_tab_1},
    {xset::panel::icon_status,      xset::name::panel2_icon_status_1},
    {xset::panel::detcol_name,      xset::name::panel2_detcol_name_1},
    {xset::panel::detcol_size,      xset::name::panel2_detcol_size_1},
    {xset::panel::detcol_type,      xset::name::panel2_detcol_type_1},
    {xset::panel::detcol_perm,      xset::name::panel2_detcol_perm_1},
    {xset::panel::detcol_owner,     xset::name::panel2_detcol_owner_1},
    {xset::panel::detcol_date,      xset::name::panel2_detcol_date_1},
    {xset::panel::sort_extra,       xset::name::panel2_sort_extra_1},
    {xset::panel::book_fol,         xset::name::panel2_book_fol_1},
    {xset::panel::tool_l,           xset::name::panel2_tool_l_1},
    {xset::panel::tool_r,           xset::name::panel2_tool_r_1},
    {xset::panel::tool_s,           xset::name::panel2_tool_s_1},
};

// panel2 mode 2
static const std::unordered_map<xset::panel, xset::name> xset_panel2_mode2_map{
    {xset::panel::show_toolbox,     xset::name::panel2_show_toolbox_2},
    {xset::panel::show_devmon,      xset::name::panel2_show_devmon_2},
    {xset::panel::show_dirtree,     xset::name::panel2_show_dirtree_2},
    {xset::panel::show_sidebar,     xset::name::panel2_show_sidebar_2},
    {xset::panel::slider_positions, xset::name::panel2_slider_positions_2},
    {xset::panel::list_detailed,    xset::name::panel2_list_detailed_2},
    {xset::panel::list_icons,       xset::name::panel2_list_icons_2},
    {xset::panel::list_compact,     xset::name::panel2_list_compact_2},
    {xset::panel::list_large,       xset::name::panel2_list_large_2},
    {xset::panel::show_hidden,      xset::name::panel2_show_hidden_2},
    {xset::panel::icon_tab,         xset::name::panel2_icon_tab_2},
    {xset::panel::icon_status,      xset::name::panel2_icon_status_2},
    {xset::panel::detcol_name,      xset::name::panel2_detcol_name_2},
    {xset::panel::detcol_size,      xset::name::panel2_detcol_size_2},
    {xset::panel::detcol_type,      xset::name::panel2_detcol_type_2},
    {xset::panel::detcol_perm,      xset::name::panel2_detcol_perm_2},
    {xset::panel::detcol_owner,     xset::name::panel2_detcol_owner_2},
    {xset::panel::detcol_date,      xset::name::panel2_detcol_date_2},
    {xset::panel::sort_extra,       xset::name::panel2_sort_extra_2},
    {xset::panel::book_fol,         xset::name::panel2_book_fol_2},
    {xset::panel::tool_l,           xset::name::panel2_tool_l_2},
    {xset::panel::tool_r,           xset::name::panel2_tool_r_2},
    {xset::panel::tool_s,           xset::name::panel2_tool_s_2},
};

// panel2 mode 3
static const std::unordered_map<xset::panel, xset::name> xset_panel2_mode3_map{
    {xset::panel::show_toolbox,     xset::name::panel2_show_toolbox_3},
    {xset::panel::show_devmon,      xset::name::panel2_show_devmon_3},
    {xset::panel::show_dirtree,     xset::name::panel2_show_dirtree_3},
    {xset::panel::show_sidebar,     xset::name::panel2_show_sidebar_3},
    {xset::panel::slider_positions, xset::name::panel2_slider_positions_3},
    {xset::panel::list_detailed,    xset::name::panel2_list_detailed_3},
    {xset::panel::list_icons,       xset::name::panel2_list_icons_3},
    {xset::panel::list_compact,     xset::name::panel2_list_compact_3},
    {xset::panel::list_large,       xset::name::panel2_list_large_3},
    {xset::panel::show_hidden,      xset::name::panel2_show_hidden_3},
    {xset::panel::icon_tab,         xset::name::panel2_icon_tab_3},
    {xset::panel::icon_status,      xset::name::panel2_icon_status_3},
    {xset::panel::detcol_name,      xset::name::panel2_detcol_name_3},
    {xset::panel::detcol_size,      xset::name::panel2_detcol_size_3},
    {xset::panel::detcol_type,      xset::name::panel2_detcol_type_3},
    {xset::panel::detcol_perm,      xset::name::panel2_detcol_perm_3},
    {xset::panel::detcol_owner,     xset::name::panel2_detcol_owner_3},
    {xset::panel::detcol_date,      xset::name::panel2_detcol_date_3},
    {xset::panel::sort_extra,       xset::name::panel2_sort_extra_3},
    {xset::panel::book_fol,         xset::name::panel2_book_fol_3},
    {xset::panel::tool_l,           xset::name::panel2_tool_l_3},
    {xset::panel::tool_r,           xset::name::panel2_tool_r_3},
    {xset::panel::tool_s,           xset::name::panel2_tool_s_3},
};

// panel3

// panel3 mode 0
static const std::unordered_map<xset::panel, xset::name> xset_panel3_mode0_map{
    {xset::panel::show_toolbox,     xset::name::panel3_show_toolbox_0},
    {xset::panel::show_devmon,      xset::name::panel3_show_devmon_0},
    {xset::panel::show_dirtree,     xset::name::panel3_show_dirtree_0},
    {xset::panel::show_sidebar,     xset::name::panel3_show_sidebar_0},
    {xset::panel::slider_positions, xset::name::panel3_slider_positions_0},
    {xset::panel::list_detailed,    xset::name::panel3_list_detailed_0},
    {xset::panel::list_icons,       xset::name::panel3_list_icons_0},
    {xset::panel::list_compact,     xset::name::panel3_list_compact_0},
    {xset::panel::list_large,       xset::name::panel3_list_large_0},
    {xset::panel::show_hidden,      xset::name::panel3_show_hidden_0},
    {xset::panel::icon_tab,         xset::name::panel3_icon_tab_0},
    {xset::panel::icon_status,      xset::name::panel3_icon_status_0},
    {xset::panel::detcol_name,      xset::name::panel3_detcol_name_0},
    {xset::panel::detcol_size,      xset::name::panel3_detcol_size_0},
    {xset::panel::detcol_type,      xset::name::panel3_detcol_type_0},
    {xset::panel::detcol_perm,      xset::name::panel3_detcol_perm_0},
    {xset::panel::detcol_owner,     xset::name::panel3_detcol_owner_0},
    {xset::panel::detcol_date,      xset::name::panel3_detcol_date_0},
    {xset::panel::sort_extra,       xset::name::panel3_sort_extra_0},
    {xset::panel::book_fol,         xset::name::panel3_book_fol_0},
    {xset::panel::tool_l,           xset::name::panel3_tool_l_0},
    {xset::panel::tool_r,           xset::name::panel3_tool_r_0},
    {xset::panel::tool_s,           xset::name::panel3_tool_s_0},
};

// panel3 mode 1
static const std::unordered_map<xset::panel, xset::name> xset_panel3_mode1_map{
    {xset::panel::show_toolbox,     xset::name::panel3_show_toolbox_1},
    {xset::panel::show_devmon,      xset::name::panel3_show_devmon_1},
    {xset::panel::show_dirtree,     xset::name::panel3_show_dirtree_1},
    {xset::panel::show_sidebar,     xset::name::panel3_show_sidebar_1},
    {xset::panel::slider_positions, xset::name::panel3_slider_positions_1},
    {xset::panel::list_detailed,    xset::name::panel3_list_detailed_1},
    {xset::panel::list_icons,       xset::name::panel3_list_icons_1},
    {xset::panel::list_compact,     xset::name::panel3_list_compact_1},
    {xset::panel::list_large,       xset::name::panel3_list_large_1},
    {xset::panel::show_hidden,      xset::name::panel3_show_hidden_1},
    {xset::panel::icon_tab,         xset::name::panel3_icon_tab_1},
    {xset::panel::icon_status,      xset::name::panel3_icon_status_1},
    {xset::panel::detcol_name,      xset::name::panel3_detcol_name_1},
    {xset::panel::detcol_size,      xset::name::panel3_detcol_size_1},
    {xset::panel::detcol_type,      xset::name::panel3_detcol_type_1},
    {xset::panel::detcol_perm,      xset::name::panel3_detcol_perm_1},
    {xset::panel::detcol_owner,     xset::name::panel3_detcol_owner_1},
    {xset::panel::detcol_date,      xset::name::panel3_detcol_date_1},
    {xset::panel::sort_extra,       xset::name::panel3_sort_extra_1},
    {xset::panel::book_fol,         xset::name::panel3_book_fol_1},
    {xset::panel::tool_l,           xset::name::panel3_tool_l_1},
    {xset::panel::tool_r,           xset::name::panel3_tool_r_1},
    {xset::panel::tool_s,           xset::name::panel3_tool_s_1},
};

// panel3 mode 2
static const std::unordered_map<xset::panel, xset::name> xset_panel3_mode2_map{
    {xset::panel::show_toolbox,     xset::name::panel3_show_toolbox_2},
    {xset::panel::show_devmon,      xset::name::panel3_show_devmon_2},
    {xset::panel::show_dirtree,     xset::name::panel3_show_dirtree_2},
    {xset::panel::show_sidebar,     xset::name::panel3_show_sidebar_2},
    {xset::panel::slider_positions, xset::name::panel3_slider_positions_2},
    {xset::panel::list_detailed,    xset::name::panel3_list_detailed_2},
    {xset::panel::list_icons,       xset::name::panel3_list_icons_2},
    {xset::panel::list_compact,     xset::name::panel3_list_compact_2},
    {xset::panel::list_large,       xset::name::panel3_list_large_2},
    {xset::panel::show_hidden,      xset::name::panel3_show_hidden_2},
    {xset::panel::icon_tab,         xset::name::panel3_icon_tab_2},
    {xset::panel::icon_status,      xset::name::panel3_icon_status_2},
    {xset::panel::detcol_name,      xset::name::panel3_detcol_name_2},
    {xset::panel::detcol_size,      xset::name::panel3_detcol_size_2},
    {xset::panel::detcol_type,      xset::name::panel3_detcol_type_2},
    {xset::panel::detcol_perm,      xset::name::panel3_detcol_perm_2},
    {xset::panel::detcol_owner,     xset::name::panel3_detcol_owner_2},
    {xset::panel::detcol_date,      xset::name::panel3_detcol_date_2},
    {xset::panel::sort_extra,       xset::name::panel3_sort_extra_2},
    {xset::panel::book_fol,         xset::name::panel3_book_fol_2},
    {xset::panel::tool_l,           xset::name::panel3_tool_l_2},
    {xset::panel::tool_r,           xset::name::panel3_tool_r_2},
    {xset::panel::tool_s,           xset::name::panel3_tool_s_2},
};

// panel3 mode 3
static const std::unordered_map<xset::panel, xset::name> xset_panel3_mode3_map{
    {xset::panel::show_toolbox,     xset::name::panel3_show_toolbox_3},
    {xset::panel::show_devmon,      xset::name::panel3_show_devmon_3},
    {xset::panel::show_dirtree,     xset::name::panel3_show_dirtree_3},
    {xset::panel::show_sidebar,     xset::name::panel3_show_sidebar_3},
    {xset::panel::slider_positions, xset::name::panel3_slider_positions_3},
    {xset::panel::list_detailed,    xset::name::panel3_list_detailed_3},
    {xset::panel::list_icons,       xset::name::panel3_list_icons_3},
    {xset::panel::list_compact,     xset::name::panel3_list_compact_3},
    {xset::panel::list_large,       xset::name::panel3_list_large_3},
    {xset::panel::show_hidden,      xset::name::panel3_show_hidden_3},
    {xset::panel::icon_tab,         xset::name::panel3_icon_tab_3},
    {xset::panel::icon_status,      xset::name::panel3_icon_status_3},
    {xset::panel::detcol_name,      xset::name::panel3_detcol_name_3},
    {xset::panel::detcol_size,      xset::name::panel3_detcol_size_3},
    {xset::panel::detcol_type,      xset::name::panel3_detcol_type_3},
    {xset::panel::detcol_perm,      xset::name::panel3_detcol_perm_3},
    {xset::panel::detcol_owner,     xset::name::panel3_detcol_owner_3},
    {xset::panel::detcol_date,      xset::name::panel3_detcol_date_3},
    {xset::panel::sort_extra,       xset::name::panel3_sort_extra_3},
    {xset::panel::book_fol,         xset::name::panel3_book_fol_3},
    {xset::panel::tool_l,           xset::name::panel3_tool_l_3},
    {xset::panel::tool_r,           xset::name::panel3_tool_r_3},
    {xset::panel::tool_s,           xset::name::panel3_tool_s_3},
};

// panel4

// panel4 mode 0
static const std::unordered_map<xset::panel, xset::name> xset_panel4_mode0_map{
    {xset::panel::show_toolbox,     xset::name::panel4_show_toolbox_0},
    {xset::panel::show_devmon,      xset::name::panel4_show_devmon_0},
    {xset::panel::show_dirtree,     xset::name::panel4_show_dirtree_0},
    {xset::panel::show_sidebar,     xset::name::panel4_show_sidebar_0},
    {xset::panel::slider_positions, xset::name::panel4_slider_positions_0},
    {xset::panel::list_detailed,    xset::name::panel4_list_detailed_0},
    {xset::panel::list_icons,       xset::name::panel4_list_icons_0},
    {xset::panel::list_compact,     xset::name::panel4_list_compact_0},
    {xset::panel::list_large,       xset::name::panel4_list_large_0},
    {xset::panel::show_hidden,      xset::name::panel4_show_hidden_0},
    {xset::panel::icon_tab,         xset::name::panel4_icon_tab_0},
    {xset::panel::icon_status,      xset::name::panel4_icon_status_0},
    {xset::panel::detcol_name,      xset::name::panel4_detcol_name_0},
    {xset::panel::detcol_size,      xset::name::panel4_detcol_size_0},
    {xset::panel::detcol_type,      xset::name::panel4_detcol_type_0},
    {xset::panel::detcol_perm,      xset::name::panel4_detcol_perm_0},
    {xset::panel::detcol_owner,     xset::name::panel4_detcol_owner_0},
    {xset::panel::detcol_date,      xset::name::panel4_detcol_date_0},
    {xset::panel::sort_extra,       xset::name::panel4_sort_extra_0},
    {xset::panel::book_fol,         xset::name::panel4_book_fol_0},
    {xset::panel::tool_l,           xset::name::panel4_tool_l_0},
    {xset::panel::tool_r,           xset::name::panel4_tool_r_0},
    {xset::panel::tool_s,           xset::name::panel4_tool_s_0},
};

// panel4 mode 1
static const std::unordered_map<xset::panel, xset::name> xset_panel4_mode1_map{
    {xset::panel::show_toolbox,     xset::name::panel4_show_toolbox_1},
    {xset::panel::show_devmon,      xset::name::panel4_show_devmon_1},
    {xset::panel::show_dirtree,     xset::name::panel4_show_dirtree_1},
    {xset::panel::show_sidebar,     xset::name::panel4_show_sidebar_1},
    {xset::panel::slider_positions, xset::name::panel4_slider_positions_1},
    {xset::panel::list_detailed,    xset::name::panel4_list_detailed_1},
    {xset::panel::list_icons,       xset::name::panel4_list_icons_1},
    {xset::panel::list_compact,     xset::name::panel4_list_compact_1},
    {xset::panel::list_large,       xset::name::panel4_list_large_1},
    {xset::panel::show_hidden,      xset::name::panel4_show_hidden_1},
    {xset::panel::icon_tab,         xset::name::panel4_icon_tab_1},
    {xset::panel::icon_status,      xset::name::panel4_icon_status_1},
    {xset::panel::detcol_name,      xset::name::panel4_detcol_name_1},
    {xset::panel::detcol_size,      xset::name::panel4_detcol_size_1},
    {xset::panel::detcol_type,      xset::name::panel4_detcol_type_1},
    {xset::panel::detcol_perm,      xset::name::panel4_detcol_perm_1},
    {xset::panel::detcol_owner,     xset::name::panel4_detcol_owner_1},
    {xset::panel::detcol_date,      xset::name::panel4_detcol_date_1},
    {xset::panel::sort_extra,       xset::name::panel4_sort_extra_1},
    {xset::panel::book_fol,         xset::name::panel4_book_fol_1},
    {xset::panel::tool_l,           xset::name::panel4_tool_l_1},
    {xset::panel::tool_r,           xset::name::panel4_tool_r_1},
    {xset::panel::tool_s,           xset::name::panel4_tool_s_1},
};

// panel4 mode 2
static const std::unordered_map<xset::panel, xset::name> xset_panel4_mode2_map{
    {xset::panel::show_toolbox,     xset::name::panel4_show_toolbox_2},
    {xset::panel::show_devmon,      xset::name::panel4_show_devmon_2},
    {xset::panel::show_dirtree,     xset::name::panel4_show_dirtree_2},
    {xset::panel::show_sidebar,     xset::name::panel4_show_sidebar_2},
    {xset::panel::slider_positions, xset::name::panel4_slider_positions_2},
    {xset::panel::list_detailed,    xset::name::panel4_list_detailed_2},
    {xset::panel::list_icons,       xset::name::panel4_list_icons_2},
    {xset::panel::list_compact,     xset::name::panel4_list_compact_2},
    {xset::panel::list_large,       xset::name::panel4_list_large_2},
    {xset::panel::show_hidden,      xset::name::panel4_show_hidden_2},
    {xset::panel::icon_tab,         xset::name::panel4_icon_tab_2},
    {xset::panel::icon_status,      xset::name::panel4_icon_status_2},
    {xset::panel::detcol_name,      xset::name::panel4_detcol_name_2},
    {xset::panel::detcol_size,      xset::name::panel4_detcol_size_2},
    {xset::panel::detcol_type,      xset::name::panel4_detcol_type_2},
    {xset::panel::detcol_perm,      xset::name::panel4_detcol_perm_2},
    {xset::panel::detcol_owner,     xset::name::panel4_detcol_owner_2},
    {xset::panel::detcol_date,      xset::name::panel4_detcol_date_2},
    {xset::panel::sort_extra,       xset::name::panel4_sort_extra_2},
    {xset::panel::book_fol,         xset::name::panel4_book_fol_2},
    {xset::panel::tool_l,           xset::name::panel4_tool_l_2},
    {xset::panel::tool_r,           xset::name::panel4_tool_r_2},
    {xset::panel::tool_s,           xset::name::panel4_tool_s_2},
};

// panel4 mode 3
static const std::unordered_map<xset::panel, xset::name> xset_panel4_mode3_map{
    {xset::panel::show_toolbox,     xset::name::panel4_show_toolbox_3},
    {xset::panel::show_devmon,      xset::name::panel4_show_devmon_3},
    {xset::panel::show_dirtree,     xset::name::panel4_show_dirtree_3},
    {xset::panel::show_sidebar,     xset::name::panel4_show_sidebar_3},
    {xset::panel::slider_positions, xset::name::panel4_slider_positions_3},
    {xset::panel::list_detailed,    xset::name::panel4_list_detailed_3},
    {xset::panel::list_icons,       xset::name::panel4_list_icons_3},
    {xset::panel::list_compact,     xset::name::panel4_list_compact_3},
    {xset::panel::list_large,       xset::name::panel4_list_large_3},
    {xset::panel::show_hidden,      xset::name::panel4_show_hidden_3},
    {xset::panel::icon_tab,         xset::name::panel4_icon_tab_3},
    {xset::panel::icon_status,      xset::name::panel4_icon_status_3},
    {xset::panel::detcol_name,      xset::name::panel4_detcol_name_3},
    {xset::panel::detcol_size,      xset::name::panel4_detcol_size_3},
    {xset::panel::detcol_type,      xset::name::panel4_detcol_type_3},
    {xset::panel::detcol_perm,      xset::name::panel4_detcol_perm_3},
    {xset::panel::detcol_owner,     xset::name::panel4_detcol_owner_3},
    {xset::panel::detcol_date,      xset::name::panel4_detcol_date_3},
    {xset::panel::sort_extra,       xset::name::panel4_sort_extra_3},
    {xset::panel::book_fol,         xset::name::panel4_book_fol_3},
    {xset::panel::tool_l,           xset::name::panel4_tool_l_3},
    {xset::panel::tool_r,           xset::name::panel4_tool_r_3},
    {xset::panel::tool_s,           xset::name::panel4_tool_s_3},
};

static const std::unordered_map<std::string_view, xset::var> xset_var_map{
    {"s"sv,                 xset::var::s},
    {"b"sv,                 xset::var::b},
    {"x"sv,                 xset::var::x},
    {"y"sv,                 xset::var::y},
    {"z"sv,                 xset::var::z},
    {"key"sv,               xset::var::key},
    {"keymod"sv,            xset::var::keymod},
    {"style"sv,             xset::var::style},
    {"desc"sv,              xset::var::desc},
    {"title"sv,             xset::var::title},
    {"menu_label"sv,        xset::var::menu_label},
    {"menu_label_custom"sv, xset::var::menu_label_custom},
    {"icn"sv,               xset::var::icn},
    {"icon"sv,              xset::var::icon},
    {"shared_key"sv,        xset::var::shared_key},
    {"next"sv,              xset::var::next},
    {"prev"sv,              xset::var::prev},
    {"parent"sv,            xset::var::parent},
    {"child"sv,             xset::var::child},
    {"context"sv,           xset::var::context},
    {"line"sv,              xset::var::line},
    {"tool"sv,              xset::var::tool},
    {"task"sv,              xset::var::task},
    {"task_pop"sv,          xset::var::task_pop},
    {"task_err"sv,          xset::var::task_out},
    {"task_out"sv,          xset::var::task_out},
    {"run_in_terminal"sv,   xset::var::run_in_terminal},
    {"keep_terminal"sv,     xset::var::keep_terminal},
    {"scroll_lock"sv,       xset::var::scroll_lock},
    {"disable"sv,           xset::var::disable},
    {"opener"sv,            xset::var::opener},
};

static const std::unordered_map<xset::main_window_panel, std::string_view> main_window_panel_mode_map{
    {xset::main_window_panel::panel_neither, "0"sv},
    {xset::main_window_panel::panel_horiz,   "1"sv},
    {xset::main_window_panel::panel_vert,    "2"sv},
    {xset::main_window_panel::panel_both,    "3"sv},
};

// clang-format on

xset::name
xset::get_xsetname_from_name(const std::string_view name)
{
#if defined(XSET_MAGIC_ENUM_LOOKUP)
    const auto enum_value = magic_enum::enum_cast<xset::name>(ztd::upper(name));
    if (enum_value.has_value())
    {
        const auto value = enum_value.value();
        return value;
    }
    else
    {
        return xset::name::custom;
    }
#else
    for (const auto& it : xset_name_map)
    {
        if (ztd::same(name, it.second))
        {
            return it.first;
        }
    }
    return xset::name::custom;
#endif
}

const std::string
xset::get_name_from_xsetname(xset::name name)
{
#if defined(XSET_MAGIC_ENUM_LOOKUP)
    const auto value = magic_enum::enum_name<xset::name>(name);
    return value.data();
#else
    try
    {
        return xset_name_map.at(name).data();
    }
    catch (const std::out_of_range& e)
    {
        const auto msg =
            std::format("Not Implemented: xset::name::{}", magic_enum::enum_name(name));
        ztd::logger::critical(msg);
        throw xset::invalid_name(msg);
    }
#endif
}

// panel

xset::name
xset::get_xsetname_from_panel(panel_t panel, xset::panel name)
{
    assert(panel == 1 || panel == 2 || panel == 3 || panel == 4);

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
xset::get_name_from_panel(panel_t panel, xset::panel name)
{
#if defined(XSET_MAGIC_ENUM_LOOKUP)
    const auto set_name = xset::get_xsetname_from_panel(panel, name);
    const auto value = magic_enum::enum_name<xset::name>(set_name);
    return value.data();
#else
    const auto set_name = xset::get_xsetname_from_panel(panel, name);
    const auto value = xset_name_map.at(set_name);
    return value.data();
#endif
}

// panel mode

xset::name
xset::get_xsetname_from_panel_mode(panel_t panel, xset::panel name, xset::main_window_panel mode)
{
    assert(panel == 1 || panel == 2 || panel == 3 || panel == 4);

    switch (panel)
    {
        case 1:
            switch (mode)
            {
                case xset::main_window_panel::panel_neither:
                    return xset_panel1_mode0_map.at(name);
                case xset::main_window_panel::panel_horiz:
                    return xset_panel1_mode1_map.at(name);
                case xset::main_window_panel::panel_vert:
                    return xset_panel1_mode2_map.at(name);
                case xset::main_window_panel::panel_both:
                    return xset_panel1_mode3_map.at(name);
            }
        case 2:
            switch (mode)
            {
                case xset::main_window_panel::panel_neither:
                    return xset_panel2_mode0_map.at(name);
                case xset::main_window_panel::panel_horiz:
                    return xset_panel2_mode1_map.at(name);
                case xset::main_window_panel::panel_vert:
                    return xset_panel2_mode2_map.at(name);
                case xset::main_window_panel::panel_both:
                    return xset_panel2_mode3_map.at(name);
            }
        case 3:
            switch (mode)
            {
                case xset::main_window_panel::panel_neither:
                    return xset_panel3_mode0_map.at(name);
                case xset::main_window_panel::panel_horiz:
                    return xset_panel3_mode1_map.at(name);
                case xset::main_window_panel::panel_vert:
                    return xset_panel3_mode2_map.at(name);
                case xset::main_window_panel::panel_both:
                    return xset_panel3_mode3_map.at(name);
            }
        case 4:
            switch (mode)
            {
                case xset::main_window_panel::panel_neither:
                    return xset_panel4_mode0_map.at(name);
                case xset::main_window_panel::panel_horiz:
                    return xset_panel4_mode1_map.at(name);
                case xset::main_window_panel::panel_vert:
                    return xset_panel4_mode2_map.at(name);
                case xset::main_window_panel::panel_both:
                    return xset_panel4_mode3_map.at(name);
            }
        default:
            // ztd::logger::warn("Panel Mode out of range: {}, using panel 1",
            // std::to_string(mode));
            return xset::get_xsetname_from_panel_mode(1, name, mode);
    }
}

const std::string
xset::get_name_from_panel_mode(panel_t panel, xset::panel name, xset::main_window_panel mode)
{
#if defined(XSET_MAGIC_ENUM_LOOKUP)
    const auto set_name = xset::get_xsetname_from_panel_mode(panel, name, mode);
    const auto value = magic_enum::enum_name<xset::name>(set_name);
    return value.data();
#else
    const auto set_name = xset::get_xsetname_from_panel_mode(panel, name, mode);
    const auto value = xset_name_map.at(set_name);
    return value.data();
#endif
}

// xset var

xset::var
xset::get_xsetvar_from_name(const std::string_view name)
{
    try
    {
        return xset_var_map.at(name);
    }
    catch (const std::out_of_range& e)
    {
        const auto msg = std::format("Unknown xset::var {}", name);
        ztd::logger::critical(msg);
        throw std::logic_error(msg);
    }
}

const std::string
xset::get_name_from_xsetvar(xset::var name)
{
    for (const auto& it : xset_var_map)
    {
        if (name == it.second)
        {
            return it.first.data();
        }
    }

    const auto msg = std::format("Not Implemented: xset::var::{}", magic_enum::enum_name(name));
    ztd::logger::critical(msg);
    throw std::logic_error(msg);
}

// main window panel mode

const std::string
xset::get_window_panel_mode(xset::main_window_panel mode)
{
    try
    {
        return main_window_panel_mode_map.at(mode).data();
    }
    catch (const std::out_of_range& e)
    {
        const auto msg = std::format("Not Implemented: xset::main_window_panel::{}",
                                     magic_enum::enum_name(mode));
        ztd::logger::critical(msg);
        throw xset::invalid_name(msg);
    }
}
