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

#include <vector>

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
    set = xset_get(XSetName::SEPARATOR);
    set->menu_style = XSetMenu::SEP;

    // dev menu
    set = xset_set(XSetName::DEV_MENU_REMOVE, XSetVar::MENU_LABEL, "Remo_ve / Eject");
    xset_set_var(set, XSetVar::ICN, "gtk-disconnect");

    set = xset_set(XSetName::DEV_MENU_UNMOUNT, XSetVar::MENU_LABEL, "_Unmount");
    xset_set_var(set, XSetVar::ICN, "gtk-remove");

    set = xset_set(XSetName::DEV_MENU_OPEN, XSetVar::MENU_LABEL, "_Open");
    xset_set_var(set, XSetVar::ICN, "gtk-open");

    set = xset_set(XSetName::DEV_MENU_TAB, XSetVar::MENU_LABEL, "Open In _Tab");
    xset_set_var(set, XSetVar::ICN, "gtk-add");

    set = xset_set(XSetName::DEV_MENU_MOUNT, XSetVar::MENU_LABEL, "_Mount");
    xset_set_var(set, XSetVar::ICN, "drive-removable-media");

    set = xset_set(XSetName::DEV_MENU_MARK, XSetVar::MENU_LABEL, "_Bookmark");
    xset_set_var(set, XSetVar::ICN, "gtk-add");

    set = xset_set(XSetName::DEV_MENU_SETTINGS, XSetVar::MENU_LABEL, "Setti_ngs");
    xset_set_var(set, XSetVar::ICN, "gtk-properties");
    set->menu_style = XSetMenu::SUBMENU;

    // dev settings
    set = xset_set(XSetName::DEV_SHOW, XSetVar::MENU_LABEL, "S_how");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "dev_show_internal_drives dev_show_empty dev_show_partition_tables dev_show_net "
                 "dev_show_file dev_ignore_udisks_hide dev_show_hide_volumes dev_dispname");

    set = xset_set(XSetName::DEV_SHOW_INTERNAL_DRIVES, XSetVar::MENU_LABEL, "_Internal Drives");
    set->menu_style = XSetMenu::CHECK;
    set->b = geteuid() == 0 ? XSetB::XSET_B_TRUE : XSetB::XSET_B_FALSE;

    set = xset_set(XSetName::DEV_SHOW_EMPTY, XSetVar::MENU_LABEL, "_Empty Drives");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE; // geteuid() == 0 ? XSetB::XSET_B_TRUE : XSetB::XSET_B_UNSET;

    set = xset_set(XSetName::DEV_SHOW_PARTITION_TABLES, XSetVar::MENU_LABEL, "_Partition Tables");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::DEV_SHOW_NET, XSetVar::MENU_LABEL, "Mounted _Networks");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::DEV_SHOW_FILE, XSetVar::MENU_LABEL, "Mounted _Other");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::DEV_SHOW_HIDE_VOLUMES, XSetVar::MENU_LABEL, "_Volumes...");
    xset_set_var(set, XSetVar::TITLE, "Show/Hide Volumes");
    xset_set_var(
        set,
        XSetVar::DESC,
        "To force showing or hiding of some volumes, overriding other settings, you can specify "
        "the devices, volume labels, or device IDs in the space-separated list "
        "below.\n\nExample:  +/dev/sdd1 -Label With Space +ata-OCZ-part4\nThis would cause "
        "/dev/sdd1 and the OCZ device to be shown, and the volume with label \"Label With "
        "Space\" to be hidden.\n\nThere must be a space between entries and a plus or minus sign "
        "directly before each item.  This list is case-sensitive.\n\n");

    set = xset_set(XSetName::DEV_IGNORE_UDISKS_HIDE, XSetVar::MENU_LABEL, "Ignore _Hide Policy");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::DEV_DISPNAME, XSetVar::MENU_LABEL, "_Display Name");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Display Name Format");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter device display name format:\n\nUse:\n\t%%v\tdevice filename (eg "
                 "sdd1)\n\t%%s\ttotal size (eg 800G)\n\t%%t\tfstype (eg ext4)\n\t%%l\tvolume "
                 "label (eg Label or [no media])\n\t%%m\tmount point if mounted, or "
                 "---\n\t%%i\tdevice ID\n\t%%n\tmajor:minor device numbers (eg 15:3)\n");
    xset_set_var(set, XSetVar::S, "%v %s %l %m");
    xset_set_var(set, XSetVar::Z, "%v %s %l %m");
    xset_set_var(set, XSetVar::ICN, "gtk-edit");

    set = xset_set(XSetName::DEV_MENU_AUTO, XSetVar::MENU_LABEL, "_Auto Mount");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "dev_automount_optical dev_automount_removable dev_ignore_udisks_nopolicy "
                 "dev_automount_volumes dev_automount_dirs dev_auto_open dev_unmount_quit");

    set = xset_set(XSetName::DEV_AUTOMOUNT_OPTICAL, XSetVar::MENU_LABEL, "Mount _Optical");
    set->b = geteuid() == 0 ? XSetB::XSET_B_FALSE : XSetB::XSET_B_TRUE;
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::DEV_AUTOMOUNT_REMOVABLE, XSetVar::MENU_LABEL, "_Mount Removable");
    set->b = geteuid() == 0 ? XSetB::XSET_B_FALSE : XSetB::XSET_B_TRUE;
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::DEV_AUTOMOUNT_VOLUMES, XSetVar::MENU_LABEL, "Mount _Volumes...");
    xset_set_var(set, XSetVar::TITLE, "Auto-Mount Volumes");
    xset_set_var(
        set,
        XSetVar::DESC,
        "To force or prevent automounting of some volumes, overriding other settings, you can "
        "specify the devices, volume labels, or device IDs in the space-separated list "
        "below.\n\nExample:  +/dev/sdd1 -Label With Space +ata-OCZ-part4\nThis would cause "
        "/dev/sdd1 and the OCZ device to be auto-mounted when detected, and the volume with "
        "label \"Label With Space\" to be ignored.\n\nThere must be a space between entries and "
        "a plus or minus sign directly before each item.  This list is case-sensitive.\n\n");

    set = xset_set(XSetName::DEV_AUTOMOUNT_DIRS, XSetVar::MENU_LABEL, "Mount _Dirs...");
    xset_set_var(set, XSetVar::TITLE, "Automatic Mount Point Dirs");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(
        set,
        XSetVar::DESC,
        "Enter the directory where SpaceFM should automatically create mount point directories "
        "for fuse and similar filesystems (%%a in handler commands).  This directory must be "
        "user-writable (do NOT use /media), and empty subdirectories will be removed.  If left "
        "blank, ~/.cache/spacefm/ (or $XDG_CACHE_HOME/spacefm/) is used.  The following "
        "variables are recognized: $USER $UID $HOME $XDG_RUNTIME_DIR $XDG_CACHE_HOME\n\nNote "
        "that some handlers or mount programs may not obey this setting.\n");

    set = xset_set(XSetName::DEV_AUTO_OPEN, XSetVar::MENU_LABEL, "Open _Tab");
    set->b = XSetB::XSET_B_TRUE;
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::DEV_UNMOUNT_QUIT, XSetVar::MENU_LABEL, "_Unmount On Exit");
    set->b = XSetB::XSET_B_UNSET;
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::DEV_EXEC, XSetVar::MENU_LABEL, "Auto _Run");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "dev_exec_fs dev_exec_audio dev_exec_video separator dev_exec_insert "
                 "dev_exec_unmount dev_exec_remove");
    xset_set_var(set, XSetVar::ICN, "gtk-execute");

    set = xset_set(XSetName::DEV_EXEC_FS, XSetVar::MENU_LABEL, "On _Mount");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Auto Run On Mount");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or fish command line to be run automatically after a removable "
                 "drive or data disc is auto-mounted:\n\nUse:\n\t%%v\tdevice (eg "
                 "/dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_set(XSetName::DEV_EXEC_AUDIO, XSetVar::MENU_LABEL, "On _Audio CD");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Auto Run On Audio CD");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or fish command line to be run automatically when an audio CD is "
                 "inserted in a qualified device:\n\nUse:\n\t%%v\tdevice (eg "
                 "/dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_set(XSetName::DEV_EXEC_VIDEO, XSetVar::MENU_LABEL, "On _Video DVD");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Auto Run On Video DVD");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or fish command line to be run automatically when a video DVD is "
                 "auto-mounted:\n\nUse:\n\t%%v\tdevice (eg /dev/sda1)\n\t%%l\tdevice "
                 "label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_set(XSetName::DEV_EXEC_INSERT, XSetVar::MENU_LABEL, "On _Insert");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Auto Run On Insert");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or fish command line to be run automatically when any device is "
                 "inserted:\n\nUse:\n\t%%v\tdevice added (eg /dev/sda1)\n\t%%l\tdevice "
                 "label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_set(XSetName::DEV_EXEC_UNMOUNT, XSetVar::MENU_LABEL, "On _Unmount");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Auto Run On Unmount");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or fish command line to be run automatically when any device is "
                 "unmounted by any means:\n\nUse:\n\t%%v\tdevice unmounted (eg "
                 "/dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_set(XSetName::DEV_EXEC_REMOVE, XSetVar::MENU_LABEL, "On _Remove");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Auto Run On Remove");
    xset_set_var(
        set,
        XSetVar::DESC,
        "Enter program or fish command line to be run automatically when any device is removed "
        "(ejection of media does not qualify):\n\nUse:\n\t%%v\tdevice removed (eg "
        "/dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)");

    set = xset_set(XSetName::DEV_IGNORE_UDISKS_NOPOLICY, XSetVar::MENU_LABEL, "Ignore _No Policy");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::DEV_MOUNT_OPTIONS, XSetVar::MENU_LABEL, "_Mount Options");
    xset_set_var(
        set,
        XSetVar::DESC,
        "Enter your comma- or space-separated list of default mount options below (%%o in "
        "handlers).\n\nIn addition to regular options, you can also specify options to be added "
        "or removed for a specific filesystem type by using the form OPTION+FSTYPE or "
        "OPTION-FSTYPE.\n\nExample:  nosuid, sync+vfat, sync+ntfs, noatime, noatime-ext4\nThis "
        "will add nosuid and noatime for all filesystem types, add sync for vfat and ntfs only, "
        "and remove noatime for ext4.\n\nNote: Some options, such as nosuid, may be added by the "
        "mount program even if you do not include them.  Options in fstab take precedence.  "
        "pmount and some handlers may ignore options set here.");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Default Mount Options");
    xset_set_var(set, XSetVar::S, "noexec, nosuid, noatime");
    xset_set_var(set, XSetVar::Z, "noexec, nosuid, noatime");
    xset_set_var(set, XSetVar::ICN, "gtk-edit");

    set = xset_set(XSetName::DEV_CHANGE, XSetVar::MENU_LABEL, "_Change Detection");
    xset_set_var(
        set,
        XSetVar::DESC,
        "Enter your comma- or space-separated list of filesystems which should NOT be monitored "
        "for file changes.  This setting only affects non-block devices (such as nfs or fuse), "
        "and is usually used to prevent SpaceFM becoming unresponsive with network filesystems.  "
        "Loading of thumbnails and subdirectory sizes will also be disabled.");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Change Detection Blacklist");
    xset_set_var(set, XSetVar::ICN, "gtk-edit");
    set->s = ztd::strdup("cifs curlftpfs ftpfs fuse.sshfs nfs smbfs");
    set->z = ztd::strdup(set->s);

    set = xset_set(XSetName::DEV_FS_CNF, XSetVar::MENU_LABEL, "_Device Handlers");
    xset_set_var(set, XSetVar::ICON, "gtk-preferences");

    set = xset_set(XSetName::DEV_NET_CNF, XSetVar::MENU_LABEL, "_Protocol Handlers");
    xset_set_var(set, XSetVar::ICON, "gtk-preferences");

    // dev icons
    set = xset_set(XSetName::DEV_ICON, XSetVar::MENU_LABEL, "_Icon");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(
        set,
        XSetVar::DESC,
        "dev_icon_internal_mounted dev_icon_internal_unmounted separator dev_icon_remove_mounted "
        "dev_icon_remove_unmounted separator dev_icon_optical_mounted dev_icon_optical_media "
        "dev_icon_optical_nomedia dev_icon_audiocd separator dev_icon_floppy_mounted "
        "dev_icon_floppy_unmounted separator dev_icon_network dev_icon_file");

    set = xset_set(XSetName::DEV_ICON_AUDIOCD, XSetVar::MENU_LABEL, "Audio CD");
    set->menu_style = XSetMenu::ICON;
    xset_set_var(set, XSetVar::ICN, "gtk-cdrom");

    set = xset_set(XSetName::DEV_ICON_OPTICAL_MOUNTED, XSetVar::MENU_LABEL, "Optical Mounted");
    set->menu_style = XSetMenu::ICON;
    xset_set_var(set, XSetVar::ICN, "gtk-cdrom");

    set = xset_set(XSetName::DEV_ICON_OPTICAL_MEDIA, XSetVar::MENU_LABEL, "Optical Has Media");
    set->menu_style = XSetMenu::ICON;
    xset_set_var(set, XSetVar::ICN, "gtk-yes");

    set = xset_set(XSetName::DEV_ICON_OPTICAL_NOMEDIA, XSetVar::MENU_LABEL, "Optical No Media");
    set->menu_style = XSetMenu::ICON;
    xset_set_var(set, XSetVar::ICN, "gtk-close");

    set = xset_set(XSetName::DEV_ICON_FLOPPY_MOUNTED, XSetVar::MENU_LABEL, "Floppy Mounted");
    set->menu_style = XSetMenu::ICON;
    xset_set_var(set, XSetVar::ICN, "gtk-floppy");

    set = xset_set(XSetName::DEV_ICON_FLOPPY_UNMOUNTED, XSetVar::MENU_LABEL, "Floppy Unmounted");
    set->menu_style = XSetMenu::ICON;
    xset_set_var(set, XSetVar::ICN, "gtk-floppy");

    set = xset_set(XSetName::DEV_ICON_REMOVE_MOUNTED, XSetVar::MENU_LABEL, "Removable Mounted");
    set->menu_style = XSetMenu::ICON;
    xset_set_var(set, XSetVar::ICN, "gtk-add");

    set = xset_set(XSetName::DEV_ICON_REMOVE_UNMOUNTED, XSetVar::MENU_LABEL, "Removable Unmounted");
    set->menu_style = XSetMenu::ICON;
    xset_set_var(set, XSetVar::ICN, "gtk-remove");

    set = xset_set(XSetName::DEV_ICON_INTERNAL_MOUNTED, XSetVar::MENU_LABEL, "Internal Mounted");
    set->menu_style = XSetMenu::ICON;
    xset_set_var(set, XSetVar::ICN, "gtk-open");

    set =
        xset_set(XSetName::DEV_ICON_INTERNAL_UNMOUNTED, XSetVar::MENU_LABEL, "Internal Unmounted");
    set->menu_style = XSetMenu::ICON;
    xset_set_var(set, XSetVar::ICN, "gtk-harddisk");

    set = xset_set(XSetName::DEV_ICON_NETWORK, XSetVar::MENU_LABEL, "Mounted Network");
    set->menu_style = XSetMenu::ICON;
    xset_set_var(set, XSetVar::ICN, "gtk-network");

    set = xset_set(XSetName::DEV_ICON_FILE, XSetVar::MENU_LABEL, "Mounted Other");
    set->menu_style = XSetMenu::ICON;
    xset_set_var(set, XSetVar::ICN, "gtk-file");

    set = xset_set(XSetName::BOOK_OPEN, XSetVar::MENU_LABEL, "_Open");
    xset_set_var(set, XSetVar::ICN, "gtk-open");

    set = xset_set(XSetName::BOOK_SETTINGS, XSetVar::MENU_LABEL, "_Settings");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::ICN, "gtk-properties");

    set = xset_set(XSetName::BOOK_ICON, XSetVar::MENU_LABEL, "Bookmark _Icon");
    set->menu_style = XSetMenu::ICON;
    // do not set a default icon for book_icon

    set = xset_set(XSetName::BOOK_MENU_ICON, XSetVar::MENU_LABEL, "Sub_menu Icon");
    set->menu_style = XSetMenu::ICON;
    // do not set a default icon for book_menu_icon

    set = xset_set(XSetName::BOOK_ADD, XSetVar::MENU_LABEL, "New _Bookmark");
    xset_set_var(set, XSetVar::ICN, "gtk-jump-to");

    set = xset_set(XSetName::MAIN_BOOK, XSetVar::MENU_LABEL, "_Bookmarks");
    xset_set_var(set, XSetVar::ICN, "gtk-directory");
    set->menu_style = XSetMenu::SUBMENU;

    // Fonts
    set = xset_set(XSetName::FONT_GENERAL, XSetVar::S, "Monospace 9");
    set = xset_set(XSetName::FONT_VIEW_ICON, XSetVar::S, "Monospace 9");
    set = xset_set(XSetName::FONT_VIEW_COMPACT, XSetVar::S, "Monospace 9");

    // Rename/Move Dialog
    set = xset_set(XSetName::MOVE_NAME, XSetVar::MENU_LABEL, "_Name");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::MOVE_FILENAME, XSetVar::MENU_LABEL, "F_ilename");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::MOVE_PARENT, XSetVar::MENU_LABEL, "_Parent");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::MOVE_PATH, XSetVar::MENU_LABEL, "P_ath");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::MOVE_TYPE, XSetVar::MENU_LABEL, "Typ_e");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::MOVE_TARGET, XSetVar::MENU_LABEL, "Ta_rget");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::MOVE_TEMPLATE, XSetVar::MENU_LABEL, "Te_mplate");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::MOVE_OPTION, XSetVar::MENU_LABEL, "_Option");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "move_copy move_link move_copyt move_linkt move_as_root");

    set = xset_set(XSetName::MOVE_COPY, XSetVar::MENU_LABEL, "_Copy");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::MOVE_LINK, XSetVar::MENU_LABEL, "_Link");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::MOVE_COPYT, XSetVar::MENU_LABEL, "Copy _Target");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::MOVE_LINKT, XSetVar::MENU_LABEL, "Lin_k Target");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::MOVE_AS_ROOT, XSetVar::MENU_LABEL, "_As Root");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::MOVE_DLG_HELP, XSetVar::MENU_LABEL, "_Help");
    xset_set_var(set, XSetVar::ICN, "gtk-help");

    set = xset_set(XSetName::MOVE_DLG_CONFIRM_CREATE, XSetVar::MENU_LABEL, "_Confirm Create");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    // status bar
    set = xset_set(XSetName::STATUS_MIDDLE, XSetVar::MENU_LABEL, "_Middle Click");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "status_name status_path status_info status_hide");

    set = xset_set(XSetName::STATUS_NAME, XSetVar::MENU_LABEL, "Copy _Name");
    set->menu_style = XSetMenu::RADIO;

    set = xset_set(XSetName::STATUS_PATH, XSetVar::MENU_LABEL, "Copy _Path");
    set->menu_style = XSetMenu::RADIO;

    set = xset_set(XSetName::STATUS_INFO, XSetVar::MENU_LABEL, "File _Info");
    set->menu_style = XSetMenu::RADIO;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::STATUS_HIDE, XSetVar::MENU_LABEL, "_Hide Panel");
    set->menu_style = XSetMenu::RADIO;

    // MAIN WINDOW MENUS

    // File
    set = xset_set(XSetName::MAIN_NEW_WINDOW, XSetVar::MENU_LABEL, "New _Window");
    xset_set_var(set, XSetVar::ICN, "spacefm");

    set = xset_set(XSetName::MAIN_ROOT_WINDOW, XSetVar::MENU_LABEL, "R_oot Window");
    xset_set_var(set, XSetVar::ICN, "gtk-dialog-warning");

    set = xset_set(XSetName::MAIN_SEARCH, XSetVar::MENU_LABEL, "_File Search");
    xset_set_var(set, XSetVar::ICN, "gtk-find");

    set = xset_set(XSetName::MAIN_TERMINAL, XSetVar::MENU_LABEL, "_Terminal");
    set->b = XSetB::XSET_B_UNSET; // discovery notification

    set = xset_set(XSetName::MAIN_ROOT_TERMINAL, XSetVar::MENU_LABEL, "_Root Terminal");
    xset_set_var(set, XSetVar::ICN, "gtk-dialog-warning");

    // was previously used for 'Save Session' < 0.9.4 as XSetMenu::NORMAL
    set = xset_set(XSetName::MAIN_SAVE_SESSION, XSetVar::MENU_LABEL, "Open _URL");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::ICN, "gtk-network");
    xset_set_var(set, XSetVar::TITLE, "Open URL");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter URL in the "
                 "format:\n\tPROTOCOL://USERNAME:PASSWORD@HOST:PORT/SHARE\n\nExamples:\n\tftp://"
                 "mirrors.kernel.org\n\tsmb://user:pass@10.0.0.1:50/docs\n\tssh://"
                 "user@sys.domain\n\tmtp://\n\nIncluding a password is unsafe.  To bookmark a "
                 "URL, right-click on the mounted network in Devices and select Bookmark.\n");
    set->line = nullptr;

    set = xset_set(XSetName::MAIN_SAVE_TABS, XSetVar::MENU_LABEL, "Save Ta_bs");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::MAIN_EXIT, XSetVar::MENU_LABEL, "E_xit");
    xset_set_var(set, XSetVar::ICN, "gtk-quit");

    // View
    set = xset_set(XSetName::PANEL1_SHOW, XSetVar::MENU_LABEL, "Panel _1");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::PANEL2_SHOW, XSetVar::MENU_LABEL, "Panel _2");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::PANEL3_SHOW, XSetVar::MENU_LABEL, "Panel _3");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::PANEL4_SHOW, XSetVar::MENU_LABEL, "Panel _4");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::MAIN_FOCUS_PANEL, XSetVar::MENU_LABEL, "F_ocus");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "panel_prev panel_next panel_hide panel_1 panel_2 panel_3 panel_4");
    xset_set_var(set, XSetVar::ICN, "gtk-go-forward");

    xset_set(XSetName::PANEL_PREV, XSetVar::MENU_LABEL, "_Prev");
    xset_set(XSetName::PANEL_NEXT, XSetVar::MENU_LABEL, "_Next");
    xset_set(XSetName::PANEL_HIDE, XSetVar::MENU_LABEL, "_Hide");
    xset_set(XSetName::PANEL_1, XSetVar::MENU_LABEL, "Panel _1");
    xset_set(XSetName::PANEL_2, XSetVar::MENU_LABEL, "Panel _2");
    xset_set(XSetName::PANEL_3, XSetVar::MENU_LABEL, "Panel _3");
    xset_set(XSetName::PANEL_4, XSetVar::MENU_LABEL, "Panel _4");

    set = xset_set(XSetName::MAIN_AUTO, XSetVar::MENU_LABEL, "_Event Manager");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "auto_inst auto_win auto_pnl auto_tab evt_device");
    xset_set_var(set, XSetVar::ICN, "gtk-execute");

    set = xset_set(XSetName::AUTO_INST, XSetVar::MENU_LABEL, "_Instance");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "evt_start evt_exit");

    set = xset_set(XSetName::EVT_START, XSetVar::MENU_LABEL, "_Startup");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Instance Startup Command");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or fish command line to be run automatically when a SpaceFM "
                 "instance starts:\n\nUse:\n\t%%e\tevent type  (evt_start)\n");

    set = xset_set(XSetName::EVT_EXIT, XSetVar::MENU_LABEL, "_Exit");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Instance Exit Command");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or fish command line to be run automatically when a SpaceFM "
                 "instance exits:\n\nUse:\n\t%%e\tevent type  (evt_exit)\n");

    set = xset_set(XSetName::AUTO_WIN, XSetVar::MENU_LABEL, "_Window");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "evt_win_new evt_win_focus evt_win_move evt_win_click evt_win_key evt_win_close");

    set = xset_set(XSetName::EVT_WIN_NEW, XSetVar::MENU_LABEL, "_New");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set New Window Command");
    xset_set_var(
        set,
        XSetVar::DESC,
        "Enter program or fish command line to be run automatically whenever a new SpaceFM "
        "window is opened:\n\nUse:\n\t%%e\tevent type  (evt_win_new)\n\t%%w\twindow id  (see "
        "spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported fish variables (eg $fm_pwd, etc) "
        "can be used in this command.");

    set = xset_set(XSetName::EVT_WIN_FOCUS, XSetVar::MENU_LABEL, "_Focus");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Window Focus Command");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or fish command line to be run automatically whenever a SpaceFM "
                 "window gets focus:\n\nUse:\n\t%%e\tevent type  (evt_win_focus)\n\t%%w\twindow "
                 "id  (see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported fish variables "
                 "(eg $fm_pwd, etc) can be used in this command.");

    set = xset_set(XSetName::EVT_WIN_MOVE, XSetVar::MENU_LABEL, "_Move/Resize");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Window Move/Resize Command");
    xset_set_var(
        set,
        XSetVar::DESC,
        "Enter program or fish command line to be run automatically whenever a SpaceFM window is "
        "moved or resized:\n\nUse:\n\t%%e\tevent type  (evt_win_move)\n\t%%w\twindow id  (see "
        "spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported fish variables (eg $fm_pwd, etc) "
        "can be used in this command.\n\nNote: This command may be run multiple times during "
        "resize.");

    set = xset_set(XSetName::EVT_WIN_CLICK, XSetVar::MENU_LABEL, "_Click");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Click Command");
    xset_set_var(
        set,
        XSetVar::DESC,
        "Enter program or fish command line to be run automatically whenever the mouse is "
        "clicked:\n\nUse:\n\t%%e\tevent type  (evt_win_click)\n\t%%w\twindow id  (see spacefm -s "
        "help)\n\t%%p\tpanel\n\t%%t\ttab\n\t%%b\tbutton  (mouse button pressed)\n\t%%m\tmodifier "
        " (modifier keys)\n\t%%f\tfocus  (element which received the click)\n\nExported fish "
        "variables (eg $fm_pwd, etc) can be used in this command when no asterisk prefix is "
        "used.\n\nPrefix your command with an asterisk (*) and conditionally return exit status "
        "0 to inhibit the default handler.  For example:\n*if [ \"%%b\" != \"2\" ];then exit 1; "
        "fi; spacefm -g --label \"\\nMiddle button was clicked in %%f\" --button ok &");

    set = xset_set(XSetName::EVT_WIN_KEY, XSetVar::MENU_LABEL, "_Keypress");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Window Keypress Command");
    xset_set_var(
        set,
        XSetVar::DESC,
        "Enter program or fish command line to be run automatically whenever a key is "
        "pressed:\n\nUse:\n\t%%e\tevent type  (evt_win_key)\n\t%%w\twindow id  (see spacefm -s "
        "help)\n\t%%p\tpanel\n\t%%t\ttab\n\t%%k\tkey code  (key pressed)\n\t%%m\tmodifier  "
        "(modifier keys)\n\nExported fish variables (eg $fm_pwd, etc) can be used in this "
        "command when no asterisk prefix is used.\n\nPrefix your command with an asterisk (*) "
        "and conditionally return exit status 0 to inhibit the default handler.  For "
        "example:\n*if [ \"%%k\" != \"0xffc5\" ];then exit 1; fi; spacefm -g --label \"\\nKey "
        "F8 was pressed.\" --button ok &");

    set = xset_set(XSetName::EVT_WIN_CLOSE, XSetVar::MENU_LABEL, "Cl_ose");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Window Close Command");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or fish command line to be run automatically whenever a SpaceFM "
                 "window is closed:\n\nUse:\n\t%%e\tevent type  (evt_win_close)\n\t%%w\twindow "
                 "id  (see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported fish variables "
                 "(eg $fm_pwd, etc) can be used in this command.");

    set = xset_set(XSetName::AUTO_PNL, XSetVar::MENU_LABEL, "_Panel");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "evt_pnl_focus evt_pnl_show evt_pnl_sel");

    set = xset_set(XSetName::EVT_PNL_FOCUS, XSetVar::MENU_LABEL, "_Focus");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Panel Focus Command");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or fish command line to be run automatically whenever a panel "
                 "gets focus:\n\nUse:\n\t%%e\tevent type  (evt_pnl_focus)\n\t%%w\twindow id  "
                 "(see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported fish variables (eg "
                 "$fm_pwd, etc) can be used in this command.");

    set = xset_set(XSetName::EVT_PNL_SHOW, XSetVar::MENU_LABEL, "_Show");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Panel Show Command");
    xset_set_var(
        set,
        XSetVar::DESC,
        "Enter program or fish command line to be run automatically whenever a panel or panel "
        "element is shown or hidden:\n\nUse:\n\t%%e\tevent type  (evt_pnl_show)\n\t%%w\twindow "
        "id  (see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\t%%f\tfocus  (element shown or "
        "hidden)\n\t%%v\tvisible  (1 or 0)\n\nExported fish variables (eg $fm_pwd, etc) can be "
        "used in this command.");

    set = xset_set(XSetName::EVT_PNL_SEL, XSetVar::MENU_LABEL, "S_elect");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Panel Select Command");
    xset_set_var(
        set,
        XSetVar::DESC,
        "Enter program or fish command line to be run automatically whenever the file selection "
        "changes:\n\nUse:\n\t%%e\tevent type  (evt_pnl_sel)\n\t%%w\twindow id  (see spacefm -s "
        "help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported fish variables (eg $fm_pwd, etc) can be "
        "used in this command.\n\nPrefix your command with an asterisk (*) and conditionally "
        "return exit status 0 to inhibit the default handler.");

    set = xset_set(XSetName::AUTO_TAB, XSetVar::MENU_LABEL, "_Tab");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "evt_tab_new evt_tab_chdir evt_tab_focus evt_tab_close");

    set = xset_set(XSetName::EVT_TAB_NEW, XSetVar::MENU_LABEL, "_New");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set New Tab Command");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or fish command line to be run automatically whenever a new tab "
                 "is opened:\n\nUse:\n\t%%e\tevent type  (evt_tab_new)\n\t%%w\twindow id  (see "
                 "spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported fish variables (eg "
                 "$fm_pwd, etc) can be used in this command.");

    set = xset_set(XSetName::EVT_TAB_CHDIR, XSetVar::MENU_LABEL, "_Change Dir");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Tab Change Dir Command");
    xset_set_var(
        set,
        XSetVar::DESC,
        "Enter program or fish command line to be run automatically whenever a tab changes to a "
        "different directory:\n\nUse:\n\t%%e\tevent type  (evt_tab_chdir)\n\t%%w\twindow id  "
        "(see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\t%%d\tnew directory\n\nExported fish "
        "variables (eg $fm_pwd, etc) can be used in this command.");

    set = xset_set(XSetName::EVT_TAB_FOCUS, XSetVar::MENU_LABEL, "_Focus");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Tab Focus Command");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or fish command line to be run automatically whenever a tab gets "
                 "focus:\n\nUse:\n\t%%e\tevent type  (evt_tab_focus)\n\t%%w\twindow id  (see "
                 "spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported fish variables (eg "
                 "$fm_pwd, etc) can be used in this command.");

    set = xset_set(XSetName::EVT_TAB_CLOSE, XSetVar::MENU_LABEL, "_Close");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Tab Close Command");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or fish command line to be run automatically whenever a tab is "
                 "closed:\n\nUse:\n\t%%e\tevent type  (evt_tab_close)\n\t%%w\twindow id  (see "
                 "spacefm -s help)\n\t%%p\tpanel\n\t%%t\tclosed tab");

    set = xset_set(XSetName::EVT_DEVICE, XSetVar::MENU_LABEL, "_Device");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Device Command");
    xset_set_var(set,
                 XSetVar::DESC,
                 "Enter program or fish command line to be run automatically whenever a device "
                 "state changes:\n\nUse:\n\t%%e\tevent type  (evt_device)\n\t%%f\tdevice "
                 "file\n\t%%v\tchange  (added|removed|changed)\n");

    set = xset_set(XSetName::MAIN_TITLE, XSetVar::MENU_LABEL, "Wi_ndow Title");
    set->menu_style = XSetMenu::STRING;
    xset_set_var(set, XSetVar::TITLE, "Set Window Title Format");
    xset_set_var(
        set,
        XSetVar::DESC,
        "Set window title format:\n\nUse:\n\t%%n\tcurrent directory name (eg "
        "bin)\n\t%%d\tcurrent "
        "directory path (eg /usr/bin)\n\t%%p\tcurrent panel number (1-4)\n\t%%t\tcurrent tab "
        "number\n\t%%P\ttotal number of panels visible\n\t%%T\ttotal number of tabs in current "
        "panel\n\t*\tasterisk shown if tasks running in window");
    xset_set_var(set, XSetVar::S, "%d");
    xset_set_var(set, XSetVar::Z, "%d");

    set = xset_set(XSetName::MAIN_ICON, XSetVar::MENU_LABEL, "_Window Icon");
    set->menu_style = XSetMenu::ICON;
    // Note: xset_text_dialog uses the title passed to know this is an
    // icon chooser, so it adds a Choose button.  If you change the title,
    // change xset_text_dialog.
    set->title = ztd::strdup("Set Window Icon");
    set->desc = ztd::strdup("Enter an icon name, icon file path, or stock item name:\n\nOr click "
                            "Choose to select an icon.  Not all icons may work properly due to "
                            "various issues.\n\nProvided alternate SpaceFM "
                            "icons:\n\tspacefm-[48|128]-[cube|pyramid]-[blue|green|red]\n\tspacefm-"
                            "48-folder-[blue|red]\n\nFor example: spacefm-48-pyramid-green");
    // x and y store global icon chooser dialog size

    set = xset_set(XSetName::MAIN_FULL, XSetVar::MENU_LABEL, "_Fullscreen");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::MAIN_DESIGN_MODE, XSetVar::MENU_LABEL, "_Design Mode");
    xset_set_var(set, XSetVar::ICN, "gtk-help");

    set = xset_set(XSetName::MAIN_PREFS, XSetVar::MENU_LABEL, "_Preferences");
    xset_set_var(set, XSetVar::ICN, "gtk-preferences");

    set = xset_set(XSetName::MAIN_TOOL, XSetVar::MENU_LABEL, "_Tool");
    set->menu_style = XSetMenu::SUBMENU;

    set = xset_get(XSetName::ROOT_BAR); // in Preferences
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::VIEW_THUMB,
                   XSetVar::MENU_LABEL,
                   "_Thumbnails (global)"); // in View|Panel View|Style
    set->menu_style = XSetMenu::CHECK;

    // Plugins
    set = xset_set(XSetName::PLUG_INSTALL, XSetVar::MENU_LABEL, "_Install");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "plug_ifile");
    xset_set_var(set, XSetVar::ICN, "gtk-add");

    set = xset_set(XSetName::PLUG_IFILE, XSetVar::MENU_LABEL, "_File");
    xset_set_var(set, XSetVar::ICN, "gtk-file");

    set = xset_set(XSetName::PLUG_COPY, XSetVar::MENU_LABEL, "_Import");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "plug_cfile separator plug_cverb");
    xset_set_var(set, XSetVar::ICN, "gtk-copy");

    set = xset_set(XSetName::PLUG_CFILE, XSetVar::MENU_LABEL, "_File");
    xset_set_var(set, XSetVar::ICN, "gtk-file");
    set = xset_set(XSetName::PLUG_CVERB, XSetVar::MENU_LABEL, "_Verbose");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    // Help
    set = xset_set(XSetName::MAIN_ABOUT, XSetVar::MENU_LABEL, "_About");
    xset_set_var(set, XSetVar::ICN, "gtk-about");

    set = xset_set(XSetName::MAIN_DEV, XSetVar::MENU_LABEL, "_Show Devices");
    xset_set_var(set, XSetVar::SHARED_KEY, "panel1_show_devmon");
    set->menu_style = XSetMenu::CHECK;

    // Tasks
    set = xset_set(XSetName::MAIN_TASKS, XSetVar::MENU_LABEL, "_Task Manager");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(
        set,
        XSetVar::DESC,
        "task_show_manager task_hide_manager separator task_columns task_popups task_errors "
        "task_queue");

    set = xset_set(XSetName::TASK_COL_STATUS, XSetVar::MENU_LABEL, "_Status");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("0");   // column position
    set->y = ztd::strdup("130"); // column width

    set = xset_set(XSetName::TASK_COL_COUNT, XSetVar::MENU_LABEL, "_Count");
    set->menu_style = XSetMenu::CHECK;
    set->x = ztd::strdup("1");

    set = xset_set(XSetName::TASK_COL_PATH, XSetVar::MENU_LABEL, "_Directory");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("2");

    set = xset_set(XSetName::TASK_COL_FILE, XSetVar::MENU_LABEL, "_Item");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("3");

    set = xset_set(XSetName::TASK_COL_TO, XSetVar::MENU_LABEL, "_To");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("4");

    set = xset_set(XSetName::TASK_COL_PROGRESS, XSetVar::MENU_LABEL, "_Progress");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("5");
    set->y = ztd::strdup("100");

    set = xset_set(XSetName::TASK_COL_TOTAL, XSetVar::MENU_LABEL, "T_otal");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("6");
    set->y = ztd::strdup("120");

    set = xset_set(XSetName::TASK_COL_STARTED, XSetVar::MENU_LABEL, "Sta_rted");
    set->menu_style = XSetMenu::CHECK;
    set->x = ztd::strdup("7");

    set = xset_set(XSetName::TASK_COL_ELAPSED, XSetVar::MENU_LABEL, "_Elapsed");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("8");
    set->y = ztd::strdup("70");

    set = xset_set(XSetName::TASK_COL_CURSPEED, XSetVar::MENU_LABEL, "C_urrent Speed");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("9");

    set = xset_set(XSetName::TASK_COL_CUREST, XSetVar::MENU_LABEL, "Current Re_main");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;
    set->x = ztd::strdup("10");

    set = xset_set(XSetName::TASK_COL_AVGSPEED, XSetVar::MENU_LABEL, "_Average Speed");
    set->menu_style = XSetMenu::CHECK;
    set->x = ztd::strdup("11");
    set->y = ztd::strdup("60");

    set = xset_set(XSetName::TASK_COL_AVGEST, XSetVar::MENU_LABEL, "A_verage Remain");
    set->menu_style = XSetMenu::CHECK;
    set->x = ztd::strdup("12");
    set->y = ztd::strdup("65");

    set = xset_set(XSetName::TASK_COL_REORDER, XSetVar::MENU_LABEL, "Reor_der");

    set = xset_set(XSetName::TASK_STOP, XSetVar::MENU_LABEL, "_Stop");
    xset_set_var(set, XSetVar::ICN, "gtk-stop");
    set = xset_set(XSetName::TASK_PAUSE, XSetVar::MENU_LABEL, "Pa_use");
    xset_set_var(set, XSetVar::ICN, "gtk-media-pause");
    set = xset_set(XSetName::TASK_QUE, XSetVar::MENU_LABEL, "_Queue");
    xset_set_var(set, XSetVar::ICN, "gtk-add");
    set = xset_set(XSetName::TASK_RESUME, XSetVar::MENU_LABEL, "_Resume");
    xset_set_var(set, XSetVar::ICN, "gtk-media-play");
    set = xset_set(XSetName::TASK_SHOWOUT, XSetVar::MENU_LABEL, "Sho_w Output");

    set = xset_set(XSetName::TASK_ALL, XSetVar::MENU_LABEL, "_All Tasks");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "task_stop_all task_pause_all task_que_all task_resume_all");

    set = xset_set(XSetName::TASK_STOP_ALL, XSetVar::MENU_LABEL, "_Stop");
    xset_set_var(set, XSetVar::ICN, "gtk-stop");
    set = xset_set(XSetName::TASK_PAUSE_ALL, XSetVar::MENU_LABEL, "Pa_use");
    xset_set_var(set, XSetVar::ICN, "gtk-media-pause");
    set = xset_set(XSetName::TASK_QUE_ALL, XSetVar::MENU_LABEL, "_Queue");
    xset_set_var(set, XSetVar::ICN, "gtk-add");
    set = xset_set(XSetName::TASK_RESUME_ALL, XSetVar::MENU_LABEL, "_Resume");
    xset_set_var(set, XSetVar::ICN, "gtk-media-play");

    set = xset_set(XSetName::TASK_SHOW_MANAGER, XSetVar::MENU_LABEL, "Show _Manager");
    set->menu_style = XSetMenu::RADIO;
    set->b = XSetB::XSET_B_FALSE;
    // set->x  used for task man height >=0.9.4

    set = xset_set(XSetName::TASK_HIDE_MANAGER, XSetVar::MENU_LABEL, "Auto-_Hide Manager");
    set->menu_style = XSetMenu::RADIO;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::TASK_COLUMNS, XSetVar::MENU_LABEL, "_Columns");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(
        set,
        XSetVar::DESC,
        "task_col_count task_col_path task_col_file task_col_to task_col_progress task_col_total "
        "task_col_started task_col_elapsed task_col_curspeed task_col_curest task_col_avgspeed "
        "task_col_avgest separator task_col_reorder");

    set = xset_set(XSetName::TASK_POPUPS, XSetVar::MENU_LABEL, "_Popups");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(
        set,
        XSetVar::DESC,
        "task_pop_all task_pop_top task_pop_above task_pop_stick separator task_pop_detail "
        "task_pop_over task_pop_err");

    set = xset_set(XSetName::TASK_POP_ALL, XSetVar::MENU_LABEL, "Popup _All Tasks");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_FALSE;

    set = xset_set(XSetName::TASK_POP_TOP, XSetVar::MENU_LABEL, "Stay On _Top");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_FALSE;

    set = xset_set(XSetName::TASK_POP_ABOVE, XSetVar::MENU_LABEL, "A_bove Others");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_FALSE;

    set = xset_set(XSetName::TASK_POP_STICK, XSetVar::MENU_LABEL, "All _Workspaces");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_FALSE;

    set = xset_set(XSetName::TASK_POP_DETAIL, XSetVar::MENU_LABEL, "_Detailed Stats");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_FALSE;

    set = xset_set(XSetName::TASK_POP_OVER, XSetVar::MENU_LABEL, "_Overwrite Option");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::TASK_POP_ERR, XSetVar::MENU_LABEL, "_Error Option");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::TASK_ERRORS, XSetVar::MENU_LABEL, "Err_ors");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "task_err_first task_err_any task_err_cont");

    set = xset_set(XSetName::TASK_ERR_FIRST, XSetVar::MENU_LABEL, "Stop If _First");
    set->menu_style = XSetMenu::RADIO;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::TASK_ERR_ANY, XSetVar::MENU_LABEL, "Stop On _Any");
    set->menu_style = XSetMenu::RADIO;
    set->b = XSetB::XSET_B_FALSE;

    set = xset_set(XSetName::TASK_ERR_CONT, XSetVar::MENU_LABEL, "_Continue");
    set->menu_style = XSetMenu::RADIO;
    set->b = XSetB::XSET_B_FALSE;

    set = xset_set(XSetName::TASK_QUEUE, XSetVar::MENU_LABEL, "Qu_eue");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "task_q_new task_q_smart task_q_pause");

    set = xset_set(XSetName::TASK_Q_NEW, XSetVar::MENU_LABEL, "_Queue New Tasks");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::TASK_Q_SMART, XSetVar::MENU_LABEL, "_Smart Queue");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::TASK_Q_PAUSE, XSetVar::MENU_LABEL, "_Pause On Error");
    set->menu_style = XSetMenu::CHECK;

    // PANELS COMMON
    xset_set(XSetName::DATE_FORMAT, XSetVar::S, "%Y-%m-%d %H:%M");

    set = xset_set(XSetName::CON_OPEN, XSetVar::MENU_LABEL, "_Open");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::ICN, "gtk-open");

    set = xset_set(XSetName::OPEN_EXECUTE, XSetVar::MENU_LABEL, "E_xecute");
    xset_set_var(set, XSetVar::ICN, "gtk-execute");

    set = xset_set(XSetName::OPEN_EDIT, XSetVar::MENU_LABEL, "Edi_t");
    xset_set_var(set, XSetVar::ICN, "gtk-edit");

    set = xset_set(XSetName::OPEN_EDIT_ROOT, XSetVar::MENU_LABEL, "Edit As _Root");
    xset_set_var(set, XSetVar::ICN, "gtk-dialog-warning");

    set = xset_set(XSetName::OPEN_OTHER, XSetVar::MENU_LABEL, "_Choose...");
    xset_set_var(set, XSetVar::ICN, "gtk-open");

    set = xset_set(XSetName::OPEN_HAND, XSetVar::MENU_LABEL, "File _Handlers...");
    xset_set_var(set, XSetVar::ICN, "gtk-preferences");

    set = xset_set(XSetName::OPEN_ALL, XSetVar::MENU_LABEL, "Open With _Default"); // virtual

    set = xset_set(XSetName::OPEN_IN_TAB, XSetVar::MENU_LABEL, "In _Tab");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "opentab_new opentab_prev opentab_next opentab_1 opentab_2 opentab_3 opentab_4 "
                 "opentab_5 opentab_6 opentab_7 opentab_8 opentab_9 opentab_10");

    xset_set(XSetName::OPENTAB_NEW, XSetVar::MENU_LABEL, "N_ew");
    xset_set(XSetName::OPENTAB_PREV, XSetVar::MENU_LABEL, "_Prev");
    xset_set(XSetName::OPENTAB_NEXT, XSetVar::MENU_LABEL, "_Next");
    xset_set(XSetName::OPENTAB_1, XSetVar::MENU_LABEL, "Tab _1");
    xset_set(XSetName::OPENTAB_2, XSetVar::MENU_LABEL, "Tab _2");
    xset_set(XSetName::OPENTAB_3, XSetVar::MENU_LABEL, "Tab _3");
    xset_set(XSetName::OPENTAB_4, XSetVar::MENU_LABEL, "Tab _4");
    xset_set(XSetName::OPENTAB_5, XSetVar::MENU_LABEL, "Tab _5");
    xset_set(XSetName::OPENTAB_6, XSetVar::MENU_LABEL, "Tab _6");
    xset_set(XSetName::OPENTAB_7, XSetVar::MENU_LABEL, "Tab _7");
    xset_set(XSetName::OPENTAB_8, XSetVar::MENU_LABEL, "Tab _8");
    xset_set(XSetName::OPENTAB_9, XSetVar::MENU_LABEL, "Tab _9");
    xset_set(XSetName::OPENTAB_10, XSetVar::MENU_LABEL, "Tab 1_0");

    set = xset_set(XSetName::OPEN_IN_PANEL, XSetVar::MENU_LABEL, "In _Panel");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "open_in_panelprev open_in_panelnext open_in_panel1 open_in_panel2 open_in_panel3 "
                 "open_in_panel4");

    xset_set(XSetName::OPEN_IN_PANELPREV, XSetVar::MENU_LABEL, "_Prev");
    xset_set(XSetName::OPEN_IN_PANELNEXT, XSetVar::MENU_LABEL, "_Next");
    xset_set(XSetName::OPEN_IN_PANEL1, XSetVar::MENU_LABEL, "Panel _1");
    xset_set(XSetName::OPEN_IN_PANEL2, XSetVar::MENU_LABEL, "Panel _2");
    xset_set(XSetName::OPEN_IN_PANEL3, XSetVar::MENU_LABEL, "Panel _3");
    xset_set(XSetName::OPEN_IN_PANEL4, XSetVar::MENU_LABEL, "Panel _4");

    set = xset_set(XSetName::ARC_EXTRACT, XSetVar::MENU_LABEL, "_Extract");
    xset_set_var(set, XSetVar::ICN, "gtk-convert");

    set = xset_set(XSetName::ARC_EXTRACTTO, XSetVar::MENU_LABEL, "Extract _To");
    xset_set_var(set, XSetVar::ICN, "gtk-convert");

    set = xset_set(XSetName::ARC_LIST, XSetVar::MENU_LABEL, "_List Contents");
    xset_set_var(set, XSetVar::ICN, "gtk-file");

    set = xset_set(XSetName::ARC_DEFAULT, XSetVar::MENU_LABEL, "_Archive Defaults");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "arc_conf2 separator arc_def_open arc_def_ex arc_def_exto arc_def_list separator "
                 "arc_def_parent arc_def_write");

    set = xset_set(XSetName::ARC_DEF_OPEN, XSetVar::MENU_LABEL, "_Open With App");
    set->menu_style = XSetMenu::RADIO;

    set = xset_set(XSetName::ARC_DEF_EX, XSetVar::MENU_LABEL, "_Extract");
    set->menu_style = XSetMenu::RADIO;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::ARC_DEF_EXTO, XSetVar::MENU_LABEL, "Extract _To");
    set->menu_style = XSetMenu::RADIO;

    set = xset_set(XSetName::ARC_DEF_LIST, XSetVar::MENU_LABEL, "_List Contents");
    set->menu_style = XSetMenu::RADIO;

    set = xset_set(XSetName::ARC_DEF_PARENT, XSetVar::MENU_LABEL, "_Create Subdirectory");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::ARC_DEF_WRITE, XSetVar::MENU_LABEL, "_Write Access");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::ARC_CONF2, XSetVar::MENU_LABEL, "Archive _Handlers");
    xset_set_var(set, XSetVar::ICON, "gtk-preferences");

    set = xset_set(XSetName::OPEN_NEW, XSetVar::MENU_LABEL, "_New");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(
        set,
        XSetVar::DESC,
        "new_file new_directory new_link new_archive separator tab_new tab_new_here new_bookmark");
    xset_set_var(set, XSetVar::ICN, "gtk-new");

    set = xset_set(XSetName::NEW_FILE, XSetVar::MENU_LABEL, "_File");
    xset_set_var(set, XSetVar::ICN, "gtk-file");

    set = xset_set(XSetName::NEW_DIRECTORY, XSetVar::MENU_LABEL, "Dir_ectory");
    xset_set_var(set, XSetVar::ICN, "gtk-directory");

    set = xset_set(XSetName::NEW_LINK, XSetVar::MENU_LABEL, "_Link");
    xset_set_var(set, XSetVar::ICN, "gtk-file");

    set = xset_set(XSetName::NEW_BOOKMARK, XSetVar::MENU_LABEL, "_Bookmark");
    xset_set_var(set, XSetVar::SHARED_KEY, "book_add");
    xset_set_var(set, XSetVar::ICN, "gtk-jump-to");

    set = xset_set(XSetName::NEW_ARCHIVE, XSetVar::MENU_LABEL, "_Archive");
    xset_set_var(set, XSetVar::ICN, "gtk-save-as");

    set = xset_get(XSetName::ARC_DLG);
    set->b = XSetB::XSET_B_TRUE; // Extract To - Create Subdirectory
    set->z = ztd::strdup("1");   // Extract To - Write Access

    set = xset_set(XSetName::TAB_NEW, XSetVar::MENU_LABEL, "_Tab");
    xset_set_var(set, XSetVar::ICN, "gtk-add");
    set = xset_set(XSetName::TAB_NEW_HERE, XSetVar::MENU_LABEL, "Tab _Here");
    xset_set_var(set, XSetVar::ICN, "gtk-add");

    set = xset_set(XSetName::NEW_APP, XSetVar::MENU_LABEL, "_Desktop Application");
    xset_set_var(set, XSetVar::ICN, "gtk-add");

    set = xset_set(XSetName::CON_GO, XSetVar::MENU_LABEL, "_Go");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "go_back go_forward go_up go_home go_default go_set_default edit_canon separator "
                 "go_tab go_focus");
    xset_set_var(set, XSetVar::ICN, "gtk-go-forward");

    set = xset_set(XSetName::GO_BACK, XSetVar::MENU_LABEL, "_Back");
    xset_set_var(set, XSetVar::ICN, "gtk-go-back");
    set = xset_set(XSetName::GO_FORWARD, XSetVar::MENU_LABEL, "_Forward");
    xset_set_var(set, XSetVar::ICN, "gtk-go-forward");
    set = xset_set(XSetName::GO_UP, XSetVar::MENU_LABEL, "_Up");
    xset_set_var(set, XSetVar::ICN, "gtk-go-up");
    set = xset_set(XSetName::GO_HOME, XSetVar::MENU_LABEL, "_Home");
    xset_set_var(set, XSetVar::ICN, "gtk-home");
    set = xset_set(XSetName::GO_DEFAULT, XSetVar::MENU_LABEL, "_Default");
    xset_set_var(set, XSetVar::ICN, "gtk-home");

    set = xset_set(XSetName::GO_SET_DEFAULT, XSetVar::MENU_LABEL, "_Set Default");
    xset_set_var(set, XSetVar::ICN, "gtk-save");

    set = xset_set(XSetName::EDIT_CANON, XSetVar::MENU_LABEL, "Re_al Path");

    set = xset_set(XSetName::GO_FOCUS, XSetVar::MENU_LABEL, "Fo_cus");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "focus_path_bar focus_filelist focus_dirtree focus_book focus_device");

    set = xset_set(XSetName::FOCUS_PATH_BAR, XSetVar::MENU_LABEL, "_Path Bar");
    xset_set_var(set, XSetVar::ICN, "gtk-dialog-question");
    set = xset_set(XSetName::FOCUS_FILELIST, XSetVar::MENU_LABEL, "_File List");
    xset_set_var(set, XSetVar::ICN, "gtk-file");
    set = xset_set(XSetName::FOCUS_DIRTREE, XSetVar::MENU_LABEL, "_Tree");
    xset_set_var(set, XSetVar::ICN, "gtk-directory");
    set = xset_set(XSetName::FOCUS_BOOK, XSetVar::MENU_LABEL, "_Bookmarks");
    xset_set_var(set, XSetVar::ICN, "gtk-jump-to");
    set = xset_set(XSetName::FOCUS_DEVICE, XSetVar::MENU_LABEL, "De_vices");
    xset_set_var(set, XSetVar::ICN, "gtk-harddisk");

    set = xset_set(XSetName::GO_TAB, XSetVar::MENU_LABEL, "_Tab");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "tab_prev tab_next tab_restore tab_close tab_1 tab_2 tab_3 tab_4 tab_5 tab_6 "
                 "tab_7 tab_8 tab_9 tab_10");

    xset_set(XSetName::TAB_PREV, XSetVar::MENU_LABEL, "_Prev");
    xset_set(XSetName::TAB_NEXT, XSetVar::MENU_LABEL, "_Next");
    xset_set(XSetName::TAB_RESTORE, XSetVar::MENU_LABEL, "_Restore");
    xset_set(XSetName::TAB_CLOSE, XSetVar::MENU_LABEL, "_Close");
    xset_set(XSetName::TAB_1, XSetVar::MENU_LABEL, "Tab _1");
    xset_set(XSetName::TAB_2, XSetVar::MENU_LABEL, "Tab _2");
    xset_set(XSetName::TAB_3, XSetVar::MENU_LABEL, "Tab _3");
    xset_set(XSetName::TAB_4, XSetVar::MENU_LABEL, "Tab _4");
    xset_set(XSetName::TAB_5, XSetVar::MENU_LABEL, "Tab _5");
    xset_set(XSetName::TAB_6, XSetVar::MENU_LABEL, "Tab _6");
    xset_set(XSetName::TAB_7, XSetVar::MENU_LABEL, "Tab _7");
    xset_set(XSetName::TAB_8, XSetVar::MENU_LABEL, "Tab _8");
    xset_set(XSetName::TAB_9, XSetVar::MENU_LABEL, "Tab _9");
    xset_set(XSetName::TAB_10, XSetVar::MENU_LABEL, "Tab 1_0");

    set = xset_set(XSetName::CON_VIEW, XSetVar::MENU_LABEL, "_View");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::ICN, "gtk-preferences");

    set = xset_set(XSetName::VIEW_LIST_STYLE, XSetVar::MENU_LABEL, "Styl_e");
    set->menu_style = XSetMenu::SUBMENU;

    set = xset_set(XSetName::VIEW_COLUMNS, XSetVar::MENU_LABEL, "C_olumns");
    set->menu_style = XSetMenu::SUBMENU;

    set = xset_set(XSetName::VIEW_REORDER_COL, XSetVar::MENU_LABEL, "_Reorder");

    set = xset_set(XSetName::RUBBERBAND, XSetVar::MENU_LABEL, "_Rubberband Select");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::VIEW_SORTBY, XSetVar::MENU_LABEL, "_Sort");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(
        set,
        XSetVar::DESC,
        "sortby_name sortby_size sortby_type sortby_perm sortby_owner sortby_date separator "
        "sortby_ascend sortby_descend separator sortx_alphanum sortx_case separator " // sortx_natural
        "sortx_directories sortx_files sortx_mix separator sortx_hidfirst sortx_hidlast");

    set = xset_set(XSetName::SORTBY_NAME, XSetVar::MENU_LABEL, "_Name");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set(XSetName::SORTBY_SIZE, XSetVar::MENU_LABEL, "_Size");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set(XSetName::SORTBY_TYPE, XSetVar::MENU_LABEL, "_Type");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set(XSetName::SORTBY_PERM, XSetVar::MENU_LABEL, "_Permission");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set(XSetName::SORTBY_OWNER, XSetVar::MENU_LABEL, "_Owner");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set(XSetName::SORTBY_DATE, XSetVar::MENU_LABEL, "_Modified");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set(XSetName::SORTBY_ASCEND, XSetVar::MENU_LABEL, "_Ascending");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set(XSetName::SORTBY_DESCEND, XSetVar::MENU_LABEL, "_Descending");
    set->menu_style = XSetMenu::RADIO;

    set = xset_set(XSetName::SORTX_ALPHANUM, XSetVar::MENU_LABEL, "Alphanumeric");
    set->menu_style = XSetMenu::CHECK;
#if 0
    set = xset_set(XSetName::SORTX_NATURAL, XSetVar::MENU_LABEL, "Nat_ural");
    set->menu_style = XSetMenu::CHECK;
#endif
    set = xset_set(XSetName::SORTX_CASE, XSetVar::MENU_LABEL, "_Case Sensitive");
    set->menu_style = XSetMenu::CHECK;
    set = xset_set(XSetName::SORTX_DIRECTORIES, XSetVar::MENU_LABEL, "Directories Fi_rst");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set(XSetName::SORTX_FILES, XSetVar::MENU_LABEL, "F_iles First");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set(XSetName::SORTX_MIX, XSetVar::MENU_LABEL, "Mi_xed");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set(XSetName::SORTX_HIDFIRST, XSetVar::MENU_LABEL, "_Hidden First");
    set->menu_style = XSetMenu::RADIO;
    set = xset_set(XSetName::SORTX_HIDLAST, XSetVar::MENU_LABEL, "Hidden _Last");
    set->menu_style = XSetMenu::RADIO;

    set = xset_set(XSetName::VIEW_REFRESH, XSetVar::MENU_LABEL, "Re_fresh");
    xset_set_var(set, XSetVar::ICN, "gtk-refresh");

    set = xset_set(XSetName::PATH_SEEK, XSetVar::MENU_LABEL, "Auto See_k");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::PATH_HAND, XSetVar::MENU_LABEL, "_Protocol Handlers");
    xset_set_var(set, XSetVar::ICN, "gtk-preferences");
    xset_set_var(set, XSetVar::SHARED_KEY, "dev_net_cnf");
    // set->s was custom protocol handler in sfm<=0.9.3 - retained

    // EDIT
    set = xset_set(XSetName::EDIT_CUT, XSetVar::MENU_LABEL, "Cu_t");
    xset_set_var(set, XSetVar::ICN, "gtk-cut");

    set = xset_set(XSetName::EDIT_COPY, XSetVar::MENU_LABEL, "_Copy");
    xset_set_var(set, XSetVar::ICN, "gtk-copy");

    set = xset_set(XSetName::EDIT_PASTE, XSetVar::MENU_LABEL, "_Paste");
    xset_set_var(set, XSetVar::ICN, "gtk-paste");

    set = xset_set(XSetName::EDIT_RENAME, XSetVar::MENU_LABEL, "_Rename");
    xset_set_var(set, XSetVar::ICN, "gtk-edit");

    set = xset_set(XSetName::EDIT_DELETE, XSetVar::MENU_LABEL, "_Delete");
    xset_set_var(set, XSetVar::ICN, "gtk-delete");

    set = xset_set(XSetName::EDIT_TRASH, XSetVar::MENU_LABEL, "_Trash");
    xset_set_var(set, XSetVar::ICN, "gtk-delete");

    set = xset_set(XSetName::EDIT_SUBMENU, XSetVar::MENU_LABEL, "_Actions");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(
        set,
        XSetVar::DESC,
        "copy_name copy_parent copy_path separator paste_link paste_target paste_as separator "
        "copy_to "
        "move_to edit_root edit_hide separator select_all select_patt select_invert select_un");
    xset_set_var(set, XSetVar::ICN, "gtk-edit");

    set = xset_set(XSetName::COPY_NAME, XSetVar::MENU_LABEL, "Copy _Name");
    xset_set_var(set, XSetVar::ICN, "gtk-copy");

    set = xset_set(XSetName::COPY_PATH, XSetVar::MENU_LABEL, "Copy _Path");
    xset_set_var(set, XSetVar::ICN, "gtk-copy");

    set = xset_set(XSetName::COPY_PARENT, XSetVar::MENU_LABEL, "Copy Pa_rent");
    xset_set_var(set, XSetVar::ICN, "gtk-copy");

    set = xset_set(XSetName::PASTE_LINK, XSetVar::MENU_LABEL, "Paste _Link");
    xset_set_var(set, XSetVar::ICN, "gtk-paste");

    set = xset_set(XSetName::PASTE_TARGET, XSetVar::MENU_LABEL, "Paste _Target");
    xset_set_var(set, XSetVar::ICN, "gtk-paste");

    set = xset_set(XSetName::PASTE_AS, XSetVar::MENU_LABEL, "Paste _As");
    xset_set_var(set, XSetVar::ICN, "gtk-paste");

    set = xset_set(XSetName::COPY_TO, XSetVar::MENU_LABEL, "_Copy To");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "copy_loc copy_loc_last separator copy_tab copy_panel");

    set = xset_set(XSetName::COPY_LOC, XSetVar::MENU_LABEL, "L_ocation");
    set = xset_set(XSetName::COPY_LOC_LAST, XSetVar::MENU_LABEL, "L_ast Location");
    xset_set_var(set, XSetVar::ICN, "gtk-redo");

    set = xset_set(XSetName::COPY_TAB, XSetVar::MENU_LABEL, "_Tab");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "copy_tab_prev copy_tab_next copy_tab_1 copy_tab_2 copy_tab_3 copy_tab_4 "
                 "copy_tab_5 copy_tab_6 copy_tab_7 copy_tab_8 copy_tab_9 copy_tab_10");

    xset_set(XSetName::COPY_TAB_PREV, XSetVar::MENU_LABEL, "_Prev");
    xset_set(XSetName::COPY_TAB_NEXT, XSetVar::MENU_LABEL, "_Next");
    xset_set(XSetName::COPY_TAB_1, XSetVar::MENU_LABEL, "Tab _1");
    xset_set(XSetName::COPY_TAB_2, XSetVar::MENU_LABEL, "Tab _2");
    xset_set(XSetName::COPY_TAB_3, XSetVar::MENU_LABEL, "Tab _3");
    xset_set(XSetName::COPY_TAB_4, XSetVar::MENU_LABEL, "Tab _4");
    xset_set(XSetName::COPY_TAB_5, XSetVar::MENU_LABEL, "Tab _5");
    xset_set(XSetName::COPY_TAB_6, XSetVar::MENU_LABEL, "Tab _6");
    xset_set(XSetName::COPY_TAB_7, XSetVar::MENU_LABEL, "Tab _7");
    xset_set(XSetName::COPY_TAB_8, XSetVar::MENU_LABEL, "Tab _8");
    xset_set(XSetName::COPY_TAB_9, XSetVar::MENU_LABEL, "Tab _9");
    xset_set(XSetName::COPY_TAB_10, XSetVar::MENU_LABEL, "Tab 1_0");

    set = xset_set(XSetName::COPY_PANEL, XSetVar::MENU_LABEL, "_Panel");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(
        set,
        XSetVar::DESC,
        "copy_panel_prev copy_panel_next copy_panel_1 copy_panel_2 copy_panel_3 copy_panel_4");

    xset_set(XSetName::COPY_PANEL_PREV, XSetVar::MENU_LABEL, "_Prev");
    xset_set(XSetName::COPY_PANEL_NEXT, XSetVar::MENU_LABEL, "_Next");
    xset_set(XSetName::COPY_PANEL_1, XSetVar::MENU_LABEL, "Panel _1");
    xset_set(XSetName::COPY_PANEL_2, XSetVar::MENU_LABEL, "Panel _2");
    xset_set(XSetName::COPY_PANEL_3, XSetVar::MENU_LABEL, "Panel _3");
    xset_set(XSetName::COPY_PANEL_4, XSetVar::MENU_LABEL, "Panel _4");

    set = xset_set(XSetName::MOVE_TO, XSetVar::MENU_LABEL, "_Move To");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "move_loc move_loc_last separator move_tab move_panel");

    set = xset_set(XSetName::MOVE_LOC, XSetVar::MENU_LABEL, "_Location");
    set = xset_set(XSetName::MOVE_LOC_LAST, XSetVar::MENU_LABEL, "L_ast Location");
    xset_set_var(set, XSetVar::ICN, "gtk-redo");
    set = xset_set(XSetName::MOVE_TAB, XSetVar::MENU_LABEL, "_Tab");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "move_tab_prev move_tab_next move_tab_1 move_tab_2 move_tab_3 move_tab_4 "
                 "move_tab_5 move_tab_6 move_tab_7 move_tab_8 move_tab_9 move_tab_10");

    xset_set(XSetName::MOVE_TAB_PREV, XSetVar::MENU_LABEL, "_Prev");
    xset_set(XSetName::MOVE_TAB_NEXT, XSetVar::MENU_LABEL, "_Next");
    xset_set(XSetName::MOVE_TAB_1, XSetVar::MENU_LABEL, "Tab _1");
    xset_set(XSetName::MOVE_TAB_2, XSetVar::MENU_LABEL, "Tab _2");
    xset_set(XSetName::MOVE_TAB_3, XSetVar::MENU_LABEL, "Tab _3");
    xset_set(XSetName::MOVE_TAB_4, XSetVar::MENU_LABEL, "Tab _4");
    xset_set(XSetName::MOVE_TAB_5, XSetVar::MENU_LABEL, "Tab _5");
    xset_set(XSetName::MOVE_TAB_6, XSetVar::MENU_LABEL, "Tab _6");
    xset_set(XSetName::MOVE_TAB_7, XSetVar::MENU_LABEL, "Tab _7");
    xset_set(XSetName::MOVE_TAB_8, XSetVar::MENU_LABEL, "Tab _8");
    xset_set(XSetName::MOVE_TAB_9, XSetVar::MENU_LABEL, "Tab _9");
    xset_set(XSetName::MOVE_TAB_10, XSetVar::MENU_LABEL, "Tab 1_0");

    set = xset_set(XSetName::MOVE_PANEL, XSetVar::MENU_LABEL, "_Panel");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(
        set,
        XSetVar::DESC,
        "move_panel_prev move_panel_next move_panel_1 move_panel_2 move_panel_3 move_panel_4");

    xset_set(XSetName::MOVE_PANEL_PREV, XSetVar::MENU_LABEL, "_Prev");
    xset_set(XSetName::MOVE_PANEL_NEXT, XSetVar::MENU_LABEL, "_Next");
    xset_set(XSetName::MOVE_PANEL_1, XSetVar::MENU_LABEL, "Panel _1");
    xset_set(XSetName::MOVE_PANEL_2, XSetVar::MENU_LABEL, "Panel _2");
    xset_set(XSetName::MOVE_PANEL_3, XSetVar::MENU_LABEL, "Panel _3");
    xset_set(XSetName::MOVE_PANEL_4, XSetVar::MENU_LABEL, "Panel _4");

    set = xset_set(XSetName::EDIT_HIDE, XSetVar::MENU_LABEL, "_Hide");

    set = xset_set(XSetName::SELECT_ALL, XSetVar::MENU_LABEL, "_Select All");
    xset_set_var(set, XSetVar::ICN, "gtk-select-all");

    set = xset_set(XSetName::SELECT_UN, XSetVar::MENU_LABEL, "_Unselect All");

    set = xset_set(XSetName::SELECT_INVERT, XSetVar::MENU_LABEL, "_Invert Selection");

    set = xset_set(XSetName::SELECT_PATT, XSetVar::MENU_LABEL, "S_elect By Pattern");

    set = xset_set(XSetName::EDIT_ROOT, XSetVar::MENU_LABEL, "R_oot");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "root_copy_loc root_move2 root_delete");
    xset_set_var(set, XSetVar::ICN, "gtk-dialog-warning");

    set = xset_set(XSetName::ROOT_COPY_LOC, XSetVar::MENU_LABEL, "_Copy To");
    set = xset_set(XSetName::ROOT_MOVE2, XSetVar::MENU_LABEL, "Move _To");
    set = xset_set(XSetName::ROOT_DELETE, XSetVar::MENU_LABEL, "_Delete");
    xset_set_var(set, XSetVar::ICN, "gtk-delete");

    // Properties
    set = xset_set(XSetName::CON_PROP, XSetVar::MENU_LABEL, "Propert_ies");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "");
    xset_set_var(set, XSetVar::ICN, "gtk-properties");

    set = xset_set(XSetName::PROP_INFO, XSetVar::MENU_LABEL, "_Info");
    xset_set_var(set, XSetVar::ICN, "gtk-dialog-info");

    set = xset_set(XSetName::PROP_PERM, XSetVar::MENU_LABEL, "_Permissions");
    xset_set_var(set, XSetVar::ICN, "dialog-password");

    set = xset_set(XSetName::PROP_QUICK, XSetVar::MENU_LABEL, "_Quick");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "perm_r perm_rw perm_rwx perm_r_r perm_rw_r perm_rw_rw perm_rwxr_x perm_rwxrwx "
                 "perm_r_r_r perm_rw_r_r perm_rw_rw_rw perm_rwxr_r perm_rwxr_xr_x perm_rwxrwxrwx "
                 "perm_rwxrwxrwt perm_unstick perm_stick perm_recurs");

    xset_set(XSetName::PERM_R, XSetVar::MENU_LABEL, "r--------");
    xset_set(XSetName::PERM_RW, XSetVar::MENU_LABEL, "rw-------");
    xset_set(XSetName::PERM_RWX, XSetVar::MENU_LABEL, "rwx------");
    xset_set(XSetName::PERM_R_R, XSetVar::MENU_LABEL, "r--r-----");
    xset_set(XSetName::PERM_RW_R, XSetVar::MENU_LABEL, "rw-r-----");
    xset_set(XSetName::PERM_RW_RW, XSetVar::MENU_LABEL, "rw-rw----");
    xset_set(XSetName::PERM_RWXR_X, XSetVar::MENU_LABEL, "rwxr-x---");
    xset_set(XSetName::PERM_RWXRWX, XSetVar::MENU_LABEL, "rwxrwx---");
    xset_set(XSetName::PERM_R_R_R, XSetVar::MENU_LABEL, "r--r--r--");
    xset_set(XSetName::PERM_RW_R_R, XSetVar::MENU_LABEL, "rw-r--r--");
    xset_set(XSetName::PERM_RW_RW_RW, XSetVar::MENU_LABEL, "rw-rw-rw-");
    xset_set(XSetName::PERM_RWXR_R, XSetVar::MENU_LABEL, "rwxr--r--");
    xset_set(XSetName::PERM_RWXR_XR_X, XSetVar::MENU_LABEL, "rwxr-xr-x");
    xset_set(XSetName::PERM_RWXRWXRWX, XSetVar::MENU_LABEL, "rwxrwxrwx");
    xset_set(XSetName::PERM_RWXRWXRWT, XSetVar::MENU_LABEL, "rwxrwxrwt");
    xset_set(XSetName::PERM_UNSTICK, XSetVar::MENU_LABEL, "-t");
    xset_set(XSetName::PERM_STICK, XSetVar::MENU_LABEL, "+t");

    set = xset_set(XSetName::PERM_RECURS, XSetVar::MENU_LABEL, "_Recursive");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set, XSetVar::DESC, "perm_go_w perm_go_rwx perm_ugo_w perm_ugo_rx perm_ugo_rwx");

    xset_set(XSetName::PERM_GO_W, XSetVar::MENU_LABEL, "go-w");
    xset_set(XSetName::PERM_GO_RWX, XSetVar::MENU_LABEL, "go-rwx");
    xset_set(XSetName::PERM_UGO_W, XSetVar::MENU_LABEL, "ugo+w");
    xset_set(XSetName::PERM_UGO_RX, XSetVar::MENU_LABEL, "ugo+rX");
    xset_set(XSetName::PERM_UGO_RWX, XSetVar::MENU_LABEL, "ugo+rwX");

    set = xset_set(XSetName::PROP_ROOT, XSetVar::MENU_LABEL, "_Root");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "rperm_rw rperm_rwx rperm_rw_r rperm_rw_rw rperm_rwxr_x rperm_rwxrwx rperm_rw_r_r "
                 "rperm_rw_rw_rw rperm_rwxr_r rperm_rwxr_xr_x rperm_rwxrwxrwx rperm_rwxrwxrwt "
                 "rperm_unstick rperm_stick rperm_recurs rperm_own");
    xset_set_var(set, XSetVar::ICN, "gtk-dialog-warning");

    xset_set(XSetName::RPERM_RW, XSetVar::MENU_LABEL, "rw-------");
    xset_set(XSetName::RPERM_RWX, XSetVar::MENU_LABEL, "rwx------");
    xset_set(XSetName::RPERM_RW_R, XSetVar::MENU_LABEL, "rw-r-----");
    xset_set(XSetName::RPERM_RW_RW, XSetVar::MENU_LABEL, "rw-rw----");
    xset_set(XSetName::RPERM_RWXR_X, XSetVar::MENU_LABEL, "rwxr-x---");
    xset_set(XSetName::RPERM_RWXRWX, XSetVar::MENU_LABEL, "rwxrwx---");
    xset_set(XSetName::RPERM_RW_R_R, XSetVar::MENU_LABEL, "rw-r--r--");
    xset_set(XSetName::RPERM_RW_RW_RW, XSetVar::MENU_LABEL, "rw-rw-rw-");
    xset_set(XSetName::RPERM_RWXR_R, XSetVar::MENU_LABEL, "rwxr--r--");
    xset_set(XSetName::RPERM_RWXR_XR_X, XSetVar::MENU_LABEL, "rwxr-xr-x");
    xset_set(XSetName::RPERM_RWXRWXRWX, XSetVar::MENU_LABEL, "rwxrwxrwx");
    xset_set(XSetName::RPERM_RWXRWXRWT, XSetVar::MENU_LABEL, "rwxrwxrwt");
    xset_set(XSetName::RPERM_UNSTICK, XSetVar::MENU_LABEL, "-t");
    xset_set(XSetName::RPERM_STICK, XSetVar::MENU_LABEL, "+t");

    set = xset_set(XSetName::RPERM_RECURS, XSetVar::MENU_LABEL, "_Recursive");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(set,
                 XSetVar::DESC,
                 "rperm_go_w rperm_go_rwx rperm_ugo_w rperm_ugo_rx rperm_ugo_rwx");

    xset_set(XSetName::RPERM_GO_W, XSetVar::MENU_LABEL, "go-w");
    xset_set(XSetName::RPERM_GO_RWX, XSetVar::MENU_LABEL, "go-rwx");
    xset_set(XSetName::RPERM_UGO_W, XSetVar::MENU_LABEL, "ugo+w");
    xset_set(XSetName::RPERM_UGO_RX, XSetVar::MENU_LABEL, "ugo+rX");
    xset_set(XSetName::RPERM_UGO_RWX, XSetVar::MENU_LABEL, "ugo+rwX");

    set = xset_set(XSetName::RPERM_OWN, XSetVar::MENU_LABEL, "_Owner");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(
        set,
        XSetVar::DESC,
        "own_myuser own_myuser_users own_user1 own_user1_users own_user2 own_user2_users own_root "
        "own_root_users own_root_myuser own_root_user1 own_root_user2 own_recurs");

    xset_set(XSetName::OWN_MYUSER, XSetVar::MENU_LABEL, "myuser");
    xset_set(XSetName::OWN_MYUSER_USERS, XSetVar::MENU_LABEL, "myuser:users");
    xset_set(XSetName::OWN_USER1, XSetVar::MENU_LABEL, "user1");
    xset_set(XSetName::OWN_USER1_USERS, XSetVar::MENU_LABEL, "user1:users");
    xset_set(XSetName::OWN_USER2, XSetVar::MENU_LABEL, "user2");
    xset_set(XSetName::OWN_USER2_USERS, XSetVar::MENU_LABEL, "user2:users");
    xset_set(XSetName::OWN_ROOT, XSetVar::MENU_LABEL, "root");
    xset_set(XSetName::OWN_ROOT_USERS, XSetVar::MENU_LABEL, "root:users");
    xset_set(XSetName::OWN_ROOT_MYUSER, XSetVar::MENU_LABEL, "root:myuser");
    xset_set(XSetName::OWN_ROOT_USER1, XSetVar::MENU_LABEL, "root:user1");
    xset_set(XSetName::OWN_ROOT_USER2, XSetVar::MENU_LABEL, "root:user2");

    set = xset_set(XSetName::OWN_RECURS, XSetVar::MENU_LABEL, "_Recursive");
    set->menu_style = XSetMenu::SUBMENU;
    xset_set_var(
        set,
        XSetVar::DESC,
        "rown_myuser rown_myuser_users rown_user1 rown_user1_users rown_user2 rown_user2_users "
        "rown_root rown_root_users rown_root_myuser rown_root_user1 rown_root_user2");

    xset_set(XSetName::ROWN_MYUSER, XSetVar::MENU_LABEL, "myuser");
    xset_set(XSetName::ROWN_MYUSER_USERS, XSetVar::MENU_LABEL, "myuser:users");
    xset_set(XSetName::ROWN_USER1, XSetVar::MENU_LABEL, "user1");
    xset_set(XSetName::ROWN_USER1_USERS, XSetVar::MENU_LABEL, "user1:users");
    xset_set(XSetName::ROWN_USER2, XSetVar::MENU_LABEL, "user2");
    xset_set(XSetName::ROWN_USER2_USERS, XSetVar::MENU_LABEL, "user2:users");
    xset_set(XSetName::ROWN_ROOT, XSetVar::MENU_LABEL, "root");
    xset_set(XSetName::ROWN_ROOT_USERS, XSetVar::MENU_LABEL, "root:users");
    xset_set(XSetName::ROWN_ROOT_MYUSER, XSetVar::MENU_LABEL, "root:myuser");
    xset_set(XSetName::ROWN_ROOT_USER1, XSetVar::MENU_LABEL, "root:user1");
    xset_set(XSetName::ROWN_ROOT_USER2, XSetVar::MENU_LABEL, "root:user2");

    // PANELS
    for (const panel_t p : PANELS)
    {
        set = xset_set_panel(p, XSetPanel::SHOW_TOOLBOX, XSetVar::MENU_LABEL, "_Toolbar");
        set->menu_style = XSetMenu::CHECK;
        set->b = XSetB::XSET_B_TRUE;
        if (p != 1)
        {
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_show_toolbox");
        }

        set = xset_set_panel(p, XSetPanel::SHOW_DEVMON, XSetVar::MENU_LABEL, "_Devices");
        set->menu_style = XSetMenu::CHECK;
        set->b = XSetB::XSET_B_UNSET;
        if (p != 1)
        {
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_show_devmon");
        }

        set = xset_set_panel(p, XSetPanel::SHOW_DIRTREE, XSetVar::MENU_LABEL, "T_ree");
        set->menu_style = XSetMenu::CHECK;
        set->b = XSetB::XSET_B_TRUE;
        if (p != 1)
        {
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_show_dirtree");
        }

        set = xset_set_panel(p, XSetPanel::SHOW_SIDEBAR, XSetVar::MENU_LABEL, "_Side Toolbar");
        set->menu_style = XSetMenu::CHECK;
        set->b = XSetB::XSET_B_UNSET;
        if (p != 1)
        {
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_show_sidebar");
        }

        set = xset_set_panel(p, XSetPanel::LIST_DETAILED, XSetVar::MENU_LABEL, "_Detailed");
        set->menu_style = XSetMenu::RADIO;
        set->b = XSetB::XSET_B_TRUE;
        if (p != 1)
        {
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_list_detailed");
        }

        set = xset_set_panel(p, XSetPanel::LIST_ICONS, XSetVar::MENU_LABEL, "_Icons");
        set->menu_style = XSetMenu::RADIO;
        if (p != 1)
        {
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_list_icons");
        }

        set = xset_set_panel(p, XSetPanel::LIST_COMPACT, XSetVar::MENU_LABEL, "_Compact");
        set->menu_style = XSetMenu::RADIO;
        if (p != 1)
        {
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_list_compact");
        }

        set = xset_set_panel(p, XSetPanel::LIST_LARGE, XSetVar::MENU_LABEL, "_Large Icons");
        set->menu_style = XSetMenu::CHECK;
        if (p != 1)
        {
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_list_large");
        }

        set = xset_set_panel(p, XSetPanel::SHOW_HIDDEN, XSetVar::MENU_LABEL, "_Hidden Files");
        set->menu_style = XSetMenu::CHECK;
        if (p != 1)
        {
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_show_hidden");
        }

        set = xset_set_panel(p, XSetPanel::ICON_TAB, XSetVar::MENU_LABEL, "_Icon");
        set->menu_style = XSetMenu::ICON;
        xset_set_var(set, XSetVar::ICN, "gtk-directory");

        set = xset_set_panel(p, XSetPanel::ICON_STATUS, XSetVar::MENU_LABEL, "_Icon");
        set->menu_style = XSetMenu::ICON;
        xset_set_var(set, XSetVar::ICN, "gtk-yes");
        if (p != 1)
        {
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_icon_status");
        }

        set = xset_set_panel(p, XSetPanel::DETCOL_NAME, XSetVar::MENU_LABEL, "_Name");
        set->menu_style = XSetMenu::CHECK;
        set->b = XSetB::XSET_B_TRUE; // visible
        set->x = ztd::strdup("0");   // position

        set = xset_set_panel(p, XSetPanel::DETCOL_SIZE, XSetVar::MENU_LABEL, "_Size");
        set->menu_style = XSetMenu::CHECK;
        set->b = XSetB::XSET_B_TRUE;
        set->x = ztd::strdup("1");
        if (p != 1)
        {
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_detcol_size");
        }

        set = xset_set_panel(p, XSetPanel::DETCOL_TYPE, XSetVar::MENU_LABEL, "_Type");
        set->menu_style = XSetMenu::CHECK;
        set->x = ztd::strdup("2");
        if (p != 1)
        {
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_detcol_type");
        }

        set = xset_set_panel(p, XSetPanel::DETCOL_PERM, XSetVar::MENU_LABEL, "_Permission");
        set->menu_style = XSetMenu::CHECK;
        set->x = ztd::strdup("3");
        if (p != 1)
        {
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_detcol_perm");
        }

        set = xset_set_panel(p, XSetPanel::DETCOL_OWNER, XSetVar::MENU_LABEL, "_Owner");
        set->menu_style = XSetMenu::CHECK;
        set->x = ztd::strdup("4");
        if (p != 1)
        {
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_detcol_owner");
        }

        set = xset_set_panel(p, XSetPanel::DETCOL_DATE, XSetVar::MENU_LABEL, "_Modified");
        set->menu_style = XSetMenu::CHECK;
        set->x = ztd::strdup("5");
        if (p != 1)
        {
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_detcol_date");
        }

        set = xset_get_panel(p, XSetPanel::SORT_EXTRA);
        set->b = XSetB::XSET_B_TRUE;               // sort_natural
        set->x = ztd::strdup(XSetB::XSET_B_FALSE); // sort_case
        set->y = ztd::strdup("1");                 // PTKFileListSortDir::PTK_LIST_SORT_DIR_FIRST
        set->z = ztd::strdup(XSetB::XSET_B_TRUE);  // sort_hidden_first

        set = xset_set_panel(p, XSetPanel::BOOK_FOL, XSetVar::MENU_LABEL, "Follow _Dir");
        set->menu_style = XSetMenu::CHECK;
        set->b = XSetB::XSET_B_TRUE;
        if (p != 1)
        {
            xset_set_var(set, XSetVar::SHARED_KEY, "panel1_book_fol");
        }
    }

    // speed
    set = xset_set(XSetName::BOOK_NEWTAB, XSetVar::MENU_LABEL, "_New Tab");
    set->menu_style = XSetMenu::CHECK;

    set = xset_set(XSetName::BOOK_SINGLE, XSetVar::MENU_LABEL, "_Single Click");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::DEV_NEWTAB, XSetVar::MENU_LABEL, "_New Tab");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    set = xset_set(XSetName::DEV_SINGLE, XSetVar::MENU_LABEL, "_Single Click");
    set->menu_style = XSetMenu::CHECK;
    set->b = XSetB::XSET_B_TRUE;

    // mark all labels and icons as default
    for (xset_t set2 : xsets)
    {
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
def_key(XSetName name, u32 key, u32 keymod)
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
    for (xset_t set : xsets)
    {
        if (set->key)
        {
            keysets.emplace_back(set);
        }
    }

    // clang-format off

    def_key(XSetName::TAB_PREV, GDK_KEY_Tab, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    def_key(XSetName::TAB_NEXT, GDK_KEY_Tab, GdkModifierType::GDK_CONTROL_MASK);
    def_key(XSetName::TAB_NEW, GDK_KEY_t, GdkModifierType::GDK_CONTROL_MASK);
    def_key(XSetName::TAB_RESTORE, GDK_KEY_T, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    def_key(XSetName::TAB_CLOSE, GDK_KEY_w, GdkModifierType::GDK_CONTROL_MASK);
    def_key(XSetName::TAB_1, GDK_KEY_1, GdkModifierType::GDK_MOD1_MASK);
    def_key(XSetName::TAB_2, GDK_KEY_2, GdkModifierType::GDK_MOD1_MASK);
    def_key(XSetName::TAB_3, GDK_KEY_3, GdkModifierType::GDK_MOD1_MASK);
    def_key(XSetName::TAB_4, GDK_KEY_4, GdkModifierType::GDK_MOD1_MASK);
    def_key(XSetName::TAB_5, GDK_KEY_5, GdkModifierType::GDK_MOD1_MASK);
    def_key(XSetName::TAB_6, GDK_KEY_6, GdkModifierType::GDK_MOD1_MASK);
    def_key(XSetName::TAB_7, GDK_KEY_7, GdkModifierType::GDK_MOD1_MASK);
    def_key(XSetName::TAB_8, GDK_KEY_8, GdkModifierType::GDK_MOD1_MASK);
    def_key(XSetName::TAB_9, GDK_KEY_9, GdkModifierType::GDK_MOD1_MASK);
    def_key(XSetName::TAB_10, GDK_KEY_0, GdkModifierType::GDK_MOD1_MASK);
    def_key(XSetName::EDIT_CUT, GDK_KEY_x, GdkModifierType::GDK_CONTROL_MASK);
    def_key(XSetName::EDIT_COPY, GDK_KEY_c, GdkModifierType::GDK_CONTROL_MASK);
    def_key(XSetName::EDIT_PASTE, GDK_KEY_v, GdkModifierType::GDK_CONTROL_MASK);
    def_key(XSetName::EDIT_RENAME, GDK_KEY_F2, 0);
    def_key(XSetName::EDIT_DELETE, GDK_KEY_Delete, GdkModifierType::GDK_SHIFT_MASK);
    def_key(XSetName::EDIT_TRASH, GDK_KEY_Delete, 0);
    def_key(XSetName::COPY_NAME, GDK_KEY_C, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_MOD1_MASK));
    def_key(XSetName::COPY_PATH, GDK_KEY_C, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    def_key(XSetName::PASTE_LINK, GDK_KEY_V, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    def_key(XSetName::PASTE_AS, GDK_KEY_A, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    def_key(XSetName::SELECT_ALL, GDK_KEY_A, GdkModifierType::GDK_CONTROL_MASK);
    def_key(XSetName::MAIN_TERMINAL, GDK_KEY_F4, 0);
    def_key(XSetName::GO_DEFAULT, GDK_KEY_Escape, 0);
    def_key(XSetName::GO_BACK, GDK_KEY_Left, GdkModifierType::GDK_MOD1_MASK);
    def_key(XSetName::GO_FORWARD, GDK_KEY_Right, GdkModifierType::GDK_MOD1_MASK);
    def_key(XSetName::GO_UP, GDK_KEY_Up, GdkModifierType::GDK_MOD1_MASK);
    def_key(XSetName::FOCUS_PATH_BAR, GDK_KEY_l, GdkModifierType::GDK_CONTROL_MASK);
    def_key(XSetName::VIEW_REFRESH, GDK_KEY_F5, 0);
    def_key(XSetName::PROP_INFO, GDK_KEY_Return, GdkModifierType::GDK_MOD1_MASK);
    def_key(XSetName::PROP_PERM, GDK_KEY_p, GdkModifierType::GDK_CONTROL_MASK);
    def_key(XSetName::PANEL1_SHOW_HIDDEN, GDK_KEY_h, GdkModifierType::GDK_CONTROL_MASK);
    def_key(XSetName::BOOK_NEW, GDK_KEY_d, GdkModifierType::GDK_CONTROL_MASK);
    def_key(XSetName::NEW_FILE, GDK_KEY_F, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    def_key(XSetName::NEW_DIRECTORY, GDK_KEY_N, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    def_key(XSetName::NEW_LINK, GDK_KEY_L, (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK));
    def_key(XSetName::MAIN_NEW_WINDOW, GDK_KEY_n, GdkModifierType::GDK_CONTROL_MASK);
    def_key(XSetName::OPEN_ALL, GDK_KEY_F6, 0);
    def_key(XSetName::MAIN_FULL, GDK_KEY_F11, 0);
    def_key(XSetName::PANEL1_SHOW, GDK_KEY_1, GdkModifierType::GDK_CONTROL_MASK);
    def_key(XSetName::PANEL2_SHOW, GDK_KEY_2, GdkModifierType::GDK_CONTROL_MASK);
    def_key(XSetName::PANEL3_SHOW, GDK_KEY_3, GdkModifierType::GDK_CONTROL_MASK);
    def_key(XSetName::PANEL4_SHOW, GDK_KEY_4, GdkModifierType::GDK_CONTROL_MASK);
    // def_key(XSetName::MAIN_HELP, GDK_KEY_F1, 0);
    def_key(XSetName::MAIN_EXIT, GDK_KEY_q, GdkModifierType::GDK_CONTROL_MASK);
    def_key(XSetName::MAIN_PREFS, GDK_KEY_F12, 0);
    def_key(XSetName::BOOK_ADD, GDK_KEY_d, GdkModifierType::GDK_CONTROL_MASK);

    // clang-format on
}
