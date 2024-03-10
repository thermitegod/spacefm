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

#include <magic_enum.hpp>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "xset/xset.hxx"

void
xset_defaults() noexcept
{
    // separator
    {
        const auto set = xset_get(xset::name::separator);
        set->menu.type = xset::set::menu_type::sep;
    }

    // dev menu
    {
        const auto set = xset_get(xset::name::dev_menu_remove);
        xset_set_var(set, xset::var::menu_label, "Remo_ve / Eject");
        xset_set_var(set, xset::var::icon, "gtk-disconnect");
    }

    {
        const auto set = xset_get(xset::name::dev_menu_unmount);
        xset_set_var(set, xset::var::menu_label, "_Unmount");
        xset_set_var(set, xset::var::icon, "gtk-remove");
    }

    {
        const auto set = xset_get(xset::name::dev_menu_open);
        xset_set_var(set, xset::var::menu_label, "_Open");
        xset_set_var(set, xset::var::icon, "gtk-open");
    }

    {
        const auto set = xset_get(xset::name::dev_menu_tab);
        xset_set_var(set, xset::var::menu_label, "Open In _Tab");
        xset_set_var(set, xset::var::icon, "gtk-add");
    }

    {
        const auto set = xset_get(xset::name::dev_menu_mount);
        xset_set_var(set, xset::var::menu_label, "_Mount");
        xset_set_var(set, xset::var::icon, "drive-removable-media");
    }

    {
        const auto set = xset_get(xset::name::dev_menu_mark);
        xset_set_var(set, xset::var::menu_label, "_Bookmark");
        xset_set_var(set, xset::var::icon, "gtk-add");
    }

    {
        const auto set = xset_get(xset::name::dev_menu_settings);
        xset_set_var(set, xset::var::menu_label, "Setti_ngs");
        xset_set_var(set, xset::var::icon, "gtk-properties");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
                             xset::name::dev_show,
                             xset::name::separator,
                             xset::name::dev_menu_auto,
                             xset::name::dev_change,
                             xset::name::separator,
                             xset::name::dev_single,
                             xset::name::dev_newtab,
                         });
    }

    // dev settings
    {
        const auto set = xset_get(xset::name::dev_show);
        xset_set_var(set, xset::var::menu_label, "S_how");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
                             xset::name::dev_show_internal_drives,
                             xset::name::dev_show_empty,
                             xset::name::dev_show_partition_tables,
                             xset::name::dev_show_net,
                             xset::name::dev_show_file,
                             xset::name::dev_ignore_udisks_hide,
                             xset::name::dev_show_hide_volumes,
                             xset::name::dev_dispname,
                         });
    }

    {
        const auto set = xset_get(xset::name::dev_show_internal_drives);
        xset_set_var(set, xset::var::menu_label, "_Internal Drives");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset_get(xset::name::dev_show_empty);
        xset_set_var(set, xset::var::menu_label, "_Empty Drives");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset_get(xset::name::dev_show_partition_tables);
        xset_set_var(set, xset::var::menu_label, "_Partition Tables");
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset_get(xset::name::dev_show_net);
        xset_set_var(set, xset::var::menu_label, "Mounted _Networks");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset_get(xset::name::dev_show_file);
        xset_set_var(set, xset::var::menu_label, "Mounted _Other");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset_get(xset::name::dev_show_hide_volumes);
        xset_set_var(set, xset::var::menu_label, "_Volumes...");
        xset_set_var(set, xset::var::title, "Show/Hide Volumes");
        xset_set_var(
            set,
            xset::var::desc,
            "To force showing or hiding of some volumes, overriding other settings, you can "
            "specify "
            "the devices, volume labels, or device IDs in the space-separated list "
            "below.\n\nExample:  +/dev/sdd1 -Label With Space +ata-OCZ-part4\nThis would cause "
            "/dev/sdd1 and the OCZ device to be shown, and the volume with label \"Label With "
            "Space\" to be hidden.\n\nThere must be a space between entries and a plus or minus "
            "sign "
            "directly before each item.  This list is case-sensitive.\n\n");
    }

    {
        const auto set = xset_get(xset::name::dev_ignore_udisks_hide);
        xset_set_var(set, xset::var::menu_label, "Ignore _Hide Policy");
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset_get(xset::name::dev_dispname);
        xset_set_var(set, xset::var::menu_label, "_Display Name");
        set->menu.type = xset::set::menu_type::string;
        xset_set_var(set, xset::var::title, "Set Display Name Format");
        xset_set_var(set,
                     xset::var::desc,
                     "Enter device display name format:\n\nUse:\n\t%%v\tdevice filename (eg "
                     "sdd1)\n\t%%s\ttotal size (eg 800G)\n\t%%t\tfstype (eg ext4)\n\t%%l\tvolume "
                     "label (eg Label or [no media])\n\t%%m\tmount point if mounted, or "
                     "---\n\t%%i\tdevice ID\n\t%%n\tmajor:minor device numbers (eg 15:3)\n");
        xset_set_var(set, xset::var::s, "%v %s %l %m");
        xset_set_var(set, xset::var::z, "%v %s %l %m");
        xset_set_var(set, xset::var::icon, "gtk-edit");
    }

    {
        const auto set = xset_get(xset::name::dev_menu_auto);
        xset_set_var(set, xset::var::menu_label, "_Auto Mount");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
                             xset::name::dev_automount_optical,
                             xset::name::dev_automount_removable,
                             xset::name::dev_ignore_udisks_nopolicy,
                             xset::name::dev_automount_volumes,
                             xset::name::dev_automount_dirs,
                             xset::name::dev_auto_open,
                             xset::name::dev_unmount_quit,
                         });
    }

    {
        const auto set = xset_get(xset::name::dev_automount_optical);
        xset_set_var(set, xset::var::menu_label, "Mount _Optical");
        set->b = xset::set::enabled::yes;
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset_get(xset::name::dev_automount_removable);
        xset_set_var(set, xset::var::menu_label, "_Mount Removable");
        set->b = xset::set::enabled::yes;
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset_get(xset::name::dev_automount_volumes);
        xset_set_var(set, xset::var::menu_label, "Mount _Volumes...");
        xset_set_var(set, xset::var::title, "Auto-Mount Volumes");
        xset_set_var(
            set,
            xset::var::desc,
            "To force or prevent automounting of some volumes, overriding other settings, you can "
            "specify the devices, volume labels, or device IDs in the space-separated list "
            "below.\n\nExample:  +/dev/sdd1 -Label With Space +ata-OCZ-part4\nThis would cause "
            "/dev/sdd1 and the OCZ device to be auto-mounted when detected, and the volume with "
            "label \"Label With Space\" to be ignored.\n\nThere must be a space between entries "
            "and "
            "a plus or minus sign directly before each item.  This list is case-sensitive.\n\n");
    }

    {
        const auto set = xset_get(xset::name::dev_automount_dirs);
        xset_set_var(set, xset::var::menu_label, "Mount _Dirs...");
        xset_set_var(set, xset::var::title, "Automatic Mount Point Dirs");
        set->menu.type = xset::set::menu_type::string;
        xset_set_var(
            set,
            xset::var::desc,
            "Enter the directory where SpaceFM should automatically create mount point directories "
            "for fuse and similar filesystems (%%a in handler commands).  This directory must be "
            "user-writable (do NOT use /media), and empty subdirectories will be removed.  If left "
            "blank, ~/.cache/spacefm/ (or $XDG_CACHE_HOME/spacefm/) is used.  The following "
            "variables are recognized: $USER $UID $HOME $XDG_RUNTIME_DIR $XDG_CACHE_HOME\n\nNote "
            "that some handlers or mount programs may not obey this setting.\n");
    }

    {
        const auto set = xset_get(xset::name::dev_auto_open);
        xset_set_var(set, xset::var::menu_label, "Open _Tab");
        set->b = xset::set::enabled::yes;
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset_get(xset::name::dev_unmount_quit);
        xset_set_var(set, xset::var::menu_label, "_Unmount On Exit");
        set->b = xset::set::enabled::unset;
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset_get(xset::name::dev_ignore_udisks_nopolicy);
        xset_set_var(set, xset::var::menu_label, "Ignore _No Policy");
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset_get(xset::name::dev_change);
        xset_set_var(set, xset::var::menu_label, "_Change Detection");
        xset_set_var(
            set,
            xset::var::desc,
            "Enter your comma- or space-separated list of filesystems which should NOT be "
            "monitored "
            "for file changes.  This setting only affects non-block devices (such as nfs or fuse), "
            "and is usually used to prevent SpaceFM becoming unresponsive with network "
            "filesystems.  "
            "Loading of thumbnails and subdirectory sizes will also be disabled.");
        set->menu.type = xset::set::menu_type::string;
        xset_set_var(set, xset::var::title, "Change Detection Blacklist");
        xset_set_var(set, xset::var::icon, "gtk-edit");
        set->s = "cifs curlftpfs ftpfs fuse.sshfs nfs smbfs";
        set->z = set->s;
    }

    // Bookmarks
    {
        const auto set = xset_get(xset::name::book_open);
        xset_set_var(set, xset::var::menu_label, "_Open");
        xset_set_var(set, xset::var::icon, "gtk-open");
    }

    {
        const auto set = xset_get(xset::name::book_settings);
        xset_set_var(set, xset::var::menu_label, "_Settings");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_var(set, xset::var::icon, "gtk-properties");
    }

    {
        const auto set = xset_get(xset::name::book_add);
        xset_set_var(set, xset::var::menu_label, "New _Bookmark");
        xset_set_var(set, xset::var::icon, "gtk-jump-to");
    }

    {
        const auto set = xset_get(xset::name::main_book);
        xset_set_var(set, xset::var::menu_label, "_Bookmarks");
        xset_set_var(set, xset::var::icon, "folder");
        set->menu.type = xset::set::menu_type::submenu;
    }

    // Fonts
    {
        const auto set = xset_get(xset::name::font_general);
        xset_set_var(set, xset::var::s, "Monospace 9");
    }

    {
        const auto set = xset_get(xset::name::font_view_icon);
        xset_set_var(set, xset::var::s, "Monospace 9");
    }

    {
        const auto set = xset_get(xset::name::font_view_compact);
        xset_set_var(set, xset::var::s, "Monospace 9");
    }

    // Rename/Move Dialog
    {
        const auto set = xset_get(xset::name::move_name);
        xset_set_var(set, xset::var::menu_label, "_Name");
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset_get(xset::name::move_filename);
        xset_set_var(set, xset::var::menu_label, "F_ilename");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset_get(xset::name::move_parent);
        xset_set_var(set, xset::var::menu_label, "_Parent");
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset_get(xset::name::move_path);
        xset_set_var(set, xset::var::menu_label, "P_ath");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset_get(xset::name::move_type);
        xset_set_var(set, xset::var::menu_label, "Typ_e");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset_get(xset::name::move_target);
        xset_set_var(set, xset::var::menu_label, "Ta_rget");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset_get(xset::name::move_template);
        xset_set_var(set, xset::var::menu_label, "Te_mplate");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset_get(xset::name::move_option);
        xset_set_var(set, xset::var::menu_label, "_Option");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
                             xset::name::move_copy,
                             xset::name::move_link,
                             xset::name::move_copyt,
                             xset::name::move_linkt,
                         });
    }

    {
        const auto set = xset_get(xset::name::move_copy);
        xset_set_var(set, xset::var::menu_label, "_Copy");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset_get(xset::name::move_link);
        xset_set_var(set, xset::var::menu_label, "_Link");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset_get(xset::name::move_copyt);
        xset_set_var(set, xset::var::menu_label, "Copy _Target");
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset_get(xset::name::move_linkt);
        xset_set_var(set, xset::var::menu_label, "Lin_k Target");
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset_get(xset::name::move_dlg_help);
        xset_set_var(set, xset::var::menu_label, "_Help");
        xset_set_var(set, xset::var::icon, "gtk-help");
    }

    {
        const auto set = xset_get(xset::name::move_dlg_confirm_create);
        xset_set_var(set, xset::var::menu_label, "_Confirm Create");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    // status bar
    {
        const auto set = xset_get(xset::name::status_middle);
        xset_set_var(set, xset::var::menu_label, "_Middle Click");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
                             xset::name::status_name,
                             xset::name::status_path,
                             xset::name::status_info,
                             xset::name::status_hide,
                         });
    }

    {
        const auto set = xset_get(xset::name::status_name);
        xset_set_var(set, xset::var::menu_label, "Copy _Name");
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset_get(xset::name::status_path);
        xset_set_var(set, xset::var::menu_label, "Copy _Path");
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset_get(xset::name::status_info);
        xset_set_var(set, xset::var::menu_label, "File _Info");
        set->menu.type = xset::set::menu_type::radio;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset_get(xset::name::status_hide);
        xset_set_var(set, xset::var::menu_label, "_Hide Panel");
        set->menu.type = xset::set::menu_type::radio;
    }

    // MAIN WINDOW MENUS

    // File
    {
        const auto set = xset_get(xset::name::main_new_window);
        xset_set_var(set, xset::var::menu_label, "New _Window");
        xset_set_var(set, xset::var::icon, "spacefm");
        set->keybinding.type = xset::set::keybinding_type::general;
    }

    {
        const auto set = xset_get(xset::name::main_search);
        xset_set_var(set, xset::var::menu_label, "_File Search");
        xset_set_var(set, xset::var::icon, "gtk-find");
        set->keybinding.type = xset::set::keybinding_type::general;
    }

    {
        const auto set = xset_get(xset::name::main_terminal);
        xset_set_var(set, xset::var::menu_label, "_Terminal");
        set->b = xset::set::enabled::unset; // discovery notification
        set->keybinding.type = xset::set::keybinding_type::general;
    }

    // was previously used for 'Save Session' < 0.9.4 as xset::set::menu_type::NORMAL
    {
        const auto set = xset_get(xset::name::main_save_session);
        xset_set_var(set, xset::var::menu_label, "Open _URL");
        set->menu.type = xset::set::menu_type::string;
        xset_set_var(set, xset::var::icon, "gtk-network");
        xset_set_var(set, xset::var::title, "Open URL");
        xset_set_var(
            set,
            xset::var::desc,
            "Enter URL in the "
            "format:\n\tPROTOCOL://USERNAME:PASSWORD@HOST:PORT/SHARE\n\nExamples:\n\tftp://"
            "mirrors.kernel.org\n\tsmb://user:pass@10.0.0.1:50/docs\n\tssh://"
            "user@sys.domain\n\tmtp://\n\nIncluding a password is unsafe.  To bookmark a "
            "URL, right-click on the mounted network in Devices and select Bookmark.\n");
    }

    {
        const auto set = xset_get(xset::name::main_save_tabs);
        xset_set_var(set, xset::var::menu_label, "Save Ta_bs");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset_get(xset::name::main_exit);
        xset_set_var(set, xset::var::menu_label, "E_xit");
        xset_set_var(set, xset::var::icon, "gtk-quit");
        set->keybinding.type = xset::set::keybinding_type::general;
    }

    // View
    {
        const auto set = xset_get(xset::name::panel1_show);
        xset_set_var(set, xset::var::menu_label, "Panel _1");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
        set->keybinding.type = xset::set::keybinding_type::view;
    }

    {
        const auto set = xset_get(xset::name::panel2_show);
        xset_set_var(set, xset::var::menu_label, "Panel _2");
        set->menu.type = xset::set::menu_type::check;
        set->keybinding.type = xset::set::keybinding_type::view;
    }

    {
        const auto set = xset_get(xset::name::panel3_show);
        xset_set_var(set, xset::var::menu_label, "Panel _3");
        set->menu.type = xset::set::menu_type::check;
        set->keybinding.type = xset::set::keybinding_type::view;
    }

    {
        const auto set = xset_get(xset::name::panel4_show);
        xset_set_var(set, xset::var::menu_label, "Panel _4");
        set->menu.type = xset::set::menu_type::check;
        set->keybinding.type = xset::set::keybinding_type::view;
    }

    {
        const auto set = xset_get(xset::name::main_focus_panel);
        xset_set_var(set, xset::var::menu_label, "F_ocus");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
                             xset::name::panel_prev,
                             xset::name::panel_next,
                             xset::name::panel_hide,
                             xset::name::panel_1,
                             xset::name::panel_2,
                             xset::name::panel_3,
                             xset::name::panel_4,
                         });
        xset_set_var(set, xset::var::icon, "gtk-go-forward");
    }

    {
        const auto set = xset_get(xset::name::panel_prev);
        xset_set_var(set, xset::var::menu_label, "_Prev");
    }

    {
        const auto set = xset_get(xset::name::panel_next);
        xset_set_var(set, xset::var::menu_label, "_Next");
    }

    {
        const auto set = xset_get(xset::name::panel_hide);
        xset_set_var(set, xset::var::menu_label, "_Hide");
    }

    {
        const auto set = xset_get(xset::name::panel_1);
        xset_set_var(set, xset::var::menu_label, "Panel _1");
    }

    {
        const auto set = xset_get(xset::name::panel_2);
        xset_set_var(set, xset::var::menu_label, "Panel _2");
    }

    {
        const auto set = xset_get(xset::name::panel_3);
        xset_set_var(set, xset::var::menu_label, "Panel _3");
    }

    {
        const auto set = xset_get(xset::name::panel_4);
        xset_set_var(set, xset::var::menu_label, "Panel _4");
    }

    {
        const auto set = xset_get(xset::name::main_title);
        xset_set_var(set, xset::var::menu_label, "Wi_ndow Title");
        set->menu.type = xset::set::menu_type::string;
        xset_set_var(set, xset::var::title, "Set Window Title Format");
        xset_set_var(
            set,
            xset::var::desc,
            "Set window title format:\n\nUse:\n\t%%n\tcurrent directory name (eg "
            "bin)\n\t%%d\tcurrent "
            "directory path (eg /usr/bin)\n\t%%p\tcurrent panel number (1-4)\n\t%%t\tcurrent tab "
            "number\n\t%%P\ttotal number of panels visible\n\t%%T\ttotal number of tabs in current "
            "panel\n\t*\tasterisk shown if tasks running in window");
        xset_set_var(set, xset::var::s, "%d");
        xset_set_var(set, xset::var::z, "%d");
    }

    {
        const auto set = xset_get(xset::name::main_full);
        xset_set_var(set, xset::var::menu_label, "_Fullscreen");
        set->menu.type = xset::set::menu_type::check;
        set->keybinding.type = xset::set::keybinding_type::general;
    }

    {
        const auto set = xset_get(xset::name::main_keybindings);
        xset_set_var(set, xset::var::menu_label, "Keybindings");
        xset_set_var(set, xset::var::icon, "gtk-preferences");
        set->keybinding.type = xset::set::keybinding_type::general;
    }

    {
        const auto set = xset_get(xset::name::main_prefs);
        xset_set_var(set, xset::var::menu_label, "_Preferences");
        xset_set_var(set, xset::var::icon, "gtk-preferences");
        set->keybinding.type = xset::set::keybinding_type::general;
    }

    {
        const auto set = xset_get(xset::name::root_bar); // in Preferences
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset_get(xset::name::view_thumb);
        xset_set_var(set,
                     xset::var::menu_label,
                     "_Thumbnails (global)"); // in View|Panel View|Style
        set->menu.type = xset::set::menu_type::check;
    }

    // Help
    {
        const auto set = xset_get(xset::name::main_about);
        xset_set_var(set, xset::var::menu_label, "_About");
        xset_set_var(set, xset::var::icon, "gtk-about");
    }

    {
        const auto set = xset_get(xset::name::main_dev);
        xset_set_var(set, xset::var::menu_label, "_Show Devices");
        set->shared_key = xset_get(xset::name::panel1_show_devmon);
        set->menu.type = xset::set::menu_type::check;
    }

    // Tasks
    {
        const auto set = xset_get(xset::name::main_tasks);
        xset_set_var(set, xset::var::menu_label, "_Task Manager");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
                             xset::name::task_show_manager,
                             xset::name::task_hide_manager,
                             xset::name::separator,
                             xset::name::task_columns,
                             xset::name::task_popups,
                             xset::name::task_errors,
                             xset::name::task_queue,
                         });
    }

    {
        const auto set = xset_get(xset::name::task_col_status);
        xset_set_var(set, xset::var::menu_label, "_Status");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
        set->x = "0";   // column position
        set->y = "130"; // column width
    }

    {
        const auto set = xset_get(xset::name::task_col_count);
        xset_set_var(set, xset::var::menu_label, "_Count");
        set->menu.type = xset::set::menu_type::check;
        set->x = "1";
    }

    {
        const auto set = xset_get(xset::name::task_col_path);
        xset_set_var(set, xset::var::menu_label, "_Directory");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
        set->x = "2";
    }

    {
        const auto set = xset_get(xset::name::task_col_file);
        xset_set_var(set, xset::var::menu_label, "_Item");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
        set->x = "3";
    }

    {
        const auto set = xset_get(xset::name::task_col_to);
        xset_set_var(set, xset::var::menu_label, "_To");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
        set->x = "4";
    }

    {
        const auto set = xset_get(xset::name::task_col_progress);
        xset_set_var(set, xset::var::menu_label, "_Progress");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
        set->x = "5";
        set->y = "100";
    }

    {
        const auto set = xset_get(xset::name::task_col_total);
        xset_set_var(set, xset::var::menu_label, "T_otal");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
        set->x = "6";
        set->y = "120";
    }

    {
        const auto set = xset_get(xset::name::task_col_started);
        xset_set_var(set, xset::var::menu_label, "Sta_rted");
        set->menu.type = xset::set::menu_type::check;
        set->x = "7";
    }

    {
        const auto set = xset_get(xset::name::task_col_elapsed);
        xset_set_var(set, xset::var::menu_label, "_Elapsed");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
        set->x = "8";
        set->y = "70";
    }

    {
        const auto set = xset_get(xset::name::task_col_curspeed);
        xset_set_var(set, xset::var::menu_label, "C_urrent Speed");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
        set->x = "9";
    }

    {
        const auto set = xset_get(xset::name::task_col_curest);
        xset_set_var(set, xset::var::menu_label, "Current Re_main");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
        set->x = "10";
    }

    {
        const auto set = xset_get(xset::name::task_col_avgspeed);
        xset_set_var(set, xset::var::menu_label, "_Average Speed");
        set->menu.type = xset::set::menu_type::check;
        set->x = "11";
        set->y = "60";
    }

    {
        const auto set = xset_get(xset::name::task_col_avgest);
        xset_set_var(set, xset::var::menu_label, "A_verage Remain");
        set->menu.type = xset::set::menu_type::check;
        set->x = "12";
        set->y = "65";
    }

    {
        const auto set = xset_get(xset::name::task_col_reorder);
        xset_set_var(set, xset::var::menu_label, "Reor_der");
    }

    {
        const auto set = xset_get(xset::name::task_stop);
        xset_set_var(set, xset::var::menu_label, "_Stop");
        xset_set_var(set, xset::var::icon, "gtk-stop");
    }

    {
        const auto set = xset_get(xset::name::task_pause);
        xset_set_var(set, xset::var::menu_label, "Pa_use");
        xset_set_var(set, xset::var::icon, "gtk-media-pause");
    }

    {
        const auto set = xset_get(xset::name::task_que);
        xset_set_var(set, xset::var::menu_label, "_Queue");
        xset_set_var(set, xset::var::icon, "gtk-add");
    }

    {
        const auto set = xset_get(xset::name::task_resume);
        xset_set_var(set, xset::var::menu_label, "_Resume");
        xset_set_var(set, xset::var::icon, "gtk-media-play");
    }

    {
        const auto set = xset_get(xset::name::task_showout);
        xset_set_var(set, xset::var::menu_label, "Sho_w Output");
    }

    {
        const auto set = xset_get(xset::name::task_all);
        xset_set_var(set, xset::var::menu_label, "_All Tasks");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
                             xset::name::task_stop_all,
                             xset::name::task_pause_all,
                             xset::name::task_que_all,
                             xset::name::task_resume_all,
                         });
    }

    {
        const auto set = xset_get(xset::name::task_stop_all);
        xset_set_var(set, xset::var::menu_label, "_Stop");
        xset_set_var(set, xset::var::icon, "gtk-stop");
    }

    {
        const auto set = xset_get(xset::name::task_pause_all);
        xset_set_var(set, xset::var::menu_label, "Pa_use");
        xset_set_var(set, xset::var::icon, "gtk-media-pause");
    }

    {
        const auto set = xset_get(xset::name::task_que_all);
        xset_set_var(set, xset::var::menu_label, "_Queue");
        xset_set_var(set, xset::var::icon, "gtk-add");
    }

    {
        const auto set = xset_get(xset::name::task_resume_all);
        xset_set_var(set, xset::var::menu_label, "_Resume");
        xset_set_var(set, xset::var::icon, "gtk-media-play");
    }

    {
        const auto set = xset_get(xset::name::task_show_manager);
        xset_set_var(set, xset::var::menu_label, "Show _Manager");
        set->menu.type = xset::set::menu_type::radio;
        set->b = xset::set::enabled::no;
    }

    {
        const auto set = xset_get(xset::name::task_hide_manager);
        xset_set_var(set, xset::var::menu_label, "Auto-_Hide Manager");
        set->menu.type = xset::set::menu_type::radio;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset_get(xset::name::task_columns);
        xset_set_var(set, xset::var::menu_label, "_Columns");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
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
                         });
    }

    {
        const auto set = xset_get(xset::name::task_popups);
        xset_set_var(set, xset::var::menu_label, "_Popups");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
                             xset::name::task_pop_all,
                             xset::name::task_pop_top,
                             xset::name::task_pop_above,
                             xset::name::task_pop_stick,
                             xset::name::separator,
                             xset::name::task_pop_detail,
                             xset::name::task_pop_over,
                             xset::name::task_pop_err,
                         });
    }

    {
        const auto set = xset_get(xset::name::task_pop_all);
        xset_set_var(set, xset::var::menu_label, "Popup _All Tasks");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::no;
    }

    {
        const auto set = xset_get(xset::name::task_pop_top);
        xset_set_var(set, xset::var::menu_label, "Stay On _Top");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::no;
    }

    {
        const auto set = xset_get(xset::name::task_pop_above);
        xset_set_var(set, xset::var::menu_label, "A_bove Others");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::no;
    }

    {
        const auto set = xset_get(xset::name::task_pop_stick);
        xset_set_var(set, xset::var::menu_label, "All _Workspaces");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::no;
    }

    {
        const auto set = xset_get(xset::name::task_pop_detail);
        xset_set_var(set, xset::var::menu_label, "_Detailed Stats");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::no;
    }

    {
        const auto set = xset_get(xset::name::task_pop_over);
        xset_set_var(set, xset::var::menu_label, "_Overwrite Option");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset_get(xset::name::task_pop_err);
        xset_set_var(set, xset::var::menu_label, "_Error Option");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset_get(xset::name::task_errors);
        xset_set_var(set, xset::var::menu_label, "Err_ors");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
                             xset::name::task_err_first,
                             xset::name::task_err_any,
                             xset::name::task_err_cont,
                         });
    }

    {
        const auto set = xset_get(xset::name::task_err_first);
        xset_set_var(set, xset::var::menu_label, "Stop If _First");
        set->menu.type = xset::set::menu_type::radio;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset_get(xset::name::task_err_any);
        xset_set_var(set, xset::var::menu_label, "Stop On _Any");
        set->menu.type = xset::set::menu_type::radio;
        set->b = xset::set::enabled::no;
    }

    {
        const auto set = xset_get(xset::name::task_err_cont);
        xset_set_var(set, xset::var::menu_label, "_Continue");
        set->menu.type = xset::set::menu_type::radio;
        set->b = xset::set::enabled::no;
    }

    {
        const auto set = xset_get(xset::name::task_queue);
        xset_set_var(set, xset::var::menu_label, "Qu_eue");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
                             xset::name::task_q_new,
                             xset::name::task_q_smart,
                             xset::name::task_q_pause,
                         });
    }

    {
        const auto set = xset_get(xset::name::task_q_new);
        xset_set_var(set, xset::var::menu_label, "_Queue New Tasks");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset_get(xset::name::task_q_smart);
        xset_set_var(set, xset::var::menu_label, "_Smart Queue");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset_get(xset::name::task_q_pause);
        xset_set_var(set, xset::var::menu_label, "_Pause On Error");
        set->menu.type = xset::set::menu_type::check;
    }

    // PANELS COMMON

    {
        const auto set = xset_get(xset::name::con_open);
        xset_set_var(set, xset::var::menu_label, "_Open");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_var(set, xset::var::icon, "gtk-open");
    }

    {
        const auto set = xset_get(xset::name::open_execute);
        xset_set_var(set, xset::var::menu_label, "E_xecute");
        xset_set_var(set, xset::var::icon, "gtk-execute");
    }

    {
        const auto set = xset_get(xset::name::open_edit);
        xset_set_var(set, xset::var::menu_label, "Edi_t");
        xset_set_var(set, xset::var::icon, "gtk-edit");
    }

    {
        const auto set = xset_get(xset::name::open_other);
        xset_set_var(set, xset::var::menu_label, "_Choose...");
        xset_set_var(set, xset::var::icon, "gtk-open");
    }

    {
        const auto set = xset_get(xset::name::open_all);
        xset_set_var(set, xset::var::menu_label, "Open With _Default"); // virtual
        set->keybinding.type = xset::set::keybinding_type::opening;
    }

    {
        const auto set = xset_get(xset::name::open_in_tab);
        xset_set_var(set, xset::var::menu_label, "In _Tab");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
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
                         });
    }

    {
        xset_set(xset::name::opentab_new, xset::var::menu_label, "N_ew");
        xset_set(xset::name::opentab_prev, xset::var::menu_label, "_Prev");
        xset_set(xset::name::opentab_next, xset::var::menu_label, "_Next");
        xset_set(xset::name::opentab_1, xset::var::menu_label, "Tab _1");
        xset_set(xset::name::opentab_2, xset::var::menu_label, "Tab _2");
        xset_set(xset::name::opentab_3, xset::var::menu_label, "Tab _3");
        xset_set(xset::name::opentab_4, xset::var::menu_label, "Tab _4");
        xset_set(xset::name::opentab_5, xset::var::menu_label, "Tab _5");
        xset_set(xset::name::opentab_6, xset::var::menu_label, "Tab _6");
        xset_set(xset::name::opentab_7, xset::var::menu_label, "Tab _7");
        xset_set(xset::name::opentab_8, xset::var::menu_label, "Tab _8");
        xset_set(xset::name::opentab_9, xset::var::menu_label, "Tab _9");
        xset_set(xset::name::opentab_10, xset::var::menu_label, "Tab 1_0");
    }

    {
        const auto set = xset_get(xset::name::open_in_panel);
        xset_set_var(set, xset::var::menu_label, "In _Panel");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
                             xset::name::open_in_panel_prev,
                             xset::name::open_in_panel_next,
                             xset::name::open_in_panel1,
                             xset::name::open_in_panel2,
                             xset::name::open_in_panel3,
                             xset::name::open_in_panel4,
                         });
    }

    {
        xset_set(xset::name::open_in_panel_prev, xset::var::menu_label, "_Prev");
        xset_set(xset::name::open_in_panel_next, xset::var::menu_label, "_Next");
        xset_set(xset::name::open_in_panel1, xset::var::menu_label, "Panel _1");
        xset_set(xset::name::open_in_panel2, xset::var::menu_label, "Panel _2");
        xset_set(xset::name::open_in_panel3, xset::var::menu_label, "Panel _3");
        xset_set(xset::name::open_in_panel4, xset::var::menu_label, "Panel _4");
    }

    {
        const auto set = xset_get(xset::name::archive_extract);
        xset_set_var(set, xset::var::menu_label, "Archive Extract");
        xset_set_var(set, xset::var::icon, "gtk-convert");
    }

    {
        const auto set = xset_get(xset::name::archive_extract_to);
        xset_set_var(set, xset::var::menu_label, "Archive Extract To");
        xset_set_var(set, xset::var::icon, "gtk-convert");
    }

    {
        const auto set = xset_get(xset::name::archive_open);
        xset_set_var(set, xset::var::menu_label, "Archive Open");
        xset_set_var(set, xset::var::icon, "gtk-file");
    }

    {
        const auto set = xset_get(xset::name::archive_default);
        xset_set_var(set, xset::var::menu_label, "_Archive Defaults");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
                             xset::name::archive_default_open_with_app,
                             xset::name::archive_default_extract,
                             xset::name::archive_default_extract_to,
                             xset::name::archive_default_open_with_archiver,
                         });
    }

    {
        const auto set = xset_get(xset::name::archive_default_open_with_app);
        xset_set_var(set, xset::var::menu_label, "Open With App");
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset_get(xset::name::archive_default_extract);
        xset_set_var(set, xset::var::menu_label, "Extract");
        set->menu.type = xset::set::menu_type::radio;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset_get(xset::name::archive_default_extract_to);
        xset_set_var(set, xset::var::menu_label, "Extract To");
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset_get(xset::name::archive_default_open_with_archiver);
        xset_set_var(set, xset::var::menu_label, "Open With Archiver");
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset_get(xset::name::open_new);
        xset_set_var(set, xset::var::menu_label, "_New");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
                             xset::name::new_file,
                             xset::name::new_directory,
                             xset::name::new_link,
                             xset::name::new_archive,
                             xset::name::separator,
                             xset::name::tab_new,
                             xset::name::tab_new_here,
                             xset::name::new_bookmark,
                         });
        xset_set_var(set, xset::var::icon, "gtk-new");
    }

    {
        const auto set = xset_get(xset::name::new_file);
        xset_set_var(set, xset::var::menu_label, "_File");
        xset_set_var(set, xset::var::icon, "gtk-file");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::new_directory);
        xset_set_var(set, xset::var::menu_label, "Dir_ectory");
        xset_set_var(set, xset::var::icon, "folder");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::new_link);
        xset_set_var(set, xset::var::menu_label, "_Link");
        xset_set_var(set, xset::var::icon, "gtk-file");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::new_bookmark);
        xset_set_var(set, xset::var::menu_label, "_Bookmark");
        set->shared_key = xset_get(xset::name::book_add);
        xset_set_var(set, xset::var::icon, "gtk-jump-to");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::new_archive);
        xset_set_var(set, xset::var::menu_label, "_Archive");
        xset_set_var(set, xset::var::icon, "gtk-save-as");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::arc_dlg);
        set->b = xset::set::enabled::yes; // Extract To - Create Subdirectory
        set->z = "1";                     // Extract To - Write Access
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::tab_new);
        xset_set_var(set, xset::var::menu_label, "_Tab");
        xset_set_var(set, xset::var::icon, "gtk-add");
        set->keybinding.type = xset::set::keybinding_type::navigation;
    }

    {
        const auto set = xset_get(xset::name::tab_new_here);
        xset_set_var(set, xset::var::menu_label, "Tab _Here");
        xset_set_var(set, xset::var::icon, "gtk-add");
        set->keybinding.type = xset::set::keybinding_type::navigation;
    }

    {
        const auto set = xset_get(xset::name::new_app);
        xset_set_var(set, xset::var::menu_label, "_Desktop Application");
        xset_set_var(set, xset::var::icon, "gtk-add");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::con_go);
        xset_set_var(set, xset::var::menu_label, "_Go");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
                             xset::name::go_back,
                             xset::name::go_forward,
                             xset::name::go_up,
                             xset::name::go_home,
                             xset::name::go_default,
                             xset::name::go_set_default,
                             xset::name::edit_canon,
                             xset::name::separator,
                             xset::name::go_tab,
                             xset::name::go_focus,
                         });
        xset_set_var(set, xset::var::icon, "gtk-go-forward");
    }

    {
        const auto set = xset_get(xset::name::go_back);
        xset_set_var(set, xset::var::menu_label, "_Back");
        xset_set_var(set, xset::var::icon, "gtk-go-back");
        set->keybinding.type = xset::set::keybinding_type::navigation;
    }

    {
        const auto set = xset_get(xset::name::go_forward);
        xset_set_var(set, xset::var::menu_label, "_Forward");
        xset_set_var(set, xset::var::icon, "gtk-go-forward");
        set->keybinding.type = xset::set::keybinding_type::navigation;
    }

    {
        const auto set = xset_get(xset::name::go_up);
        xset_set_var(set, xset::var::menu_label, "_Up");
        xset_set_var(set, xset::var::icon, "gtk-go-up");
        set->keybinding.type = xset::set::keybinding_type::navigation;
    }

    {
        const auto set = xset_get(xset::name::go_home);
        xset_set_var(set, xset::var::menu_label, "_Home");
        xset_set_var(set, xset::var::icon, "gtk-home");
        set->keybinding.type = xset::set::keybinding_type::navigation;
    }

    {
        const auto set = xset_get(xset::name::go_default);
        xset_set_var(set, xset::var::menu_label, "_Default");
        xset_set_var(set, xset::var::icon, "gtk-home");
        set->keybinding.type = xset::set::keybinding_type::navigation;
    }

    {
        const auto set = xset_get(xset::name::go_set_default);
        xset_set_var(set, xset::var::menu_label, "_Set Default");
        xset_set_var(set, xset::var::icon, "gtk-save");
        set->keybinding.type = xset::set::keybinding_type::navigation;
    }

    {
        const auto set = xset_get(xset::name::edit_canon);
        xset_set_var(set, xset::var::menu_label, "Re_al Path");
    }

    {
        const auto set = xset_get(xset::name::go_focus);
        xset_set_var(set, xset::var::menu_label, "Fo_cus");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
                             xset::name::focus_path_bar,
                             xset::name::focus_filelist,
                             xset::name::focus_dirtree,
                             xset::name::focus_book,
                             xset::name::focus_device,
                         });
    }

    {
        const auto set = xset_get(xset::name::focus_path_bar);
        xset_set_var(set, xset::var::menu_label, "_Path Bar");
        xset_set_var(set, xset::var::icon, "gtk-dialog-question");
        set->keybinding.type = xset::set::keybinding_type::navigation;
    }

    {
        const auto set = xset_get(xset::name::focus_filelist);
        xset_set_var(set, xset::var::menu_label, "_File List");
        xset_set_var(set, xset::var::icon, "gtk-file");
    }

    {
        const auto set = xset_get(xset::name::focus_dirtree);
        xset_set_var(set, xset::var::menu_label, "_Tree");
        xset_set_var(set, xset::var::icon, "folder");
    }

    {
        const auto set = xset_get(xset::name::focus_book);
        xset_set_var(set, xset::var::menu_label, "_Bookmarks");
        xset_set_var(set, xset::var::icon, "gtk-jump-to");
    }

    {
        const auto set = xset_get(xset::name::focus_device);
        xset_set_var(set, xset::var::menu_label, "De_vices");
        xset_set_var(set, xset::var::icon, "gtk-harddisk");
    }

    {
        const auto set = xset_get(xset::name::go_tab);
        xset_set_var(set, xset::var::menu_label, "_Tab");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
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
                         });
    }

    {
        const auto set = xset_get(xset::name::tab_prev);
        xset_set(set, xset::var::menu_label, "_Prev");
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset_get(xset::name::tab_next);
        xset_set(set, xset::var::menu_label, "_Next");
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset_get(xset::name::tab_restore);
        xset_set(set, xset::var::menu_label, "_Restore");
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset_get(xset::name::tab_close);
        xset_set(set, xset::var::menu_label, "_Close");
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset_get(xset::name::tab_1);
        xset_set(set, xset::var::menu_label, "Tab _1");
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset_get(xset::name::tab_2);
        xset_set(set, xset::var::menu_label, "Tab _2");
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset_get(xset::name::tab_3);
        xset_set(set, xset::var::menu_label, "Tab _3");
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset_get(xset::name::tab_4);
        xset_set(set, xset::var::menu_label, "Tab _4");
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset_get(xset::name::tab_5);
        xset_set(set, xset::var::menu_label, "Tab _5");
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset_get(xset::name::tab_6);
        xset_set(set, xset::var::menu_label, "Tab _6");
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset_get(xset::name::tab_7);
        xset_set(set, xset::var::menu_label, "Tab _7");
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset_get(xset::name::tab_8);
        xset_set(set, xset::var::menu_label, "Tab _8");
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset_get(xset::name::tab_9);
        xset_set(set, xset::var::menu_label, "Tab _9");
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset_get(xset::name::tab_10);
        xset_set(set, xset::var::menu_label, "Tab 1_0");
        set->keybinding.type = xset::set::keybinding_type::tabs;
    }

    {
        const auto set = xset_get(xset::name::con_view);
        xset_set_var(set, xset::var::menu_label, "_View");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_var(set, xset::var::icon, "gtk-preferences");
    }

    {
        const auto set = xset_get(xset::name::view_list_style);
        xset_set_var(set, xset::var::menu_label, "Styl_e");
        set->menu.type = xset::set::menu_type::submenu;
    }

    {
        const auto set = xset_get(xset::name::view_columns);
        xset_set_var(set, xset::var::menu_label, "C_olumns");
        set->menu.type = xset::set::menu_type::submenu;
    }

    {
        const auto set = xset_get(xset::name::view_reorder_col);
        xset_set_var(set, xset::var::menu_label, "_Reorder");
    }

    {
        const auto set = xset_get(xset::name::rubberband);
        xset_set_var(set, xset::var::menu_label, "_Rubberband Select");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset_get(xset::name::view_sortby);
        xset_set_var(set, xset::var::menu_label, "_Sort");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
                             xset::name::sortby_name,    xset::name::sortby_size,
                             xset::name::sortby_bytes,   xset::name::sortby_type,
                             xset::name::sortby_mime,    xset::name::sortby_perm,
                             xset::name::sortby_owner,   xset::name::sortby_group,
                             xset::name::sortby_atime,   xset::name::sortby_btime,
                             xset::name::sortby_ctime,   xset::name::sortby_mtime,
                             xset::name::separator,      xset::name::sortby_ascend,
                             xset::name::sortby_descend, xset::name::separator,
                             xset::name::sortx_natural,  xset::name::sortx_case,
                             xset::name::separator,      xset::name::sortx_directories,
                             xset::name::sortx_files,    xset::name::sortx_mix,
                             xset::name::separator,      xset::name::sortx_hidfirst,
                             xset::name::sortx_hidlast,
                         });
    }

    {
        const auto set = xset_get(xset::name::sortby_name);
        xset_set_var(set, xset::var::menu_label, "_Name");
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset_get(xset::name::sortby_size);
        xset_set_var(set, xset::var::menu_label, "_Size");
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset_get(xset::name::sortby_bytes);
        xset_set_var(set, xset::var::menu_label, "_Size in Bytes");
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset_get(xset::name::sortby_type);
        xset_set_var(set, xset::var::menu_label, "_Type");
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset_get(xset::name::sortby_mime);
        xset_set_var(set, xset::var::menu_label, "_MIME Type");
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset_get(xset::name::sortby_perm);
        xset_set_var(set, xset::var::menu_label, "_Permissions");
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset_get(xset::name::sortby_owner);
        xset_set_var(set, xset::var::menu_label, "_Owner");
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset_get(xset::name::sortby_group);
        xset_set_var(set, xset::var::menu_label, "_Group");
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset_get(xset::name::sortby_atime);
        xset_set_var(set, xset::var::menu_label, "_Date Accessed");
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset_get(xset::name::sortby_btime);
        xset_set_var(set, xset::var::menu_label, "_Date Created");
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset_get(xset::name::sortby_ctime);
        xset_set_var(set, xset::var::menu_label, "_Date Metadata Changed");
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset_get(xset::name::sortby_mtime);
        xset_set_var(set, xset::var::menu_label, "_Date Modified");
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset_get(xset::name::sortby_ascend);
        xset_set_var(set, xset::var::menu_label, "_Ascending");
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset_get(xset::name::sortby_descend);
        xset_set_var(set, xset::var::menu_label, "_Descending");
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset_get(xset::name::sortx_natural);
        xset_set_var(set, xset::var::menu_label, "_Natural");
        set->menu.type = xset::set::menu_type::check;
    }

#if 0
    {
        const auto set = xset_get(xset::name::sortx_natural);
        xset_set_var(set, xset::var::menu_label, "Nat_ural");
        set->menu.type = xset::set::menu_type::check;
    }
#endif

    {
        const auto set = xset_get(xset::name::sortx_case);
        xset_set_var(set, xset::var::menu_label, "_Case Sensitive");
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset_get(xset::name::sortx_directories);
        xset_set_var(set, xset::var::menu_label, "Directories Fi_rst");
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset_get(xset::name::sortx_files);
        xset_set_var(set, xset::var::menu_label, "F_iles First");
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset_get(xset::name::sortx_mix);
        xset_set_var(set, xset::var::menu_label, "Mi_xed");
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset_get(xset::name::sortx_hidfirst);
        xset_set_var(set, xset::var::menu_label, "_Hidden First");
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset_get(xset::name::sortx_hidlast);
        xset_set_var(set, xset::var::menu_label, "Hidden _Last");
        set->menu.type = xset::set::menu_type::radio;
    }

    {
        const auto set = xset_get(xset::name::view_refresh);
        xset_set_var(set, xset::var::menu_label, "Re_fresh");
        xset_set_var(set, xset::var::icon, "gtk-refresh");
        set->keybinding.type = xset::set::keybinding_type::view;
    }

    {
        const auto set = xset_get(xset::name::path_seek);
        xset_set_var(set, xset::var::menu_label, "Auto See_k");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    // EDIT
    {
        const auto set = xset_get(xset::name::edit_cut);
        xset_set_var(set, xset::var::menu_label, "Cu_t");
        xset_set_var(set, xset::var::icon, "gtk-cut");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::edit_copy);
        xset_set_var(set, xset::var::menu_label, "_Copy");
        xset_set_var(set, xset::var::icon, "gtk-copy");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::edit_paste);
        xset_set_var(set, xset::var::menu_label, "_Paste");
        xset_set_var(set, xset::var::icon, "gtk-paste");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::edit_rename);
        xset_set_var(set, xset::var::menu_label, "_Rename");
        xset_set_var(set, xset::var::icon, "gtk-edit");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::edit_delete);
        xset_set_var(set, xset::var::menu_label, "_Delete");
        xset_set_var(set, xset::var::icon, "gtk-delete");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::edit_trash);
        xset_set_var(set, xset::var::menu_label, "_Trash");
        xset_set_var(set, xset::var::icon, "gtk-delete");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::edit_submenu);
        xset_set_var(set, xset::var::menu_label, "_Actions");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
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
                         });
        xset_set_var(set, xset::var::icon, "gtk-edit");
    }

    {
        const auto set = xset_get(xset::name::copy_name);
        xset_set_var(set, xset::var::menu_label, "Copy _Name");
        xset_set_var(set, xset::var::icon, "gtk-copy");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::copy_path);
        xset_set_var(set, xset::var::menu_label, "Copy _Path");
        xset_set_var(set, xset::var::icon, "gtk-copy");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::copy_parent);
        xset_set_var(set, xset::var::menu_label, "Copy Pa_rent");
        xset_set_var(set, xset::var::icon, "gtk-copy");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::paste_link);
        xset_set_var(set, xset::var::menu_label, "Paste _Link");
        xset_set_var(set, xset::var::icon, "gtk-paste");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::paste_target);
        xset_set_var(set, xset::var::menu_label, "Paste _Target");
        xset_set_var(set, xset::var::icon, "gtk-paste");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::paste_as);
        xset_set_var(set, xset::var::menu_label, "Paste _As");
        xset_set_var(set, xset::var::icon, "gtk-paste");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::copy_to);
        xset_set_var(set, xset::var::menu_label, "_Copy To");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
                             xset::name::copy_loc,
                             xset::name::copy_loc_last,
                             xset::name::separator,
                             xset::name::copy_tab,
                             xset::name::copy_panel,
                         });
    }

    {
        const auto set = xset_get(xset::name::copy_loc);
        xset_set_var(set, xset::var::menu_label, "L_ocation");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::copy_loc_last);
        xset_set_var(set, xset::var::menu_label, "L_ast Location");
        xset_set_var(set, xset::var::icon, "gtk-redo");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::copy_tab);
        xset_set_var(set, xset::var::menu_label, "_Tab");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
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
                         });
    }

    {
        xset_set(xset::name::copy_tab_prev, xset::var::menu_label, "_Prev");
        xset_set(xset::name::copy_tab_next, xset::var::menu_label, "_Next");
        xset_set(xset::name::copy_tab_1, xset::var::menu_label, "Tab _1");
        xset_set(xset::name::copy_tab_2, xset::var::menu_label, "Tab _2");
        xset_set(xset::name::copy_tab_3, xset::var::menu_label, "Tab _3");
        xset_set(xset::name::copy_tab_4, xset::var::menu_label, "Tab _4");
        xset_set(xset::name::copy_tab_5, xset::var::menu_label, "Tab _5");
        xset_set(xset::name::copy_tab_6, xset::var::menu_label, "Tab _6");
        xset_set(xset::name::copy_tab_7, xset::var::menu_label, "Tab _7");
        xset_set(xset::name::copy_tab_8, xset::var::menu_label, "Tab _8");
        xset_set(xset::name::copy_tab_9, xset::var::menu_label, "Tab _9");
        xset_set(xset::name::copy_tab_10, xset::var::menu_label, "Tab 1_0");
    }

    {
        const auto set = xset_get(xset::name::copy_panel);
        xset_set_var(set, xset::var::menu_label, "_Panel");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
                             xset::name::copy_panel_prev,
                             xset::name::copy_panel_next,
                             xset::name::copy_panel_1,
                             xset::name::copy_panel_2,
                             xset::name::copy_panel_3,
                             xset::name::copy_panel_4,
                         });
    }

    {
        xset_set(xset::name::copy_panel_prev, xset::var::menu_label, "_Prev");
        xset_set(xset::name::copy_panel_next, xset::var::menu_label, "_Next");
        xset_set(xset::name::copy_panel_1, xset::var::menu_label, "Panel _1");
        xset_set(xset::name::copy_panel_2, xset::var::menu_label, "Panel _2");
        xset_set(xset::name::copy_panel_3, xset::var::menu_label, "Panel _3");
        xset_set(xset::name::copy_panel_4, xset::var::menu_label, "Panel _4");
    }

    {
        const auto set = xset_get(xset::name::move_to);
        xset_set_var(set, xset::var::menu_label, "_Move To");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
                             xset::name::move_loc,
                             xset::name::move_loc_last,
                             xset::name::separator,
                             xset::name::move_tab,
                             xset::name::move_panel,
                         });
    }

    {
        const auto set = xset_get(xset::name::move_loc);
        xset_set_var(set, xset::var::menu_label, "_Location");
    }

    {
        const auto set = xset_get(xset::name::move_loc_last);
        xset_set_var(set, xset::var::menu_label, "L_ast Location");
        xset_set_var(set, xset::var::icon, "gtk-redo");
    }

    {
        const auto set = xset_get(xset::name::move_tab);
        xset_set_var(set, xset::var::menu_label, "_Tab");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
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
                         });
    }

    {
        xset_set(xset::name::move_tab_prev, xset::var::menu_label, "_Prev");
        xset_set(xset::name::move_tab_next, xset::var::menu_label, "_Next");
        xset_set(xset::name::move_tab_1, xset::var::menu_label, "Tab _1");
        xset_set(xset::name::move_tab_2, xset::var::menu_label, "Tab _2");
        xset_set(xset::name::move_tab_3, xset::var::menu_label, "Tab _3");
        xset_set(xset::name::move_tab_4, xset::var::menu_label, "Tab _4");
        xset_set(xset::name::move_tab_5, xset::var::menu_label, "Tab _5");
        xset_set(xset::name::move_tab_6, xset::var::menu_label, "Tab _6");
        xset_set(xset::name::move_tab_7, xset::var::menu_label, "Tab _7");
        xset_set(xset::name::move_tab_8, xset::var::menu_label, "Tab _8");
        xset_set(xset::name::move_tab_9, xset::var::menu_label, "Tab _9");
        xset_set(xset::name::move_tab_10, xset::var::menu_label, "Tab 1_0");
    }

    {
        const auto set = xset_get(xset::name::move_panel);
        xset_set_var(set, xset::var::menu_label, "_Panel");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
                             xset::name::move_panel_prev,
                             xset::name::move_panel_next,
                             xset::name::move_panel_1,
                             xset::name::move_panel_2,
                             xset::name::move_panel_3,
                             xset::name::move_panel_4,
                         });
    }

    {
        xset_set(xset::name::move_panel_prev, xset::var::menu_label, "_Prev");
        xset_set(xset::name::move_panel_next, xset::var::menu_label, "_Next");
        xset_set(xset::name::move_panel_1, xset::var::menu_label, "Panel _1");
        xset_set(xset::name::move_panel_2, xset::var::menu_label, "Panel _2");
        xset_set(xset::name::move_panel_3, xset::var::menu_label, "Panel _3");
        xset_set(xset::name::move_panel_4, xset::var::menu_label, "Panel _4");
    }

    {
        const auto set = xset_get(xset::name::edit_hide);
        xset_set_var(set, xset::var::menu_label, "_Hide");
    }

    {
        const auto set = xset_get(xset::name::select_all);
        xset_set_var(set, xset::var::menu_label, "_Select All");
        xset_set_var(set, xset::var::icon, "gtk-select-all");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::select_un);
        xset_set_var(set, xset::var::menu_label, "_Unselect All");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::select_invert);
        xset_set_var(set, xset::var::menu_label, "_Invert Selection");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::select_patt);
        xset_set_var(set, xset::var::menu_label, "S_elect By Pattern");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    // Properties
    {
        const auto set = xset_get(xset::name::con_prop);
        xset_set_var(set, xset::var::menu_label, "Propert_ies");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
                             xset::name::prop_info,
                             xset::name::prop_attr,
                             xset::name::prop_perm,
                             xset::name::prop_quick,
                         });
        xset_set_var(set, xset::var::icon, "gtk-properties");
    }

    {
        const auto set = xset_get(xset::name::prop_info);
        xset_set_var(set, xset::var::menu_label, "_Info");
        xset_set_var(set, xset::var::icon, "gtk-dialog-info");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::prop_attr);
        xset_set_var(set, xset::var::menu_label, "_Attributes");
        xset_set_var(set, xset::var::icon, "gtk-dialog-info");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::prop_perm);
        xset_set_var(set, xset::var::menu_label, "_Permissions");
        xset_set_var(set, xset::var::icon, "dialog-password");
        set->keybinding.type = xset::set::keybinding_type::editing;
    }

    {
        const auto set = xset_get(xset::name::prop_quick);
        xset_set_var(set, xset::var::menu_label, "_Quick");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
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
                         });
    }

    {
        xset_set(xset::name::perm_r, xset::var::menu_label, "r--------");
        xset_set(xset::name::perm_rw, xset::var::menu_label, "rw-------");
        xset_set(xset::name::perm_rwx, xset::var::menu_label, "rwx------");
        xset_set(xset::name::perm_r_r, xset::var::menu_label, "r--r-----");
        xset_set(xset::name::perm_rw_r, xset::var::menu_label, "rw-r-----");
        xset_set(xset::name::perm_rw_rw, xset::var::menu_label, "rw-rw----");
        xset_set(xset::name::perm_rwxr_x, xset::var::menu_label, "rwxr-x---");
        xset_set(xset::name::perm_rwxrwx, xset::var::menu_label, "rwxrwx---");
        xset_set(xset::name::perm_r_r_r, xset::var::menu_label, "r--r--r--");
        xset_set(xset::name::perm_rw_r_r, xset::var::menu_label, "rw-r--r--");
        xset_set(xset::name::perm_rw_rw_rw, xset::var::menu_label, "rw-rw-rw-");
        xset_set(xset::name::perm_rwxr_r, xset::var::menu_label, "rwxr--r--");
        xset_set(xset::name::perm_rwxr_xr_x, xset::var::menu_label, "rwxr-xr-x");
        xset_set(xset::name::perm_rwxrwxrwx, xset::var::menu_label, "rwxrwxrwx");
        xset_set(xset::name::perm_rwxrwxrwt, xset::var::menu_label, "rwxrwxrwt");
        xset_set(xset::name::perm_unstick, xset::var::menu_label, "-t");
        xset_set(xset::name::perm_stick, xset::var::menu_label, "+t");
    }

    {
        const auto set = xset_get(xset::name::perm_recurs);
        xset_set_var(set, xset::var::menu_label, "_Recursive");
        set->menu.type = xset::set::menu_type::submenu;
        xset_set_submenu(set,
                         {
                             xset::name::perm_go_w,
                             xset::name::perm_go_rwx,
                             xset::name::perm_ugo_w,
                             xset::name::perm_ugo_rx,
                             xset::name::perm_ugo_rwx,
                         });
    }

    {
        xset_set(xset::name::perm_go_w, xset::var::menu_label, "go-w");
        xset_set(xset::name::perm_go_rwx, xset::var::menu_label, "go-rwx");
        xset_set(xset::name::perm_ugo_w, xset::var::menu_label, "ugo+w");
        xset_set(xset::name::perm_ugo_rx, xset::var::menu_label, "ugo+rX");
        xset_set(xset::name::perm_ugo_rwx, xset::var::menu_label, "ugo+rwX");
    }

    // PANELS
    for (const panel_t p : PANELS)
    {
        {
            const auto set = xset_get_panel(p, xset::panel::show_toolbox);
            xset_set_var(set, xset::var::menu_label, "_Toolbar");
            set->menu.type = xset::set::menu_type::check;
            set->b = xset::set::enabled::yes;
            if (p != 1)
            {
                set->shared_key = xset_get(xset::name::panel1_show_toolbox);
            }
        }

        {
            const auto set = xset_get_panel(p, xset::panel::show_devmon);
            xset_set_var(set, xset::var::menu_label, "_Devices");
            set->menu.type = xset::set::menu_type::check;
            set->b = xset::set::enabled::unset;
            if (p != 1)
            {
                set->shared_key = xset_get(xset::name::panel1_show_devmon);
            }
        }

        {
            const auto set = xset_get_panel(p, xset::panel::show_dirtree);
            xset_set_var(set, xset::var::menu_label, "T_ree");
            set->menu.type = xset::set::menu_type::check;
            set->b = xset::set::enabled::yes;
            if (p != 1)
            {
                set->shared_key = xset_get(xset::name::panel1_show_dirtree);
            }
        }

        {
            const auto set = xset_get_panel(p, xset::panel::list_detailed);
            xset_set_var(set, xset::var::menu_label, "_Detailed");
            set->menu.type = xset::set::menu_type::radio;
            set->b = xset::set::enabled::yes;
            if (p != 1)
            {
                set->shared_key = xset_get(xset::name::panel1_list_detailed);
            }
        }

        {
            const auto set = xset_get_panel(p, xset::panel::list_icons);
            xset_set_var(set, xset::var::menu_label, "_Icons");
            set->menu.type = xset::set::menu_type::radio;
            if (p != 1)
            {
                set->shared_key = xset_get(xset::name::panel1_list_icons);
            }
        }

        {
            const auto set = xset_get_panel(p, xset::panel::list_compact);
            xset_set_var(set, xset::var::menu_label, "_Compact");
            set->menu.type = xset::set::menu_type::radio;
            if (p != 1)
            {
                set->shared_key = xset_get(xset::name::panel1_list_compact);
            }
        }

        {
            const auto set = xset_get_panel(p, xset::panel::list_large);
            xset_set_var(set, xset::var::menu_label, "_Large Icons");
            set->menu.type = xset::set::menu_type::check;
            if (p != 1)
            {
                set->shared_key = xset_get(xset::name::panel1_list_large);
            }
        }

        {
            const auto set = xset_get_panel(p, xset::panel::show_hidden);
            xset_set_var(set, xset::var::menu_label, "_Hidden Files");
            set->menu.type = xset::set::menu_type::check;
            if (p == 1)
            {
                set->keybinding.type = xset::set::keybinding_type::view;
            }
            if (p != 1)
            {
                set->shared_key = xset_get(xset::name::panel1_show_hidden);
            }
        }

        {
            const auto set = xset_get_panel(p, xset::panel::detcol_name);
            xset_set_var(set, xset::var::menu_label, "_Name");
            set->menu.type = xset::set::menu_type::check;
            set->b = xset::set::enabled::yes; // visible
            set->x = "0";                     // position
        }

        {
            const auto set = xset_get_panel(p, xset::panel::detcol_size);
            xset_set_var(set, xset::var::menu_label, "_Size");
            set->menu.type = xset::set::menu_type::check;
            set->b = xset::set::enabled::yes;
            set->x = "1";
            if (p != 1)
            {
                set->shared_key = xset_get(xset::name::panel1_detcol_size);
            }
        }

        {
            const auto set = xset_get_panel(p, xset::panel::detcol_bytes);
            xset_set_var(set, xset::var::menu_label, "_Bytes");
            set->menu.type = xset::set::menu_type::check;
            set->b = xset::set::enabled::yes;
            set->x = "2";
            if (p != 1)
            {
                set->shared_key = xset_get(xset::name::panel1_detcol_bytes);
            }
        }

        {
            const auto set = xset_get_panel(p, xset::panel::detcol_type);
            xset_set_var(set, xset::var::menu_label, "_Type");
            set->menu.type = xset::set::menu_type::check;
            set->x = "3";
            if (p != 1)
            {
                set->shared_key = xset_get(xset::name::panel1_detcol_type);
            }
        }

        {
            const auto set = xset_get_panel(p, xset::panel::detcol_mime);
            xset_set_var(set, xset::var::menu_label, "_MIME Type");
            set->menu.type = xset::set::menu_type::check;
            set->x = "4";
            if (p != 1)
            {
                set->shared_key = xset_get(xset::name::panel1_detcol_mime);
            }
        }

        {
            const auto set = xset_get_panel(p, xset::panel::detcol_perm);
            xset_set_var(set, xset::var::menu_label, "_Permissions");
            set->menu.type = xset::set::menu_type::check;
            set->x = "5";
            if (p != 1)
            {
                set->shared_key = xset_get(xset::name::panel1_detcol_perm);
            }
        }

        {
            const auto set = xset_get_panel(p, xset::panel::detcol_owner);
            xset_set_var(set, xset::var::menu_label, "_Owner");
            set->menu.type = xset::set::menu_type::check;
            set->x = "6";
            if (p != 1)
            {
                set->shared_key = xset_get(xset::name::panel1_detcol_owner);
            }
        }

        {
            const auto set = xset_get_panel(p, xset::panel::detcol_group);
            xset_set_var(set, xset::var::menu_label, "_Group");
            set->menu.type = xset::set::menu_type::check;
            set->x = "7";
            if (p != 1)
            {
                set->shared_key = xset_get(xset::name::panel1_detcol_group);
            }
        }

        {
            const auto set = xset_get_panel(p, xset::panel::detcol_atime);
            xset_set_var(set, xset::var::menu_label, "_Accessed");
            set->menu.type = xset::set::menu_type::check;
            set->x = "8";
            if (p != 1)
            {
                set->shared_key = xset_get(xset::name::panel1_detcol_atime);
            }
        }

        {
            const auto set = xset_get_panel(p, xset::panel::detcol_btime);
            xset_set_var(set, xset::var::menu_label, "_Created");
            set->menu.type = xset::set::menu_type::check;
            set->x = "9";
            if (p != 1)
            {
                set->shared_key = xset_get(xset::name::panel1_detcol_btime);
            }
        }

        {
            const auto set = xset_get_panel(p, xset::panel::detcol_ctime);
            xset_set_var(set, xset::var::menu_label, "_Metadata");
            set->menu.type = xset::set::menu_type::check;
            set->x = "10";
            if (p != 1)
            {
                set->shared_key = xset_get(xset::name::panel1_detcol_ctime);
            }
        }

        {
            const auto set = xset_get_panel(p, xset::panel::detcol_mtime);
            xset_set_var(set, xset::var::menu_label, "_Modified");
            set->menu.type = xset::set::menu_type::check;
            set->x = "11";
            if (p != 1)
            {
                set->shared_key = xset_get(xset::name::panel1_detcol_mtime);
            }
        }

        {
            const auto set = xset_get_panel(p, xset::panel::sort_extra);
            set->b = xset::set::enabled::yes; // sort_natural
            set->x =
                std::format("{}", magic_enum::enum_integer(xset::set::enabled::no)); // sort_case
            set->y = "1"; // ptk::file_list::sort_dir::PTK_LIST_SORT_DIR_FIRST
            set->z =
                std::format("{}",
                            magic_enum::enum_integer(xset::set::enabled::yes)); // sort_hidden_first
        }

        {
            const auto set = xset_get_panel(p, xset::panel::book_fol);
            xset_set_var(set, xset::var::menu_label, "Follow _Dir");
            set->menu.type = xset::set::menu_type::check;
            set->b = xset::set::enabled::yes;
            if (p != 1)
            {
                set->shared_key = xset_get(xset::name::panel1_book_fol);
            }
        }
    }

    // speed
    {
        const auto set = xset_get(xset::name::book_newtab);
        xset_set_var(set, xset::var::menu_label, "_New Tab");
        set->menu.type = xset::set::menu_type::check;
    }

    {
        const auto set = xset_get(xset::name::book_single);
        xset_set_var(set, xset::var::menu_label, "_Single Click");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset_get(xset::name::dev_newtab);
        xset_set_var(set, xset::var::menu_label, "_New Tab");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }

    {
        const auto set = xset_get(xset::name::dev_single);
        xset_set_var(set, xset::var::menu_label, "_Single Click");
        set->menu.type = xset::set::menu_type::check;
        set->b = xset::set::enabled::yes;
    }
}

static void
def_key(const std::span<const xset_t> keysets, xset::name name, u32 key, u32 keymod) noexcept
{
    const xset_t set = xset_get(name);

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
            ztd::logger::warn("Duplicate keybinding: {}, {}", set->name, keyset->name);
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
    for (const xset_t& set : xsets)
    {
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
    def_key(keysets, xset::name::go_default, GDK_KEY_Escape, 0);
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
