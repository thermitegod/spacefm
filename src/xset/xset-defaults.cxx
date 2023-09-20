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

#include <vector>

#include <cassert>

#include <magic_enum.hpp>

#include <gtk/gtk.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "xset/xset.hxx"

static std::vector<xset_t> keysets;

void
xset_defaults()
{
    xset_t set;

    // separator
    set = xset_get(xset::name::separator);
    set->menu_style = xset::menu::sep;

    // dev menu
    set = xset_get(xset::name::dev_menu_remove);
    xset_set_var(set, xset::var::menu_label, "Remo_ve / Eject");
    xset_set_var(set, xset::var::icn, "gtk-disconnect");

    set = xset_get(xset::name::dev_menu_unmount);
    xset_set_var(set, xset::var::menu_label, "_Unmount");
    xset_set_var(set, xset::var::icn, "gtk-remove");

    set = xset_get(xset::name::dev_menu_open);
    xset_set_var(set, xset::var::menu_label, "_Open");
    xset_set_var(set, xset::var::icn, "gtk-open");

    set = xset_get(xset::name::dev_menu_tab);
    xset_set_var(set, xset::var::menu_label, "Open In _Tab");
    xset_set_var(set, xset::var::icn, "gtk-add");

    set = xset_get(xset::name::dev_menu_mount);
    xset_set_var(set, xset::var::menu_label, "_Mount");
    xset_set_var(set, xset::var::icn, "drive-removable-media");

    set = xset_get(xset::name::dev_menu_mark);
    xset_set_var(set, xset::var::menu_label, "_Bookmark");
    xset_set_var(set, xset::var::icn, "gtk-add");

    set = xset_get(xset::name::dev_menu_settings);
    xset_set_var(set, xset::var::menu_label, "Setti_ngs");
    xset_set_var(set, xset::var::icn, "gtk-properties");
    set->menu_style = xset::menu::submenu;

    // dev settings
    set = xset_get(xset::name::dev_show);
    xset_set_var(set, xset::var::menu_label, "S_how");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set,
                 xset::var::desc,
                 "dev_show_internal_drives dev_show_empty dev_show_partition_tables dev_show_net "
                 "dev_show_file dev_ignore_udisks_hide dev_show_hide_volumes dev_dispname");

    set = xset_get(xset::name::dev_show_internal_drives);
    xset_set_var(set, xset::var::menu_label, "_Internal Drives");
    set->menu_style = xset::menu::check;
    set->b = geteuid() == 0 ? xset::b::xtrue : xset::b::xfalse;

    set = xset_get(xset::name::dev_show_empty);
    xset_set_var(set, xset::var::menu_label, "_Empty Drives");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue; // geteuid() == 0 ? xset::b::XSET_B_TRUE : xset::b::XSET_B_UNSET;

    set = xset_get(xset::name::dev_show_partition_tables);
    xset_set_var(set, xset::var::menu_label, "_Partition Tables");
    set->menu_style = xset::menu::check;

    set = xset_get(xset::name::dev_show_net);
    xset_set_var(set, xset::var::menu_label, "Mounted _Networks");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;

    set = xset_get(xset::name::dev_show_file);
    xset_set_var(set, xset::var::menu_label, "Mounted _Other");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;

    set = xset_get(xset::name::dev_show_hide_volumes);
    xset_set_var(set, xset::var::menu_label, "_Volumes...");
    xset_set_var(set, xset::var::title, "Show/Hide Volumes");
    xset_set_var(
        set,
        xset::var::desc,
        "To force showing or hiding of some volumes, overriding other settings, you can specify "
        "the devices, volume labels, or device IDs in the space-separated list "
        "below.\n\nExample:  +/dev/sdd1 -Label With Space +ata-OCZ-part4\nThis would cause "
        "/dev/sdd1 and the OCZ device to be shown, and the volume with label \"Label With "
        "Space\" to be hidden.\n\nThere must be a space between entries and a plus or minus sign "
        "directly before each item.  This list is case-sensitive.\n\n");

    set = xset_get(xset::name::dev_ignore_udisks_hide);
    xset_set_var(set, xset::var::menu_label, "Ignore _Hide Policy");
    set->menu_style = xset::menu::check;

    set = xset_get(xset::name::dev_dispname);
    xset_set_var(set, xset::var::menu_label, "_Display Name");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::title, "Set Display Name Format");
    xset_set_var(set,
                 xset::var::desc,
                 "Enter device display name format:\n\nUse:\n\t%%v\tdevice filename (eg "
                 "sdd1)\n\t%%s\ttotal size (eg 800G)\n\t%%t\tfstype (eg ext4)\n\t%%l\tvolume "
                 "label (eg Label or [no media])\n\t%%m\tmount point if mounted, or "
                 "---\n\t%%i\tdevice ID\n\t%%n\tmajor:minor device numbers (eg 15:3)\n");
    xset_set_var(set, xset::var::s, "%v %s %l %m");
    xset_set_var(set, xset::var::z, "%v %s %l %m");
    xset_set_var(set, xset::var::icn, "gtk-edit");

    set = xset_get(xset::name::dev_menu_auto);
    xset_set_var(set, xset::var::menu_label, "_Auto Mount");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set,
                 xset::var::desc,
                 "dev_automount_optical dev_automount_removable dev_ignore_udisks_nopolicy "
                 "dev_automount_volumes dev_automount_dirs dev_auto_open dev_unmount_quit");

    set = xset_get(xset::name::dev_automount_optical);
    xset_set_var(set, xset::var::menu_label, "Mount _Optical");
    set->b = geteuid() == 0 ? xset::b::xfalse : xset::b::xtrue;
    set->menu_style = xset::menu::check;

    set = xset_get(xset::name::dev_automount_removable);
    xset_set_var(set, xset::var::menu_label, "_Mount Removable");
    set->b = geteuid() == 0 ? xset::b::xfalse : xset::b::xtrue;
    set->menu_style = xset::menu::check;

    set = xset_get(xset::name::dev_automount_volumes);
    xset_set_var(set, xset::var::menu_label, "Mount _Volumes...");
    xset_set_var(set, xset::var::title, "Auto-Mount Volumes");
    xset_set_var(
        set,
        xset::var::desc,
        "To force or prevent automounting of some volumes, overriding other settings, you can "
        "specify the devices, volume labels, or device IDs in the space-separated list "
        "below.\n\nExample:  +/dev/sdd1 -Label With Space +ata-OCZ-part4\nThis would cause "
        "/dev/sdd1 and the OCZ device to be auto-mounted when detected, and the volume with "
        "label \"Label With Space\" to be ignored.\n\nThere must be a space between entries and "
        "a plus or minus sign directly before each item.  This list is case-sensitive.\n\n");

    set = xset_get(xset::name::dev_automount_dirs);
    xset_set_var(set, xset::var::menu_label, "Mount _Dirs...");
    xset_set_var(set, xset::var::title, "Automatic Mount Point Dirs");
    set->menu_style = xset::menu::string;
    xset_set_var(
        set,
        xset::var::desc,
        "Enter the directory where SpaceFM should automatically create mount point directories "
        "for fuse and similar filesystems (%%a in handler commands).  This directory must be "
        "user-writable (do NOT use /media), and empty subdirectories will be removed.  If left "
        "blank, ~/.cache/spacefm/ (or $XDG_CACHE_HOME/spacefm/) is used.  The following "
        "variables are recognized: $USER $UID $HOME $XDG_RUNTIME_DIR $XDG_CACHE_HOME\n\nNote "
        "that some handlers or mount programs may not obey this setting.\n");

    set = xset_get(xset::name::dev_auto_open);
    xset_set_var(set, xset::var::menu_label, "Open _Tab");
    set->b = xset::b::xtrue;
    set->menu_style = xset::menu::check;

    set = xset_get(xset::name::dev_unmount_quit);
    xset_set_var(set, xset::var::menu_label, "_Unmount On Exit");
    set->b = xset::b::unset;
    set->menu_style = xset::menu::check;

    set = xset_get(xset::name::dev_exec);
    xset_set_var(set, xset::var::menu_label, "Auto _Run");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set,
                 xset::var::desc,
                 "dev_exec_fs dev_exec_audio dev_exec_video separator dev_exec_insert "
                 "dev_exec_unmount dev_exec_remove");
    xset_set_var(set, xset::var::icn, "gtk-execute");

    set = xset_get(xset::name::dev_exec_fs);
    xset_set_var(set, xset::var::menu_label, "On _Mount");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::title, "Auto Run On Mount");
    xset_set_var(set,
                 xset::var::desc,
                 "Enter program or fish command line to be run automatically after a removable "
                 "drive or data disc is auto-mounted:\n\nUse:\n\t%%v\tdevice (eg "
                 "/dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_get(xset::name::dev_exec_audio);
    xset_set_var(set, xset::var::menu_label, "On _Audio CD");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::title, "Auto Run On Audio CD");
    xset_set_var(set,
                 xset::var::desc,
                 "Enter program or fish command line to be run automatically when an audio CD is "
                 "inserted in a qualified device:\n\nUse:\n\t%%v\tdevice (eg "
                 "/dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_get(xset::name::dev_exec_video);
    xset_set_var(set, xset::var::menu_label, "On _Video DVD");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::title, "Auto Run On Video DVD");
    xset_set_var(set,
                 xset::var::desc,
                 "Enter program or fish command line to be run automatically when a video DVD is "
                 "auto-mounted:\n\nUse:\n\t%%v\tdevice (eg /dev/sda1)\n\t%%l\tdevice "
                 "label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_get(xset::name::dev_exec_insert);
    xset_set_var(set, xset::var::menu_label, "On _Insert");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::title, "Auto Run On Insert");
    xset_set_var(set,
                 xset::var::desc,
                 "Enter program or fish command line to be run automatically when any device is "
                 "inserted:\n\nUse:\n\t%%v\tdevice added (eg /dev/sda1)\n\t%%l\tdevice "
                 "label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_get(xset::name::dev_exec_unmount);
    xset_set_var(set, xset::var::menu_label, "On _Unmount");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::title, "Auto Run On Unmount");
    xset_set_var(set,
                 xset::var::desc,
                 "Enter program or fish command line to be run automatically when any device is "
                 "unmounted by any means:\n\nUse:\n\t%%v\tdevice unmounted (eg "
                 "/dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_get(xset::name::dev_exec_remove);
    xset_set_var(set, xset::var::menu_label, "On _Remove");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::title, "Auto Run On Remove");
    xset_set_var(
        set,
        xset::var::desc,
        "Enter program or fish command line to be run automatically when any device is removed "
        "(ejection of media does not qualify):\n\nUse:\n\t%%v\tdevice removed (eg "
        "/dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_get(xset::name::dev_ignore_udisks_nopolicy);
    xset_set_var(set, xset::var::menu_label, "Ignore _No Policy");
    set->menu_style = xset::menu::check;

    set = xset_get(xset::name::dev_change);
    xset_set_var(set, xset::var::menu_label, "_Change Detection");
    xset_set_var(
        set,
        xset::var::desc,
        "Enter your comma- or space-separated list of filesystems which should NOT be monitored "
        "for file changes.  This setting only affects non-block devices (such as nfs or fuse), "
        "and is usually used to prevent SpaceFM becoming unresponsive with network filesystems.  "
        "Loading of thumbnails and subdirectory sizes will also be disabled.");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::title, "Change Detection Blacklist");
    xset_set_var(set, xset::var::icn, "gtk-edit");
    set->s = "cifs curlftpfs ftpfs fuse.sshfs nfs smbfs";
    set->z = set->s;

    // dev icons
    set = xset_get(xset::name::dev_icon);
    xset_set_var(set, xset::var::menu_label, "_Icon");
    set->menu_style = xset::menu::submenu;
    xset_set_var(
        set,
        xset::var::desc,
        "dev_icon_internal_mounted dev_icon_internal_unmounted separator dev_icon_remove_mounted "
        "dev_icon_remove_unmounted separator dev_icon_optical_mounted dev_icon_optical_media "
        "dev_icon_optical_nomedia dev_icon_audiocd separator dev_icon_floppy_mounted "
        "dev_icon_floppy_unmounted separator dev_icon_network dev_icon_file");

    set = xset_get(xset::name::dev_icon_audiocd);
    xset_set_var(set, xset::var::menu_label, "Audio CD");
    set->menu_style = xset::menu::icon;
    xset_set_var(set, xset::var::icn, "gtk-cdrom");

    set = xset_get(xset::name::dev_icon_optical_mounted);
    xset_set_var(set, xset::var::menu_label, "Optical Mounted");
    set->menu_style = xset::menu::icon;
    xset_set_var(set, xset::var::icn, "gtk-cdrom");

    set = xset_get(xset::name::dev_icon_optical_media);
    xset_set_var(set, xset::var::menu_label, "Optical Has Media");
    set->menu_style = xset::menu::icon;
    xset_set_var(set, xset::var::icn, "gtk-yes");

    set = xset_get(xset::name::dev_icon_optical_nomedia);
    xset_set_var(set, xset::var::menu_label, "Optical No Media");
    set->menu_style = xset::menu::icon;
    xset_set_var(set, xset::var::icn, "gtk-close");

    set = xset_get(xset::name::dev_icon_floppy_mounted);
    xset_set_var(set, xset::var::menu_label, "Floppy Mounted");
    set->menu_style = xset::menu::icon;
    xset_set_var(set, xset::var::icn, "gtk-floppy");

    set = xset_get(xset::name::dev_icon_floppy_unmounted);
    xset_set_var(set, xset::var::menu_label, "Floppy Unmounted");
    set->menu_style = xset::menu::icon;
    xset_set_var(set, xset::var::icn, "gtk-floppy");

    set = xset_get(xset::name::dev_icon_remove_mounted);
    xset_set_var(set, xset::var::menu_label, "Removable Mounted");
    set->menu_style = xset::menu::icon;
    xset_set_var(set, xset::var::icn, "gtk-add");

    set = xset_get(xset::name::dev_icon_remove_unmounted);
    xset_set_var(set, xset::var::menu_label, "Removable Unmounted");
    set->menu_style = xset::menu::icon;
    xset_set_var(set, xset::var::icn, "gtk-remove");

    set = xset_get(xset::name::dev_icon_internal_mounted);
    xset_set_var(set, xset::var::menu_label, "Internal Mounted");
    set->menu_style = xset::menu::icon;
    xset_set_var(set, xset::var::icn, "gtk-open");

    set = xset_get(xset::name::dev_icon_internal_unmounted);
    xset_set_var(set, xset::var::menu_label, "Internal Unmounted");
    set->menu_style = xset::menu::icon;
    xset_set_var(set, xset::var::icn, "gtk-harddisk");

    set = xset_get(xset::name::dev_icon_network);
    xset_set_var(set, xset::var::menu_label, "Mounted Network");
    set->menu_style = xset::menu::icon;
    xset_set_var(set, xset::var::icn, "gtk-network");

    set = xset_get(xset::name::dev_icon_file);
    xset_set_var(set, xset::var::menu_label, "Mounted Other");
    set->menu_style = xset::menu::icon;
    xset_set_var(set, xset::var::icn, "gtk-file");

    set = xset_get(xset::name::book_open);
    xset_set_var(set, xset::var::menu_label, "_Open");
    xset_set_var(set, xset::var::icn, "gtk-open");

    set = xset_get(xset::name::book_settings);
    xset_set_var(set, xset::var::menu_label, "_Settings");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set, xset::var::icn, "gtk-properties");

    set = xset_get(xset::name::book_icon);
    xset_set_var(set, xset::var::menu_label, "Bookmark _Icon");
    set->menu_style = xset::menu::icon;
    // do not set a default icon for book_icon

    set = xset_get(xset::name::book_menu_icon);
    xset_set_var(set, xset::var::menu_label, "Sub_menu Icon");
    set->menu_style = xset::menu::icon;
    // do not set a default icon for book_menu_icon

    set = xset_get(xset::name::book_add);
    xset_set_var(set, xset::var::menu_label, "New _Bookmark");
    xset_set_var(set, xset::var::icn, "gtk-jump-to");

    set = xset_get(xset::name::main_book);
    xset_set_var(set, xset::var::menu_label, "_Bookmarks");
    xset_set_var(set, xset::var::icn, "gtk-directory");
    set->menu_style = xset::menu::submenu;

    // Fonts
    set = xset_get(xset::name::font_general);
    xset_set_var(set, xset::var::s, "Monospace 9");

    set = xset_get(xset::name::font_view_icon);
    xset_set_var(set, xset::var::s, "Monospace 9");

    set = xset_get(xset::name::font_view_compact);
    xset_set_var(set, xset::var::s, "Monospace 9");

    // Rename/Move Dialog
    set = xset_get(xset::name::move_name);
    xset_set_var(set, xset::var::menu_label, "_Name");
    set->menu_style = xset::menu::check;

    set = xset_get(xset::name::move_filename);
    xset_set_var(set, xset::var::menu_label, "F_ilename");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;

    set = xset_get(xset::name::move_parent);
    xset_set_var(set, xset::var::menu_label, "_Parent");
    set->menu_style = xset::menu::check;

    set = xset_get(xset::name::move_path);
    xset_set_var(set, xset::var::menu_label, "P_ath");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;

    set = xset_get(xset::name::move_type);
    xset_set_var(set, xset::var::menu_label, "Typ_e");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;

    set = xset_get(xset::name::move_target);
    xset_set_var(set, xset::var::menu_label, "Ta_rget");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;

    set = xset_get(xset::name::move_template);
    xset_set_var(set, xset::var::menu_label, "Te_mplate");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;

    set = xset_get(xset::name::move_option);
    xset_set_var(set, xset::var::menu_label, "_Option");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set, xset::var::desc, "move_copy move_link move_copyt move_linkt move_as_root");

    set = xset_get(xset::name::move_copy);
    xset_set_var(set, xset::var::menu_label, "_Copy");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;

    set = xset_get(xset::name::move_link);
    xset_set_var(set, xset::var::menu_label, "_Link");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;

    set = xset_get(xset::name::move_copyt);
    xset_set_var(set, xset::var::menu_label, "Copy _Target");
    set->menu_style = xset::menu::check;

    set = xset_get(xset::name::move_linkt);
    xset_set_var(set, xset::var::menu_label, "Lin_k Target");
    set->menu_style = xset::menu::check;

    set = xset_get(xset::name::move_as_root);
    xset_set_var(set, xset::var::menu_label, "_As Root");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;

    set = xset_get(xset::name::move_dlg_help);
    xset_set_var(set, xset::var::menu_label, "_Help");
    xset_set_var(set, xset::var::icn, "gtk-help");

    set = xset_get(xset::name::move_dlg_confirm_create);
    xset_set_var(set, xset::var::menu_label, "_Confirm Create");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;

    // status bar
    set = xset_get(xset::name::status_middle);
    xset_set_var(set, xset::var::menu_label, "_Middle Click");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set, xset::var::desc, "status_name status_path status_info status_hide");

    set = xset_get(xset::name::status_name);
    xset_set_var(set, xset::var::menu_label, "Copy _Name");
    set->menu_style = xset::menu::radio;

    set = xset_get(xset::name::status_path);
    xset_set_var(set, xset::var::menu_label, "Copy _Path");
    set->menu_style = xset::menu::radio;

    set = xset_get(xset::name::status_info);
    xset_set_var(set, xset::var::menu_label, "File _Info");
    set->menu_style = xset::menu::radio;
    set->b = xset::b::xtrue;

    set = xset_get(xset::name::status_hide);
    xset_set_var(set, xset::var::menu_label, "_Hide Panel");
    set->menu_style = xset::menu::radio;

    // MAIN WINDOW MENUS

    // File
    set = xset_get(xset::name::main_new_window);
    xset_set_var(set, xset::var::menu_label, "New _Window");
    xset_set_var(set, xset::var::icn, "spacefm");

    set = xset_get(xset::name::main_root_window);
    xset_set_var(set, xset::var::menu_label, "R_oot Window");
    xset_set_var(set, xset::var::icn, "gtk-dialog-warning");

    set = xset_get(xset::name::main_search);
    xset_set_var(set, xset::var::menu_label, "_File Search");
    xset_set_var(set, xset::var::icn, "gtk-find");

    set = xset_get(xset::name::main_terminal);
    xset_set_var(set, xset::var::menu_label, "_Terminal");
    set->b = xset::b::unset; // discovery notification

    set = xset_get(xset::name::main_root_terminal);
    xset_set_var(set, xset::var::menu_label, "_Root Terminal");
    xset_set_var(set, xset::var::icn, "gtk-dialog-warning");

    // was previously used for 'Save Session' < 0.9.4 as xset::menu::NORMAL
    set = xset_get(xset::name::main_save_session);
    xset_set_var(set, xset::var::menu_label, "Open _URL");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::icn, "gtk-network");
    xset_set_var(set, xset::var::title, "Open URL");
    xset_set_var(set,
                 xset::var::desc,
                 "Enter URL in the "
                 "format:\n\tPROTOCOL://USERNAME:PASSWORD@HOST:PORT/SHARE\n\nExamples:\n\tftp://"
                 "mirrors.kernel.org\n\tsmb://user:pass@10.0.0.1:50/docs\n\tssh://"
                 "user@sys.domain\n\tmtp://\n\nIncluding a password is unsafe.  To bookmark a "
                 "URL, right-click on the mounted network in Devices and select Bookmark.\n");

    set = xset_get(xset::name::main_save_tabs);
    xset_set_var(set, xset::var::menu_label, "Save Ta_bs");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;

    set = xset_get(xset::name::main_exit);
    xset_set_var(set, xset::var::menu_label, "E_xit");
    xset_set_var(set, xset::var::icn, "gtk-quit");

    // View
    set = xset_get(xset::name::panel1_show);
    xset_set_var(set, xset::var::menu_label, "Panel _1");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;

    set = xset_get(xset::name::panel2_show);
    xset_set_var(set, xset::var::menu_label, "Panel _2");
    set->menu_style = xset::menu::check;

    set = xset_get(xset::name::panel3_show);
    xset_set_var(set, xset::var::menu_label, "Panel _3");
    set->menu_style = xset::menu::check;

    set = xset_get(xset::name::panel4_show);
    xset_set_var(set, xset::var::menu_label, "Panel _4");
    set->menu_style = xset::menu::check;

    set = xset_get(xset::name::main_focus_panel);
    xset_set_var(set, xset::var::menu_label, "F_ocus");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set,
                 xset::var::desc,
                 "panel_prev panel_next panel_hide panel_1 panel_2 panel_3 panel_4");
    xset_set_var(set, xset::var::icn, "gtk-go-forward");

    set = xset_get(xset::name::panel_prev);
    xset_set_var(set, xset::var::menu_label, "_Prev");

    set = xset_get(xset::name::panel_next);
    xset_set_var(set, xset::var::menu_label, "_Next");

    set = xset_get(xset::name::panel_hide);
    xset_set_var(set, xset::var::menu_label, "_Hide");

    set = xset_get(xset::name::panel_1);
    xset_set_var(set, xset::var::menu_label, "Panel _1");

    set = xset_get(xset::name::panel_2);
    xset_set_var(set, xset::var::menu_label, "Panel _2");

    set = xset_get(xset::name::panel_3);
    xset_set_var(set, xset::var::menu_label, "Panel _3");

    set = xset_get(xset::name::panel_4);
    xset_set_var(set, xset::var::menu_label, "Panel _4");

    set = xset_get(xset::name::main_auto);
    xset_set_var(set, xset::var::menu_label, "_Event Manager");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set, xset::var::desc, "auto_inst auto_win auto_pnl auto_tab evt_device");
    xset_set_var(set, xset::var::icn, "gtk-execute");

    set = xset_get(xset::name::auto_inst);
    xset_set_var(set, xset::var::menu_label, "_Instance");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set, xset::var::desc, "evt_start evt_exit");

    set = xset_get(xset::name::evt_start);
    xset_set_var(set, xset::var::menu_label, "_Startup");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::title, "Set Instance Startup Command");
    xset_set_var(set,
                 xset::var::desc,
                 "Enter program or fish command line to be run automatically when a SpaceFM "
                 "instance starts:\n\nUse:\n\t%%e\tevent type  (evt_start)\n");

    set = xset_get(xset::name::evt_exit);
    xset_set_var(set, xset::var::menu_label, "_Exit");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::title, "Set Instance Exit Command");
    xset_set_var(set,
                 xset::var::desc,
                 "Enter program or fish command line to be run automatically when a SpaceFM "
                 "instance exits:\n\nUse:\n\t%%e\tevent type  (evt_exit)\n");

    set = xset_get(xset::name::auto_win);
    xset_set_var(set, xset::var::menu_label, "_Window");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set,
                 xset::var::desc,
                 "evt_win_new evt_win_focus evt_win_move evt_win_click evt_win_key evt_win_close");

    set = xset_get(xset::name::evt_win_new);
    xset_set_var(set, xset::var::menu_label, "_New");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::title, "Set New Window Command");
    xset_set_var(
        set,
        xset::var::desc,
        "Enter program or fish command line to be run automatically whenever a new SpaceFM "
        "window is opened:\n\nUse:\n\t%%e\tevent type  (evt_win_new)\n\t%%w\twindow id  (see "
        "spacefm socket help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported fish variables (eg $fm_pwd, "
        "etc) "
        "can be used in this command.");

    set = xset_get(xset::name::evt_win_focus);
    xset_set_var(set, xset::var::menu_label, "_Focus");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::title, "Set Window Focus Command");
    xset_set_var(
        set,
        xset::var::desc,
        "Enter program or fish command line to be run automatically whenever a SpaceFM "
        "window gets focus:\n\nUse:\n\t%%e\tevent type  (evt_win_focus)\n\t%%w\twindow "
        "id  (see spacefm socket help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported fish variables "
        "(eg $fm_pwd, etc) can be used in this command.");

    set = xset_get(xset::name::evt_win_move);
    xset_set_var(set, xset::var::menu_label, "_Move/Resize");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::title, "Set Window Move/Resize Command");
    xset_set_var(
        set,
        xset::var::desc,
        "Enter program or fish command line to be run automatically whenever a SpaceFM window is "
        "moved or resized:\n\nUse:\n\t%%e\tevent type  (evt_win_move)\n\t%%w\twindow id  (see "
        "spacefm socket help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported fish variables (eg $fm_pwd, "
        "etc) "
        "can be used in this command.\n\nNote: This command may be run multiple times during "
        "resize.");

    set = xset_get(xset::name::evt_win_click);
    xset_set_var(set, xset::var::menu_label, "_Click");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::title, "Set Click Command");
    xset_set_var(
        set,
        xset::var::desc,
        "Enter program or fish command line to be run automatically whenever the mouse is "
        "clicked:\n\nUse:\n\t%%e\tevent type  (evt_win_click)\n\t%%w\twindow id  (see spacefm "
        "socket "
        "help)\n\t%%p\tpanel\n\t%%t\ttab\n\t%%b\tbutton  (mouse button pressed)\n\t%%m\tmodifier "
        " (modifier keys)\n\t%%f\tfocus  (element which received the click)\n\nExported fish "
        "variables (eg $fm_pwd, etc) can be used in this command when no asterisk prefix is "
        "used.\n\nPrefix your command with an asterisk (*) and conditionally return exit status "
        "0 to inhibit the default handler.  For example:\n*if [ \"%%b\" != \"2\" ];then exit 1; "
        "fi; spacefm -g --label \"\\nMiddle button was clicked in %%f\" --button ok &");

    set = xset_get(xset::name::evt_win_key);
    xset_set_var(set, xset::var::menu_label, "_Keypress");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::title, "Set Window Keypress Command");
    xset_set_var(
        set,
        xset::var::desc,
        "Enter program or fish command line to be run automatically whenever a key is "
        "pressed:\n\nUse:\n\t%%e\tevent type  (evt_win_key)\n\t%%w\twindow id  (see spacefm socket "
        "help)\n\t%%p\tpanel\n\t%%t\ttab\n\t%%k\tkey code  (key pressed)\n\t%%m\tmodifier  "
        "(modifier keys)\n\nExported fish variables (eg $fm_pwd, etc) can be used in this "
        "command when no asterisk prefix is used.\n\nPrefix your command with an asterisk (*) "
        "and conditionally return exit status 0 to inhibit the default handler.  For "
        "example:\n*if [ \"%%k\" != \"0xffc5\" ];then exit 1; fi; spacefm -g --label \"\\nKey "
        "F8 was pressed.\" --button ok &");

    set = xset_get(xset::name::evt_win_close);
    xset_set_var(set, xset::var::menu_label, "Cl_ose");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::title, "Set Window Close Command");
    xset_set_var(
        set,
        xset::var::desc,
        "Enter program or fish command line to be run automatically whenever a SpaceFM "
        "window is closed:\n\nUse:\n\t%%e\tevent type  (evt_win_close)\n\t%%w\twindow "
        "id  (see spacefm socket help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported fish variables "
        "(eg $fm_pwd, etc) can be used in this command.");

    set = xset_get(xset::name::auto_pnl);
    xset_set_var(set, xset::var::menu_label, "_Panel");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set, xset::var::desc, "evt_pnl_focus evt_pnl_show evt_pnl_sel");

    set = xset_get(xset::name::evt_pnl_focus);
    xset_set_var(set, xset::var::menu_label, "_Focus");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::title, "Set Panel Focus Command");
    xset_set_var(
        set,
        xset::var::desc,
        "Enter program or fish command line to be run automatically whenever a panel "
        "gets focus:\n\nUse:\n\t%%e\tevent type  (evt_pnl_focus)\n\t%%w\twindow id  "
        "(see spacefm socket help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported fish variables (eg "
        "$fm_pwd, etc) can be used in this command.");

    set = xset_get(xset::name::evt_pnl_show);
    xset_set_var(set, xset::var::menu_label, "_Show");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::title, "Set Panel Show Command");
    xset_set_var(
        set,
        xset::var::desc,
        "Enter program or fish command line to be run automatically whenever a panel or panel "
        "element is shown or hidden:\n\nUse:\n\t%%e\tevent type  (evt_pnl_show)\n\t%%w\twindow "
        "id  (see spacefm socket help)\n\t%%p\tpanel\n\t%%t\ttab\n\t%%f\tfocus  (element shown or "
        "hidden)\n\t%%v\tvisible  (1 or 0)\n\nExported fish variables (eg $fm_pwd, etc) can be "
        "used in this command.");

    set = xset_get(xset::name::evt_pnl_sel);
    xset_set_var(set, xset::var::menu_label, "S_elect");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::title, "Set Panel Select Command");
    xset_set_var(
        set,
        xset::var::desc,
        "Enter program or fish command line to be run automatically whenever the file selection "
        "changes:\n\nUse:\n\t%%e\tevent type  (evt_pnl_sel)\n\t%%w\twindow id  (see spacefm socket "
        "help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported fish variables (eg $fm_pwd, etc) can be "
        "used in this command.\n\nPrefix your command with an asterisk (*) and conditionally "
        "return exit status 0 to inhibit the default handler.");

    set = xset_get(xset::name::auto_tab);
    xset_set_var(set, xset::var::menu_label, "_Tab");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set, xset::var::desc, "evt_tab_new evt_tab_chdir evt_tab_focus evt_tab_close");

    set = xset_get(xset::name::evt_tab_new);
    xset_set_var(set, xset::var::menu_label, "_New");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::title, "Set New Tab Command");
    xset_set_var(set,
                 xset::var::desc,
                 "Enter program or fish command line to be run automatically whenever a new tab "
                 "is opened:\n\nUse:\n\t%%e\tevent type  (evt_tab_new)\n\t%%w\twindow id  (see "
                 "spacefm socket help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported fish variables (eg "
                 "$fm_pwd, etc) can be used in this command.");

    set = xset_get(xset::name::evt_tab_chdir);
    xset_set_var(set, xset::var::menu_label, "_Change Dir");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::title, "Set Tab Change Dir Command");
    xset_set_var(
        set,
        xset::var::desc,
        "Enter program or fish command line to be run automatically whenever a tab changes to a "
        "different directory:\n\nUse:\n\t%%e\tevent type  (evt_tab_chdir)\n\t%%w\twindow id  "
        "(see spacefm socket help)\n\t%%p\tpanel\n\t%%t\ttab\n\t%%d\tnew directory\n\nExported "
        "fish "
        "variables (eg $fm_pwd, etc) can be used in this command.");

    set = xset_get(xset::name::evt_tab_focus);
    xset_set_var(set, xset::var::menu_label, "_Focus");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::title, "Set Tab Focus Command");
    xset_set_var(set,
                 xset::var::desc,
                 "Enter program or fish command line to be run automatically whenever a tab gets "
                 "focus:\n\nUse:\n\t%%e\tevent type  (evt_tab_focus)\n\t%%w\twindow id  (see "
                 "spacefm socket help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported fish variables (eg "
                 "$fm_pwd, etc) can be used in this command.");

    set = xset_get(xset::name::evt_tab_close);
    xset_set_var(set, xset::var::menu_label, "_Close");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::title, "Set Tab Close Command");
    xset_set_var(set,
                 xset::var::desc,
                 "Enter program or fish command line to be run automatically whenever a tab is "
                 "closed:\n\nUse:\n\t%%e\tevent type  (evt_tab_close)\n\t%%w\twindow id  (see "
                 "spacefm socket help)\n\t%%p\tpanel\n\t%%t\tclosed tab");

    set = xset_get(xset::name::evt_device);
    xset_set_var(set, xset::var::menu_label, "_Device");
    set->menu_style = xset::menu::string;
    xset_set_var(set, xset::var::title, "Set Device Command");
    xset_set_var(set,
                 xset::var::desc,
                 "Enter program or fish command line to be run automatically whenever a device "
                 "state changes:\n\nUse:\n\t%%e\tevent type  (evt_device)\n\t%%f\tdevice "
                 "file\n\t%%v\tchange  (added|removed|changed)\n");

    set = xset_get(xset::name::main_title);
    xset_set_var(set, xset::var::menu_label, "Wi_ndow Title");
    set->menu_style = xset::menu::string;
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

    set = xset_get(xset::name::main_icon);
    xset_set_var(set, xset::var::menu_label, "_Window Icon");
    set->menu_style = xset::menu::icon;
    // Note: xset_text_dialog uses the title passed to know this is an
    // icon chooser, so it adds a Choose button.  If you change the title,
    // change xset_text_dialog.
    set->title = "Set Window Icon";
    set->desc = "Enter an icon name, icon file path, or stock item name:\n\nOr click "
                "Choose to select an icon.  Not all icons may work properly due to "
                "various issues.\n\nProvided alternate SpaceFM "
                "icons:\n\tspacefm-[48|128]-[cube|pyramid]-[blue|green|red]\n\tspacefm-"
                "48-folder-[blue|red]\n\nFor example: spacefm-48-pyramid-green";
    // x and y store global icon chooser dialog size

    set = xset_get(xset::name::main_full);
    xset_set_var(set, xset::var::menu_label, "_Fullscreen");
    set->menu_style = xset::menu::check;

    set = xset_get(xset::name::main_design_mode);
    xset_set_var(set, xset::var::menu_label, "_Design Mode");
    xset_set_var(set, xset::var::icn, "gtk-help");

    set = xset_get(xset::name::main_prefs);
    xset_set_var(set, xset::var::menu_label, "_Preferences");
    xset_set_var(set, xset::var::icn, "gtk-preferences");

    set = xset_get(xset::name::main_tool);
    xset_set_var(set, xset::var::menu_label, "_Tool");
    set->menu_style = xset::menu::submenu;

    set = xset_get(xset::name::root_bar); // in Preferences
    set->b = xset::b::xtrue;

    set = xset_get(xset::name::view_thumb);
    xset_set_var(set, xset::var::menu_label, "_Thumbnails (global)"); // in View|Panel View|Style
    set->menu_style = xset::menu::check;

    // Help
    set = xset_get(xset::name::main_about);
    xset_set_var(set, xset::var::menu_label, "_About");
    xset_set_var(set, xset::var::icn, "gtk-about");

    set = xset_get(xset::name::main_dev);
    xset_set_var(set, xset::var::menu_label, "_Show Devices");
    xset_set_var(set, xset::var::shared_key, "panel1_show_devmon");
    set->menu_style = xset::menu::check;

    // Tasks
    set = xset_get(xset::name::main_tasks);
    xset_set_var(set, xset::var::menu_label, "_Task Manager");
    set->menu_style = xset::menu::submenu;
    xset_set_var(
        set,
        xset::var::desc,
        "task_show_manager task_hide_manager separator task_columns task_popups task_errors "
        "task_queue");

    set = xset_get(xset::name::task_col_status);
    xset_set_var(set, xset::var::menu_label, "_Status");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;
    set->x = "0";   // column position
    set->y = "130"; // column width

    set = xset_get(xset::name::task_col_count);
    xset_set_var(set, xset::var::menu_label, "_Count");
    set->menu_style = xset::menu::check;
    set->x = "1";

    set = xset_get(xset::name::task_col_path);
    xset_set_var(set, xset::var::menu_label, "_Directory");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;
    set->x = "2";

    set = xset_get(xset::name::task_col_file);
    xset_set_var(set, xset::var::menu_label, "_Item");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;
    set->x = "3";

    set = xset_get(xset::name::task_col_to);
    xset_set_var(set, xset::var::menu_label, "_To");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;
    set->x = "4";

    set = xset_get(xset::name::task_col_progress);
    xset_set_var(set, xset::var::menu_label, "_Progress");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;
    set->x = "5";
    set->y = "100";

    set = xset_get(xset::name::task_col_total);
    xset_set_var(set, xset::var::menu_label, "T_otal");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;
    set->x = "6";
    set->y = "120";

    set = xset_get(xset::name::task_col_started);
    xset_set_var(set, xset::var::menu_label, "Sta_rted");
    set->menu_style = xset::menu::check;
    set->x = "7";

    set = xset_get(xset::name::task_col_elapsed);
    xset_set_var(set, xset::var::menu_label, "_Elapsed");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;
    set->x = "8";
    set->y = "70";

    set = xset_get(xset::name::task_col_curspeed);
    xset_set_var(set, xset::var::menu_label, "C_urrent Speed");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;
    set->x = "9";

    set = xset_get(xset::name::task_col_curest);
    xset_set_var(set, xset::var::menu_label, "Current Re_main");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;
    set->x = "10";

    set = xset_get(xset::name::task_col_avgspeed);
    xset_set_var(set, xset::var::menu_label, "_Average Speed");
    set->menu_style = xset::menu::check;
    set->x = "11";
    set->y = "60";

    set = xset_get(xset::name::task_col_avgest);
    xset_set_var(set, xset::var::menu_label, "A_verage Remain");
    set->menu_style = xset::menu::check;
    set->x = "12";
    set->y = "65";

    set = xset_get(xset::name::task_col_reorder);
    xset_set_var(set, xset::var::menu_label, "Reor_der");

    set = xset_get(xset::name::task_stop);
    xset_set_var(set, xset::var::menu_label, "_Stop");
    xset_set_var(set, xset::var::icn, "gtk-stop");

    set = xset_get(xset::name::task_pause);
    xset_set_var(set, xset::var::menu_label, "Pa_use");
    xset_set_var(set, xset::var::icn, "gtk-media-pause");

    set = xset_get(xset::name::task_que);
    xset_set_var(set, xset::var::menu_label, "_Queue");
    xset_set_var(set, xset::var::icn, "gtk-add");

    set = xset_get(xset::name::task_resume);
    xset_set_var(set, xset::var::menu_label, "_Resume");
    xset_set_var(set, xset::var::icn, "gtk-media-play");

    set = xset_get(xset::name::task_showout);
    xset_set_var(set, xset::var::menu_label, "Sho_w Output");

    set = xset_get(xset::name::task_all);
    xset_set_var(set, xset::var::menu_label, "_All Tasks");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set, xset::var::desc, "task_stop_all task_pause_all task_que_all task_resume_all");

    set = xset_get(xset::name::task_stop_all);
    xset_set_var(set, xset::var::menu_label, "_Stop");
    xset_set_var(set, xset::var::icn, "gtk-stop");

    set = xset_get(xset::name::task_pause_all);
    xset_set_var(set, xset::var::menu_label, "Pa_use");
    xset_set_var(set, xset::var::icn, "gtk-media-pause");

    set = xset_get(xset::name::task_que_all);
    xset_set_var(set, xset::var::menu_label, "_Queue");
    xset_set_var(set, xset::var::icn, "gtk-add");

    set = xset_get(xset::name::task_resume_all);
    xset_set_var(set, xset::var::menu_label, "_Resume");
    xset_set_var(set, xset::var::icn, "gtk-media-play");

    set = xset_get(xset::name::task_show_manager);
    xset_set_var(set, xset::var::menu_label, "Show _Manager");
    set->menu_style = xset::menu::radio;
    set->b = xset::b::xfalse;

    set = xset_get(xset::name::task_hide_manager);
    xset_set_var(set, xset::var::menu_label, "Auto-_Hide Manager");
    set->menu_style = xset::menu::radio;
    set->b = xset::b::xtrue;

    set = xset_get(xset::name::task_columns);
    xset_set_var(set, xset::var::menu_label, "_Columns");
    set->menu_style = xset::menu::submenu;
    xset_set_var(
        set,
        xset::var::desc,
        "task_col_count task_col_path task_col_file task_col_to task_col_progress task_col_total "
        "task_col_started task_col_elapsed task_col_curspeed task_col_curest task_col_avgspeed "
        "task_col_avgest separator task_col_reorder");

    set = xset_get(xset::name::task_popups);
    xset_set_var(set, xset::var::menu_label, "_Popups");
    set->menu_style = xset::menu::submenu;
    xset_set_var(
        set,
        xset::var::desc,
        "task_pop_all task_pop_top task_pop_above task_pop_stick separator task_pop_detail "
        "task_pop_over task_pop_err");

    set = xset_get(xset::name::task_pop_all);
    xset_set_var(set, xset::var::menu_label, "Popup _All Tasks");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xfalse;

    set = xset_get(xset::name::task_pop_top);
    xset_set_var(set, xset::var::menu_label, "Stay On _Top");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xfalse;

    set = xset_get(xset::name::task_pop_above);
    xset_set_var(set, xset::var::menu_label, "A_bove Others");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xfalse;

    set = xset_get(xset::name::task_pop_stick);
    xset_set_var(set, xset::var::menu_label, "All _Workspaces");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xfalse;

    set = xset_get(xset::name::task_pop_detail);
    xset_set_var(set, xset::var::menu_label, "_Detailed Stats");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xfalse;

    set = xset_get(xset::name::task_pop_over);
    xset_set_var(set, xset::var::menu_label, "_Overwrite Option");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;

    set = xset_get(xset::name::task_pop_err);
    xset_set_var(set, xset::var::menu_label, "_Error Option");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;

    set = xset_get(xset::name::task_errors);
    xset_set_var(set, xset::var::menu_label, "Err_ors");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set, xset::var::desc, "task_err_first task_err_any task_err_cont");

    set = xset_get(xset::name::task_err_first);
    xset_set_var(set, xset::var::menu_label, "Stop If _First");
    set->menu_style = xset::menu::radio;
    set->b = xset::b::xtrue;

    set = xset_get(xset::name::task_err_any);
    xset_set_var(set, xset::var::menu_label, "Stop On _Any");
    set->menu_style = xset::menu::radio;
    set->b = xset::b::xfalse;

    set = xset_get(xset::name::task_err_cont);
    xset_set_var(set, xset::var::menu_label, "_Continue");
    set->menu_style = xset::menu::radio;
    set->b = xset::b::xfalse;

    set = xset_get(xset::name::task_queue);
    xset_set_var(set, xset::var::menu_label, "Qu_eue");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set, xset::var::desc, "task_q_new task_q_smart task_q_pause");

    set = xset_get(xset::name::task_q_new);
    xset_set_var(set, xset::var::menu_label, "_Queue New Tasks");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;

    set = xset_get(xset::name::task_q_smart);
    xset_set_var(set, xset::var::menu_label, "_Smart Queue");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;

    set = xset_get(xset::name::task_q_pause);
    xset_set_var(set, xset::var::menu_label, "_Pause On Error");
    set->menu_style = xset::menu::check;

    // PANELS COMMON
    xset_set(xset::name::date_format, xset::var::s, "%Y-%m-%d %H:%M");

    set = xset_get(xset::name::con_open);
    xset_set_var(set, xset::var::menu_label, "_Open");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set, xset::var::icn, "gtk-open");

    set = xset_get(xset::name::open_execute);
    xset_set_var(set, xset::var::menu_label, "E_xecute");
    xset_set_var(set, xset::var::icn, "gtk-execute");

    set = xset_get(xset::name::open_edit);
    xset_set_var(set, xset::var::menu_label, "Edi_t");
    xset_set_var(set, xset::var::icn, "gtk-edit");

    set = xset_get(xset::name::open_edit_root);
    xset_set_var(set, xset::var::menu_label, "Edit As _Root");
    xset_set_var(set, xset::var::icn, "gtk-dialog-warning");

    set = xset_get(xset::name::open_other);
    xset_set_var(set, xset::var::menu_label, "_Choose...");
    xset_set_var(set, xset::var::icn, "gtk-open");

    set = xset_get(xset::name::open_all);
    xset_set_var(set, xset::var::menu_label, "Open With _Default"); // virtual

    set = xset_get(xset::name::open_in_tab);
    xset_set_var(set, xset::var::menu_label, "In _Tab");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set,
                 xset::var::desc,
                 "opentab_new opentab_prev opentab_next opentab_1 opentab_2 opentab_3 opentab_4 "
                 "opentab_5 opentab_6 opentab_7 opentab_8 opentab_9 opentab_10");

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

    set = xset_get(xset::name::open_in_panel);
    xset_set_var(set, xset::var::menu_label, "In _Panel");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set,
                 xset::var::desc,
                 "open_in_panelprev open_in_panelnext open_in_panel1 open_in_panel2 open_in_panel3 "
                 "open_in_panel4");

    xset_set(xset::name::open_in_panelprev, xset::var::menu_label, "_Prev");
    xset_set(xset::name::open_in_panelnext, xset::var::menu_label, "_Next");
    xset_set(xset::name::open_in_panel1, xset::var::menu_label, "Panel _1");
    xset_set(xset::name::open_in_panel2, xset::var::menu_label, "Panel _2");
    xset_set(xset::name::open_in_panel3, xset::var::menu_label, "Panel _3");
    xset_set(xset::name::open_in_panel4, xset::var::menu_label, "Panel _4");

    set = xset_get(xset::name::archive_extract);
    xset_set_var(set, xset::var::menu_label, "Archive Extract");
    xset_set_var(set, xset::var::icn, "gtk-convert");

    set = xset_get(xset::name::archive_extract_to);
    xset_set_var(set, xset::var::menu_label, "Archive Extract To");
    xset_set_var(set, xset::var::icn, "gtk-convert");

    set = xset_get(xset::name::archive_open);
    xset_set_var(set, xset::var::menu_label, "Archive Open");
    xset_set_var(set, xset::var::icn, "gtk-file");

    set = xset_get(xset::name::archive_default);
    xset_set_var(set, xset::var::menu_label, "_Archive Defaults");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set,
                 xset::var::desc,
                 "archive_default_open_with_app archive_default_extract "
                 "archive_default_extract_to archive_default_open_with_archiver");

    set = xset_get(xset::name::archive_default_open_with_app);
    xset_set_var(set, xset::var::menu_label, "Open With App");
    set->menu_style = xset::menu::radio;

    set = xset_get(xset::name::archive_default_extract);
    xset_set_var(set, xset::var::menu_label, "Extract");
    set->menu_style = xset::menu::radio;
    set->b = xset::b::xtrue;

    set = xset_get(xset::name::archive_default_extract_to);
    xset_set_var(set, xset::var::menu_label, "Extract To");
    set->menu_style = xset::menu::radio;

    set = xset_get(xset::name::archive_default_open_with_archiver);
    xset_set_var(set, xset::var::menu_label, "Open With Archiver");
    set->menu_style = xset::menu::radio;

    set = xset_get(xset::name::open_new);
    xset_set_var(set, xset::var::menu_label, "_New");
    set->menu_style = xset::menu::submenu;
    xset_set_var(
        set,
        xset::var::desc,
        "new_file new_directory new_link new_archive separator tab_new tab_new_here new_bookmark");
    xset_set_var(set, xset::var::icn, "gtk-new");

    set = xset_get(xset::name::new_file);
    xset_set_var(set, xset::var::menu_label, "_File");
    xset_set_var(set, xset::var::icn, "gtk-file");

    set = xset_get(xset::name::new_directory);
    xset_set_var(set, xset::var::menu_label, "Dir_ectory");
    xset_set_var(set, xset::var::icn, "gtk-directory");

    set = xset_get(xset::name::new_link);
    xset_set_var(set, xset::var::menu_label, "_Link");
    xset_set_var(set, xset::var::icn, "gtk-file");

    set = xset_get(xset::name::new_bookmark);
    xset_set_var(set, xset::var::menu_label, "_Bookmark");
    xset_set_var(set, xset::var::shared_key, "book_add");
    xset_set_var(set, xset::var::icn, "gtk-jump-to");

    set = xset_get(xset::name::new_archive);
    xset_set_var(set, xset::var::menu_label, "_Archive");
    xset_set_var(set, xset::var::icn, "gtk-save-as");

    set = xset_get(xset::name::arc_dlg);
    set->b = xset::b::xtrue; // Extract To - Create Subdirectory
    set->z = "1";            // Extract To - Write Access

    set = xset_get(xset::name::tab_new);
    xset_set_var(set, xset::var::menu_label, "_Tab");
    xset_set_var(set, xset::var::icn, "gtk-add");

    set = xset_get(xset::name::tab_new_here);
    xset_set_var(set, xset::var::menu_label, "Tab _Here");
    xset_set_var(set, xset::var::icn, "gtk-add");

    set = xset_get(xset::name::new_app);
    xset_set_var(set, xset::var::menu_label, "_Desktop Application");
    xset_set_var(set, xset::var::icn, "gtk-add");

    set = xset_get(xset::name::con_go);
    xset_set_var(set, xset::var::menu_label, "_Go");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set,
                 xset::var::desc,
                 "go_back go_forward go_up go_home go_default go_set_default edit_canon separator "
                 "go_tab go_focus");
    xset_set_var(set, xset::var::icn, "gtk-go-forward");

    set = xset_get(xset::name::go_back);
    xset_set_var(set, xset::var::menu_label, "_Back");
    xset_set_var(set, xset::var::icn, "gtk-go-back");

    set = xset_get(xset::name::go_forward);
    xset_set_var(set, xset::var::menu_label, "_Forward");
    xset_set_var(set, xset::var::icn, "gtk-go-forward");

    set = xset_get(xset::name::go_up);
    xset_set_var(set, xset::var::menu_label, "_Up");
    xset_set_var(set, xset::var::icn, "gtk-go-up");

    set = xset_get(xset::name::go_home);
    xset_set_var(set, xset::var::menu_label, "_Home");
    xset_set_var(set, xset::var::icn, "gtk-home");

    set = xset_get(xset::name::go_default);
    xset_set_var(set, xset::var::menu_label, "_Default");
    xset_set_var(set, xset::var::icn, "gtk-home");

    set = xset_get(xset::name::go_set_default);
    xset_set_var(set, xset::var::menu_label, "_Set Default");
    xset_set_var(set, xset::var::icn, "gtk-save");

    set = xset_get(xset::name::edit_canon);
    xset_set_var(set, xset::var::menu_label, "Re_al Path");

    set = xset_get(xset::name::go_focus);
    xset_set_var(set, xset::var::menu_label, "Fo_cus");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set,
                 xset::var::desc,
                 "focus_path_bar focus_filelist focus_dirtree focus_book focus_device");

    set = xset_get(xset::name::focus_path_bar);
    xset_set_var(set, xset::var::menu_label, "_Path Bar");
    xset_set_var(set, xset::var::icn, "gtk-dialog-question");

    set = xset_get(xset::name::focus_filelist);
    xset_set_var(set, xset::var::menu_label, "_File List");
    xset_set_var(set, xset::var::icn, "gtk-file");

    set = xset_get(xset::name::focus_dirtree);
    xset_set_var(set, xset::var::menu_label, "_Tree");
    xset_set_var(set, xset::var::icn, "gtk-directory");

    set = xset_get(xset::name::focus_book);
    xset_set_var(set, xset::var::menu_label, "_Bookmarks");
    xset_set_var(set, xset::var::icn, "gtk-jump-to");

    set = xset_get(xset::name::focus_device);
    xset_set_var(set, xset::var::menu_label, "De_vices");
    xset_set_var(set, xset::var::icn, "gtk-harddisk");

    set = xset_get(xset::name::go_tab);
    xset_set_var(set, xset::var::menu_label, "_Tab");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set,
                 xset::var::desc,
                 "tab_prev tab_next tab_restore tab_close tab_1 tab_2 tab_3 tab_4 tab_5 tab_6 "
                 "tab_7 tab_8 tab_9 tab_10");

    xset_set(xset::name::tab_prev, xset::var::menu_label, "_Prev");
    xset_set(xset::name::tab_next, xset::var::menu_label, "_Next");
    xset_set(xset::name::tab_restore, xset::var::menu_label, "_Restore");
    xset_set(xset::name::tab_close, xset::var::menu_label, "_Close");
    xset_set(xset::name::tab_1, xset::var::menu_label, "Tab _1");
    xset_set(xset::name::tab_2, xset::var::menu_label, "Tab _2");
    xset_set(xset::name::tab_3, xset::var::menu_label, "Tab _3");
    xset_set(xset::name::tab_4, xset::var::menu_label, "Tab _4");
    xset_set(xset::name::tab_5, xset::var::menu_label, "Tab _5");
    xset_set(xset::name::tab_6, xset::var::menu_label, "Tab _6");
    xset_set(xset::name::tab_7, xset::var::menu_label, "Tab _7");
    xset_set(xset::name::tab_8, xset::var::menu_label, "Tab _8");
    xset_set(xset::name::tab_9, xset::var::menu_label, "Tab _9");
    xset_set(xset::name::tab_10, xset::var::menu_label, "Tab 1_0");

    set = xset_get(xset::name::con_view);
    xset_set_var(set, xset::var::menu_label, "_View");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set, xset::var::icn, "gtk-preferences");

    set = xset_get(xset::name::view_list_style);
    xset_set_var(set, xset::var::menu_label, "Styl_e");
    set->menu_style = xset::menu::submenu;

    set = xset_get(xset::name::view_columns);
    xset_set_var(set, xset::var::menu_label, "C_olumns");
    set->menu_style = xset::menu::submenu;

    set = xset_get(xset::name::view_reorder_col);
    xset_set_var(set, xset::var::menu_label, "_Reorder");

    set = xset_get(xset::name::rubberband);
    xset_set_var(set, xset::var::menu_label, "_Rubberband Select");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;

    set = xset_get(xset::name::view_sortby);
    xset_set_var(set, xset::var::menu_label, "_Sort");
    set->menu_style = xset::menu::submenu;
    xset_set_var(
        set,
        xset::var::desc,
        "sortby_name sortby_size sortby_bytes sortby_type sortby_mime sortby_perm "
        "sortby_owner sortby_group sortby_atime sortby_btime sortby_ctime sortby_mtime "
        "separator sortby_ascend sortby_descend separator sortx_alphanum sortx_case separator "
        "sortx_directories sortx_files sortx_mix separator sortx_hidfirst sortx_hidlast");

    set = xset_get(xset::name::sortby_name);
    xset_set_var(set, xset::var::menu_label, "_Name");
    set->menu_style = xset::menu::radio;

    set = xset_get(xset::name::sortby_size);
    xset_set_var(set, xset::var::menu_label, "_Size");
    set->menu_style = xset::menu::radio;

    set = xset_get(xset::name::sortby_bytes);
    xset_set_var(set, xset::var::menu_label, "_Size in Bytes");
    set->menu_style = xset::menu::radio;

    set = xset_get(xset::name::sortby_type);
    xset_set_var(set, xset::var::menu_label, "_Type");
    set->menu_style = xset::menu::radio;

    set = xset_get(xset::name::sortby_mime);
    xset_set_var(set, xset::var::menu_label, "_MIME Type");
    set->menu_style = xset::menu::radio;

    set = xset_get(xset::name::sortby_perm);
    xset_set_var(set, xset::var::menu_label, "_Permissions");
    set->menu_style = xset::menu::radio;

    set = xset_get(xset::name::sortby_owner);
    xset_set_var(set, xset::var::menu_label, "_Owner");
    set->menu_style = xset::menu::radio;

    set = xset_get(xset::name::sortby_group);
    xset_set_var(set, xset::var::menu_label, "_Group");
    set->menu_style = xset::menu::radio;

    set = xset_get(xset::name::sortby_atime);
    xset_set_var(set, xset::var::menu_label, "_Date Accessed");
    set->menu_style = xset::menu::radio;

    set = xset_get(xset::name::sortby_btime);
    xset_set_var(set, xset::var::menu_label, "_Date Created");
    set->menu_style = xset::menu::radio;

    set = xset_get(xset::name::sortby_ctime);
    xset_set_var(set, xset::var::menu_label, "_Date Metadata Changed");
    set->menu_style = xset::menu::radio;

    set = xset_get(xset::name::sortby_mtime);
    xset_set_var(set, xset::var::menu_label, "_Date Modified");
    set->menu_style = xset::menu::radio;

    set = xset_get(xset::name::sortby_ascend);
    xset_set_var(set, xset::var::menu_label, "_Ascending");
    set->menu_style = xset::menu::radio;

    set = xset_get(xset::name::sortby_descend);
    xset_set_var(set, xset::var::menu_label, "_Descending");
    set->menu_style = xset::menu::radio;

    set = xset_get(xset::name::sortx_alphanum);
    xset_set_var(set, xset::var::menu_label, "_Alphanumeric");
    set->menu_style = xset::menu::check;

#if 0
    set = xset_get(xset::name::sortx_natural);
    xset_set_var(set, xset::var::MENU_LABEL, "Nat_ural");
    set->menu_style = xset::menu::CHECK;
#endif

    set = xset_get(xset::name::sortx_case);
    xset_set_var(set, xset::var::menu_label, "_Case Sensitive");
    set->menu_style = xset::menu::check;

    set = xset_get(xset::name::sortx_directories);
    xset_set_var(set, xset::var::menu_label, "Directories Fi_rst");
    set->menu_style = xset::menu::radio;

    set = xset_get(xset::name::sortx_files);
    xset_set_var(set, xset::var::menu_label, "F_iles First");
    set->menu_style = xset::menu::radio;

    set = xset_get(xset::name::sortx_mix);
    xset_set_var(set, xset::var::menu_label, "Mi_xed");
    set->menu_style = xset::menu::radio;

    set = xset_get(xset::name::sortx_hidfirst);
    xset_set_var(set, xset::var::menu_label, "_Hidden First");
    set->menu_style = xset::menu::radio;

    set = xset_get(xset::name::sortx_hidlast);
    xset_set_var(set, xset::var::menu_label, "Hidden _Last");
    set->menu_style = xset::menu::radio;

    set = xset_get(xset::name::view_refresh);
    xset_set_var(set, xset::var::menu_label, "Re_fresh");
    xset_set_var(set, xset::var::icn, "gtk-refresh");

    set = xset_get(xset::name::path_seek);
    xset_set_var(set, xset::var::menu_label, "Auto See_k");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;

    // EDIT
    set = xset_get(xset::name::edit_cut);
    xset_set_var(set, xset::var::menu_label, "Cu_t");
    xset_set_var(set, xset::var::icn, "gtk-cut");

    set = xset_get(xset::name::edit_copy);
    xset_set_var(set, xset::var::menu_label, "_Copy");
    xset_set_var(set, xset::var::icn, "gtk-copy");

    set = xset_get(xset::name::edit_paste);
    xset_set_var(set, xset::var::menu_label, "_Paste");
    xset_set_var(set, xset::var::icn, "gtk-paste");

    set = xset_get(xset::name::edit_rename);
    xset_set_var(set, xset::var::menu_label, "_Rename");
    xset_set_var(set, xset::var::icn, "gtk-edit");

    set = xset_get(xset::name::edit_delete);
    xset_set_var(set, xset::var::menu_label, "_Delete");
    xset_set_var(set, xset::var::icn, "gtk-delete");

    set = xset_get(xset::name::edit_trash);
    xset_set_var(set, xset::var::menu_label, "_Trash");
    xset_set_var(set, xset::var::icn, "gtk-delete");

    set = xset_get(xset::name::edit_submenu);
    xset_set_var(set, xset::var::menu_label, "_Actions");
    set->menu_style = xset::menu::submenu;
    xset_set_var(
        set,
        xset::var::desc,
        "copy_name copy_parent copy_path separator paste_link paste_target paste_as separator "
        "copy_to "
        "move_to edit_root edit_hide separator select_all select_patt select_invert select_un");
    xset_set_var(set, xset::var::icn, "gtk-edit");

    set = xset_get(xset::name::copy_name);
    xset_set_var(set, xset::var::menu_label, "Copy _Name");
    xset_set_var(set, xset::var::icn, "gtk-copy");

    set = xset_get(xset::name::copy_path);
    xset_set_var(set, xset::var::menu_label, "Copy _Path");
    xset_set_var(set, xset::var::icn, "gtk-copy");

    set = xset_get(xset::name::copy_parent);
    xset_set_var(set, xset::var::menu_label, "Copy Pa_rent");
    xset_set_var(set, xset::var::icn, "gtk-copy");

    set = xset_get(xset::name::paste_link);
    xset_set_var(set, xset::var::menu_label, "Paste _Link");
    xset_set_var(set, xset::var::icn, "gtk-paste");

    set = xset_get(xset::name::paste_target);
    xset_set_var(set, xset::var::menu_label, "Paste _Target");
    xset_set_var(set, xset::var::icn, "gtk-paste");

    set = xset_get(xset::name::paste_as);
    xset_set_var(set, xset::var::menu_label, "Paste _As");
    xset_set_var(set, xset::var::icn, "gtk-paste");

    set = xset_get(xset::name::copy_to);
    xset_set_var(set, xset::var::menu_label, "_Copy To");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set, xset::var::desc, "copy_loc copy_loc_last separator copy_tab copy_panel");

    set = xset_get(xset::name::copy_loc);
    xset_set_var(set, xset::var::menu_label, "L_ocation");

    set = xset_get(xset::name::copy_loc_last);
    xset_set_var(set, xset::var::menu_label, "L_ast Location");
    xset_set_var(set, xset::var::icn, "gtk-redo");

    set = xset_get(xset::name::copy_tab);
    xset_set_var(set, xset::var::menu_label, "_Tab");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set,
                 xset::var::desc,
                 "copy_tab_prev copy_tab_next copy_tab_1 copy_tab_2 copy_tab_3 copy_tab_4 "
                 "copy_tab_5 copy_tab_6 copy_tab_7 copy_tab_8 copy_tab_9 copy_tab_10");

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

    set = xset_get(xset::name::copy_panel);
    xset_set_var(set, xset::var::menu_label, "_Panel");
    set->menu_style = xset::menu::submenu;
    xset_set_var(
        set,
        xset::var::desc,
        "copy_panel_prev copy_panel_next copy_panel_1 copy_panel_2 copy_panel_3 copy_panel_4");

    xset_set(xset::name::copy_panel_prev, xset::var::menu_label, "_Prev");
    xset_set(xset::name::copy_panel_next, xset::var::menu_label, "_Next");
    xset_set(xset::name::copy_panel_1, xset::var::menu_label, "Panel _1");
    xset_set(xset::name::copy_panel_3, xset::var::menu_label, "Panel _2");
    xset_set(xset::name::copy_panel_3, xset::var::menu_label, "Panel _3");
    xset_set(xset::name::copy_panel_4, xset::var::menu_label, "Panel _4");

    set = xset_get(xset::name::move_to);
    xset_set_var(set, xset::var::menu_label, "_Move To");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set, xset::var::desc, "move_loc move_loc_last separator move_tab move_panel");

    set = xset_get(xset::name::move_loc);
    xset_set_var(set, xset::var::menu_label, "_Location");

    set = xset_get(xset::name::move_loc_last);
    xset_set_var(set, xset::var::menu_label, "L_ast Location");
    xset_set_var(set, xset::var::icn, "gtk-redo");

    set = xset_get(xset::name::move_tab);
    xset_set_var(set, xset::var::menu_label, "_Tab");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set,
                 xset::var::desc,
                 "move_tab_prev move_tab_next move_tab_1 move_tab_2 move_tab_3 move_tab_4 "
                 "move_tab_5 move_tab_6 move_tab_7 move_tab_8 move_tab_9 move_tab_10");

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

    set = xset_get(xset::name::move_panel);
    xset_set_var(set, xset::var::menu_label, "_Panel");
    set->menu_style = xset::menu::submenu;
    xset_set_var(
        set,
        xset::var::desc,
        "move_panel_prev move_panel_next move_panel_1 move_panel_2 move_panel_3 move_panel_4");

    xset_set(xset::name::move_panel_prev, xset::var::menu_label, "_Prev");
    xset_set(xset::name::move_panel_next, xset::var::menu_label, "_Next");
    xset_set(xset::name::move_panel_1, xset::var::menu_label, "Panel _1");
    xset_set(xset::name::move_panel_2, xset::var::menu_label, "Panel _2");
    xset_set(xset::name::move_panel_3, xset::var::menu_label, "Panel _3");
    xset_set(xset::name::move_panel_4, xset::var::menu_label, "Panel _4");

    set = xset_get(xset::name::edit_hide);
    xset_set_var(set, xset::var::menu_label, "_Hide");

    set = xset_get(xset::name::select_all);
    xset_set_var(set, xset::var::menu_label, "_Select All");
    xset_set_var(set, xset::var::icn, "gtk-select-all");

    set = xset_get(xset::name::select_un);
    xset_set_var(set, xset::var::menu_label, "_Unselect All");

    set = xset_get(xset::name::select_invert);
    xset_set_var(set, xset::var::menu_label, "_Invert Selection");

    set = xset_get(xset::name::select_patt);
    xset_set_var(set, xset::var::menu_label, "S_elect By Pattern");

    set = xset_get(xset::name::edit_root);
    xset_set_var(set, xset::var::menu_label, "R_oot");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set, xset::var::desc, "root_copy_loc root_move2 root_delete");
    xset_set_var(set, xset::var::icn, "gtk-dialog-warning");

    set = xset_get(xset::name::root_copy_loc);
    xset_set_var(set, xset::var::menu_label, "_Copy To");

    set = xset_get(xset::name::root_move2);
    xset_set_var(set, xset::var::menu_label, "Move _To");

    set = xset_get(xset::name::root_delete);
    xset_set_var(set, xset::var::menu_label, "_Delete");
    xset_set_var(set, xset::var::icn, "gtk-delete");

    // Properties
    set = xset_get(xset::name::con_prop);
    xset_set_var(set, xset::var::menu_label, "Propert_ies");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set, xset::var::desc, "");
    xset_set_var(set, xset::var::icn, "gtk-properties");

    set = xset_get(xset::name::prop_info);
    xset_set_var(set, xset::var::menu_label, "_Info");
    xset_set_var(set, xset::var::icn, "gtk-dialog-info");

    set = xset_get(xset::name::prop_perm);
    xset_set_var(set, xset::var::menu_label, "_Permissions");
    xset_set_var(set, xset::var::icn, "dialog-password");

    set = xset_get(xset::name::prop_quick);
    xset_set_var(set, xset::var::menu_label, "_Quick");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set,
                 xset::var::desc,
                 "perm_r perm_rw perm_rwx perm_r_r perm_rw_r perm_rw_rw perm_rwxr_x perm_rwxrwx "
                 "perm_r_r_r perm_rw_r_r perm_rw_rw_rw perm_rwxr_r perm_rwxr_xr_x perm_rwxrwxrwx "
                 "perm_rwxrwxrwt perm_unstick perm_stick perm_recurs");

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

    set = xset_get(xset::name::perm_recurs);
    xset_set_var(set, xset::var::menu_label, "_Recursive");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set, xset::var::desc, "perm_go_w perm_go_rwx perm_ugo_w perm_ugo_rx perm_ugo_rwx");

    xset_set(xset::name::perm_go_w, xset::var::menu_label, "go-w");
    xset_set(xset::name::perm_go_rwx, xset::var::menu_label, "go-rwx");
    xset_set(xset::name::perm_ugo_w, xset::var::menu_label, "ugo+w");
    xset_set(xset::name::perm_ugo_rx, xset::var::menu_label, "ugo+rX");
    xset_set(xset::name::perm_ugo_rwx, xset::var::menu_label, "ugo+rwX");

    set = xset_get(xset::name::prop_root);
    xset_set_var(set, xset::var::menu_label, "_Root");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set,
                 xset::var::desc,
                 "rperm_rw rperm_rwx rperm_rw_r rperm_rw_rw rperm_rwxr_x rperm_rwxrwx rperm_rw_r_r "
                 "rperm_rw_rw_rw rperm_rwxr_r rperm_rwxr_xr_x rperm_rwxrwxrwx rperm_rwxrwxrwt "
                 "rperm_unstick rperm_stick rperm_recurs rperm_own");
    xset_set_var(set, xset::var::icn, "gtk-dialog-warning");

    xset_set(xset::name::rperm_rw, xset::var::menu_label, "rw-------");
    xset_set(xset::name::rperm_rwx, xset::var::menu_label, "rwx------");
    xset_set(xset::name::rperm_rw_r, xset::var::menu_label, "rw-r-----");
    xset_set(xset::name::rperm_rw_rw, xset::var::menu_label, "rw-rw----");
    xset_set(xset::name::rperm_rwxr_x, xset::var::menu_label, "rwxr-x---");
    xset_set(xset::name::rperm_rwxrwx, xset::var::menu_label, "rwxrwx---");
    xset_set(xset::name::rperm_rw_r_r, xset::var::menu_label, "rw-r--r--");
    xset_set(xset::name::rperm_rw_rw_rw, xset::var::menu_label, "rw-rw-rw-");
    xset_set(xset::name::rperm_rwxr_r, xset::var::menu_label, "rwxr--r--");
    xset_set(xset::name::rperm_rwxr_xr_x, xset::var::menu_label, "rwxr-xr-x");
    xset_set(xset::name::rperm_rwxrwxrwx, xset::var::menu_label, "rwxrwxrwx");
    xset_set(xset::name::rperm_rwxrwxrwt, xset::var::menu_label, "rwxrwxrwt");
    xset_set(xset::name::rperm_unstick, xset::var::menu_label, "-t");
    xset_set(xset::name::rperm_stick, xset::var::menu_label, "+t");

    set = xset_get(xset::name::rperm_recurs);
    xset_set_var(set, xset::var::menu_label, "_Recursive");
    set->menu_style = xset::menu::submenu;
    xset_set_var(set,
                 xset::var::desc,
                 "rperm_go_w rperm_go_rwx rperm_ugo_w rperm_ugo_rx rperm_ugo_rwx");

    xset_set(xset::name::rperm_go_w, xset::var::menu_label, "go-w");
    xset_set(xset::name::rperm_go_rwx, xset::var::menu_label, "go-rwx");
    xset_set(xset::name::rperm_ugo_w, xset::var::menu_label, "ugo+w");
    xset_set(xset::name::rperm_ugo_rx, xset::var::menu_label, "ugo+rX");
    xset_set(xset::name::rperm_ugo_rwx, xset::var::menu_label, "ugo+rwX");

    set = xset_get(xset::name::rperm_own);
    xset_set_var(set, xset::var::menu_label, "_Owner");
    set->menu_style = xset::menu::submenu;
    xset_set_var(
        set,
        xset::var::desc,
        "own_myuser own_myuser_users own_user1 own_user1_users own_user2 own_user2_users own_root "
        "own_root_users own_root_myuser own_root_user1 own_root_user2 own_recurs");

    xset_set(xset::name::own_myuser, xset::var::menu_label, "myuser");
    xset_set(xset::name::own_myuser_users, xset::var::menu_label, "myuser:users");
    xset_set(xset::name::own_user1, xset::var::menu_label, "user1");
    xset_set(xset::name::own_user1_users, xset::var::menu_label, "user1:users");
    xset_set(xset::name::own_user2, xset::var::menu_label, "user2");
    xset_set(xset::name::own_user2_users, xset::var::menu_label, "user2:users");
    xset_set(xset::name::own_root, xset::var::menu_label, "root");
    xset_set(xset::name::own_root_users, xset::var::menu_label, "root:users");
    xset_set(xset::name::own_root_myuser, xset::var::menu_label, "root:myuser");
    xset_set(xset::name::own_root_user1, xset::var::menu_label, "root:user1");
    xset_set(xset::name::own_root_user2, xset::var::menu_label, "root:user2");

    set = xset_get(xset::name::own_recurs);
    xset_set_var(set, xset::var::menu_label, "_Recursive");
    set->menu_style = xset::menu::submenu;
    xset_set_var(
        set,
        xset::var::desc,
        "rown_myuser rown_myuser_users rown_user1 rown_user1_users rown_user2 rown_user2_users "
        "rown_root rown_root_users rown_root_myuser rown_root_user1 rown_root_user2");

    xset_set(xset::name::rown_myuser, xset::var::menu_label, "myuser");
    xset_set(xset::name::rown_myuser_users, xset::var::menu_label, "myuser:users");
    xset_set(xset::name::rown_user1, xset::var::menu_label, "user1");
    xset_set(xset::name::rown_user1_users, xset::var::menu_label, "user1:users");
    xset_set(xset::name::rown_user2, xset::var::menu_label, "user2");
    xset_set(xset::name::rown_user2_users, xset::var::menu_label, "user2:users");
    xset_set(xset::name::rown_root, xset::var::menu_label, "root");
    xset_set(xset::name::rown_root_users, xset::var::menu_label, "root:users");
    xset_set(xset::name::rown_root_myuser, xset::var::menu_label, "root:myuser");
    xset_set(xset::name::rown_root_user1, xset::var::menu_label, "root:user1");
    xset_set(xset::name::rown_root_user2, xset::var::menu_label, "root:user2");

    // PANELS
    for (const panel_t p : PANELS)
    {
        set = xset_get_panel(p, xset::panel::show_toolbox);
        xset_set_var(set, xset::var::menu_label, "_Toolbar");
        set->menu_style = xset::menu::check;
        set->b = xset::b::xtrue;
        if (p != 1)
        {
            xset_set_var(set, xset::var::shared_key, "panel1_show_toolbox");
        }

        set = xset_get_panel(p, xset::panel::show_devmon);
        xset_set_var(set, xset::var::menu_label, "_Devices");
        set->menu_style = xset::menu::check;
        set->b = xset::b::unset;
        if (p != 1)
        {
            xset_set_var(set, xset::var::shared_key, "panel1_show_devmon");
        }

        set = xset_get_panel(p, xset::panel::show_dirtree);
        xset_set_var(set, xset::var::menu_label, "T_ree");
        set->menu_style = xset::menu::check;
        set->b = xset::b::xtrue;
        if (p != 1)
        {
            xset_set_var(set, xset::var::shared_key, "panel1_show_dirtree");
        }

        set = xset_get_panel(p, xset::panel::show_sidebar);
        xset_set_var(set, xset::var::menu_label, "_Side Toolbar");
        set->menu_style = xset::menu::check;
        set->b = xset::b::unset;
        if (p != 1)
        {
            xset_set_var(set, xset::var::shared_key, "panel1_show_sidebar");
        }

        set = xset_get_panel(p, xset::panel::list_detailed);
        xset_set_var(set, xset::var::menu_label, "_Detailed");
        set->menu_style = xset::menu::radio;
        set->b = xset::b::xtrue;
        if (p != 1)
        {
            xset_set_var(set, xset::var::shared_key, "panel1_list_detailed");
        }

        set = xset_get_panel(p, xset::panel::list_icons);
        xset_set_var(set, xset::var::menu_label, "_Icons");
        set->menu_style = xset::menu::radio;
        if (p != 1)
        {
            xset_set_var(set, xset::var::shared_key, "panel1_list_icons");
        }

        set = xset_get_panel(p, xset::panel::list_compact);
        xset_set_var(set, xset::var::menu_label, "_Compact");
        set->menu_style = xset::menu::radio;
        if (p != 1)
        {
            xset_set_var(set, xset::var::shared_key, "panel1_list_compact");
        }

        set = xset_get_panel(p, xset::panel::list_large);
        xset_set_var(set, xset::var::menu_label, "_Large Icons");
        set->menu_style = xset::menu::check;
        if (p != 1)
        {
            xset_set_var(set, xset::var::shared_key, "panel1_list_large");
        }

        set = xset_get_panel(p, xset::panel::show_hidden);
        xset_set_var(set, xset::var::menu_label, "_Hidden Files");
        set->menu_style = xset::menu::check;
        if (p != 1)
        {
            xset_set_var(set, xset::var::shared_key, "panel1_show_hidden");
        }

        set = xset_get_panel(p, xset::panel::icon_tab);
        xset_set_var(set, xset::var::menu_label, "_Icon");
        set->menu_style = xset::menu::icon;
        xset_set_var(set, xset::var::icn, "gtk-directory");

        set = xset_get_panel(p, xset::panel::icon_status);
        xset_set_var(set, xset::var::menu_label, "_Icon");
        set->menu_style = xset::menu::icon;
        xset_set_var(set, xset::var::icn, "gtk-yes");
        if (p != 1)
        {
            xset_set_var(set, xset::var::shared_key, "panel1_icon_status");
        }

        set = xset_get_panel(p, xset::panel::detcol_name);
        xset_set_var(set, xset::var::menu_label, "_Name");
        set->menu_style = xset::menu::check;
        set->b = xset::b::xtrue; // visible
        set->x = "0";            // position

        set = xset_get_panel(p, xset::panel::detcol_size);
        xset_set_var(set, xset::var::menu_label, "_Size");
        set->menu_style = xset::menu::check;
        set->b = xset::b::xtrue;
        set->x = "1";
        if (p != 1)
        {
            xset_set_var(set, xset::var::shared_key, "panel1_detcol_size");
        }

        set = xset_get_panel(p, xset::panel::detcol_bytes);
        xset_set_var(set, xset::var::menu_label, "_Bytes");
        set->menu_style = xset::menu::check;
        set->b = xset::b::xtrue;
        set->x = "2";
        if (p != 1)
        {
            xset_set_var(set, xset::var::shared_key, "panel1_detcol_bytes");
        }

        set = xset_get_panel(p, xset::panel::detcol_type);
        xset_set_var(set, xset::var::menu_label, "_Type");
        set->menu_style = xset::menu::check;
        set->x = "3";
        if (p != 1)
        {
            xset_set_var(set, xset::var::shared_key, "panel1_detcol_type");
        }

        set = xset_get_panel(p, xset::panel::detcol_mime);
        xset_set_var(set, xset::var::menu_label, "_MIME Type");
        set->menu_style = xset::menu::check;
        set->x = "4";
        if (p != 1)
        {
            xset_set_var(set, xset::var::shared_key, "panel1_detcol_mime");
        }

        set = xset_get_panel(p, xset::panel::detcol_perm);
        xset_set_var(set, xset::var::menu_label, "_Permissions");
        set->menu_style = xset::menu::check;
        set->x = "5";
        if (p != 1)
        {
            xset_set_var(set, xset::var::shared_key, "panel1_detcol_perm");
        }

        set = xset_get_panel(p, xset::panel::detcol_owner);
        xset_set_var(set, xset::var::menu_label, "_Owner");
        set->menu_style = xset::menu::check;
        set->x = "6";
        if (p != 1)
        {
            xset_set_var(set, xset::var::shared_key, "panel1_detcol_owner");
        }

        set = xset_get_panel(p, xset::panel::detcol_group);
        xset_set_var(set, xset::var::menu_label, "_Group");
        set->menu_style = xset::menu::check;
        set->x = "7";
        if (p != 1)
        {
            xset_set_var(set, xset::var::shared_key, "panel1_detcol_group");
        }

        set = xset_get_panel(p, xset::panel::detcol_atime);
        xset_set_var(set, xset::var::menu_label, "_Accessed");
        set->menu_style = xset::menu::check;
        set->x = "8";
        if (p != 1)
        {
            xset_set_var(set, xset::var::shared_key, "panel1_detcol_atime");
        }

        set = xset_get_panel(p, xset::panel::detcol_btime);
        xset_set_var(set, xset::var::menu_label, "_Created");
        set->menu_style = xset::menu::check;
        set->x = "9";
        if (p != 1)
        {
            xset_set_var(set, xset::var::shared_key, "panel1_detcol_btime");
        }

        set = xset_get_panel(p, xset::panel::detcol_ctime);
        xset_set_var(set, xset::var::menu_label, "_Metadata");
        set->menu_style = xset::menu::check;
        set->x = "10";
        if (p != 1)
        {
            xset_set_var(set, xset::var::shared_key, "panel1_detcol_ctime");
        }

        set = xset_get_panel(p, xset::panel::detcol_mtime);
        xset_set_var(set, xset::var::menu_label, "_Modified");
        set->menu_style = xset::menu::check;
        set->x = "11";
        if (p != 1)
        {
            xset_set_var(set, xset::var::shared_key, "panel1_detcol_mtime");
        }

        set = xset_get_panel(p, xset::panel::sort_extra);
        set->b = xset::b::xtrue;                                            // sort_natural
        set->x = std::to_string(magic_enum::enum_integer(xset::b::xfalse)); // sort_case
        set->y = "1"; // ptk::file_list::sort_dir::PTK_LIST_SORT_DIR_FIRST
        set->z = std::to_string(magic_enum::enum_integer(xset::b::xtrue)); // sort_hidden_first

        set = xset_get_panel(p, xset::panel::book_fol);
        xset_set_var(set, xset::var::menu_label, "Follow _Dir");
        set->menu_style = xset::menu::check;
        set->b = xset::b::xtrue;
        if (p != 1)
        {
            xset_set_var(set, xset::var::shared_key, "panel1_book_fol");
        }
    }

    // speed
    set = xset_get(xset::name::book_newtab);
    xset_set_var(set, xset::var::menu_label, "_New Tab");
    set->menu_style = xset::menu::check;

    set = xset_get(xset::name::book_single);
    xset_set_var(set, xset::var::menu_label, "_Single Click");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;

    set = xset_get(xset::name::dev_newtab);
    xset_set_var(set, xset::var::menu_label, "_New Tab");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;

    set = xset_get(xset::name::dev_single);
    xset_set_var(set, xset::var::menu_label, "_Single Click");
    set->menu_style = xset::menu::check;
    set->b = xset::b::xtrue;

    // mark all labels and icons as default
    for (xset_t set2 : xsets)
    {
        assert(set2 != nullptr);

        if (set2->lock)
        {
            if (set2->in_terminal)
            {
                set2->in_terminal = false;
            }
            if (set2->keep_terminal)
            {
                set2->keep_terminal = false;
            }
        }
    }
}

static void
def_key(xset::name name, u32 key, u32 keymod)
{
    xset_t set = xset_get(name);

    // key already set or unset?
    if (set->key != 0 || key == 0)
    {
        return;
    }

    // key combo already in use?
    for (xset_t set2 : keysets)
    {
        assert(set2 != nullptr);

        if (set2->key == key && set2->keymod == keymod)
        {
            return;
        }
    }
    set->key = key;
    set->keymod = keymod;
}

void
xset_default_keys()
{
    // read all currently set or unset keys
    keysets.reserve(xsets.size());
    for (xset_t set : xsets)
    {
        assert(set != nullptr);

        if (set->key)
        {
            keysets.emplace_back(set);
        }
    }

    // clang-format off

    def_key(xset::name::tab_prev, GDK_KEY_Tab, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    def_key(xset::name::tab_next, GDK_KEY_Tab, GdkModifierType::GDK_CONTROL_MASK);
    def_key(xset::name::tab_new, GDK_KEY_t, GdkModifierType::GDK_CONTROL_MASK);
    def_key(xset::name::tab_restore, GDK_KEY_T, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    def_key(xset::name::tab_close, GDK_KEY_w, GdkModifierType::GDK_CONTROL_MASK);
    def_key(xset::name::tab_1, GDK_KEY_1, GdkModifierType::GDK_MOD1_MASK);
    def_key(xset::name::tab_2, GDK_KEY_2, GdkModifierType::GDK_MOD1_MASK);
    def_key(xset::name::tab_3, GDK_KEY_3, GdkModifierType::GDK_MOD1_MASK);
    def_key(xset::name::tab_4, GDK_KEY_4, GdkModifierType::GDK_MOD1_MASK);
    def_key(xset::name::tab_5, GDK_KEY_5, GdkModifierType::GDK_MOD1_MASK);
    def_key(xset::name::tab_6, GDK_KEY_6, GdkModifierType::GDK_MOD1_MASK);
    def_key(xset::name::tab_7, GDK_KEY_7, GdkModifierType::GDK_MOD1_MASK);
    def_key(xset::name::tab_8, GDK_KEY_8, GdkModifierType::GDK_MOD1_MASK);
    def_key(xset::name::tab_9, GDK_KEY_9, GdkModifierType::GDK_MOD1_MASK);
    def_key(xset::name::tab_10, GDK_KEY_0, GdkModifierType::GDK_MOD1_MASK);
    def_key(xset::name::edit_cut, GDK_KEY_x, GdkModifierType::GDK_CONTROL_MASK);
    def_key(xset::name::edit_copy, GDK_KEY_c, GdkModifierType::GDK_CONTROL_MASK);
    def_key(xset::name::edit_paste, GDK_KEY_v, GdkModifierType::GDK_CONTROL_MASK);
    def_key(xset::name::edit_rename, GDK_KEY_F2, 0);
    def_key(xset::name::edit_delete, GDK_KEY_Delete, GdkModifierType::GDK_SHIFT_MASK);
    def_key(xset::name::edit_trash, GDK_KEY_Delete, 0);
    def_key(xset::name::copy_name, GDK_KEY_C, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_MOD1_MASK));
    def_key(xset::name::copy_path, GDK_KEY_C, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    def_key(xset::name::paste_link, GDK_KEY_V, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    def_key(xset::name::paste_as, GDK_KEY_A, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    def_key(xset::name::select_all, GDK_KEY_A, GdkModifierType::GDK_CONTROL_MASK);
    def_key(xset::name::main_terminal, GDK_KEY_F4, 0);
    def_key(xset::name::go_default, GDK_KEY_Escape, 0);
    def_key(xset::name::go_back, GDK_KEY_Left, GdkModifierType::GDK_MOD1_MASK);
    def_key(xset::name::go_forward, GDK_KEY_Right, GdkModifierType::GDK_MOD1_MASK);
    def_key(xset::name::go_up, GDK_KEY_Up, GdkModifierType::GDK_MOD1_MASK);
    def_key(xset::name::focus_path_bar, GDK_KEY_l, GdkModifierType::GDK_CONTROL_MASK);
    def_key(xset::name::view_refresh, GDK_KEY_F5, 0);
    def_key(xset::name::prop_info, GDK_KEY_Return, GdkModifierType::GDK_MOD1_MASK);
    def_key(xset::name::prop_perm, GDK_KEY_p, GdkModifierType::GDK_CONTROL_MASK);
    def_key(xset::name::panel1_show_hidden, GDK_KEY_h, GdkModifierType::GDK_CONTROL_MASK);
    def_key(xset::name::book_new, GDK_KEY_d, GdkModifierType::GDK_CONTROL_MASK);
    def_key(xset::name::new_file, GDK_KEY_F, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    def_key(xset::name::new_directory, GDK_KEY_N, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    def_key(xset::name::new_link, GDK_KEY_L, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    // def_key(xset::name::new_archive, GDK_KEY_A, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    def_key(xset::name::main_new_window, GDK_KEY_n, GdkModifierType::GDK_CONTROL_MASK);
    def_key(xset::name::open_all, GDK_KEY_F6, 0);
    def_key(xset::name::main_full, GDK_KEY_F11, 0);
    def_key(xset::name::panel1_show, GDK_KEY_1, GdkModifierType::GDK_CONTROL_MASK);
    def_key(xset::name::panel2_show, GDK_KEY_2, GdkModifierType::GDK_CONTROL_MASK);
    def_key(xset::name::panel3_show, GDK_KEY_3, GdkModifierType::GDK_CONTROL_MASK);
    def_key(xset::name::panel4_show, GDK_KEY_4, GdkModifierType::GDK_CONTROL_MASK);
    // def_key(xset::name::MAIN_HELP, GDK_KEY_F1, 0);
    def_key(xset::name::main_exit, GDK_KEY_q, GdkModifierType::GDK_CONTROL_MASK);
    def_key(xset::name::main_prefs, GDK_KEY_F12, 0);
    def_key(xset::name::book_add, GDK_KEY_d, GdkModifierType::GDK_CONTROL_MASK);

    // clang-format on
}
