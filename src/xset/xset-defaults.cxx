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

#include <span>
#include <vector>

#include <gtkmm.h>

#include <magic_enum/magic_enum.hpp>

#include <ztd/ztd.hxx>

#include "xset/xset-defaults.hxx"
#include "xset/xset.hxx"

#include "logger.hxx"

void
xset_defaults() noexcept
{
    // separator
    {
        const auto set = xset::set::get(xset::name::separator);
        set->menu.type = xset::set::menu_type::sep;
    }

    // dev menu
    {
        const auto set = xset::set::get(xset::name::dev_menu_remove);
        set->menu.label = "Remo_ve / Eject";
        set->icon = "gtk-disconnect";
    }

    {
        const auto set = xset::set::get(xset::name::dev_menu_unmount);
        set->menu.label = "_Unmount";
        set->icon = "gtk-remove";
    }

    {
        const auto set = xset::set::get(xset::name::dev_menu_open);
        set->menu.label = "_Open";
        set->icon = "gtk-open";
    }

    {
        const auto set = xset::set::get(xset::name::dev_menu_tab);
        set->menu.label = "Open In _Tab";
        set->icon = "gtk-add";
    }

    {
        const auto set = xset::set::get(xset::name::dev_menu_mount);
        set->menu.label = "_Mount";
        set->icon = "drive-removable-media";
    }

    {
        const auto set = xset::set::get(xset::name::dev_menu_mark);
        set->menu.label = "_Bookmark";
        set->icon = "gtk-add";
    }

    {
        const auto set = xset::set::get(xset::name::dev_menu_settings);
        set->menu.label = "Setti_ngs";
        set->icon = "gtk-properties";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::dev_show,
            xset::name::separator,
            xset::name::dev_menu_auto,
            xset::name::dev_change,
            xset::name::separator,
            xset::name::dev_single,
            xset::name::dev_newtab,
        };
    }

    // dev settings
    {
        const auto set = xset::set::get(xset::name::dev_show);
        set->menu.label = "S_how";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::dev_show_internal_drives,
            xset::name::dev_show_empty,
            xset::name::dev_show_partition_tables,
            xset::name::dev_show_net,
            xset::name::dev_show_file,
            xset::name::dev_ignore_udisks_hide,
            xset::name::dev_show_hide_volumes,
            xset::name::dev_dispname,
        };
    }

    {
        const auto set = xset::set::get(xset::name::dev_show_internal_drives);
        set->menu.label = "_Internal Drives";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset::set::get(xset::name::dev_show_empty);
        set->menu.label = "_Empty Drives";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset::set::get(xset::name::dev_show_partition_tables);
        set->menu.label = "_Partition Tables";
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset::set::get(xset::name::dev_show_net);
        set->menu.label = "Mounted _Networks";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset::set::get(xset::name::dev_show_file);
        set->menu.label = "Mounted _Other";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset::set::get(xset::name::dev_show_hide_volumes);
        set->menu.label = "_Volumes...";
        set->title = "Show/Hide Volumes";
        set->desc =
            "To force showing or hiding of some volumes, overriding other settings, you can "
            "specify the devices, volume labels, or device IDs in the space-separated list "
            "below.\n\nExample:  +/dev/sdd1 -Label With Space +ata-OCZ-part4\nThis would cause "
            "/dev/sdd1 and the OCZ device to be shown, and the volume with label \"Label With "
            "Space\" to be hidden.\n\nThere must be a space between entries and a plus or minus "
            "sign directly before each item.  This list is case-sensitive.\n\n";
    }

    {
        const auto set = xset::set::get(xset::name::dev_ignore_udisks_hide);
        set->menu.label = "Ignore _Hide Policy";
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset::set::get(xset::name::dev_dispname);
        set->menu.label = "_Display Name";
        set->menu.type = xset::set::menu_type::string;
        set->title = "Set Display Name Format";
        set->desc = "Enter device display name format:\n\nUse:\n\t%%v\tdevice filename (eg "
                    "sdd1)\n\t%%s\ttotal size (eg 800G)\n\t%%t\tfstype (eg ext4)\n\t%%l\tvolume "
                    "label (eg Label or [no media])\n\t%%m\tmount point if mounted, or "
                    "---\n\t%%i\tdevice ID\n\t%%n\tmajor:minor device numbers (eg 15:3)\n";
        set->s = "%v %s %l %m";
        set->z = "%v %s %l %m";
        set->icon = "gtk-edit";
    }

    {
        const auto set = xset::set::get(xset::name::dev_menu_auto);
        set->menu.label = "_Auto Mount";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::dev_automount_optical,
            xset::name::dev_automount_removable,
            xset::name::dev_ignore_udisks_nopolicy,
            xset::name::dev_automount_volumes,
            xset::name::dev_automount_dirs,
            xset::name::dev_auto_open,
            xset::name::dev_unmount_quit,
        };
    }

    {
        const auto set = xset::set::get(xset::name::dev_automount_optical);
        set->menu.label = "Mount _Optical";
        set->b = xset::set::enabled::yes;
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset::set::get(xset::name::dev_automount_removable);
        set->menu.label = "_Mount Removable";
        set->b = xset::set::enabled::yes;
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset::set::get(xset::name::dev_automount_volumes);
        set->menu.label = "Mount _Volumes...";
        set->title = "Auto-Mount Volumes";
        set->desc =
            "To force or prevent automounting of some volumes, overriding other settings, you can "
            "specify the devices, volume labels, or device IDs in the space-separated list "
            "below.\n\nExample:  +/dev/sdd1 -Label With Space +ata-OCZ-part4\nThis would cause "
            "/dev/sdd1 and the OCZ device to be auto-mounted when detected, and the volume with "
            "label \"Label With Space\" to be ignored.\n\nThere must be a space between entries "
            "and a plus or minus sign directly before each item.  This list is case-sensitive.\n\n";
    }

    {
        const auto set = xset::set::get(xset::name::dev_automount_dirs);
        set->menu.label = "Mount _Dirs...";
        set->title = "Automatic Mount Point Dirs";
        set->menu.type = xset::set::menu_type::string;
        set->desc =
            "Enter the directory where SpaceFM should automatically create mount point directories "
            "for fuse and similar filesystems (%%a in handler commands).  This directory must be "
            "user-writable (do NOT use /media), and empty subdirectories will be removed.  If left "
            "blank, ~/.cache/spacefm/ (or $XDG_CACHE_HOME/spacefm/) is used.  The following "
            "variables are recognized: $USER $UID $HOME $XDG_RUNTIME_DIR $XDG_CACHE_HOME\n\nNote "
            "that some handlers or mount programs may not obey this setting.\n";
    }

    {
        const auto set = xset::set::get(xset::name::dev_auto_open);
        set->menu.label = "Open _Tab";
        set->b = xset::set::enabled::yes;
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset::set::get(xset::name::dev_unmount_quit);
        set->menu.label = "_Unmount On Exit";
        set->b = xset::set::enabled::unset;
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset::set::get(xset::name::dev_ignore_udisks_nopolicy);
        set->menu.label = "Ignore _No Policy";
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset::set::get(xset::name::dev_change);
        set->menu.label = "_Change Detection";
        set->desc =
            "Enter your comma- or space-separated list of filesystems which should NOT be "
            "monitored for file changes.  This setting only affects non-block devices (such as nfs "
            "or fuse), and is usually used to prevent SpaceFM becoming unresponsive with network "
            "filesystems.  Loading of thumbnails and subdirectory sizes will also be disabled.";
        set->menu.type = xset::set::menu_type::string;
        set->title = "Change Detection Blacklist";
        set->icon = "gtk-edit";
        set->s = "cifs curlftpfs ftpfs fuse.sshfs nfs smbfs";
        set->z = set->s;
    }

    // Bookmarks
    {
        const auto set = xset::set::get(xset::name::book_open);
        set->menu.label = "_Open";
        set->icon = "gtk-open";
    }

    {
        const auto set = xset::set::get(xset::name::book_settings);
        set->menu.label = "_Settings";
        set->menu.type = xset::set::menu_type::submenu;
        set->icon = "gtk-properties";
    }

    {
        const auto set = xset::set::get(xset::name::book_add);
        set->menu.label = "New _Bookmark";
        set->icon = "gtk-jump-to";
    }

    {
        const auto set = xset::set::get(xset::name::main_book);
        set->menu.label = "_Bookmarks";
        set->icon = "folder";
        set->menu.type = xset::set::menu_type::submenu;
    }

    // Fonts
    {
        const auto set = xset::set::get(xset::name::font_general);
        set->s = "Monospace 9";
    }

    {
        const auto set = xset::set::get(xset::name::font_view_icon);
        set->s = "Monospace 9";
    }

    {
        const auto set = xset::set::get(xset::name::font_view_compact);
        set->s = "Monospace 9";
    }

    // Rename/Move Dialog
    {
        const auto set = xset::set::get(xset::name::move_name);
        set->menu.label = "_Name";
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset::set::get(xset::name::move_filename);
        set->menu.label = "F_ilename";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset::set::get(xset::name::move_parent);
        set->menu.label = "_Parent";
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset::set::get(xset::name::move_path);
        set->menu.label = "P_ath";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset::set::get(xset::name::move_type);
        set->menu.label = "Typ_e";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset::set::get(xset::name::move_target);
        set->menu.label = "Ta_rget";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset::set::get(xset::name::move_option);
        set->menu.label = "_Option";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::move_copy,
            xset::name::move_link,
            xset::name::move_copyt,
            xset::name::move_linkt,
        };
    }

    {
        const auto set = xset::set::get(xset::name::move_copy);
        set->menu.label = "_Copy";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset::set::get(xset::name::move_link);
        set->menu.label = "_Link";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset::set::get(xset::name::move_copyt);
        set->menu.label = "Copy _Target";
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset::set::get(xset::name::move_linkt);
        set->menu.label = "Lin_k Target";
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset::set::get(xset::name::move_dlg_help);
        set->menu.label = "_Help";
        set->icon = "gtk-help";
    }

    {
        const auto set = xset::set::get(xset::name::move_dlg_confirm_create);
        set->menu.label = "_Confirm Create";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    // status bar
    {
        const auto set = xset::set::get(xset::name::status_middle);
        set->menu.label = "_Middle Click";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::status_name,
            xset::name::status_path,
            xset::name::status_info,
            xset::name::status_hide,
        };
    }

    {
        const auto set = xset::set::get(xset::name::status_name);
        set->menu.label = "Copy _Name";
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset::set::get(xset::name::status_path);
        set->menu.label = "Copy _Path";
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset::set::get(xset::name::status_info);
        set->menu.label = "File _Info";
        set->menu.type = xset::set::menu_type::radio;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset::set::get(xset::name::status_hide);
        set->menu.label = "_Hide Panel";
        set->menu.type = xset::set::menu_type::radio;
    }

    // MAIN WINDOW MENUS

    // File
    {
        const auto set = xset::set::get(xset::name::main_new_window);
        set->menu.label = "New _Window";
        set->icon = "spacefm";
        set->keybinding.type = xset::set::keybinding_type::general;
    }

    {
        const auto set = xset::set::get(xset::name::main_search);
        set->menu.label = "_File Search";
        set->icon = "gtk-find";
        set->keybinding.type = xset::set::keybinding_type::general;
    }

    {
        const auto set = xset::set::get(xset::name::main_terminal);
        set->menu.label = "_Terminal";
        set->b = xset::set::enabled::unset; // discovery notification
        set->keybinding.type = xset::set::keybinding_type::general;
    }

    // was previously used for 'Save Session' < 0.9.4 as xset::set::menu_type::NORMAL
    {
        const auto set = xset::set::get(xset::name::main_save_session);
        set->menu.label = "Open _URL";
        set->menu.type = xset::set::menu_type::string;
        set->icon = "gtk-network";
        set->title = "Open URL";
        set->desc = "Enter URL in the "
                    "format:\n\tPROTOCOL://USERNAME:PASSWORD@HOST:PORT/SHARE\n\nExamples:\n\tftp://"
                    "mirrors.kernel.org\n\tsmb://user:pass@10.0.0.1:50/docs\n\tssh://"
                    "user@sys.domain\n\tmtp://\n\nIncluding a password is unsafe.  To bookmark a "
                    "URL, right-click on the mounted network in Devices and select Bookmark.\n";
    }

    {
        const auto set = xset::set::get(xset::name::main_save_tabs);
        set->menu.label = "Save Ta_bs";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset::set::get(xset::name::main_exit);
        set->menu.label = "E_xit";
        set->icon = "gtk-quit";
        set->keybinding.type = xset::set::keybinding_type::general;
    }

    // View
    {
        const auto set = xset::set::get(xset::name::panel1_show);
        set->menu.label = "Panel _1";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
        set->keybinding.type = xset::set::keybinding_type::view;
    }

    {
        const auto set = xset::set::get(xset::name::panel2_show);
        set->menu.label = "Panel _2";
        set->menu.type = xset::set::menu_type::check;
        set->keybinding.type = xset::set::keybinding_type::view;
    }

    {
        const auto set = xset::set::get(xset::name::panel3_show);
        set->menu.label = "Panel _3";
        set->menu.type = xset::set::menu_type::check;
        set->keybinding.type = xset::set::keybinding_type::view;
    }

    {
        const auto set = xset::set::get(xset::name::panel4_show);
        set->menu.label = "Panel _4";
        set->menu.type = xset::set::menu_type::check;
        set->keybinding.type = xset::set::keybinding_type::view;
    }

    {
        const auto set = xset::set::get(xset::name::main_focus_panel);
        set->menu.label = "F_ocus";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::panel_prev,
            xset::name::panel_next,
            xset::name::panel_hide,
            xset::name::panel_1,
            xset::name::panel_2,
            xset::name::panel_3,
            xset::name::panel_4,
        };
        set->icon = "gtk-go-forward";
    }

    {
        const auto set = xset::set::get(xset::name::panel_prev);
        set->menu.label = "_Prev";
    }

    {
        const auto set = xset::set::get(xset::name::panel_next);
        set->menu.label = "_Next";
    }

    {
        const auto set = xset::set::get(xset::name::panel_hide);
        set->menu.label = "_Hide";
    }

    {
        const auto set = xset::set::get(xset::name::panel_1);
        set->menu.label = "Panel _1";
    }

    {
        const auto set = xset::set::get(xset::name::panel_2);
        set->menu.label = "Panel _2";
    }

    {
        const auto set = xset::set::get(xset::name::panel_3);
        set->menu.label = "Panel _3";
    }

    {
        const auto set = xset::set::get(xset::name::panel_4);
        set->menu.label = "Panel _4";
    }

    {
        const auto set = xset::set::get(xset::name::main_title);
        set->menu.label = "Wi_ndow Title";
        set->menu.type = xset::set::menu_type::string;
        set->title = "Set Window Title Format";
        set->desc =
            "Set window title format:\n\nUse:\n\t%%n\tcurrent directory name (eg "
            "bin)\n\t%%d\tcurrent directory path (eg /usr/bin)\n\t%%p\tcurrent panel number "
            "(1-4)\n\t%%t\tcurrent tab number\n\t%%P\ttotal number of panels visible\n\t%%T\ttotal "
            "number of tabs in current panel\n\t*\tasterisk shown if tasks running in window";
        set->s = "%d";
        set->z = "%d";
    }

    {
        const auto set = xset::set::get(xset::name::main_full);
        set->menu.label = "_Fullscreen";
        set->menu.type = xset::set::menu_type::check;
        set->keybinding.type = xset::set::keybinding_type::general;
    }

    {
        const auto set = xset::set::get(xset::name::main_keybindings);
        set->menu.label = "Keybindings";
        set->icon = "gtk-preferences";
        set->keybinding.type = xset::set::keybinding_type::general;
    }

    {
        const auto set = xset::set::get(xset::name::main_prefs);
        set->menu.label = "_Preferences";
        set->icon = "gtk-preferences";
        set->keybinding.type = xset::set::keybinding_type::general;
    }

    {
        const auto set = xset::set::get(xset::name::root_bar); // in Preferences
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset::set::get(xset::name::view_thumb);
        set->menu.label = "_Thumbnails (global)"; // in View|Panel View|Style
        set->menu.type = xset::set::menu_type::check;
    }

    // Help
    {
        const auto set = xset::set::get(xset::name::main_about);
        set->menu.label = "_About";
        set->icon = "gtk-about";
    }

    {
        const auto set = xset::set::get(xset::name::main_dev);
        set->menu.label = "_Show Devices";
        set->shared_key = xset::set::get(xset::name::panel1_show_devmon);
        set->menu.type = xset::set::menu_type::check;
    }

    // Tasks
    {
        const auto set = xset::set::get(xset::name::main_tasks);
        set->menu.label = "_Task Manager";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::task_show_manager,
            xset::name::task_hide_manager,
            xset::name::separator,
            xset::name::task_columns,
            xset::name::task_popups,
            xset::name::task_errors,
            xset::name::task_queue,
        };
    }

    {
        const auto set = xset::set::get(xset::name::task_col_status);
        set->menu.label = "_Status";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
        set->x = "0";   // column position
        set->y = "130"; // column width
    }

    {
        const auto set = xset::set::get(xset::name::task_col_count);
        set->menu.label = "_Count";
        set->menu.type = xset::set::menu_type::check;
        set->x = "1";
    }

    {
        const auto set = xset::set::get(xset::name::task_col_path);
        set->menu.label = "_Directory";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
        set->x = "2";
    }

    {
        const auto set = xset::set::get(xset::name::task_col_file);
        set->menu.label = "_Item";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
        set->x = "3";
    }

    {
        const auto set = xset::set::get(xset::name::task_col_to);
        set->menu.label = "_To";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
        set->x = "4";
    }

    {
        const auto set = xset::set::get(xset::name::task_col_progress);
        set->menu.label = "_Progress";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
        set->x = "5";
        set->y = "100";
    }

    {
        const auto set = xset::set::get(xset::name::task_col_total);
        set->menu.label = "T_otal";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
        set->x = "6";
        set->y = "120";
    }

    {
        const auto set = xset::set::get(xset::name::task_col_started);
        set->menu.label = "Sta_rted";
        set->menu.type = xset::set::menu_type::check;
        set->x = "7";
    }

    {
        const auto set = xset::set::get(xset::name::task_col_elapsed);
        set->menu.label = "_Elapsed";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
        set->x = "8";
        set->y = "70";
    }

    {
        const auto set = xset::set::get(xset::name::task_col_curspeed);
        set->menu.label = "C_urrent Speed";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
        set->x = "9";
    }

    {
        const auto set = xset::set::get(xset::name::task_col_curest);
        set->menu.label = "Current Re_main";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
        set->x = "10";
    }

    {
        const auto set = xset::set::get(xset::name::task_col_avgspeed);
        set->menu.label = "_Average Speed";
        set->menu.type = xset::set::menu_type::check;
        set->x = "11";
        set->y = "60";
    }

    {
        const auto set = xset::set::get(xset::name::task_col_avgest);
        set->menu.label = "A_verage Remain";
        set->menu.type = xset::set::menu_type::check;
        set->x = "12";
        set->y = "65";
    }

    {
        const auto set = xset::set::get(xset::name::task_col_reorder);
        set->menu.label = "Reor_der";
    }

    {
        const auto set = xset::set::get(xset::name::task_stop);
        set->menu.label = "_Stop";
        set->icon = "gtk-stop";
    }

    {
        const auto set = xset::set::get(xset::name::task_pause);
        set->menu.label = "Pa_use";
        set->icon = "gtk-media-pause";
    }

    {
        const auto set = xset::set::get(xset::name::task_que);
        set->menu.label = "_Queue";
        set->icon = "gtk-add";
    }

    {
        const auto set = xset::set::get(xset::name::task_resume);
        set->menu.label = "_Resume";
        set->icon = "gtk-media-play";
    }

    {
        const auto set = xset::set::get(xset::name::task_showout);
        set->menu.label = "Sho_w Output";
    }

    {
        const auto set = xset::set::get(xset::name::task_all);
        set->menu.label = "_All Tasks";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::task_stop_all,
            xset::name::task_pause_all,
            xset::name::task_que_all,
            xset::name::task_resume_all,
        };
    }

    {
        const auto set = xset::set::get(xset::name::task_stop_all);
        set->menu.label = "_Stop";
        set->icon = "gtk-stop";
    }

    {
        const auto set = xset::set::get(xset::name::task_pause_all);
        set->menu.label = "Pa_use";
        set->icon = "gtk-media-pause";
    }

    {
        const auto set = xset::set::get(xset::name::task_que_all);
        set->menu.label = "_Queue";
        set->icon = "gtk-add";
    }

    {
        const auto set = xset::set::get(xset::name::task_resume_all);
        set->menu.label = "_Resume";
        set->icon = "gtk-media-play";
    }

    {
        const auto set = xset::set::get(xset::name::task_show_manager);
        set->menu.label = "Show _Manager";
        set->menu.type = xset::set::menu_type::radio;
        set->b = xset::set::enabled::no;
    }

    {
        const auto set = xset::set::get(xset::name::task_hide_manager);
        set->menu.label = "Auto-_Hide Manager";
        set->menu.type = xset::set::menu_type::radio;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset::set::get(xset::name::task_columns);
        set->menu.label = "_Columns";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::task_col_count,
            xset::name::task_col_path,
            xset::name::task_col_file,
            xset::name::task_col_to,
            xset::name::task_col_progress,
            xset::name::task_col_total,
            xset::name::task_col_started,
            xset::name::task_col_elapsed,
            xset::name::task_col_curspeed,
            xset::name::task_col_curest,
            xset::name::task_col_avgspeed,
            xset::name::task_col_avgest,
            xset::name::separator,
            xset::name::task_col_reorder,
        };
    }

    {
        const auto set = xset::set::get(xset::name::task_popups);
        set->menu.label = "_Popups";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::task_pop_all,
            xset::name::task_pop_top,
            xset::name::task_pop_above,
            xset::name::task_pop_stick,
            xset::name::separator,
            xset::name::task_pop_detail,
            xset::name::task_pop_over,
            xset::name::task_pop_err,
        };
    }

    {
        const auto set = xset::set::get(xset::name::task_pop_all);
        set->menu.label = "Popup _All Tasks";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::no;
    }

    {
        const auto set = xset::set::get(xset::name::task_pop_top);
        set->menu.label = "Stay On _Top";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::no;
    }

    {
        const auto set = xset::set::get(xset::name::task_pop_above);
        set->menu.label = "A_bove Others";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::no;
    }

    {
        const auto set = xset::set::get(xset::name::task_pop_stick);
        set->menu.label = "All _Workspaces";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::no;
    }

    {
        const auto set = xset::set::get(xset::name::task_pop_detail);
        set->menu.label = "_Detailed Stats";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::no;
    }

    {
        const auto set = xset::set::get(xset::name::task_pop_over);
        set->menu.label = "_Overwrite Option";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset::set::get(xset::name::task_pop_err);
        set->menu.label = "_Error Option";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset::set::get(xset::name::task_errors);
        set->menu.label = "Err_ors";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::task_err_first,
            xset::name::task_err_any,
            xset::name::task_err_cont,
        };
    }

    {
        const auto set = xset::set::get(xset::name::task_err_first);
        set->menu.label = "Stop If _First";
        set->menu.type = xset::set::menu_type::radio;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset::set::get(xset::name::task_err_any);
        set->menu.label = "Stop On _Any";
        set->menu.type = xset::set::menu_type::radio;
        set->b = xset::set::enabled::no;
    }

    {
        const auto set = xset::set::get(xset::name::task_err_cont);
        set->menu.label = "_Continue";
        set->menu.type = xset::set::menu_type::radio;
        set->b = xset::set::enabled::no;
    }

    {
        const auto set = xset::set::get(xset::name::task_queue);
        set->menu.label = "Qu_eue";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::task_q_new,
            xset::name::task_q_smart,
            xset::name::task_q_pause,
        };
    }

    {
        const auto set = xset::set::get(xset::name::task_q_new);
        set->menu.label = "_Queue New Tasks";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset::set::get(xset::name::task_q_smart);
        set->menu.label = "_Smart Queue";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset::set::get(xset::name::task_q_pause);
        set->menu.label = "_Pause On Error";
        set->menu.type = xset::set::menu_type::check;
    }

    // PANELS COMMON

    {
        const auto set = xset::set::get(xset::name::con_open);
        set->menu.label = "_Open";
        set->menu.type = xset::set::menu_type::submenu;
        set->icon = "gtk-open";
    }

    {
        const auto set = xset::set::get(xset::name::open_execute);
        set->menu.label = "E_xecute";
        set->icon = "gtk-execute";
    }

    {
        const auto set = xset::set::get(xset::name::open_edit);
        set->menu.label = "Edi_t";
        set->icon = "gtk-edit";
    }

    {
        const auto set = xset::set::get(xset::name::open_other);
        set->menu.label = "_Choose...";
        set->icon = "gtk-open";
    }

    {
        const auto set = xset::set::get(xset::name::open_all);
        set->menu.label = "Open With _Default"; // virtual
        set->keybinding.type = xset::set::keybinding_type::opening;
    }

    {
        const auto set = xset::set::get(xset::name::open_in_tab);
        set->menu.label = "In _Tab";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::opentab_new,
            xset::name::opentab_prev,
            xset::name::opentab_next,
            xset::name::opentab_1,
            xset::name::opentab_2,
            xset::name::opentab_3,
            xset::name::opentab_4,
            xset::name::opentab_5,
            xset::name::opentab_6,
            xset::name::opentab_7,
            xset::name::opentab_8,
            xset::name::opentab_9,
            xset::name::opentab_10,
        };
    }

    {
        const auto set = xset::set::get(xset::name::opentab_new);
        set->menu.label = "N_ew";
    }

    {
        const auto set = xset::set::get(xset::name::opentab_prev);
        set->menu.label = "_Prev";
    }

    {
        const auto set = xset::set::get(xset::name::opentab_next);
        set->menu.label = "_Next";
    }

    {
        const auto set = xset::set::get(xset::name::opentab_1);
        set->menu.label = "Tab _1";
    }

    {
        const auto set = xset::set::get(xset::name::opentab_2);
        set->menu.label = "Tab _2";
    }

    {
        const auto set = xset::set::get(xset::name::opentab_3);
        set->menu.label = "Tab _3";
    }

    {
        const auto set = xset::set::get(xset::name::opentab_4);
        set->menu.label = "Tab _4";
    }

    {
        const auto set = xset::set::get(xset::name::opentab_5);
        set->menu.label = "Tab _5";
    }

    {
        const auto set = xset::set::get(xset::name::opentab_6);
        set->menu.label = "Tab _6";
    }

    {
        const auto set = xset::set::get(xset::name::opentab_7);
        set->menu.label = "Tab _7";
    }

    {
        const auto set = xset::set::get(xset::name::opentab_8);
        set->menu.label = "Tab _8";
    }

    {
        const auto set = xset::set::get(xset::name::opentab_9);
        set->menu.label = "Tab _9";
    }

    {
        const auto set = xset::set::get(xset::name::opentab_10);
        set->menu.label = "Tab 1_0";
    }

    {
        const auto set = xset::set::get(xset::name::open_in_panel);
        set->menu.label = "In _Panel";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::open_in_panel_prev,
            xset::name::open_in_panel_next,
            xset::name::open_in_panel_1,
            xset::name::open_in_panel_2,
            xset::name::open_in_panel_3,
            xset::name::open_in_panel_4,
        };
    }

    {
        const auto set = xset::set::get(xset::name::open_in_panel_prev);
        set->menu.label = "_Prev";
    }

    {
        const auto set = xset::set::get(xset::name::open_in_panel_next);
        set->menu.label = "_Next";
    }

    {
        const auto set = xset::set::get(xset::name::open_in_panel_1);
        set->menu.label = "Panel _1";
    }

    {
        const auto set = xset::set::get(xset::name::open_in_panel_2);
        set->menu.label = "Panel _2";
    }

    {
        const auto set = xset::set::get(xset::name::open_in_panel_3);
        set->menu.label = "Panel _3";
    }

    {
        const auto set = xset::set::get(xset::name::open_in_panel_4);
        set->menu.label = "Panel _4";
    }

    {
        const auto set = xset::set::get(xset::name::archive_extract);
        set->menu.label = "Archive Extract";
        set->icon = "gtk-convert";
    }

    {
        const auto set = xset::set::get(xset::name::archive_extract_to);
        set->menu.label = "Archive Extract To";
        set->icon = "gtk-convert";
    }

    {
        const auto set = xset::set::get(xset::name::archive_open);
        set->menu.label = "Archive Open";
        set->icon = "gtk-file";
    }

    {
        const auto set = xset::set::get(xset::name::archive_default);
        set->menu.label = "_Archive Defaults";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::archive_default_open_with_app,
            xset::name::archive_default_extract,
            xset::name::archive_default_extract_to,
            xset::name::archive_default_open_with_archiver,
        };
    }

    {
        const auto set = xset::set::get(xset::name::archive_default_open_with_app);
        set->menu.label = "Open With App";
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset::set::get(xset::name::archive_default_extract);
        set->menu.label = "Extract";
        set->menu.type = xset::set::menu_type::radio;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset::set::get(xset::name::archive_default_extract_to);
        set->menu.label = "Extract To";
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset::set::get(xset::name::archive_default_open_with_archiver);
        set->menu.label = "Open With Archiver";
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset::set::get(xset::name::open_new);
        set->menu.label = "_New";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::new_file,
            xset::name::new_directory,
            xset::name::new_link,
            xset::name::new_archive,
            xset::name::separator,
            xset::name::tab_new,
            xset::name::tab_new_here,
            xset::name::new_bookmark,
        };
        set->icon = "gtk-new";
    }

    {
        const auto set = xset::set::get(xset::name::new_file);
        set->menu.label = "_File";
        set->icon = "gtk-file";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::new_directory);
        set->menu.label = "Dir_ectory";
        set->icon = "folder";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::new_link);
        set->menu.label = "_Link";
        set->icon = "gtk-file";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::new_bookmark);
        set->menu.label = "_Bookmark";
        set->shared_key = xset::set::get(xset::name::book_add);
        set->icon = "gtk-jump-to";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::new_archive);
        set->menu.label = "_Archive";
        set->icon = "gtk-save-as";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::arc_dlg);
        set->b = xset::set::enabled::yes; // Extract To - Create Subdirectory
        set->z = "1";                     // Extract To - Write Access
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::tab_new);
        set->menu.label = "_Tab";
        set->icon = "gtk-add";
        set->keybinding.type = xset::set::keybinding_type::navigation;
    }

    {
        const auto set = xset::set::get(xset::name::tab_new_here);
        set->menu.label = "Tab _Here";
        set->icon = "gtk-add";
        set->keybinding.type = xset::set::keybinding_type::navigation;
    }

    {
        const auto set = xset::set::get(xset::name::new_app);
        set->menu.label = "_Desktop Application";
        set->icon = "gtk-add";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::con_go);
        set->menu.label = "_Go";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::go_back,
            xset::name::go_forward,
            xset::name::go_up,
            xset::name::go_home,
            xset::name::edit_canon,
            xset::name::separator,
            xset::name::go_tab,
            xset::name::go_focus,
        };
        set->icon = "gtk-go-forward";
    }

    {
        const auto set = xset::set::get(xset::name::go_back);
        set->menu.label = "_Back";
        set->icon = "gtk-go-back";
        set->keybinding.type = xset::set::keybinding_type::navigation;
    }

    {
        const auto set = xset::set::get(xset::name::go_forward);
        set->menu.label = "_Forward";
        set->icon = "gtk-go-forward";
        set->keybinding.type = xset::set::keybinding_type::navigation;
    }

    {
        const auto set = xset::set::get(xset::name::go_up);
        set->menu.label = "_Up";
        set->icon = "gtk-go-up";
        set->keybinding.type = xset::set::keybinding_type::navigation;
    }

    {
        const auto set = xset::set::get(xset::name::go_home);
        set->menu.label = "_Home";
        set->icon = "gtk-home";
        set->keybinding.type = xset::set::keybinding_type::navigation;
    }

    {
        const auto set = xset::set::get(xset::name::edit_canon);
        set->menu.label = "Re_al Path";
    }

    {
        const auto set = xset::set::get(xset::name::go_focus);
        set->menu.label = "Fo_cus";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::focus_path_bar,
            xset::name::focus_search_bar,
            xset::name::focus_filelist,
            xset::name::focus_dirtree,
            xset::name::focus_device,
        };
    }

    {
        const auto set = xset::set::get(xset::name::focus_path_bar);
        set->menu.label = "_Path Bar";
        set->icon = "gtk-dialog-question";
        set->keybinding.type = xset::set::keybinding_type::navigation;
    }

    {
        const auto set = xset::set::get(xset::name::focus_search_bar);
        set->menu.label = "_Search Bar";
        set->icon = "gtk-dialog-question";
        set->keybinding.type = xset::set::keybinding_type::navigation;
    }

    {
        const auto set = xset::set::get(xset::name::focus_filelist);
        set->menu.label = "_File List";
        set->icon = "gtk-file";
    }

    {
        const auto set = xset::set::get(xset::name::focus_dirtree);
        set->menu.label = "_Tree";
        set->icon = "folder";
    }

    {
        const auto set = xset::set::get(xset::name::focus_device);
        set->menu.label = "De_vices";
        set->icon = "gtk-harddisk";
    }

    {
        const auto set = xset::set::get(xset::name::go_tab);
        set->menu.label = "_Tab";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::tab_prev,
            xset::name::tab_next,
            xset::name::tab_restore,
            xset::name::tab_close,
            xset::name::tab_1,
            xset::name::tab_2,
            xset::name::tab_3,
            xset::name::tab_4,
            xset::name::tab_5,
            xset::name::tab_6,
            xset::name::tab_7,
            xset::name::tab_8,
            xset::name::tab_9,
            xset::name::tab_10,
        };
    }

    {
        const auto set = xset::set::get(xset::name::tab_prev);
        set->menu.label = "_Prev";
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset::set::get(xset::name::tab_next);
        set->menu.label = "_Next";
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset::set::get(xset::name::tab_restore);
        set->menu.label = "_Restore";
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset::set::get(xset::name::tab_close);
        set->menu.label = "_Close";
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset::set::get(xset::name::tab_1);
        set->menu.label = "Tab _1";
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset::set::get(xset::name::tab_2);
        set->menu.label = "Tab _2";
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset::set::get(xset::name::tab_3);
        set->menu.label = "Tab _3";
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset::set::get(xset::name::tab_4);
        set->menu.label = "Tab _4";
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset::set::get(xset::name::tab_5);
        set->menu.label = "Tab _5";
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset::set::get(xset::name::tab_6);
        set->menu.label = "Tab _6";
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset::set::get(xset::name::tab_7);
        set->menu.label = "Tab _7";
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset::set::get(xset::name::tab_8);
        set->menu.label = "Tab _8";
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset::set::get(xset::name::tab_9);
        set->menu.label = "Tab _9";
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset::set::get(xset::name::tab_10);
        set->menu.label = "Tab 1_0";
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset::set::get(xset::name::con_view);
        set->menu.label = "_View";
        set->menu.type = xset::set::menu_type::submenu;
        set->icon = "gtk-preferences";
    }

    {
        const auto set = xset::set::get(xset::name::view_list_style);
        set->menu.label = "Styl_e";
        set->menu.type = xset::set::menu_type::submenu;
    }

    {
        const auto set = xset::set::get(xset::name::view_columns);
        set->menu.label = "C_olumns";
        set->menu.type = xset::set::menu_type::submenu;
    }

    {
        const auto set = xset::set::get(xset::name::view_reorder_col);
        set->menu.label = "_Reorder";
    }

    {
        const auto set = xset::set::get(xset::name::rubberband);
        set->menu.label = "_Rubberband Select";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset::set::get(xset::name::view_sortby);
        set->menu.label = "_Sort";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::sortby_name,   xset::name::sortby_size,       xset::name::sortby_bytes,
            xset::name::sortby_type,   xset::name::sortby_mime,       xset::name::sortby_perm,
            xset::name::sortby_owner,  xset::name::sortby_group,      xset::name::sortby_atime,
            xset::name::sortby_btime,  xset::name::sortby_ctime,      xset::name::sortby_mtime,
            xset::name::separator,     xset::name::sortby_ascend,     xset::name::sortby_descend,
            xset::name::separator,     xset::name::sortx_natural,     xset::name::sortx_case,
            xset::name::separator,     xset::name::sortx_directories, xset::name::sortx_files,
            xset::name::sortx_mix,     xset::name::separator,         xset::name::sortx_hidfirst,
            xset::name::sortx_hidlast,
        };
    }

    {
        const auto set = xset::set::get(xset::name::sortby_name);
        set->menu.label = "_Name";
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset::set::get(xset::name::sortby_size);
        set->menu.label = "_Size";
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset::set::get(xset::name::sortby_bytes);
        set->menu.label = "_Size in Bytes";
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset::set::get(xset::name::sortby_type);
        set->menu.label = "_Type";
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset::set::get(xset::name::sortby_mime);
        set->menu.label = "_MIME Type";
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset::set::get(xset::name::sortby_perm);
        set->menu.label = "_Permissions";
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset::set::get(xset::name::sortby_owner);
        set->menu.label = "_Owner";
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset::set::get(xset::name::sortby_group);
        set->menu.label = "_Group";
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset::set::get(xset::name::sortby_atime);
        set->menu.label = "_Date Accessed";
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset::set::get(xset::name::sortby_btime);
        set->menu.label = "_Date Created";
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset::set::get(xset::name::sortby_ctime);
        set->menu.label = "_Date Metadata Changed";
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset::set::get(xset::name::sortby_mtime);
        set->menu.label = "_Date Modified";
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset::set::get(xset::name::sortby_ascend);
        set->menu.label = "_Ascending";
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset::set::get(xset::name::sortby_descend);
        set->menu.label = "_Descending";
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset::set::get(xset::name::sortx_natural);
        set->menu.label = "_Natural";
        set->menu.type = xset::set::menu_type::check;
    }

#if 0
    {
        const auto set = xset::set::get(xset::name::sortx_natural);
        set->menu.label =  "Nat_ural";
        set->menu.type = xset::set::menu_type::check;
    }
#endif

    {
        const auto set = xset::set::get(xset::name::sortx_case);
        set->menu.label = "_Case Sensitive";
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset::set::get(xset::name::sortx_directories);
        set->menu.label = "Directories Fi_rst";
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset::set::get(xset::name::sortx_files);
        set->menu.label = "F_iles First";
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset::set::get(xset::name::sortx_mix);
        set->menu.label = "Mi_xed";
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset::set::get(xset::name::sortx_hidfirst);
        set->menu.label = "_Hidden First";
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset::set::get(xset::name::sortx_hidlast);
        set->menu.label = "Hidden _Last";
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset::set::get(xset::name::view_refresh);
        set->menu.label = "Re_fresh";
        set->icon = "gtk-refresh";
        set->keybinding.type = xset::set::keybinding_type::view;
    }

    {
        const auto set = xset::set::get(xset::name::path_seek);
        set->menu.label = "Auto See_k";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset::set::get(xset::name::search_select);
        set->menu.label = "Select Matches";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    // EDIT
    {
        const auto set = xset::set::get(xset::name::edit_cut);
        set->menu.label = "Cu_t";
        set->icon = "gtk-cut";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::edit_copy);
        set->menu.label = "_Copy";
        set->icon = "gtk-copy";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::edit_paste);
        set->menu.label = "_Paste";
        set->icon = "gtk-paste";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::edit_rename);
        set->menu.label = "_Rename";
        set->icon = "gtk-edit";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::edit_delete);
        set->menu.label = "_Delete";
        set->icon = "gtk-delete";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::edit_trash);
        set->menu.label = "_Trash";
        set->icon = "gtk-delete";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::edit_submenu);
        set->menu.label = "_Actions";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::copy_name,
            xset::name::copy_parent,
            xset::name::copy_path,
            xset::name::separator,
            xset::name::paste_link,
            xset::name::paste_target,
            xset::name::paste_as,
            xset::name::separator,
            xset::name::copy_to,
            xset::name::move_to,
            xset::name::edit_hide,
            xset::name::separator,
            xset::name::select_all,
            xset::name::select_patt,
            xset::name::select_invert,
            xset::name::select_un,
        };
        set->icon = "gtk-edit";
    }

    {
        const auto set = xset::set::get(xset::name::copy_name);
        set->menu.label = "Copy _Name";
        set->icon = "gtk-copy";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::copy_path);
        set->menu.label = "Copy _Path";
        set->icon = "gtk-copy";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::copy_parent);
        set->menu.label = "Copy Pa_rent";
        set->icon = "gtk-copy";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::paste_link);
        set->menu.label = "Paste _Link";
        set->icon = "gtk-paste";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::paste_target);
        set->menu.label = "Paste _Target";
        set->icon = "gtk-paste";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::paste_as);
        set->menu.label = "Paste _As";
        set->icon = "gtk-paste";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::copy_to);
        set->menu.label = "_Copy To";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::copy_loc,
            xset::name::copy_loc_last,
            xset::name::separator,
            xset::name::copy_tab,
            xset::name::copy_panel,
        };
    }

    {
        const auto set = xset::set::get(xset::name::copy_loc);
        set->menu.label = "L_ocation";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::copy_loc_last);
        set->menu.label = "L_ast Location";
        set->icon = "gtk-redo";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::copy_tab);
        set->menu.label = "_Tab";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::copy_tab_prev,
            xset::name::copy_tab_next,
            xset::name::copy_tab_1,
            xset::name::copy_tab_2,
            xset::name::copy_tab_3,
            xset::name::copy_tab_4,
            xset::name::copy_tab_5,
            xset::name::copy_tab_6,
            xset::name::copy_tab_7,
            xset::name::copy_tab_8,
            xset::name::copy_tab_9,
            xset::name::copy_tab_10,
        };
    }

    {
        const auto set = xset::set::get(xset::name::copy_tab_prev);
        set->menu.label = "_Prev";
    }

    {
        const auto set = xset::set::get(xset::name::copy_tab_next);
        set->menu.label = "_Next";
    }

    {
        const auto set = xset::set::get(xset::name::copy_tab_1);
        set->menu.label = "Tab _1";
    }

    {
        const auto set = xset::set::get(xset::name::copy_tab_2);
        set->menu.label = "Tab _2";
    }

    {
        const auto set = xset::set::get(xset::name::copy_tab_3);
        set->menu.label = "Tab _3";
    }

    {
        const auto set = xset::set::get(xset::name::copy_tab_4);
        set->menu.label = "Tab _4";
    }

    {
        const auto set = xset::set::get(xset::name::copy_tab_5);
        set->menu.label = "Tab _5";
    }

    {
        const auto set = xset::set::get(xset::name::copy_tab_6);
        set->menu.label = "Tab _6";
    }

    {
        const auto set = xset::set::get(xset::name::copy_tab_7);
        set->menu.label = "Tab _7";
    }

    {
        const auto set = xset::set::get(xset::name::copy_tab_8);
        set->menu.label = "Tab _8";
    }

    {
        const auto set = xset::set::get(xset::name::copy_tab_9);
        set->menu.label = "Tab _9";
    }

    {
        const auto set = xset::set::get(xset::name::copy_tab_10);
        set->menu.label = "Tab 1_0";
    }

    {
        const auto set = xset::set::get(xset::name::copy_panel);
        set->menu.label = "_Panel";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::copy_panel_prev,
            xset::name::copy_panel_next,
            xset::name::copy_panel_1,
            xset::name::copy_panel_2,
            xset::name::copy_panel_3,
            xset::name::copy_panel_4,
        };
    }

    {
        const auto set = xset::set::get(xset::name::copy_panel_prev);
        set->menu.label = "_Prev";
    }

    {
        const auto set = xset::set::get(xset::name::copy_panel_next);
        set->menu.label = "_Next";
    }

    {
        const auto set = xset::set::get(xset::name::copy_panel_1);
        set->menu.label = "Panel _1";
    }

    {
        const auto set = xset::set::get(xset::name::copy_panel_2);
        set->menu.label = "Panel _2";
    }

    {
        const auto set = xset::set::get(xset::name::copy_panel_3);
        set->menu.label = "Panel _3";
    }

    {
        const auto set = xset::set::get(xset::name::copy_panel_4);
        set->menu.label = "Panel _4";
    }

    {
        const auto set = xset::set::get(xset::name::move_to);
        set->menu.label = "_Move To";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::move_loc,
            xset::name::move_loc_last,
            xset::name::separator,
            xset::name::move_tab,
            xset::name::move_panel,
        };
    }

    {
        const auto set = xset::set::get(xset::name::move_loc);
        set->menu.label = "_Location";
    }

    {
        const auto set = xset::set::get(xset::name::move_loc_last);
        set->menu.label = "L_ast Location";
        set->icon = "gtk-redo";
    }

    {
        const auto set = xset::set::get(xset::name::move_tab);
        set->menu.label = "_Tab";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::move_tab_prev,
            xset::name::move_tab_next,
            xset::name::move_tab_1,
            xset::name::move_tab_2,
            xset::name::move_tab_3,
            xset::name::move_tab_4,
            xset::name::move_tab_5,
            xset::name::move_tab_6,
            xset::name::move_tab_7,
            xset::name::move_tab_8,
            xset::name::move_tab_9,
            xset::name::move_tab_10,
        };
    }

    {
        const auto set = xset::set::get(xset::name::move_tab_prev);
        set->menu.label = "_Prev";
    }

    {
        const auto set = xset::set::get(xset::name::move_tab_next);
        set->menu.label = "_Next";
    }

    {
        const auto set = xset::set::get(xset::name::move_tab_1);
        set->menu.label = "Tab _1";
    }

    {
        const auto set = xset::set::get(xset::name::move_tab_2);
        set->menu.label = "Tab _2";
    }

    {
        const auto set = xset::set::get(xset::name::move_tab_3);
        set->menu.label = "Tab _3";
    }

    {
        const auto set = xset::set::get(xset::name::move_tab_4);
        set->menu.label = "Tab _4";
    }

    {
        const auto set = xset::set::get(xset::name::move_tab_5);
        set->menu.label = "Tab _5";
    }

    {
        const auto set = xset::set::get(xset::name::move_tab_6);
        set->menu.label = "Tab _6";
    }

    {
        const auto set = xset::set::get(xset::name::move_tab_7);
        set->menu.label = "Tab _7";
    }

    {
        const auto set = xset::set::get(xset::name::move_tab_8);
        set->menu.label = "Tab _8";
    }

    {
        const auto set = xset::set::get(xset::name::move_tab_9);
        set->menu.label = "Tab _9";
    }

    {
        const auto set = xset::set::get(xset::name::move_tab_10);
        set->menu.label = "Tab 1_0";
    }

    {
        const auto set = xset::set::get(xset::name::move_panel);
        set->menu.label = "_Panel";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::move_panel_prev,
            xset::name::move_panel_next,
            xset::name::move_panel_1,
            xset::name::move_panel_2,
            xset::name::move_panel_3,
            xset::name::move_panel_4,
        };
    }

    {
        const auto set = xset::set::get(xset::name::move_panel_prev);
        set->menu.label = "_Prev";
    }

    {
        const auto set = xset::set::get(xset::name::move_panel_next);
        set->menu.label = "_Next";
    }

    {
        const auto set = xset::set::get(xset::name::move_panel_1);
        set->menu.label = "Panel _1";
    }

    {
        const auto set = xset::set::get(xset::name::move_panel_2);
        set->menu.label = "Panel _2";
    }

    {
        const auto set = xset::set::get(xset::name::move_panel_3);
        set->menu.label = "Panel _3";
    }

    {
        const auto set = xset::set::get(xset::name::move_panel_4);
        set->menu.label = "Panel _4";
    }

    {
        const auto set = xset::set::get(xset::name::edit_hide);
        set->menu.label = "_Hide";
    }

    {
        const auto set = xset::set::get(xset::name::select_all);
        set->menu.label = "_Select All";
        set->icon = "gtk-select-all";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::select_un);
        set->menu.label = "_Unselect All";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::select_invert);
        set->menu.label = "_Invert Selection";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::select_patt);
        set->menu.label = "S_elect By Pattern";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    // Properties
    {
        const auto set = xset::set::get(xset::name::con_prop);
        set->menu.label = "Propert_ies";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::prop_info,
            xset::name::prop_attr,
            xset::name::prop_perm,
            xset::name::prop_quick,
        };
        set->icon = "gtk-properties";
    }

    {
        const auto set = xset::set::get(xset::name::prop_info);
        set->menu.label = "_Info";
        set->icon = "gtk-dialog-info";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::prop_attr);
        set->menu.label = "_Attributes";
        set->icon = "gtk-dialog-info";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::prop_perm);
        set->menu.label = "_Permissions";
        set->icon = "dialog-password";
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset::set::get(xset::name::prop_quick);
        set->menu.label = "_Quick";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::perm_r,
            xset::name::perm_rw,
            xset::name::perm_rwx,
            xset::name::perm_r_r,
            xset::name::perm_rw_r,
            xset::name::perm_rw_rw,
            xset::name::perm_rwxr_x,
            xset::name::perm_rwxrwx,
            xset::name::perm_r_r_r,
            xset::name::perm_rw_r_r,
            xset::name::perm_rw_rw_rw,
            xset::name::perm_rwxr_r,
            xset::name::perm_rwxr_xr_x,
            xset::name::perm_rwxrwxrwx,
            xset::name::perm_rwxrwxrwt,
            xset::name::perm_unstick,
            xset::name::perm_stick,
            xset::name::perm_recurs,
        };
    }

    {
        const auto set = xset::set::get(xset::name::perm_r);
        set->menu.label = "r--------";
    }

    {
        const auto set = xset::set::get(xset::name::perm_rw);
        set->menu.label = "rw-------";
    }

    {
        const auto set = xset::set::get(xset::name::perm_rwx);
        set->menu.label = "rwx------";
    }

    {
        const auto set = xset::set::get(xset::name::perm_r_r);
        set->menu.label = "r--r-----";
    }

    {
        const auto set = xset::set::get(xset::name::perm_rw_r);
        set->menu.label = "rw-r-----";
    }

    {
        const auto set = xset::set::get(xset::name::perm_rw_rw);
        set->menu.label = "rw-rw----";
    }

    {
        const auto set = xset::set::get(xset::name::perm_rwxr_x);
        set->menu.label = "rwxr-x---";
    }

    {
        const auto set = xset::set::get(xset::name::perm_rwxrwx);
        set->menu.label = "rwxrwx---";
    }

    {
        const auto set = xset::set::get(xset::name::perm_r_r_r);
        set->menu.label = "r--r--r--";
    }

    {
        const auto set = xset::set::get(xset::name::perm_rw_r_r);
        set->menu.label = "rw-r--r--";
    }

    {
        const auto set = xset::set::get(xset::name::perm_rw_rw_rw);
        set->menu.label = "rw-rw-rw-";
    }

    {
        const auto set = xset::set::get(xset::name::perm_rwxr_r);
        set->menu.label = "rwxr--r--";
    }

    {
        const auto set = xset::set::get(xset::name::perm_rwxr_xr_x);
        set->menu.label = "rwxr-xr-x";
    }

    {
        const auto set = xset::set::get(xset::name::perm_rwxrwxrwx);
        set->menu.label = "rwxrwxrwx";
    }

    {
        const auto set = xset::set::get(xset::name::perm_rwxrwxrwt);
        set->menu.label = "rwxrwxrwt";
    }

    {
        const auto set = xset::set::get(xset::name::perm_unstick);
        set->menu.label = "-t";
    }

    {
        const auto set = xset::set::get(xset::name::perm_stick);
        set->menu.label = "+t";
    }

    {
        const auto set = xset::set::get(xset::name::perm_recurs);
        set->menu.label = "_Recursive";
        set->menu.type = xset::set::menu_type::submenu;
        set->context_menu_entries = {
            xset::name::perm_go_w,
            xset::name::perm_go_rwx,
            xset::name::perm_ugo_w,
            xset::name::perm_ugo_rx,
            xset::name::perm_ugo_rwx,
        };
    }

    {
        const auto set = xset::set::get(xset::name::perm_go_w);
        set->menu.label = "go-w";
    }

    {
        const auto set = xset::set::get(xset::name::perm_go_rwx);
        set->menu.label = "go-rwx";
    }

    {
        const auto set = xset::set::get(xset::name::perm_ugo_w);
        set->menu.label = "ugo+w";
    }

    {
        const auto set = xset::set::get(xset::name::perm_ugo_rx);
        set->menu.label = "ugo+rX";
    }

    {
        const auto set = xset::set::get(xset::name::perm_ugo_rwx);
        set->menu.label = "ugo+rwX";
    }

    // PANELS
    for (const panel_t p : PANELS)
    {
        {
            const auto set = xset::set::get(xset::panel::show_toolbox, p);
            set->menu.label = "_Toolbar";
            set->menu.type = xset::set::menu_type::check;
            set->b = xset::set::enabled::yes;
            if (p != 1)
            {
                set->shared_key = xset::set::get(xset::name::panel1_show_toolbox);
            }
        }

        {
            const auto set = xset::set::get(xset::panel::show_devmon, p);
            set->menu.label = "_Devices";
            set->menu.type = xset::set::menu_type::check;
            set->b = xset::set::enabled::unset;
            if (p != 1)
            {
                set->shared_key = xset::set::get(xset::name::panel1_show_devmon);
            }
        }

        {
            const auto set = xset::set::get(xset::panel::show_dirtree, p);
            set->menu.label = "T_ree";
            set->menu.type = xset::set::menu_type::check;
            set->b = xset::set::enabled::yes;
            if (p != 1)
            {
                set->shared_key = xset::set::get(xset::name::panel1_show_dirtree);
            }
        }

        {
            const auto set = xset::set::get(xset::panel::list_detailed, p);
            set->menu.label = "_Detailed";
            set->menu.type = xset::set::menu_type::radio;
            set->b = xset::set::enabled::yes;
            if (p != 1)
            {
                set->shared_key = xset::set::get(xset::name::panel1_list_detailed);
            }
        }

        {
            const auto set = xset::set::get(xset::panel::list_icons, p);
            set->menu.label = "_Icons";
            set->menu.type = xset::set::menu_type::radio;
            if (p != 1)
            {
                set->shared_key = xset::set::get(xset::name::panel1_list_icons);
            }
        }

        {
            const auto set = xset::set::get(xset::panel::list_compact, p);
            set->menu.label = "_Compact";
            set->menu.type = xset::set::menu_type::radio;
            if (p != 1)
            {
                set->shared_key = xset::set::get(xset::name::panel1_list_compact);
            }
        }

        {
            const auto set = xset::set::get(xset::panel::list_large, p);
            set->menu.label = "_Large Icons";
            set->menu.type = xset::set::menu_type::check;
            if (p != 1)
            {
                set->shared_key = xset::set::get(xset::name::panel1_list_large);
            }
        }

        {
            const auto set = xset::set::get(xset::panel::show_hidden, p);
            set->menu.label = "_Hidden Files";
            set->menu.type = xset::set::menu_type::check;
            if (p == 1)
            {
                set->keybinding.type = xset::set::keybinding_type::view;
            }
            if (p != 1)
            {
                set->shared_key = xset::set::get(xset::name::panel1_show_hidden);
            }
        }

        {
            const auto set = xset::set::get(xset::panel::detcol_name, p);
            set->menu.label = "_Name";
            set->menu.type = xset::set::menu_type::check;
            set->b = xset::set::enabled::yes; // visible
            set->x = "0";                     // position
        }

        {
            const auto set = xset::set::get(xset::panel::detcol_size, p);
            set->menu.label = "_Size";
            set->menu.type = xset::set::menu_type::check;
            set->b = xset::set::enabled::yes;
            set->x = "1";
            if (p != 1)
            {
                set->shared_key = xset::set::get(xset::name::panel1_detcol_size);
            }
        }

        {
            const auto set = xset::set::get(xset::panel::detcol_bytes, p);
            set->menu.label = "_Bytes";
            set->menu.type = xset::set::menu_type::check;
            set->b = xset::set::enabled::yes;
            set->x = "2";
            if (p != 1)
            {
                set->shared_key = xset::set::get(xset::name::panel1_detcol_bytes);
            }
        }

        {
            const auto set = xset::set::get(xset::panel::detcol_type, p);
            set->menu.label = "_Type";
            set->menu.type = xset::set::menu_type::check;
            set->x = "3";
            if (p != 1)
            {
                set->shared_key = xset::set::get(xset::name::panel1_detcol_type);
            }
        }

        {
            const auto set = xset::set::get(xset::panel::detcol_mime, p);
            set->menu.label = "_MIME Type";
            set->menu.type = xset::set::menu_type::check;
            set->x = "4";
            if (p != 1)
            {
                set->shared_key = xset::set::get(xset::name::panel1_detcol_mime);
            }
        }

        {
            const auto set = xset::set::get(xset::panel::detcol_perm, p);
            set->menu.label = "_Permissions";
            set->menu.type = xset::set::menu_type::check;
            set->x = "5";
            if (p != 1)
            {
                set->shared_key = xset::set::get(xset::name::panel1_detcol_perm);
            }
        }

        {
            const auto set = xset::set::get(xset::panel::detcol_owner, p);
            set->menu.label = "_Owner";
            set->menu.type = xset::set::menu_type::check;
            set->x = "6";
            if (p != 1)
            {
                set->shared_key = xset::set::get(xset::name::panel1_detcol_owner);
            }
        }

        {
            const auto set = xset::set::get(xset::panel::detcol_group, p);
            set->menu.label = "_Group";
            set->menu.type = xset::set::menu_type::check;
            set->x = "7";
            if (p != 1)
            {
                set->shared_key = xset::set::get(xset::name::panel1_detcol_group);
            }
        }

        {
            const auto set = xset::set::get(xset::panel::detcol_atime, p);
            set->menu.label = "_Accessed";
            set->menu.type = xset::set::menu_type::check;
            set->x = "8";
            if (p != 1)
            {
                set->shared_key = xset::set::get(xset::name::panel1_detcol_atime);
            }
        }

        {
            const auto set = xset::set::get(xset::panel::detcol_btime, p);
            set->menu.label = "_Created";
            set->menu.type = xset::set::menu_type::check;
            set->x = "9";
            if (p != 1)
            {
                set->shared_key = xset::set::get(xset::name::panel1_detcol_btime);
            }
        }

        {
            const auto set = xset::set::get(xset::panel::detcol_ctime, p);
            set->menu.label = "_Metadata";
            set->menu.type = xset::set::menu_type::check;
            set->x = "10";
            if (p != 1)
            {
                set->shared_key = xset::set::get(xset::name::panel1_detcol_ctime);
            }
        }

        {
            const auto set = xset::set::get(xset::panel::detcol_mtime, p);
            set->menu.label = "_Modified";
            set->menu.type = xset::set::menu_type::check;
            set->x = "11";
            if (p != 1)
            {
                set->shared_key = xset::set::get(xset::name::panel1_detcol_mtime);
            }
        }

        {
            const auto set = xset::set::get(xset::panel::sort_extra, p);
            set->b = xset::set::enabled::yes; // sort_natural
            set->x =
                std::format("{}", magic_enum::enum_integer(xset::set::enabled::no)); // sort_case
            set->y = "1"; // ptk::file_list::sort_dir::PTK_LIST_SORT_DIR_FIRST
            set->z =
                std::format("{}",
                            magic_enum::enum_integer(xset::set::enabled::yes)); // sort_hidden_first
        }

        {
            const auto set = xset::set::get(xset::panel::book_fol, p);
            set->menu.label = "Follow _Dir";
            set->menu.type = xset::set::menu_type::check;
            set->b = xset::set::enabled::yes;
            if (p != 1)
            {
                set->shared_key = xset::set::get(xset::name::panel1_book_fol);
            }
        }
    }

    // speed
    {
        const auto set = xset::set::get(xset::name::book_newtab);
        set->menu.label = "_New Tab";
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset::set::get(xset::name::book_single);
        set->menu.label = "_Single Click";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset::set::get(xset::name::dev_newtab);
        set->menu.label = "_New Tab";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset::set::get(xset::name::dev_single);
        set->menu.label = "_Single Click";
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }
}

static void
def_key(const std::span<const xset_t> keysets, xset::name name, u32 key, u32 keymod) noexcept
{
    const auto set = xset::set::get(name);

    // key already set or unset?
    if (set->keybinding.key != 0 || key == 0)
    {
        return;
    }

    // key combo already in use?
    for (const xset_t& keyset : keysets)
    {
        if (keyset->keybinding.key == key && keyset->keybinding.modifier == keymod)
        {
            logger::warn("Duplicate keybinding: {}, {}", set->name(), keyset->name());
            return;
        }
    }
    set->keybinding.key = key;
    set->keybinding.modifier = keymod;
}

void
xset_default_keys() noexcept
{
    // read all currently set or unset keys
    std::vector<xset_t> keysets;
    for (const xset_t& set : xset::sets())
    {
        assert(set != nullptr);

        if (set->keybinding.key != 0)
        {
            keysets.push_back(set);
        }
    }

    // clang-format off

    def_key(keysets, xset::name::tab_prev, GDK_KEY_Tab, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    def_key(keysets, xset::name::tab_next, GDK_KEY_Tab, GdkModifierType::GDK_CONTROL_MASK);
    def_key(keysets, xset::name::tab_new, GDK_KEY_t, GdkModifierType::GDK_CONTROL_MASK);
    def_key(keysets, xset::name::tab_restore, GDK_KEY_T, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    def_key(keysets, xset::name::tab_close, GDK_KEY_w, GdkModifierType::GDK_CONTROL_MASK);
#if (GTK_MAJOR_VERSION == 4)
    def_key(keysets, xset::name::tab_1, GDK_KEY_1, GdkModifierType::GDK_ALT_MASK);
    def_key(keysets, xset::name::tab_2, GDK_KEY_2, GdkModifierType::GDK_ALT_MASK);
    def_key(keysets, xset::name::tab_3, GDK_KEY_3, GdkModifierType::GDK_ALT_MASK);
    def_key(keysets, xset::name::tab_4, GDK_KEY_4, GdkModifierType::GDK_ALT_MASK);
    def_key(keysets, xset::name::tab_5, GDK_KEY_5, GdkModifierType::GDK_ALT_MASK);
    def_key(keysets, xset::name::tab_6, GDK_KEY_6, GdkModifierType::GDK_ALT_MASK);
    def_key(keysets, xset::name::tab_7, GDK_KEY_7, GdkModifierType::GDK_ALT_MASK);
    def_key(keysets, xset::name::tab_8, GDK_KEY_8, GdkModifierType::GDK_ALT_MASK);
    def_key(keysets, xset::name::tab_9, GDK_KEY_9, GdkModifierType::GDK_ALT_MASK);
    def_key(keysets, xset::name::tab_10, GDK_KEY_0, GdkModifierType::GDK_ALT_MASK);
#elif (GTK_MAJOR_VERSION == 3)
    def_key(keysets, xset::name::tab_1, GDK_KEY_1, GdkModifierType::GDK_MOD1_MASK);
    def_key(keysets, xset::name::tab_2, GDK_KEY_2, GdkModifierType::GDK_MOD1_MASK);
    def_key(keysets, xset::name::tab_3, GDK_KEY_3, GdkModifierType::GDK_MOD1_MASK);
    def_key(keysets, xset::name::tab_4, GDK_KEY_4, GdkModifierType::GDK_MOD1_MASK);
    def_key(keysets, xset::name::tab_5, GDK_KEY_5, GdkModifierType::GDK_MOD1_MASK);
    def_key(keysets, xset::name::tab_6, GDK_KEY_6, GdkModifierType::GDK_MOD1_MASK);
    def_key(keysets, xset::name::tab_7, GDK_KEY_7, GdkModifierType::GDK_MOD1_MASK);
    def_key(keysets, xset::name::tab_8, GDK_KEY_8, GdkModifierType::GDK_MOD1_MASK);
    def_key(keysets, xset::name::tab_9, GDK_KEY_9, GdkModifierType::GDK_MOD1_MASK);
    def_key(keysets, xset::name::tab_10, GDK_KEY_0, GdkModifierType::GDK_MOD1_MASK);
#endif
    def_key(keysets, xset::name::edit_cut, GDK_KEY_x, GdkModifierType::GDK_CONTROL_MASK);
    def_key(keysets, xset::name::edit_copy, GDK_KEY_c, GdkModifierType::GDK_CONTROL_MASK);
    def_key(keysets, xset::name::edit_paste, GDK_KEY_v, GdkModifierType::GDK_CONTROL_MASK);
    def_key(keysets, xset::name::edit_rename, GDK_KEY_F2, 0);
    def_key(keysets, xset::name::edit_delete, GDK_KEY_Delete, GdkModifierType::GDK_SHIFT_MASK);
    def_key(keysets, xset::name::edit_trash, GDK_KEY_Delete, 0);
#if (GTK_MAJOR_VERSION == 4)
    def_key(keysets, xset::name::copy_name, GDK_KEY_C, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_ALT_MASK));
#elif (GTK_MAJOR_VERSION == 3)
    def_key(keysets, xset::name::copy_name, GDK_KEY_C, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_MOD1_MASK));
#endif
    def_key(keysets, xset::name::copy_path, GDK_KEY_C, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    def_key(keysets, xset::name::paste_link, GDK_KEY_V, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    def_key(keysets, xset::name::paste_as, GDK_KEY_A, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    def_key(keysets, xset::name::select_all, GDK_KEY_A, GdkModifierType::GDK_CONTROL_MASK);
    def_key(keysets, xset::name::main_terminal, GDK_KEY_F4, 0);
    def_key(keysets, xset::name::go_home, GDK_KEY_Escape, 0);
#if (GTK_MAJOR_VERSION == 4)
    def_key(keysets, xset::name::go_back, GDK_KEY_Left, GdkModifierType::GDK_ALT_MASK);
    def_key(keysets, xset::name::go_forward, GDK_KEY_Right, GdkModifierType::GDK_ALT_MASK);
    def_key(keysets, xset::name::go_up, GDK_KEY_Up, GdkModifierType::GDK_ALT_MASK);
#elif (GTK_MAJOR_VERSION == 3)
    def_key(keysets, xset::name::go_back, GDK_KEY_Left, GdkModifierType::GDK_MOD1_MASK);
    def_key(keysets, xset::name::go_forward, GDK_KEY_Right, GdkModifierType::GDK_MOD1_MASK);
    def_key(keysets, xset::name::go_up, GDK_KEY_Up, GdkModifierType::GDK_MOD1_MASK);
#endif
    def_key(keysets, xset::name::focus_path_bar, GDK_KEY_l, GdkModifierType::GDK_CONTROL_MASK);
    def_key(keysets, xset::name::focus_search_bar, GDK_KEY_f, GdkModifierType::GDK_CONTROL_MASK);
    def_key(keysets, xset::name::view_refresh, GDK_KEY_F5, 0);
#if (GTK_MAJOR_VERSION == 4)
    def_key(keysets, xset::name::prop_info, GDK_KEY_Return, GdkModifierType::GDK_ALT_MASK);
#elif (GTK_MAJOR_VERSION == 3)
    def_key(keysets, xset::name::prop_info, GDK_KEY_Return, GdkModifierType::GDK_MOD1_MASK);
#endif
    def_key(keysets, xset::name::prop_perm, GDK_KEY_p, GdkModifierType::GDK_CONTROL_MASK);
    def_key(keysets, xset::name::panel1_show_hidden, GDK_KEY_h, GdkModifierType::GDK_CONTROL_MASK);
    def_key(keysets, xset::name::new_file, GDK_KEY_F, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    def_key(keysets, xset::name::new_directory, GDK_KEY_N, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    def_key(keysets, xset::name::new_link, GDK_KEY_L, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    // def_key(keysets, xset::name::new_archive, GDK_KEY_A, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    def_key(keysets, xset::name::main_new_window, GDK_KEY_n, GdkModifierType::GDK_CONTROL_MASK);
    def_key(keysets, xset::name::open_all, GDK_KEY_F6, 0);
    def_key(keysets, xset::name::main_full, GDK_KEY_F11, 0);
    def_key(keysets, xset::name::panel1_show, GDK_KEY_1, GdkModifierType::GDK_CONTROL_MASK);
    def_key(keysets, xset::name::panel2_show, GDK_KEY_2, GdkModifierType::GDK_CONTROL_MASK);
    def_key(keysets, xset::name::panel3_show, GDK_KEY_3, GdkModifierType::GDK_CONTROL_MASK);
    def_key(keysets, xset::name::panel4_show, GDK_KEY_4, GdkModifierType::GDK_CONTROL_MASK);
    // def_key(keysets, xset::name::main_help, GDK_KEY_F1, 0);
    def_key(keysets, xset::name::main_exit, GDK_KEY_q, GdkModifierType::GDK_CONTROL_MASK);
    def_key(keysets, xset::name::main_prefs, GDK_KEY_F12, 0);
    def_key(keysets, xset::name::book_add, GDK_KEY_d, GdkModifierType::GDK_CONTROL_MASK);

    // clang-format on
}
