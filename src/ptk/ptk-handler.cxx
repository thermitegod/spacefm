/**
 * Copyright (C) 2014-2015 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2013-2015 OmegaPhil <OmegaPhil@startmail.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *
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
#include <filesystem>

#include <vector>

#include <iostream>
#include <fstream>

#include <fnmatch.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "ptk/ptk-handler.hxx"
#include "ptk/ptk-utils.hxx"

#include "autosave.hxx"
#include "utils.hxx"

#define MOUNT_EXAMPLE                                                                            \
    "# Enter mount command or leave blank for auto:\n\n\n# # Examples: (remove # to enable a "   \
    "mount command)\n#\n# # udevil:\n#     udevil mount -o '%o' %v\n#\n# # pmount: (does not "   \
    "accept mount options)\n#     pmount %v\n#\n# # udisks v2:\n#     udisksctl mount -b %v -o " \
    "'%o'\n"

#define UNMOUNT_EXAMPLE                                                                           \
    "# Enter unmount command or leave blank for auto:\n\n\n# # Examples: (remove # to enable an " \
    "unmount command)\n#\n# # udevil:\n#     udevil umount %v\n#\n# # pmount:\n#     pumount "    \
    "%v\n#\n# # udisks v2:\n#     udisksctl unmount -b %v\n#\n"

#define INFO_EXAMPLE                                                                            \
    "# Enter command to show properties or leave blank for auto:\n\n\n# # Example:\n\n# echo "  \
    "MOUNT\n# mount | grep \" on %a \"\n# echo\n# echo PROCESSES\n# /usr/bin/lsof -w \"%a\" | " \
    "head -n 500\n"

enum PtkHandlerJob
{
    HANDLER_JOB_EXPORT,
    HANDLER_JOB_IMPORT_FILE,
    HANDLER_JOB_RESTORE_ALL,
    HANDLER_JOB_REMOVE
};

// Archive handlers treeview model enum
enum PtkHandlerCol
{
    COL_XSET_NAME,
    COL_HANDLER_NAME
};

// Archive creation handlers combobox model enum
enum PtkHandlerExtensionsCol
{
    COL_HANDLER_EXTENSIONS = 1
};

// clang-format off
// xset name prefixes of default handlers
static const char* handler_def_prefix[] =
{
    "hand_arc_+",
    "hand_fs_+",
    "hand_net_+",
    "hand_f_+"
};

// xset name prefixes of custom handlers
static const char* handler_cust_prefix[] =
{
    "hand_arc_",
    "hand_fs_",
    "hand_net_",
    "hand_f_"
};

static const char* handler_conf_xset[] =
{
    "arc_conf2",
    "dev_fs_cnf",
    "dev_net_cnf",
    "open_hand"
};

static const char* dialog_titles[] =
{
    "Archive Handlers",
    "Device Handlers",
    "Protocol Handlers",
    "File Handlers",
};

static const char* dialog_mnemonics[] =
{
    "Archive Hand_lers",
    "Device Hand_lers",
    "Protocol Hand_lers",
    "File Hand_lers",
};

static const char* modes[] =
{
    "archive",
    "device",
    "protocol",
    "file"
};

static const char* cmds_arc[] =
{
    "compress",
    "extract",
    "list"
};

static const char* cmds_mnt[] =
{
    "mount",
    "unmount",
    "info"
};
// clang-format on

/* don't change this script header or it will break header detection on
 * existing scripts! */
static const char* script_header = "#!/bin/bash\n";

struct HandlerData
{
    GtkWidget* dlg;
    GtkWidget* parent;
    int mode;
    bool changed;
    PtkFileBrowser* browser;

    GtkWidget* view_handlers;
    GtkListStore* list;

    GtkWidget* chkbtn_handler_enabled;
    GtkWidget* entry_handler_name;
    GtkWidget* entry_handler_mime;
    GtkWidget* entry_handler_extension;
    GtkWidget* entry_handler_icon;
    GtkWidget* view_handler_compress;
    GtkWidget* view_handler_extract;
    GtkWidget* view_handler_list;
    GtkTextBuffer* buf_handler_compress;
    GtkTextBuffer* buf_handler_extract;
    GtkTextBuffer* buf_handler_list;
    bool compress_changed;
    bool extract_changed;
    bool list_changed;
    GtkWidget* chkbtn_handler_compress_term;
    GtkWidget* chkbtn_handler_extract_term;
    GtkWidget* chkbtn_handler_list_term;
    GtkWidget* btn_remove;
    GtkWidget* btn_add;
    GtkWidget* btn_apply;
    GtkWidget* btn_up;
    GtkWidget* btn_down;
    GtkWidget* btn_ok;
    GtkWidget* btn_cancel;
    GtkWidget* btn_defaults;
    GtkWidget* btn_defaults0;
    GtkWidget* icon_choose_btn;
};

struct Handler
{
    // enabled              set->b
    const char* xset_name;    //                      set->name
    const char* handler_name; //                      set->menu_label
    const char* type;         // or whitelist         set->s
    const char* ext;          // or blacklist         set->x
    const char* compress_cmd; // or mount             (script)
    bool compress_term;       //                      set->in_terminal
    const char* extract_cmd;  // or unmount           (script)
    bool extract_term;        // or run task file     set->keep_terminal
    const char* list_cmd;     // or info              (script)
    bool list_term;           //                      set->scroll_lock
    /*  save as custom item                                 set->lock = false
        if handler equals default, don't save in session    set->disable = true
        icon (file handlers only)                           set->icon
    */
};

/* If you add a new handler, add it to (end of ) existing session file handler
 * list so existing users see the new handler. */
static const Handler handlers_arc[] = {
    /* In compress commands:
     *   %n: First selected filename/dir to archive
     *   %N: All selected filenames/dirs to archive, or (with %O) a single filename
     *   %o: Resulting single archive file
     *   %O: Resulting archive per source file/directory (use changes %N meaning)
     *
     * In extract commands:
     *   %x: Archive file to extract
     *   %g: Unique extraction target filename with optional subfolder
     *   %G: Unique extraction target filename, never with subfolder
     *
     * In list commands:
     *     %x: Archive to list
     *
     * Plus standard bash variables are accepted.
     */
    {"hand_arc_+7z",
     "7-Zip",
     "application/x-7z-compressed",
     "*.7z",
     "\"$(which 7za || echo 7zr)\" a %o %N",
     true,
     "\"$(which 7za || echo 7zr)\" x %x",
     true,
     "\"$(which 7za || echo 7zr)\" l %x",
     false},
    {"hand_arc_+rar",
     "RAR",
     "application/x-rar",
     "*.rar *.RAR",
     "rar a -r %o %N",
     true,
     "unrar -o- x %x",
     true,
     "unrar lt %x",
     false},
    {"hand_arc_+tar",
     "Tar",
     "application/x-tar",
     "*.tar",
     "tar -cvf %o %N",
     false,
     "tar -xvf %x",
     false,
     "tar -tvf %x",
     false},
    {"hand_arc_+tar_bz2",
     "Tar bzip2",
     "application/x-bzip-compressed-tar",
     "*.tar.bz2",
     "tar -cvjf %o %N",
     false,
     "tar -xvjf %x",
     false,
     "tar -tvf %x",
     false},
    {"hand_arc_+tar_gz",
     "Tar Gzip",
     "application/x-compressed-tar",
     "*.tar.gz *.tgz",
     "tar -cvzf %o %N",
     false,
     "tar -xvzf %x",
     false,
     "tar -tvf %x",
     false},
    {"hand_arc_+tar_xz",
     "Tar xz",
     "application/x-xz-compressed-tar",
     "*.tar.xz *.txz",
     "tar -cvJf %o %N",
     false,
     "tar -xvJf %x",
     false,
     "tar -tvf %x",
     false},
    {"hand_arc_+zip",
     "Zip",
     "application/x-zip application/zip",
     "*.zip *.ZIP",
     "zip -r %o %N",
     true,
     "unzip %x",
     true,
     "unzip -l %x",
     false},
    {"hand_arc_+gz",
     "Gzip",
     "application/x-gzip application/x-gzpdf application/gzip",
     "*.gz",
     "gzip -c %N >| %O",
     false,
     "gzip -cd %x >| %G",
     false,
     "gunzip -l %x",
     false},
    {"hand_arc_+xz",
     "XZ",
     "application/x-xz",
     "*.xz",
     "xz -cz %N >| %O",
     false,
     "xz -cd %x >| %G",
     false,
     "xz -tv %x",
     false},
    {"hand_arc_+tar_lz4",
     "Tar Lz4",
     "application/x-lz4-compressed-tar",
     "*.tar.lz4",
     "tar -I lz4 -cvf %o %N",
     false,
     "tar -I lz4 -xvf %x",
     false,
     "lz4 -dc %x | tar tvf -",
     false},
    {"hand_arc_+lz4",
     "Lz4",
     "application/x-lz4",
     "*.lz4",
     "lz4 -c %N >| %O",
     false,
     "lz4 -d -c %x >| %G",
     false,
     "lz4 -tv %x",
     false},
    {"hand_arc_+tar_zst",
     "Tar Zstd",
     "application/x-zstd-compressed-tar",
     "*.tar.zst",
     "tar -I 'zstd --long=31' -cvf %o %N",
     false,
     "zstd -dc --long=31 %x | tar xvf -",
     false,
     "zstd -dc --long=31 %x | tar tvf -",
     false},
    {"hand_arc_+zst",
     "Zstd",
     "application/zstd",
     "*.zst",
     "zstd -c --long=31 %N >| %O",
     false,
     "zstd -dc --long=31 -d %x >| %G",
     false,
     "zstd -dc --long=31 -tlv %x",
     false}};

const Handler handlers_fs[] = {
    /* In commands:
     *      %v  device
     *      %o  volume-specific mount options (use in mount command only)
     *      %a  mount point, or create auto mount point
     *  Plus standard substitution variables are accepted.
     *
     *  Whitelist/Blacklist: (prefix list element with '+' if required)
     *      fstype (eg ext3)
     *      dev=DEVICE (/dev/sdd1)
     *      id=UDI
     *      label=VOLUME_LABEL (includes spaces as underscores)
     *      point=MOUNT_POINT
     *      audiocd=0 or 1
     *      optical=0 or 1
     *      removable=0 or 1
     *      mountable=0 or 1
     *
     *      eg: +ext3 dev=/dev/sdb* id=ata-* label=Label_With_Spaces
     */
    {"hand_fs_+fuseiso",
     "fuseiso unmount",
     "*fuseiso",
     "",
     "# Mounting of iso files is performed by fuseiso in a file handler,\n# not this device "
     "handler.  Right-click on any file and select\n# Open|File Handlers, and select Mount ISO to "
     "see this command.",
     false,
     "fusermount -u \"%a\"",
     false,
     "grep \"%a\" ~/.mtab.fuseiso",
     false},
    {"hand_fs_+udiso",
     "udevil iso unmount",
     "+iso9660 +dev=/dev/loop*",
     "optical=1 removable=1",
     "# Mounting of iso files is performed by udevil in a file handler,\n# not this device "
     "handler.  Right-click on any file and select\n# Open|File Handlers, and select Mount ISO to "
     "see this command.",
     false,
     "# Note: non-iso9660 types will fall through to Default unmount handler\nudevil umount "
     "\"%a\"\n",
     false,
     INFO_EXAMPLE,
     false},
    {"hand_fs_+def",
     "Default",
     "*",
     "",
     MOUNT_EXAMPLE,
     false,
     UNMOUNT_EXAMPLE,
     false,
     INFO_EXAMPLE,
     false}};

static const Handler handlers_net[] = {
    /* In commands:
     *       %url%     $fm_url
     *       %proto%   $fm_url_proto
     *       %host%    $fm_url_host
     *       %port%    $fm_url_port
     *       %user%    $fm_url_user
     *       %pass%    $fm_url_pass
     *       %path%    $fm_url_path
     *       %a        mount point, or create auto mount point
     *                 $fm_mtab_fs   (mounted mtab fs type)
     *                 $fm_mtab_url  (mounted mtab url)
     *
     *  Whitelist/Blacklist: (prefix list element with '+' if required)
     *      protocol (eg ssh)
     *      url=URL (ssh://...)
     *      mtab_fs=TYPE    (mounted mtab fs type)
     *      mtab_url=URL    (mounted mtab url)
     *      host=HOSTNAME
     *      user=USERNAME
     *      point=MOUNT_POINT
     *
     *      eg: +ssh url=ssh://...
     */
    {"hand_net_+http",
     "http & webdav",
     "http https webdav davfs davs dav mtab_fs=davfs*",
     "",
     "# This handler opens http:// and webdav://\n\n# Set your web browser in "
     "Help|Options|Browser\n\n# set missing_davfs=1 if you always want to open http in web "
     "browser\n# set missing_davfs=0 if you always want to mount http with "
     "davfs\nmissing_davfs=\n\nif [ -z \"$missing_davfs\" ];then\n    grep -qs "
     "'^[[:space:]]*allowed_types[[:space:]]*=[^#]*davfs' \\\n                                    "
     "/etc/udevil/udevil.conf 2>/dev/null\n    missing_davfs=$?  \nfi\nif [ \"$fm_url_proto\" = "
     "\"webdav\" ] || [ \"$fm_url_proto\" = \"davfs\" ] || \\\n   [ \"$fm_url_proto\" = \"dav\" ] "
     "|| [ \"$fm_url_proto\" = \"davs\" ] || \\\n                                    [ "
     "$missing_davfs -eq 0 ];then\n    fm_url_proto=\"${fm_url_proto/webdav/http}\"\n    "
     "fm_url_proto=\"${fm_url_proto/davfs/http}\"\n    "
     "fm_url_proto=\"${fm_url_proto/davs/https}\"\n    fm_url_proto=\"${fm_url_proto/dav/http}\"\n "
     "   "
     "url=\"${fm_url_proto}://${fm_url_host}${fm_url_port:+:}${fm_url_port}${fm_url_path:-/}\"\n   "
     " [[ -z \"$fm_url_user$fm_url_password\" ]] && msg=\"\" || \\\n            msg=\"Warning: "
     "user:password in URL is not supported by davfs.\"\n    # attempt davfs mount in terminal\n   "
     " spacefm -s run-task cmd --terminal \\\n        \"echo $msg; echo 'udevil mount $url'; "
     "udevil mount '$url' || \" \\\n                        \"( echo; echo 'Press Enter to "
     "close:'; read )\"\n    exit\nfi\n# open in web browser\nspacefm -s run-task web "
     "\"$fm_url\"\n",
     false,
     "# Note: Unmount is usually performed by the 'fuse unmount' handler.\n\nudevil umount \"%a\"",
     false,
     INFO_EXAMPLE,
     false},
    {"hand_net_+ftp",
     "ftp",
     "ftp",
     "",
     "options=\"nonempty\"\nif [ -n \"%user%\" ];then\n    user=\",user=%user%\"\n    [[ -n "
     "\"%pass%\" ]] && user=\"$user:%pass%\"\nfi\n[[ -n \"%port%\" ]] && portcolon=:\necho \">>> "
     "curlftpfs -o $options$user ftp://%host%${portcolon}%port%%path% %a\"\necho\ncurlftpfs -o "
     "$options$user ftp://%host%${portcolon}%port%%path% \"%a\"\n[[ $? -eq 0 ]] && sleep 1 && ls "
     "\"%a\"  # set error status or wait until ready\n",
     true,
     "fusermount -u \"%a\"",
     false,
     INFO_EXAMPLE,
     false},
    {"hand_net_+ssh",
     "ssh",
     "ssh sftp mtab_fs=fuse.sshfs",
     "",
     "[[ -n \"$fm_url_user\" ]] && fm_url_user=\"${fm_url_user}@\"\n[[ -z \"$fm_url_port\" ]] && "
     "fm_url_port=22\necho \">>> sshfs -p $fm_url_port $fm_url_user$fm_url_host:$fm_url_path "
     "%a\"\necho\n# Run sshfs through nohup to prevent disconnect on terminal "
     "close\nsshtmp=\"$(mktemp --tmpdir spacefm-ssh-output-XXXXXXXX.tmp)\" || exit 1\nnohup sshfs "
     "-p $fm_url_port $fm_url_user$fm_url_host:$fm_url_path %a &> \"$sshtmp\"\nerr=$?\n[[ -e "
     "\"$sshtmp\" ]] && cat \"$sshtmp\" ; rm -f \"$sshtmp\"\n[[ $err -eq 0 ]]  # set error "
     "status\n\n# Alternate Method - if enabled, disable nohup line above and\n#                   "
     " uncheck Run In Terminal\n# # Run sshfs in a terminal without SpaceFM task.  sshfs "
     "disconnects when the\n# # terminal is closed\n# spacefm -s run-task cmd --terminal \"echo "
     "'Connecting to $fm_url'; echo; sshfs -p $fm_url_port $fm_url_user$fm_url_host:$fm_url_path "
     "%a; if [ $? -ne 0 ];then echo; echo '[ Finished ] Press Enter to close'; else echo; echo "
     "'Press Enter to close (closing this window may unmount sshfs)'; fi; read\" & sleep 1\n",
     true,
     "fusermount -u \"%a\"",
     false,
     INFO_EXAMPLE,
     false},
    {"hand_net_+mtp",
     "mtp",
     "mtp mtab_fs=fuse.jmtpfs mtab_fs=fuse.simple-mtpfs mtab_fs=fuse.mtpfs mtab_fs=fuse.DeviceFs(*",
     "",
     "mtpmount=\"$(which jmtpfs || which simple-mtpfs || which mtpfs || which go-mtpfs)\"\nif [ -z "
     "\"$mtpmount\" ];then\n    echo \"To mount mtp:// you must install jmtpfs, simple-mtpfs, "
     "mtpfs, or go-mtpfs,\"\n    echo \"or add a custom protocol handler.\"\n    exit 1\nelif [ "
     "\"${mtpmount##*/}\" = \"go-mtpfs\" ];then\n    # Run go-mtpfs in background, as it does not "
     "exit after mount\n    outputtmp=\"$(mktemp --tmpdir spacefm-go-mtpfs-output-XXXXXXXX)\" || "
     "exit 1\n    go-mtpfs \"%a\" &> \"$outputtmp\" &\n    sleep 2s\n    [[ -e \"$outputtmp\" ]] "
     "&& cat \"$outputtmp\" ; rm -f \"$outputtmp\"\n    # set success status only if positive that "
     "mountpoint is mountpoint\n    mountpoint \"%a\"\nelse\n    $mtpmount \"%a\"\nfi\n",
     false,
     "fusermount -u \"%a\"",
     false,
     INFO_EXAMPLE,
     false},
    {"hand_net_+gphoto",
     "ptp",
     "ptp gphoto mtab_fs=fuse.gphotofs",
     "",
     "gphotofs \"%a\"",
     false,
     "fusermount -u \"%a\"",
     false,
     INFO_EXAMPLE,
     false},
    {"hand_net_+ifuse",
     "ifuse",
     "ifuse ios mtab_fs=fuse.ifuse",
     "",
     "ifuse \"%a\"",
     false,
     "fusermount -u \"%a\"",
     false,
     INFO_EXAMPLE,
     false},
    {"hand_net_+udevil",
     "udevil",
     "ftp http https nfs ssh mtab_fs=fuse.sshfs mtab_fs=davfs*",
     "",
     "udevil mount \"$fm_url\"",
     true,
     "udevil umount \"%a\"",
     false,
     INFO_EXAMPLE,
     false},
    {"hand_net_+udevilsmb",
     "udevil-smb",
     "smb mtab_fs=cifs",
     "",
     "UDEVIL_RESULT=\"$(udevil mount \"$fm_url\" | grep Mounted)\"\n[ -n \"$UDEVIL_RESULT\" ] && "
     "spacefm -s set new_tab \"${UDEVIL_RESULT#* at }\"",
     true,
     "udevil umount \"%a\"",
     false,
     INFO_EXAMPLE,
     false},
    {"hand_net_+fusesmb",
     "fusesmb",
     "smb mtab_fs=fuse.fusesmb",
     "",
     "fusesmb \"%a\"",
     true,
     "fusermount -u \"%a\"",
     false,
     INFO_EXAMPLE,
     false},
    {"hand_net_+fuse",
     "fuse unmount",
     "mtab_fs=fuse.* mtab_fs=fuse",
     "",
     "",
     false,
     "fusermount -u \"%a\"",
     false,
     INFO_EXAMPLE,
     false}};

static const Handler handlers_file[] = {
    /* %a custom mount point
     * Plus standard bash variables are accepted.
     * For file handlers, extract_term is used for Run As Task. */
    {"hand_f_+iso",
     "Mount ISO",
     "application/x-iso9660-image application/x-iso-image application/x-cd-image",
     "*.img *.iso *.mdf *.nrg",
     "# Note: Unmounting of iso files is performed by the fuseiso or udevil device\n# handler, not "
     "this file handler.\n\n# Use fuseiso or udevil ?\nfuse=\"$(which fuseiso)\"  # remove this "
     "line to use udevil only\nif [[ -z \"$fuse\" ]];then\n    udevil=\"$(which udevil)\"\n    if "
     "[[ -z \"$udevil\" ]];then\n        echo \"You must install fuseiso or udevil to mount ISOs "
     "with this handler.\"\n        exit 1\n    fi\n    # use udevil - attempt mount\n    "
     "uout=\"$($udevil mount \"$fm_file\" 2>&1)\"\n    err=$?; echo \"$uout\"\n    if [ $err -eq 2 "
     "];then\n        # is file already mounted? (english only)\n        point=\"${uout#* is "
     "already mounted at }\"\n        if [ \"$point\" != \"$uout\" ];then\n            "
     "point=\"${point% (*}\"\n            if [ -x \"$point\" ];then\n                spacefm -t "
     "\"$point\"\n                exit 0\n            fi\n        fi\n    fi\n    [[ $err -ne 0 ]] "
     "&& exit 1\n    point=\"${uout#Mounted }\"\n    [[ \"$point\" = \"$uout\" ]] && exit 0\n    "
     "point=\"${point##* at }\"\n    [[ -d \"$point\" ]] && spacefm \"$point\" &\n    exit "
     "0\nfi\n# use fuseiso - is file already mounted?\ncanon=\"$(readlink -f \"$fm_file\" "
     "2>/dev/null)\"\nif [ -n \"$canon\" ];then\n    canon_enc=\"${canon// /\\\\040}\" # encode "
     "spaces for mtab+grep\n    if grep -q \"^$canon_enc \" ~/.mtab.fuseiso 2>/dev/null;then\n    "
     "    # file is mounted - get mount point\n        point=\"$(grep -m 1 \"^$canon_enc \" "
     "~/.mtab.fuseiso \\\n                 | sed 's/.* \\(.*\\) fuseiso .*/\\1/' )\"\n    if [ -x "
     "\"$point\" ];then\n            spacefm \"$point\" &\n            exit\n        fi\n    "
     "fi\nfi\n# mount & open\nfuseiso %f %a && spacefm %a &\n",
     false,
     "",
     true, // Run As Task
     "",
     false}};

// Function prototypes
static void on_configure_handler_enabled_check(GtkToggleButton* togglebutton, HandlerData* hnd);
static void restore_defaults(HandlerData* hnd, bool all);
static bool validate_archive_handler(HandlerData* hnd);
static void on_options_button_clicked(GtkWidget* btn, HandlerData* hnd);

bool
ptk_handler_command_is_empty(const std::string& command)
{
    const char* check_command = command.c_str();

    // test if command contains only comments and whitespace
    if (!check_command)
        return true;
    char** lines = g_strsplit(check_command, "\n", 0);
    if (!lines)
        return true;

    int i;
    bool found = false;
    for (i = 0; lines[i]; i++)
    {
        g_strstrip(lines[i]);
        if (lines[i][0] != '\0' && lines[i][0] != '#')
        {
            found = true;
            break;
        }
    }
    g_strfreev(lines);
    return !found;
}

void
ptk_handler_load_text_view(GtkTextView* view, const char* text)
{
    if (view)
    {
        GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
        gtk_text_buffer_set_text(buf, text ? text : "", -1);
    }
}

static char*
ptk_handler_get_text_view(GtkTextView* view)
{
    if (!view)
        return ztd::strdup("");
    GtkTextBuffer* buf = gtk_text_view_get_buffer(view);
    GtkTextIter iter, siter;
    gtk_text_buffer_get_start_iter(buf, &siter);
    gtk_text_buffer_get_end_iter(buf, &iter);
    char* text = gtk_text_buffer_get_text(buf, &siter, &iter, false);
    if (!text)
        return ztd::strdup("");
    return text;
}

char*
ptk_handler_get_command(int mode, int cmd, XSet* handler_set)
{ /* if handler->disable, get const command, else get script path if exists */
    if (!handler_set)
        return nullptr;
    if (handler_set->disable)
    {
        // is default handler - get command from const char
        const Handler* handler;
        int i;
        int nelements;
        const char* command;

        switch (mode)
        {
            case HANDLER_MODE_ARC:
                nelements = G_N_ELEMENTS(handlers_arc);
                break;
            case HANDLER_MODE_FS:
                nelements = G_N_ELEMENTS(handlers_fs);
                break;
            case HANDLER_MODE_NET:
                nelements = G_N_ELEMENTS(handlers_net);
                break;
            case HANDLER_MODE_FILE:
                nelements = G_N_ELEMENTS(handlers_file);
                break;
            default:
                return nullptr;
        }

        for (i = 0; i < nelements; i++)
        {
            switch (mode)
            {
                case HANDLER_MODE_ARC:
                    handler = &handlers_arc[i];
                    break;
                case HANDLER_MODE_FS:
                    handler = &handlers_fs[i];
                    break;
                case HANDLER_MODE_NET:
                    handler = &handlers_net[i];
                    break;
                case HANDLER_MODE_FILE:
                    handler = &handlers_file[i];
                    break;
                default:
                    break;
            }

            if (!strcmp(handler->xset_name, handler_set->name))
            {
                // found default handler
                switch (cmd)
                {
                    case HANDLER_COMPRESS:
                        command = handler->compress_cmd;
                        break;
                    case HANDLER_EXTRACT:
                        command = handler->extract_cmd;
                        break;
                    case HANDLER_LIST:
                        command = handler->list_cmd;
                        break;
                    default:
                        break;
                }

                return ztd::strdup(command);
            }
        }
        return nullptr;
    }
    // get default script path
    char* def_script = xset_custom_get_script(handler_set, false);
    if (!def_script)
    {
        LOG_WARN("ptk_handler_get_command unable to get script for custom {}", handler_set->name);
        return nullptr;
    }
    // name script
    std::string str = fmt::format("/hand-{}-{}.sh",
                                  modes[mode],
                                  mode == HANDLER_MODE_ARC ? cmds_arc[cmd] : cmds_mnt[cmd]);
    std::string script = ztd::replace(def_script, "/exec.sh", str);
    free(def_script);
    if (std::filesystem::exists(script))
        return ztd::strdup(script.c_str());

    LOG_WARN("ptk_handler_get_command missing script for custom {}", handler_set->name);
    return nullptr;
}

bool
ptk_handler_load_script(int mode, int cmd, XSet* handler_set, GtkTextView* view,
                        std::string& script, std::string& error_message)
{
    // returns true if there is an error

    GtkTextBuffer* buf = nullptr;

    if (view)
    {
        buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
        gtk_text_buffer_set_text(buf, "", -1);
    }

    if (handler_set->disable)
    {
        // is default handler - get contents from const char
        char* command = ptk_handler_get_command(mode, cmd, handler_set);
        if (!command)
        {
            error_message = "Error: unable to load command (internal error)";
            return true;
        }
        if (view)
            gtk_text_buffer_insert_at_cursor(buf, command, -1);
        else
            script = command;

        // success
        return false;
    }

    // get default script path
    char* def_script = xset_custom_get_script(handler_set, false);
    if (!def_script)
    {
        error_message =
            fmt::format("get_handler_script unable to get script for custom {}", handler_set->name);
        return true;
    }
    // name script
    std::string script_name = fmt::format("/hand-{}-{}.sh",
                                          modes[mode],
                                          mode == HANDLER_MODE_ARC ? cmds_arc[cmd] : cmds_mnt[cmd]);
    script = ztd::replace(def_script, "/exec.sh", script_name);
    free(def_script);
    // load script
    // bool modified = false;
    bool start = true;

    if (std::filesystem::exists(script))
    {
        std::string line;
        std::ifstream file(script);
        if (!file.is_open())
        {
            error_message =
                fmt::format("Error reading file '{}':\n\n{}", script, g_strerror(errno));
            return true;
        }
        else
        {
            while (std::getline(file, line))
            {
                if (!g_utf8_validate(line.c_str(), -1, nullptr))
                {
                    file.close();
                    if (view)
                        gtk_text_buffer_set_text(buf, "", -1);
                    else
                        script.clear();
                    // modified = true;
                    error_message = fmt::format("file '{}' contents are not valid UTF-8", script);
                    break;
                }
                if (start)
                {
                    // skip script header
                    if (ztd::same(line, script_header))
                        continue;

                    start = false;
                }
                // add line to buffer
                if (view)
                    gtk_text_buffer_insert_at_cursor(buf, line.c_str(), -1);
                else
                    script.append(line);
            }
            file.close();
        }
    }
    // success
    return false;
}

bool
ptk_handler_save_script(int mode, int cmd, XSet* handler_set, GtkTextView* view,
                        const std::string command, std::string& error_message)
{
    // returns true if there is an error

    /* writes command in textview buffer or const command to script */
    if (!(handler_set && !handler_set->disable))
    {
        error_message = "Error: unable to save command (internal error)";
        return true;
    }
    // get default script path
    char* def_script = xset_custom_get_script(handler_set, false);
    if (!def_script)
    {
        LOG_WARN("save_handler_script unable to get script for custom {}", handler_set->name);
        error_message = "Error: unable to save command (can't get script path?)";
        return true;
    }
    // create parent dir
    char* parent_dir = g_path_get_dirname(def_script);
    if (!std::filesystem::is_directory(parent_dir))
    {
        std::filesystem::create_directories(parent_dir);
        std::filesystem::permissions(parent_dir, std::filesystem::perms::owner_all);
    }
    free(parent_dir);
    // name script
    std::string script;
    std::string str;
    str = fmt::format("/hand-{}-{}.sh",
                      modes[mode],
                      mode == HANDLER_MODE_ARC ? cmds_arc[cmd] : cmds_mnt[cmd]);
    script = ztd::replace(def_script, "/exec.sh", str);
    free(def_script);
    // get text
    std::string text;
    if (view)
    {
        GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
        GtkTextIter iter, siter;
        gtk_text_buffer_get_start_iter(buf, &siter);
        gtk_text_buffer_get_end_iter(buf, &iter);
        text = gtk_text_buffer_get_text(buf, &siter, &iter, false);
    }
    else
        text = command;

    // LOG_INFO("WRITE {}", script);
    // write script
    std::ofstream file(script);
    if (file.is_open())
    {
        file << script_header;
        file << "\n";
        file << text;
        file << "\n";
    }
    else
    {
        error_message =
            fmt::format("{} '{}':\n\n{}", "Error writing to file", script, g_strerror(errno));
        return true;
    }
    file.close();

    // success
    return false;
}

bool
ptk_handler_values_in_list(const std::string list, const std::vector<std::string>& values,
                           std::string& msg)
{ /* test for the presence of values in list, using wildcards.
   *  list is space-separated, plus sign (+) indicates required. */
    if (values.empty())
        return false;

    // get elements of list
    char** elements = g_strsplit(list.c_str(), " ", -1);
    if (!elements)
        return false;

    // test each element for match
    int i;
    char* element;
    bool required, match;
    bool ret = false;
    for (i = 0; elements[i]; i++)
    {
        if (!elements[i][0])
            continue;
        if (elements[i][0] == '+')
        {
            // plus prefix indicates this element is required
            element = elements[i] + 1;
            required = true;
        }
        else
        {
            element = elements[i];
            required = false;
        }
        match = false;
        for (std::string handler: values)
        {
            if (fnmatch(element, handler.c_str(), 0) == 0)
            {
                // match
                ret = match = true;
                break;
            }
        }
        if (required && !match)
        {
            // no match of required
            g_strfreev(elements);
            return false;
        }

        msg = fmt::format("{}{}{}", match ? "[" : "", elements[i], match ? "]" : "");
    }
    g_strfreev(elements);

    return ret;
}

static bool
value_in_list(const char* list, const char* value)
{ // this function must be FAST - is run multiple times on menu popup
    char* ptr;
    char* delim;
    char ch;

    // value in space-separated list with wildcards?
    if (value && (ptr = (char*)list) && ptr[0])
    {
        while (true)
        {
            while (ptr[0] == ' ')
                ptr++;
            if (!ptr[0])
                break;
            delim = ptr;
            while (delim[0] != ' ' && delim[0])
                delim++;
            ch = delim[0];
            delim[0] = '\0'; // set temporary end of string
            if (fnmatch(ptr, value, 0) == 0)
            {
                delim[0] = ch; // restore
                return true;
            }
            delim[0] = ch; // restore
            if (ch == '\0')
                break; // is end of string
            ptr = delim + 1;
        }
    }
    return false;
}

GSList*
ptk_handler_file_has_handlers(int mode, int cmd, const char* path, VFSMimeType* mime_type,
                              bool test_cmd, bool multiple, bool enabled_only)
{ /* this function must be FAST - is run multiple times on menu popup
   * command must be non-empty if test_cmd */
    const char* type;
    char* delim;
    char* ptr;
    char* under_path;
    GSList* handlers = nullptr;

    if (!path && !mime_type)
        return nullptr;
    // Fetching and validating MIME type if provided
    if (mime_type)
        type = (char*)vfs_mime_type_get_type(mime_type);
    else
        type = nullptr;

    // replace spaces in path with underscores for matching
    if (path && strchr(path, ' '))
    {
        std::string cleaned = ztd::replace(path, " ", "_");
        under_path = ztd::strdup(cleaned.c_str());
    }
    else
        under_path = (char*)path;

    // parsing handlers space-separated list
    if ((ptr = xset_get_s(handler_conf_xset[mode])))
    {
        while (ptr[0])
        {
            while (ptr[0] == ' ')
                ptr++;
            if (!ptr[0])
                break;
            if ((delim = strchr(ptr, ' ')))
                delim[0] = '\0'; // set temporary end of string

            // Fetching handler
            XSet* handler_set = xset_is(ptr);
            if (delim)
                delim[0] = ' '; // remove temporary end of string

            // handler supports type or path ?
            if (handler_set && (!enabled_only || handler_set->b == XSET_B_TRUE) &&
                (value_in_list(handler_set->s, type) || value_in_list(handler_set->x, under_path)))
            {
                // test command
                if (test_cmd)
                {
                    std::string command;
                    std::string error_message;
                    bool error = ptk_handler_load_script(mode,
                                                         cmd,
                                                         handler_set,
                                                         nullptr,
                                                         command,
                                                         error_message);
                    if (error)
                        LOG_ERROR(error_message);
                    else if (!ptk_handler_command_is_empty(command))
                    {
                        handlers = g_slist_prepend(handlers, handler_set);
                        if (!multiple)
                            break;
                    }
                }
                else
                {
                    handlers = g_slist_prepend(handlers, handler_set);
                    if (!multiple)
                        break;
                }
            }
            if (!delim)
                break;
            ptr = delim + 1;
        }
    }
    return g_slist_reverse(handlers);
}

static void
string_copy_free(char** s, const char* src)
{
    char* discard = *s;
    *s = ztd::strdup(src);
    free(discard);
}

void
ptk_handler_add_defaults(int mode, bool overwrite, bool add_missing)
{
    int i;
    int nelements;
    char* list;
    char* str;
    char* testlist;
    char* testname;
    XSet* set;
    XSet* set_conf;
    const Handler* handler;

    switch (mode)
    {
        case HANDLER_MODE_ARC:
            nelements = G_N_ELEMENTS(handlers_arc);
            break;
        case HANDLER_MODE_FS:
            nelements = G_N_ELEMENTS(handlers_fs);
            break;
        case HANDLER_MODE_NET:
            nelements = G_N_ELEMENTS(handlers_net);
            break;
        case HANDLER_MODE_FILE:
            nelements = G_N_ELEMENTS(handlers_file);
            break;
        default:
            return;
    }

    set_conf = xset_get(handler_conf_xset[mode]);
    list = ztd::strdup(set_conf->s);

    if (!list)
    {
        // create default list - eg sets arc_conf2 ->s
        list = ztd::strdup("");
        overwrite = add_missing = true;
    }

    for (i = 0; i < nelements; i++)
    {
        switch (mode)
        {
            case HANDLER_MODE_ARC:
                handler = &handlers_arc[i];
                break;
            case HANDLER_MODE_FS:
                handler = &handlers_fs[i];
                break;
            case HANDLER_MODE_NET:
                handler = &handlers_net[i];
                break;
            case HANDLER_MODE_FILE:
                handler = &handlers_file[i];
                break;
            default:
                return;
        }

        // add a space to end of list and end of name before testing to avoid
        // substring false positive
        testlist = g_strdup_printf("%s ", list);
        testname = g_strdup_printf("%s ", handler->xset_name);
        if (add_missing && !strstr(testlist, testname))
        {
            // add a missing default handler to the list
            str = list;
            list = g_strconcat(list, list[0] ? " " : "", handler->xset_name, nullptr);
            free(str);
        }
        free(testlist);
        testlist = g_strdup_printf("%s ", list);
        if (add_missing || strstr(testlist, testname))
        {
            set = xset_is(handler->xset_name);
            if (!set || overwrite)
            {
                // create xset if missing
                if (!set)
                    set = xset_get(handler->xset_name);
                // set handler values to defaults
                string_copy_free(&set->menu_label, handler->handler_name);
                string_copy_free(&set->s, handler->type);
                string_copy_free(&set->x, handler->ext);
                set->in_terminal = handler->compress_term;
                // extract in terminal or (file handler) run as task
                set->keep_terminal = handler->extract_term;
                if (mode != HANDLER_MODE_FILE)
                    set->scroll_lock = handler->list_term;
                set->b = XSET_B_TRUE;
                set->lock = false;
                // handler equals default, so don't save in session
                set->disable = true;
            }
        }
        free(testlist);
        free(testname);
    }
    // update handler list
    free(set_conf->s);
    set_conf->s = list;
}

XSet*
add_new_handler(int mode)
{
    // creates a new xset for a custom handler type
    char* rand;
    char* name = nullptr;

    // get a unique new xset name
    do
    {
        free(name);
        rand = randhex8();
        name = g_strconcat(handler_cust_prefix[mode], rand, nullptr);
        free(rand);
    } while (xset_is(name));

    // create and return the xset
    XSet* set = xset_get(name);
    free(name);
    set->lock = false;
    return set;
}

void
ptk_handler_import(int mode, GtkWidget* handler_dlg, XSet* set)
{
    // Adding new handler as a copy of the imported plugin set
    XSet* new_handler_xset = add_new_handler(mode);
    new_handler_xset->b = set->b;
    new_handler_xset->disable = false; // not default - save in session
    new_handler_xset->menu_label = ztd::strdup(set->menu_label);
    new_handler_xset->icon = ztd::strdup(set->icon);
    new_handler_xset->s = ztd::strdup(set->s); // Mime Type(s) or whitelist
    new_handler_xset->x = ztd::strdup(set->x); // Extension(s) or blacklist
    new_handler_xset->in_terminal = set->in_terminal;
    new_handler_xset->keep_terminal = set->keep_terminal;
    new_handler_xset->scroll_lock = set->scroll_lock;

    // build copy scripts command
    char* path_src = g_build_filename(set->plug_dir, set->plug_name, nullptr);
    char* path_dest = g_build_filename(xset_get_config_dir(), "scripts", nullptr);
    std::filesystem::create_directories(path_dest);
    std::filesystem::permissions(path_dest, std::filesystem::perms::owner_all);
    free(path_dest);
    path_dest = g_build_filename(xset_get_config_dir(), "scripts", new_handler_xset->name, nullptr);
    std::string command = fmt::format("cp -a {} {}", path_src, path_dest);
    free(path_src);

    // run command
    std::string* standard_output = nullptr;
    std::string* standard_error = nullptr;
    int exit_status;

    std::string msg;

    print_command(command);
    Glib::spawn_command_line_sync(command, standard_output, standard_error, &exit_status);
    LOG_INFO("{}{}", *standard_output, *standard_error);
    if (exit_status && WIFEXITED(exit_status))
    {
        msg = fmt::format("An error occured copying command files\n\n{}", *standard_error);
        xset_msg_dialog(nullptr, GTK_MESSAGE_ERROR, "Copy Command Error", GTK_BUTTONS_OK, msg);
    }
    command = g_strdup_printf("chmod -R go-rwx %s", path_dest);
    print_command(command);
    Glib::spawn_command_line_sync(command);
    free(path_dest);

    // add to handler list
    if (g_strcmp0(xset_get_s(handler_conf_xset[mode]), "") <= 0)
    {
        // No handlers present - adding new handler
        xset_set(handler_conf_xset[mode], "s", new_handler_xset->name);
    }
    else
    {
        // Adding new handler to handlers
        char* new_handlers_list =
            g_strdup_printf("%s %s", new_handler_xset->name, xset_get_s(handler_conf_xset[mode]));
        xset_set(handler_conf_xset[mode], "s", new_handlers_list);
        free(new_handlers_list);
    }

    // have handler dialog open?
    HandlerData* hnd =
        handler_dlg && GTK_IS_WIDGET(handler_dlg)
            ? static_cast<HandlerData*>(g_object_get_data(G_OBJECT(handler_dlg), "hnd"))
            : nullptr;
    if (!(hnd && hnd->dlg == handler_dlg && hnd->mode == mode))
    {
        // dialog not shown or invalid
        const char* mode_name;
        switch (mode)
        {
            case HANDLER_MODE_ARC:
                mode_name = "Archive";
                break;
            case HANDLER_MODE_FS:
                mode_name = "Device";
                break;
            case HANDLER_MODE_NET:
                mode_name = "Protocol";
                break;
            case HANDLER_MODE_FILE:
                mode_name = "File";
                break;
            default:
                return;
        }
        msg = fmt::format("The selected {} Handler file has been imported to the {} Handlers list.",
                          mode_name,
                          mode_name);
        xset_msg_dialog(nullptr, GTK_MESSAGE_INFO, "Handler Imported", GTK_BUTTONS_OK, msg);
        return;
    }

    // Have valid handler data and dialog

    // Obtaining appending iterator for treeview model
    GtkTreeIter iter;
    gtk_list_store_prepend(GTK_LIST_STORE(hnd->list), &iter);

    // Adding handler to model
    const char* disabled = hnd->mode == HANDLER_MODE_FILE ? "(optional)" : "(disabled)";
    char* dis_name = g_strdup_printf("%s %s",
                                     new_handler_xset->menu_label,
                                     new_handler_xset->b == XSET_B_TRUE ? "" : disabled);
    gtk_list_store_set(GTK_LIST_STORE(hnd->list),
                       &iter,
                       COL_XSET_NAME,
                       new_handler_xset->name,
                       COL_HANDLER_NAME,
                       dis_name,
                       -1);
    free(dis_name);

    // Activating the new handler - the normal loading code
    // automatically kicks in
    GtkTreePath* new_handler_path = gtk_tree_model_get_path(GTK_TREE_MODEL(hnd->list), &iter);
    gtk_tree_view_set_cursor(GTK_TREE_VIEW(hnd->view_handlers), new_handler_path, nullptr, false);
    gtk_tree_path_free(new_handler_path);

    // Making sure the remove and apply buttons are sensitive
    gtk_widget_set_sensitive(hnd->btn_remove, true);
    gtk_widget_set_sensitive(hnd->btn_apply, false);

    hnd->changed = hnd->compress_changed = hnd->extract_changed = hnd->list_changed = false;
}

static void
config_load_handler_settings(XSet* handler_xset, char* handler_xset_name, const Handler* handler,
                             HandlerData* hnd)
{ // handler_xset_name optional if handler_xset passed
    // Fetching actual xset if only the name has been passed
    if (!handler_xset)
        if (!(handler_xset = xset_is(handler_xset_name)))
            return;

    /* At this point a handler exists, so making remove and apply buttons
     * sensitive as well as the enabled checkbutton */
    gtk_widget_set_sensitive(GTK_WIDGET(hnd->btn_remove), true);
    gtk_widget_set_sensitive(GTK_WIDGET(hnd->btn_apply), false);
    gtk_widget_set_sensitive(GTK_WIDGET(hnd->btn_up), true);
    gtk_widget_set_sensitive(GTK_WIDGET(hnd->btn_down), true);
    gtk_widget_set_sensitive(GTK_WIDGET(hnd->chkbtn_handler_enabled), true);
    gtk_widget_set_sensitive(
        GTK_WIDGET(hnd->btn_defaults0),
        Glib::str_has_prefix(handler_xset->name, handler_def_prefix[hnd->mode]));

    /* Configuring widgets with handler settings. Only name, MIME and
     * extension warrant a warning
     * Commands are prefixed with '+' when they are ran in a terminal */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hnd->chkbtn_handler_enabled),
                                 handler_xset->b == XSET_B_TRUE);

    gtk_entry_set_text(GTK_ENTRY(hnd->entry_handler_name),
                       handler_xset->menu_label ? handler_xset->menu_label : "");
    gtk_entry_set_text(GTK_ENTRY(hnd->entry_handler_mime), handler_xset->s ? handler_xset->s : "");
    gtk_entry_set_text(GTK_ENTRY(hnd->entry_handler_extension),
                       handler_xset->x ? handler_xset->x : "");
    if (hnd->entry_handler_icon)
        gtk_entry_set_text(GTK_ENTRY(hnd->entry_handler_icon),
                           handler_xset->icon ? handler_xset->icon : "");

    if (handler)
    {
        // load commands from const handler
        ptk_handler_load_text_view(GTK_TEXT_VIEW(hnd->view_handler_compress),
                                   handler->compress_cmd);
        if (hnd->mode != HANDLER_MODE_FILE)
        {
            ptk_handler_load_text_view(GTK_TEXT_VIEW(hnd->view_handler_extract),
                                       handler->extract_cmd);
            ptk_handler_load_text_view(GTK_TEXT_VIEW(hnd->view_handler_list), handler->list_cmd);
        }
    }
    else
    {
        bool error = false;
        std::string command;
        std::string error_message;

        error = ptk_handler_load_script(hnd->mode,
                                        HANDLER_COMPRESS,
                                        handler_xset,
                                        GTK_TEXT_VIEW(hnd->view_handler_compress),
                                        command,
                                        error_message);
        if (hnd->mode != HANDLER_MODE_FILE)
        {
            if (!error)
                error = ptk_handler_load_script(hnd->mode,
                                                HANDLER_EXTRACT,
                                                handler_xset,
                                                GTK_TEXT_VIEW(hnd->view_handler_extract),
                                                command,
                                                error_message);
            if (!error)
                error = ptk_handler_load_script(hnd->mode,
                                                HANDLER_LIST,
                                                handler_xset,
                                                GTK_TEXT_VIEW(hnd->view_handler_list),
                                                command,
                                                error_message);
        }
        if (error)
        {
            xset_msg_dialog(GTK_WIDGET(hnd->dlg),
                            GTK_MESSAGE_ERROR,
                            "Error Loading Handler",
                            GTK_BUTTONS_OK,
                            error_message);
        }
    }
    // Run In Terminal checkboxes
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hnd->chkbtn_handler_compress_term),
                                 handler_xset->in_terminal);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hnd->chkbtn_handler_extract_term),
                                 handler_xset->keep_terminal);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hnd->chkbtn_handler_list_term),
                                 handler_xset->scroll_lock);
}

static void
config_unload_handler_settings(HandlerData* hnd)
{
    // Disabling main change buttons
    gtk_widget_set_sensitive(GTK_WIDGET(hnd->btn_remove), false);
    gtk_widget_set_sensitive(GTK_WIDGET(hnd->btn_up), false);
    gtk_widget_set_sensitive(GTK_WIDGET(hnd->btn_down), false);
    gtk_widget_set_sensitive(GTK_WIDGET(hnd->btn_defaults0), false);

    // Unchecking handler
    if (hnd->mode != HANDLER_MODE_FILE)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hnd->chkbtn_handler_enabled), false);

    // Resetting all widgets
    gtk_entry_set_text(GTK_ENTRY(hnd->entry_handler_name), "");
    gtk_entry_set_text(GTK_ENTRY(hnd->entry_handler_mime), "");
    gtk_entry_set_text(GTK_ENTRY(hnd->entry_handler_extension), "");
    if (hnd->entry_handler_icon)
        gtk_entry_set_text(GTK_ENTRY(hnd->entry_handler_icon), "");
    ptk_handler_load_text_view(GTK_TEXT_VIEW(hnd->view_handler_compress), nullptr);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hnd->chkbtn_handler_compress_term), false);
    ptk_handler_load_text_view(GTK_TEXT_VIEW(hnd->view_handler_extract), nullptr);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hnd->chkbtn_handler_extract_term), false);
    ptk_handler_load_text_view(GTK_TEXT_VIEW(hnd->view_handler_list), nullptr);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hnd->chkbtn_handler_list_term), false);

    gtk_widget_set_sensitive(GTK_WIDGET(hnd->btn_apply), false);
    hnd->changed = hnd->compress_changed = hnd->extract_changed = hnd->list_changed = false;
}

static void
populate_archive_handlers(HandlerData* hnd, XSet* def_handler_set)
{
    /* Fetching available archive handlers (literally gets member s from
     * the xset) - user-defined order has already been set */
    char* archive_handlers_s = xset_get_s(handler_conf_xset[hnd->mode]);

    // Making sure archive handlers are available
    if (!archive_handlers_s)
        return;

    char** archive_handlers = g_strsplit(archive_handlers_s, " ", -1);

    // Debug code
    // LOG_INFO("archive_handlers_s: {}", archive_handlers_s);

    // Looping for handlers (nullptr-terminated list)
    GtkTreeIter iter;
    GtkTreeIter def_handler_iter;
    def_handler_iter.stamp = 0;
    int i;
    for (i = 0; archive_handlers[i] != nullptr; ++i)
    {
        if (Glib::str_has_prefix(archive_handlers[i], handler_cust_prefix[hnd->mode]))
        {
            // Fetching handler  - ignoring invalid handler xset names
            XSet* handler_xset = xset_is(archive_handlers[i]);
            if (handler_xset)
            {
                // Obtaining appending iterator for treeview model
                gtk_list_store_append(GTK_LIST_STORE(hnd->list), &iter);
                // Adding handler to model
                const char* disabled = hnd->mode == HANDLER_MODE_FILE ? "(optional)" : "(disabled)";
                char* dis_name = g_strdup_printf("%s %s",
                                                 handler_xset->menu_label,
                                                 handler_xset->b == XSET_B_TRUE ? "" : disabled);
                gtk_list_store_set(GTK_LIST_STORE(hnd->list),
                                   &iter,
                                   COL_XSET_NAME,
                                   archive_handlers[i],
                                   COL_HANDLER_NAME,
                                   dis_name,
                                   -1);
                free(dis_name);
                if (def_handler_set == handler_xset)
                    def_handler_iter = iter;
            }
        }
    }

    // Clearing up archive_handlers
    g_strfreev(archive_handlers);

    // Fetching selection from treeview
    GtkTreeSelection* selection;
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(hnd->view_handlers));

    /* Loading first or default archive handler if there is one and no selection is
     * present */
    if (i > 0 && !gtk_tree_selection_get_selected(GTK_TREE_SELECTION(selection), nullptr, nullptr))
    {
        GtkTreePath* tree_path;
        if (def_handler_set && def_handler_iter.stamp)
            tree_path = gtk_tree_model_get_path(GTK_TREE_MODEL(hnd->list), &def_handler_iter);
        else
            tree_path = gtk_tree_path_new_first();
        gtk_tree_selection_select_path(GTK_TREE_SELECTION(selection), tree_path);
        gtk_tree_path_free(tree_path);
    }
}

static void
on_configure_drag_end(GtkWidget* widget, GdkDragContext* drag_context, HandlerData* hnd)
{
    (void)widget;
    (void)drag_context;
    // Regenerating archive handlers list xset
    // Obtaining iterator pointing at first handler
    GtkTreeIter iter;
    if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(hnd->list), &iter))
    {
        // Failed to get iterator - warning user and exiting
        LOG_WARN("Drag'n'drop end event detected, but unable to get an"
                 " iterator to the start of the model!");
        return;
    }

    // Looping for all handlers
    char* xset_name;
    char* archive_handlers = ztd::strdup("");
    char* archive_handlers_temp;
    do
    {
        gtk_tree_model_get(GTK_TREE_MODEL(hnd->list), &iter, COL_XSET_NAME, &xset_name, -1);

        archive_handlers_temp = archive_handlers;
        if (!g_strcmp0(archive_handlers, ""))
        {
            archive_handlers = ztd::strdup(xset_name);
        }
        else
        {
            archive_handlers = g_strdup_printf("%s %s", archive_handlers, xset_name);
        }
        free(archive_handlers_temp);
        free(xset_name);
    } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(hnd->list), &iter));

    // Saving the new archive handlers list
    xset_set(handler_conf_xset[hnd->mode], "s", archive_handlers);
    free(archive_handlers);

    // Saving settings
    autosave_request();
}

static void
on_configure_button_press(GtkButton* widget, HandlerData* hnd)
{
    std::string command;
    std::string error_message;
    bool error = false;

    const char* handler_name = gtk_entry_get_text(GTK_ENTRY(hnd->entry_handler_name));
    const char* handler_mime = gtk_entry_get_text(GTK_ENTRY(hnd->entry_handler_mime));
    const char* handler_extension = gtk_entry_get_text(GTK_ENTRY(hnd->entry_handler_extension));
    const char* handler_icon;
    if (hnd->entry_handler_icon)
        handler_icon = gtk_entry_get_text(GTK_ENTRY(hnd->entry_handler_icon));
    else
        handler_icon = nullptr;

    const bool handler_compress_term =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(hnd->chkbtn_handler_compress_term));
    const bool handler_extract_term =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(hnd->chkbtn_handler_extract_term));
    const bool handler_list_term =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(hnd->chkbtn_handler_list_term));

    // Fetching the model and iter from the selection
    GtkTreeIter it, iter;
    GtkTreeModel* model;
    char* handler_name_from_model = nullptr; // Used to detect renames
    char* xset_name = nullptr;
    XSet* handler_xset = nullptr;

    // Fetching selection from treeview
    GtkTreeSelection* selection;
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(hnd->view_handlers));

    // Getting selection fails if there are no handlers
    if (gtk_tree_selection_get_selected(GTK_TREE_SELECTION(selection), &model, &it))
    {
        // Succeeded - handlers present
        // Fetching data from the model based on the iterator.
        gtk_tree_model_get(model,
                           &it,
                           COL_XSET_NAME,
                           &xset_name,
                           COL_HANDLER_NAME,
                           &handler_name_from_model,
                           -1);

        // Fetching the xset now I have the xset name
        handler_xset = xset_is(xset_name);

        // Making sure it has been fetched
        if (!handler_xset)
        {
            LOG_WARN("Unable to fetch the xset for the archive handler '%s'", handler_name);
            goto _clean_exit;
        }
    }

    if (GTK_WIDGET(widget) == GTK_WIDGET(hnd->btn_add))
    {
        // Exiting if there is no handler to add
        if (!(handler_name && handler_name[0]))
            goto _clean_exit;

        // Adding new handler as a copy of the current active handler
        XSet* new_handler_xset = add_new_handler(hnd->mode);
        new_handler_xset->b =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(hnd->chkbtn_handler_enabled))
                ? XSET_B_TRUE
                : XSET_B_FALSE;
        new_handler_xset->disable = false; // not default - save in session
        xset_set_set(new_handler_xset, XSET_SET_SET_LABEL, handler_name);
        xset_set_set(new_handler_xset, XSET_SET_SET_S,
                     handler_mime); // Mime Type(s) or whitelist
        xset_set_set(new_handler_xset,
                     XSET_SET_SET_X,
                     handler_extension); // Extension(s) or blacklist
        new_handler_xset->in_terminal = handler_compress_term;
        new_handler_xset->keep_terminal = handler_extract_term;

        error = ptk_handler_save_script(hnd->mode,
                                        HANDLER_COMPRESS,
                                        new_handler_xset,
                                        GTK_TEXT_VIEW(hnd->view_handler_compress),
                                        command,
                                        error_message);
        if (hnd->mode == HANDLER_MODE_FILE)
        {
            if (handler_icon)
                xset_set_set(new_handler_xset, XSET_SET_SET_ICN, handler_icon);
        }
        else
        {
            new_handler_xset->scroll_lock = handler_list_term;
            if (!error)
                error = ptk_handler_save_script(hnd->mode,
                                                HANDLER_EXTRACT,
                                                new_handler_xset,
                                                GTK_TEXT_VIEW(hnd->view_handler_extract),
                                                command,
                                                error_message);
            if (!error)
                error = ptk_handler_save_script(hnd->mode,
                                                HANDLER_LIST,
                                                new_handler_xset,
                                                GTK_TEXT_VIEW(hnd->view_handler_list),
                                                command,
                                                error_message);
        }

        // Obtaining appending iterator for treeview model
        gtk_list_store_prepend(GTK_LIST_STORE(hnd->list), &iter);

        // Adding handler to model
        const char* disabled = hnd->mode == HANDLER_MODE_FILE ? "(optional)" : "(disabled)";
        char* dis_name = g_strdup_printf("%s %s",
                                         handler_name,
                                         new_handler_xset->b == XSET_B_TRUE ? "" : disabled);
        gtk_list_store_set(GTK_LIST_STORE(hnd->list),
                           &iter,
                           COL_XSET_NAME,
                           new_handler_xset->name,
                           COL_HANDLER_NAME,
                           dis_name,
                           -1);
        free(dis_name);

        // Updating available archive handlers list
        if (g_strcmp0(xset_get_s(handler_conf_xset[hnd->mode]), "") <= 0)
        {
            // No handlers present - adding new handler
            xset_set(handler_conf_xset[hnd->mode], "s", new_handler_xset->name);
        }
        else
        {
            // Adding new handler to handlers
            char* new_handlers_list = g_strdup_printf("%s %s",
                                                      new_handler_xset->name,
                                                      xset_get_s(handler_conf_xset[hnd->mode]));
            xset_set(handler_conf_xset[hnd->mode], "s", new_handlers_list);

            // Clearing up
            free(new_handlers_list);
        }

        // Activating the new handler - the normal loading code
        // automatically kicks in
        GtkTreePath* new_handler_path = gtk_tree_model_get_path(GTK_TREE_MODEL(model), &iter);
        gtk_tree_view_set_cursor(GTK_TREE_VIEW(hnd->view_handlers),
                                 new_handler_path,
                                 nullptr,
                                 false);
        gtk_tree_path_free(new_handler_path);

        // Making sure the remove and apply buttons are sensitive
        gtk_widget_set_sensitive(hnd->btn_remove, true);
        gtk_widget_set_sensitive(hnd->btn_apply, false);

        /* Validating - remember that IG wants the handler to be saved
         * even with invalid commands */
        validate_archive_handler(hnd);
        hnd->changed = hnd->compress_changed = hnd->extract_changed = hnd->list_changed = false;
    }
    else if (GTK_WIDGET(widget) == GTK_WIDGET(hnd->btn_apply))
    {
        // Exiting if apply has been pressed when no handlers are present
        if (xset_name == nullptr)
            goto _clean_exit;

        // Validating - to be saved even with invalid commands
        validate_archive_handler(hnd);

        // Determining current handler enabled state
        bool handler_enabled =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(hnd->chkbtn_handler_enabled));

        // Checking if the handler has been renamed
        if (g_strcmp0(handler_name_from_model, handler_name) != 0)
        {
            // It has - updating model
            const char* disabled = hnd->mode == HANDLER_MODE_FILE ? "(optional)" : "(disabled)";
            char* dis_name =
                g_strdup_printf("%s %s", handler_name, handler_enabled ? "" : disabled);
            gtk_list_store_set(GTK_LIST_STORE(model),
                               &it,
                               COL_XSET_NAME,
                               xset_name,
                               COL_HANDLER_NAME,
                               dis_name,
                               -1);
            free(dis_name);
        }

        // Saving archive handler
        handler_xset->b = handler_enabled ? XSET_B_TRUE : XSET_B_UNSET;
        bool was_default = handler_xset->disable;
        handler_xset->disable = false; // not default - save in session
        xset_set_set(handler_xset, XSET_SET_SET_LABEL, handler_name);
        xset_set_set(handler_xset, XSET_SET_SET_S, handler_mime);
        xset_set_set(handler_xset, XSET_SET_SET_X, handler_extension);
        handler_xset->in_terminal = handler_compress_term;
        handler_xset->keep_terminal = handler_extract_term;
        if (hnd->compress_changed || was_default)
        {
            error = ptk_handler_save_script(hnd->mode,
                                            HANDLER_COMPRESS,
                                            handler_xset,
                                            GTK_TEXT_VIEW(hnd->view_handler_compress),
                                            command,
                                            error_message);
        }
        if (hnd->mode == HANDLER_MODE_FILE)
        {
            if (handler_icon)
                xset_set_set(handler_xset, XSET_SET_SET_ICN, handler_icon);
        }
        else
        {
            handler_xset->scroll_lock = handler_list_term;
            if (hnd->extract_changed || was_default)
            {
                error = ptk_handler_save_script(hnd->mode,
                                                HANDLER_EXTRACT,
                                                handler_xset,
                                                GTK_TEXT_VIEW(hnd->view_handler_extract),
                                                command,
                                                error_message);
            }
            if (hnd->list_changed || was_default)
            {
                error = ptk_handler_save_script(hnd->mode,
                                                HANDLER_LIST,
                                                handler_xset,
                                                GTK_TEXT_VIEW(hnd->view_handler_list),
                                                command,
                                                error_message);
            }
        }
        hnd->changed = hnd->compress_changed = hnd->extract_changed = hnd->list_changed = false;
        gtk_widget_set_sensitive(hnd->btn_apply, false);
    }
    else if (GTK_WIDGET(widget) == GTK_WIDGET(hnd->btn_remove))
    {
        // Exiting if remove has been pressed when no handlers are present
        if (xset_name == nullptr)
            goto _clean_exit;

        if (xset_msg_dialog(hnd->dlg,
                            GTK_MESSAGE_WARNING,
                            "Confirm Remove",
                            GTK_BUTTONS_YES_NO,
                            "Permanently remove the selected handler?") != GTK_RESPONSE_YES)
            goto _clean_exit;

        // Updating available archive handlers list - fetching current
        // handlers
        const char* archive_handlers_s = xset_get_s(handler_conf_xset[hnd->mode]);
        char** archive_handlers =
            archive_handlers_s ? g_strsplit(archive_handlers_s, " ", -1) : nullptr;
        char* new_archive_handlers_s = ztd::strdup("");
        char* new_archive_handlers_s_temp;

        // Looping for handlers (nullptr-terminated list)
        if (archive_handlers)
        {
            for (int i = 0; archive_handlers[i] != nullptr; ++i)
            {
                // Appending to new archive handlers list when it isnt the
                // deleted handler - remember that archive handlers are
                // referred to by their xset names, not handler names!!
                if (g_strcmp0(archive_handlers[i], xset_name) != 0)
                {
                    // Debug code
                    // LOG_INFO("archive_handlers[i] : {}", archive_handlers[i])
                    // LOG_INFO("xset_name           : {}", xset_name);

                    new_archive_handlers_s_temp = new_archive_handlers_s;
                    if (!g_strcmp0(new_archive_handlers_s, ""))
                    {
                        new_archive_handlers_s = ztd::strdup(archive_handlers[i]);
                    }
                    else
                    {
                        new_archive_handlers_s =
                            g_strdup_printf("%s %s", new_archive_handlers_s, archive_handlers[i]);
                    }
                    free(new_archive_handlers_s_temp);
                }
            }
        }

        // Finally updating handlers
        xset_set(handler_conf_xset[hnd->mode], "s", new_archive_handlers_s);

        // Deleting xset
        xset_custom_delete(handler_xset, false);
        handler_xset = nullptr;

        // Removing handler from the list
        gtk_list_store_remove(GTK_LIST_STORE(model), &it);

        if (!g_strcmp0(new_archive_handlers_s, ""))
        {
            /* Making remove and apply buttons insensitive if the last
             * handler has been removed */
            gtk_widget_set_sensitive(GTK_WIDGET(hnd->btn_remove), false);
            gtk_widget_set_sensitive(GTK_WIDGET(hnd->btn_apply), false);

            /* Now that all are removed, the user needs sensitive widgets
             * to be able to add a further handler */
            gtk_widget_set_sensitive(hnd->chkbtn_handler_enabled, true);
            gtk_widget_set_sensitive(hnd->entry_handler_name, true);
            gtk_widget_set_sensitive(hnd->entry_handler_mime, true);
            gtk_widget_set_sensitive(hnd->entry_handler_extension, true);
            gtk_widget_set_sensitive(hnd->view_handler_compress, true);
            gtk_widget_set_sensitive(hnd->view_handler_extract, true);
            gtk_widget_set_sensitive(hnd->view_handler_list, true);
            gtk_widget_set_sensitive(hnd->chkbtn_handler_compress_term, true);
            gtk_widget_set_sensitive(hnd->chkbtn_handler_extract_term, true);
            gtk_widget_set_sensitive(hnd->chkbtn_handler_list_term, true);
        }
        else
        {
            /* Still remaining handlers - selecting the first one,
             * otherwise nothing is now selected */
            GtkTreePath* new_path = gtk_tree_path_new_first();
            gtk_tree_selection_select_path(GTK_TREE_SELECTION(selection), new_path);
            gtk_tree_path_free(new_path);
        }

        // Clearing up
        g_strfreev(archive_handlers);
        free(new_archive_handlers_s);
    }
    else if (GTK_WIDGET(widget) == GTK_WIDGET(hnd->btn_up) ||
             GTK_WIDGET(widget) == GTK_WIDGET(hnd->btn_down))
    {
        if (!handler_xset)
            // no row selected
            goto _clean_exit;

        // Note: gtk_tree_model_iter_previous requires GTK3, so not using
        GtkTreeIter iter_prev;
        if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
            goto _clean_exit;
        iter_prev = iter;
        do
        {
            // find my it (stamp is NOT unique - compare whole struct)
            if (iter.stamp == it.stamp && iter.user_data == it.user_data &&
                iter.user_data2 == it.user_data2 && iter.user_data3 == it.user_data3)
            {
                if (GTK_WIDGET(widget) == GTK_WIDGET(hnd->btn_up))
                    iter = iter_prev;
                else if (!gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter))
                    goto _clean_exit; // was last row
                break;
            }
            iter_prev = iter;
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter));
        gtk_list_store_swap(GTK_LIST_STORE(model), &it, &iter);
        // save the new list
        on_configure_drag_end(nullptr, nullptr, hnd);
    }

    // Saving settings
    autosave_request();

    if (error)
    {
        xset_msg_dialog(GTK_WIDGET(hnd->dlg),
                        GTK_MESSAGE_ERROR,
                        "Error Saving Handler",
                        GTK_BUTTONS_OK,
                        error_message);
    }

_clean_exit:
    free(xset_name);
    free(handler_name_from_model);
}

static void
on_configure_changed(GtkTreeSelection* selection, HandlerData* hnd)
{
    /* This event is triggered when the selected row is changed or no row is
     * selected at all */

    // Fetching the model and iter from the selection
    GtkTreeIter it;
    GtkTreeModel* model;
    if (!gtk_tree_selection_get_selected(selection, &model, &it))
    {
        // User has unselected all rows - removing loaded handler
        config_unload_handler_settings(hnd);
        return;
    }

    // Fetching data from the model based on the iterator.
    char* xset_name;
    gtk_tree_model_get(model, &it, COL_XSET_NAME, &xset_name, -1);

    // Loading new archive handler values
    config_load_handler_settings(nullptr, xset_name, nullptr, hnd);
    free(xset_name);
    hnd->changed = hnd->compress_changed = hnd->extract_changed = hnd->list_changed = false;
    gtk_widget_set_sensitive(hnd->btn_apply, false);

    /* Focussing archive handler name
     * Selects the text rather than just placing the cursor at the start
     * of the text... */
    /*GtkWidget* entry_handler_name = GTK_WIDGET(g_object_get_data(G_OBJECT(dlg),
                                                 "entry_handler_name"));
    gtk_widget_grab_focus( entry_handler_name );*/
}

static void
on_configure_handler_enabled_check(GtkToggleButton* togglebutton, HandlerData* hnd)
{
    /* When no handler is selected (i.e. the user selects outside of the
     * populated handlers list), the enabled checkbox might be checked
     * off - however the widgets must not be set insensitive when this
     * happens */

    if (!hnd->changed)
    {
        hnd->changed = true;
        gtk_widget_set_sensitive(hnd->btn_apply, gtk_widget_get_sensitive(hnd->btn_remove));
    }

    if (hnd->mode == HANDLER_MODE_FILE)
        return;

    // Fetching selection from treeview
    GtkTreeSelection* selection;
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(hnd->view_handlers));

    // Exiting if no handler is selected
    GtkTreeIter it;
    GtkTreeModel* model;
    if (!gtk_tree_selection_get_selected(selection, &model, &it))
        return;

    // Fetching current status
    bool enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(togglebutton));

    // Setting sensitive/insensitive various widgets as appropriate
    gtk_widget_set_sensitive(hnd->entry_handler_name, enabled);
    gtk_widget_set_sensitive(hnd->entry_handler_mime, enabled);
    gtk_widget_set_sensitive(hnd->entry_handler_extension, enabled);
    gtk_widget_set_sensitive(hnd->view_handler_compress, enabled);
    gtk_widget_set_sensitive(hnd->view_handler_extract, enabled);
    gtk_widget_set_sensitive(hnd->view_handler_list, enabled);
    gtk_widget_set_sensitive(hnd->chkbtn_handler_compress_term, enabled);
    gtk_widget_set_sensitive(hnd->chkbtn_handler_extract_term, enabled);
    gtk_widget_set_sensitive(hnd->chkbtn_handler_list_term, enabled);
}

static bool
on_handlers_key_press(GtkWidget* widget, GdkEventKey* evt, HandlerData* hnd)
{
    (void)widget;
    (void)evt;
    // Current handler hasn't been changed?
    if (!hnd->changed /* was !gtk_widget_get_sensitive( hnd->btn_apply )*/)
        return false;

    if (xset_msg_dialog(hnd->dlg,
                        GTK_MESSAGE_QUESTION,
                        "Apply Changes ?",
                        GTK_BUTTONS_YES_NO,
                        "Apply changes to the current handler?") == GTK_RESPONSE_YES)
        on_configure_button_press(GTK_BUTTON(hnd->btn_apply), hnd);
    else
        hnd->changed = false;
    return true; // false doesn't retain key after dialog shown
}

static bool
on_handlers_button_press(GtkWidget* view, GdkEventButton* evt, HandlerData* hnd)
{
    GtkTreeModel* model;
    GtkTreePath* tree_path = nullptr;
    GtkTreeIter it;
    GtkTreeSelection* selection;
    bool item_clicked = false;
    bool ret = false;

    // get clicked item
    if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view),
                                      evt->x,
                                      evt->y,
                                      &tree_path,
                                      nullptr,
                                      nullptr,
                                      nullptr))
    {
        model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
        if (gtk_tree_model_get_iter(model, &it, tree_path))
            item_clicked = true;
    }

    // Current handler has been changed?
    if (gtk_widget_get_sensitive(hnd->btn_apply))
    {
        // Query apply changes
        if (xset_msg_dialog(hnd->dlg,
                            GTK_MESSAGE_QUESTION,
                            "Apply Changes ?",
                            GTK_BUTTONS_YES_NO,
                            "Apply changes to the current handler?") == GTK_RESPONSE_YES)
            on_configure_button_press(GTK_BUTTON(hnd->btn_apply), hnd);

        // Move cursor or unselect
        if (item_clicked)
            // select clicked item
            gtk_tree_view_set_cursor(GTK_TREE_VIEW(hnd->view_handlers), tree_path, nullptr, false);
        else if ((selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(hnd->view_handlers))))
            // unselect all
            gtk_tree_selection_unselect_all(selection);
        ret = true;
    }
    else if (evt->button == 3)
    {
        // right click - Move cursor or unselect
        if (item_clicked)
            // select clicked item
            gtk_tree_view_set_cursor(GTK_TREE_VIEW(hnd->view_handlers), tree_path, nullptr, false);
        else if ((selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(hnd->view_handlers))))
            // unselect all
            gtk_tree_selection_unselect_all(selection);

        // show menu
        on_options_button_clicked(nullptr, hnd);
        ret = true;
    }

    if (tree_path)
        gtk_tree_path_free(tree_path);
    return ret;
}

static void
restore_defaults(HandlerData* hnd, bool all)
{
    if (all)
    {
        int response = xset_msg_dialog(GTK_WIDGET(hnd->dlg),
                                       GTK_MESSAGE_WARNING,
                                       "Restore Default Handlers",
                                       GTK_BUTTONS_YES_NO,
                                       "Missing default handlers will be restored.\n\nAlso "
                                       "OVERWRITE ALL EXISTING default handlers?");
        if (response != GTK_RESPONSE_YES && response != GTK_RESPONSE_NO)
            // dialog was closed with no button pressed - cancel
            return;
        ptk_handler_add_defaults(hnd->mode, response == GTK_RESPONSE_YES, true);

        /* Reset archive handlers list (this also selects
         * the first handler and therefore populates the handler widgets) */
        gtk_list_store_clear(GTK_LIST_STORE(hnd->list));
        populate_archive_handlers(hnd, nullptr);
    }
    else
    {
        // Fetching selection from treeview
        GtkTreeSelection* selection;
        selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(hnd->view_handlers));

        // Exiting if no handler is selected
        GtkTreeIter it;
        GtkTreeModel* model;
        if (!gtk_tree_selection_get_selected(selection, &model, &it))
            return;

        char* xset_name;
        gtk_tree_model_get(model, &it, COL_XSET_NAME, &xset_name, -1);
        // a default handler is selected?
        if (!(xset_name && Glib::str_has_prefix(xset_name, handler_def_prefix[hnd->mode])))
        {
            free(xset_name);
            return;
        }

        // get default handler
        int nelements;
        const Handler* handler = nullptr;

        switch (hnd->mode)
        {
            case HANDLER_MODE_ARC:
                nelements = G_N_ELEMENTS(handlers_arc);
                break;
            case HANDLER_MODE_FS:
                nelements = G_N_ELEMENTS(handlers_fs);
                break;
            case HANDLER_MODE_NET:
                nelements = G_N_ELEMENTS(handlers_net);
                break;
            case HANDLER_MODE_FILE:
                nelements = G_N_ELEMENTS(handlers_file);
                break;
            default:
                return;
        }

        bool found_handler = false;
        int i;
        for (i = 0; i < nelements; i++)
        {
            switch (hnd->mode)
            {
                case HANDLER_MODE_ARC:
                    handler = &handlers_arc[i];
                    break;
                case HANDLER_MODE_FS:
                    handler = &handlers_fs[i];
                    break;
                case HANDLER_MODE_NET:
                    handler = &handlers_net[i];
                    break;
                case HANDLER_MODE_FILE:
                    handler = &handlers_file[i];
                    return;
                default:
                    break;
            }

            if (!g_strcmp0(handler->xset_name, xset_name))
            {
                found_handler = true;
                break;
            }
        }
        free(xset_name);
        if (!found_handler)
            return;

        // create fake xset
        XSet* set = g_slice_new(XSet);
        set->name = (char*)handler->xset_name;
        set->menu_label = (char*)handler->handler_name;
        set->s = (char*)handler->type;
        set->x = (char*)handler->ext;
        set->in_terminal = handler->compress_term;
        set->keep_terminal = handler->extract_term;
        if (hnd->mode != HANDLER_MODE_FILE)
            set->scroll_lock = handler->list_term;
        set->b = XSET_B_TRUE;
        set->icon = nullptr;

        // show fake xset values
        config_load_handler_settings(set, nullptr, handler, hnd);

        g_slice_free(XSet, set);
    }
}

static bool
validate_archive_handler(HandlerData* hnd)
{
    if (hnd->mode != HANDLER_MODE_ARC)
        // only archive handlers currently have validity checks
        return true;

    const char* handler_name = gtk_entry_get_text(GTK_ENTRY(hnd->entry_handler_name));
    const char* handler_mime = gtk_entry_get_text(GTK_ENTRY(hnd->entry_handler_mime));
    const char* handler_extension = gtk_entry_get_text(GTK_ENTRY(hnd->entry_handler_extension));

    /* Validating data. Note that data straight from widgets shouldnt
     * be modified or stored
     * Note that archive creation also allows for a command to be
     * saved */
    if (g_strcmp0(handler_name, "") <= 0)
    {
        /* Handler name not set - warning user and exiting. Note
         * that the created dialog does not have an icon set */
        xset_msg_dialog(GTK_WIDGET(hnd->dlg),
                        GTK_MESSAGE_WARNING,
                        dialog_titles[hnd->mode],
                        GTK_BUTTONS_OK,
                        "Please enter a valid handler name.");
        gtk_widget_grab_focus(hnd->entry_handler_name);
        return false;
    }

    // MIME and Pathname can't both be empty
    if (g_strcmp0(handler_mime, "") <= 0 && g_strcmp0(handler_extension, "") <= 0)
    {
        xset_msg_dialog(GTK_WIDGET(hnd->dlg),
                        GTK_MESSAGE_WARNING,
                        dialog_titles[hnd->mode],
                        GTK_BUTTONS_OK,
                        "Please enter a valid MIME Type or Pathname pattern.");
        gtk_widget_grab_focus(hnd->entry_handler_mime);
        return false;
    }

    char* handler_compress = ptk_handler_get_text_view(GTK_TEXT_VIEW(hnd->view_handler_compress));
    char* handler_extract = ptk_handler_get_text_view(GTK_TEXT_VIEW(hnd->view_handler_extract));
    char* handler_list = ptk_handler_get_text_view(GTK_TEXT_VIEW(hnd->view_handler_list));
    bool ret = true;

    /* Other settings are commands to run in different situations -
     * since different handlers may or may not need different
     * commands, empty commands are allowed but if something is given,
     * relevant substitution characters should be in place */

    /* Compression handler validation - remember to maintain this code
     * in ptk_file_archiver_create too
     * Checking if a compression command has been entered */
    if (g_strcmp0(handler_compress, "") != 0)
    {
        /* It has - making sure all substitution characters are in
         * place - not mandatory to only have one of the particular
         * type */
        if ((!g_strstr_len(handler_compress, -1, "%o") &&
             !g_strstr_len(handler_compress, -1, "%O")) ||
            (!g_strstr_len(handler_compress, -1, "%n") &&
             !g_strstr_len(handler_compress, -1, "%N")))
        {
            xset_msg_dialog(GTK_WIDGET(hnd->dlg),
                            GTK_MESSAGE_WARNING,
                            dialog_titles[hnd->mode],
                            GTK_BUTTONS_OK,
                            "The following "
                            "substitution variables should probably be in the "
                            "compression command:\n\n"
                            "One of the following:\n\n"
                            "%%n: First selected file/directory to"
                            " archive\n"
                            "%%N: All selected files/directories to"
                            " archive\n\n"
                            "and one of the following:\n\n"
                            "%%o: Resulting single archive\n"
                            "%%O: Resulting archive per source "
                            "file/directory");
            gtk_widget_grab_focus(hnd->view_handler_compress);
            ret = false;
            goto _cleanup;
        }
    }

    if (g_strcmp0(handler_extract, "") != 0 && (!g_strstr_len(handler_extract, -1, "%x")))
    {
        /* Not all substitution characters are in place - warning
         * user and exiting. Note that the created dialog does not
         * have an icon set */
        xset_msg_dialog(GTK_WIDGET(hnd->dlg),
                        GTK_MESSAGE_WARNING,
                        dialog_titles[hnd->mode],
                        GTK_BUTTONS_OK,
                        "The following "
                        "variables should probably be in the extraction "
                        "command:\n\n%%x: "
                        "Archive to extract");
        gtk_widget_grab_focus(hnd->view_handler_extract);
        ret = false;
        goto _cleanup;
    }

    if (g_strcmp0(handler_list, "") != 0 && (!g_strstr_len(handler_list, -1, "%x")))
    {
        /* Not all substitution characters are in place  - warning
         * user and exiting. Note that the created dialog does not
         * have an icon set */
        xset_msg_dialog(GTK_WIDGET(hnd->dlg),
                        GTK_MESSAGE_WARNING,
                        dialog_titles[hnd->mode],
                        GTK_BUTTONS_OK,
                        "The following "
                        "variables should probably be in the list "
                        "command:\n\n%%x: "
                        "Archive to list");
        gtk_widget_grab_focus(hnd->view_handler_list);
        ret = false;
        goto _cleanup;
    }

_cleanup:
    free(handler_compress);
    free(handler_extract);
    free(handler_list);

    // Validation passed
    return ret;
}

static void
on_textview_popup(GtkTextView* input, GtkMenu* menu, HandlerData* hnd)
{
    (void)input;
    (void)hnd;
    // uses same xsets as item-prop.c:on_script_popup()
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    XSet* set = xset_get("separator");
    set->menu_style = XSET_MENU_SEP;
    set->browser = nullptr;
    xset_add_menuitem(nullptr, GTK_WIDGET(menu), accel_group, set);

    gtk_widget_show_all(GTK_WIDGET(menu));
}

static bool
on_activate_link(GtkLabel* label, char* uri, HandlerData* hnd)
{
    (void)label;
    // click apply to save handler
    on_configure_button_press(GTK_BUTTON(hnd->btn_apply), hnd);
    // open in editor
    int action = strtol(uri, nullptr, 10);
    if (action > HANDLER_LIST || action < 0)
        return true;

    // Fetching selection from treeview
    GtkTreeSelection* selection;
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(hnd->view_handlers));

    // Exiting if no handler is selected
    GtkTreeIter it;
    GtkTreeModel* model;
    if (!gtk_tree_selection_get_selected(selection, &model, &it))
        return true;

    char* xset_name = nullptr;
    gtk_tree_model_get(model, &it, COL_XSET_NAME, &xset_name, -1);
    XSet* set = xset_is(xset_name);
    free(xset_name);
    if (!(set && !set->disable && set->b == XSET_B_TRUE))
        return true;
    char* script = ptk_handler_get_command(hnd->mode, action, set);
    if (!script)
        return true;
    xset_edit(hnd->dlg, script, false, false);
    free(script);
    return true;
}

static bool
on_textview_keypress(GtkWidget* widget, GdkEventKey* event, HandlerData* hnd)
{ // also used on dlg keypress
    unsigned int keymod = ptk_get_keymod(event->state);
    switch (event->keyval)
    {
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
            if (keymod == GDK_MOD1_MASK)
            {
                // Alt+Enter == Open Handler Command In Editor
                if (widget == hnd->view_handler_compress)
                    keymod = 0;
                else if (widget == hnd->view_handler_extract)
                    keymod = 1;
                else if (widget == hnd->view_handler_list)
                    keymod = 2;
                else
                    return false;
                char* uri = g_strdup_printf("%d", keymod);
                on_activate_link(nullptr, uri, hnd);
                free(uri);
                return true;
            }
            break;
    }

    return false;
}

static void
on_textview_buffer_changed(GtkTextBuffer* buf, HandlerData* hnd)
{
    if (buf == hnd->buf_handler_compress && !hnd->compress_changed)
        hnd->compress_changed = true;
    else if (buf == hnd->buf_handler_extract && !hnd->extract_changed)
        hnd->extract_changed = true;
    if (buf == hnd->buf_handler_list && !hnd->list_changed)
        hnd->list_changed = true;
    if (!hnd->changed)
    {
        hnd->changed = true;
        gtk_widget_set_sensitive(hnd->btn_apply, gtk_widget_get_sensitive(hnd->btn_remove));
    }
}

static void
on_entry_text_insert(GtkEntryBuffer* buffer, unsigned int position, char* chars,
                     unsigned int n_chars, HandlerData* hnd)
{
    (void)buffer;
    (void)position;
    (void)chars;
    (void)n_chars;
    if (!hnd->changed)
    {
        hnd->changed = true;
        gtk_widget_set_sensitive(hnd->btn_apply, gtk_widget_get_sensitive(hnd->btn_remove));
    }
}

static void
on_entry_text_delete(GtkEntryBuffer* buffer, unsigned int position, unsigned int n_chars,
                     HandlerData* hnd)
{
    on_entry_text_insert(buffer, position, nullptr, n_chars, hnd);
}

static void
on_terminal_toggled(GtkToggleButton* togglebutton, HandlerData* hnd)
{
    (void)togglebutton;
    if (!hnd->changed)
    {
        hnd->changed = true;
        gtk_widget_set_sensitive(hnd->btn_apply, gtk_widget_get_sensitive(hnd->btn_remove));
    }
}

static void
on_icon_choose_button_clicked(GtkWidget* widget, HandlerData* hnd)
{
    (void)widget;
    // get current icon
    const char* icon = gtk_entry_get_text(GTK_ENTRY(hnd->entry_handler_icon));
    char* new_icon = xset_icon_chooser_dialog(GTK_WINDOW(hnd->dlg), icon);

    if (new_icon)
    {
        gtk_entry_set_text(GTK_ENTRY(hnd->entry_handler_icon), new_icon);
        free(new_icon);
    }
}

static void
on_option_cb(GtkMenuItem* item, HandlerData* hnd)
{
    if (hnd->changed)
        on_configure_button_press(GTK_BUTTON(hnd->btn_apply), hnd);

    int job = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "job"));

    // Determine handler selected
    XSet* set_sel = nullptr;
    char* xset_name;
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(hnd->view_handlers));
    GtkTreeIter it;
    GtkTreeModel* model;
    if (gtk_tree_selection_get_selected(selection, &model, &it))
    {
        gtk_tree_model_get(model, &it, COL_XSET_NAME, &xset_name, -1);
        set_sel = xset_is(xset_name);
        free(xset_name);
    }

    // determine job
    char* folder;
    char* file;
    XSet* save;
    switch (job)
    {
        case HANDLER_JOB_IMPORT_FILE:
            // get file path
            save = xset_get("plug_ifile");
            if (save->s) //&& std::filesystem::is_directory(save->s)
                folder = save->s;
            else
            {
                if (!(folder = xset_get_s("go_set_default")))
                    folder = ztd::strdup("/");
            }
            file = xset_file_dialog(GTK_WIDGET(hnd->dlg),
                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                    "Choose Handler Plugin File",
                                    folder,
                                    nullptr);
            if (!file)
                return;
            if (save->s)
                free(save->s);
            save->s = g_path_get_dirname(file);
            break;
        case HANDLER_JOB_RESTORE_ALL:
            restore_defaults(hnd, true);
            return;
        case HANDLER_JOB_REMOVE:
            on_configure_button_press(GTK_BUTTON(hnd->btn_remove), hnd);
            return;
        case HANDLER_JOB_EXPORT:
            // export
            if (!set_sel)
                return; // nothing selected - failsafe

            if (Glib::str_has_prefix(set_sel->name, handler_def_prefix[hnd->mode]) &&
                set_sel->disable)
            {
                // is an unsaved default handler, click Defaults then Apply to save
                restore_defaults(hnd, false);
                on_configure_button_press(GTK_BUTTON(hnd->btn_apply), hnd);
                if (set_sel->disable)
                    return; // failsafe
            }
            xset_custom_export(hnd->dlg, nullptr, set_sel);
            return;
        default:
            return;
    }

    // Make Plugin Dir
    const char* user_tmp = xset_get_user_tmp_dir();
    if (!user_tmp)
    {
        xset_msg_dialog(GTK_WIDGET(hnd->dlg),
                        GTK_MESSAGE_ERROR,
                        "Error Creating Temp Directory",
                        GTK_BUTTONS_OK,
                        "Unable to create temporary directory");
        free(file);
        return;
    }
    char* hex8;
    folder = nullptr;
    while (!folder || std::filesystem::exists(folder))
    {
        hex8 = randhex8();
        if (folder)
            free(folder);
        folder = g_build_filename(user_tmp, hex8, nullptr);
        free(hex8);
    }

    // Install plugin
    install_plugin_file(nullptr, hnd->dlg, file, folder, PLUGIN_JOB_COPY, nullptr);
    free(file);
    free(folder);
}

static void
on_archive_default(GtkMenuItem* menuitem, XSet* set)
{
    (void)menuitem;
    const char* arcname[] = {"arc_def_open", "arc_def_ex", "arc_def_exto", "arc_def_list"};
    for (unsigned int i = 0; i < G_N_ELEMENTS(arcname); i++)
    {
        if (!strcmp(set->name, arcname[i]))
            set->b = XSET_B_TRUE;
        else
            xset_set_b(arcname[i], false);
    }
}

static GtkWidget*
add_popup_menuitem(GtkWidget* popup, GtkAccelGroup* accel_group, const char* label, int job,
                   HandlerData* hnd)
{
    (void)accel_group;
    GtkWidget* item = gtk_menu_item_new_with_mnemonic(label);
    gtk_container_add(GTK_CONTAINER(popup), item);
    g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_option_cb), (void*)hnd);
    g_object_set_data(G_OBJECT(item), "job", GINT_TO_POINTER(job));
    return item;
}

static void
on_options_button_clicked(GtkWidget* btn, HandlerData* hnd)
{
    GtkWidget* item;
    XSet* set;

    // Determine if a handler is selected
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(hnd->view_handlers));
    bool handler_selected = gtk_tree_selection_get_selected(selection, nullptr, nullptr);

    // build menu
    GtkWidget* popup = gtk_menu_new();
    GtkAccelGroup* accel_group = gtk_accel_group_new();

    if (!btn)
    {
        // menu is shown from right-click on list
        item = add_popup_menuitem(popup, accel_group, "_Remove", HANDLER_JOB_REMOVE, hnd);
        gtk_widget_set_sensitive(item, handler_selected);
    }

    item = add_popup_menuitem(popup, accel_group, "_Export", HANDLER_JOB_EXPORT, hnd);
    gtk_widget_set_sensitive(item, handler_selected);

    add_popup_menuitem(popup, accel_group, "Import _File", HANDLER_JOB_IMPORT_FILE, hnd);
    add_popup_menuitem(popup,
                       accel_group,
                       "Restore _Default Handlers",
                       HANDLER_JOB_RESTORE_ALL,
                       hnd);
    if (btn)
    {
        // menu is shown from Options button
        if (hnd->mode == HANDLER_MODE_ARC)
        {
            // Archive options
            xset_context_new();
            gtk_container_add(GTK_CONTAINER(popup), gtk_separator_menu_item_new());
            set = xset_get("arc_def_open");
            // do NOT use set = xset_set_cb here or wrong set is passed
            xset_set_cb("arc_def_open", (GFunc)on_archive_default, set);
            xset_set_ob2(set, nullptr, nullptr);
            XSet* set_radio = set;

            set = xset_get("arc_def_ex");
            xset_set_cb("arc_def_ex", (GFunc)on_archive_default, set);
            xset_set_ob2(set, nullptr, set_radio);

            set = xset_get("arc_def_exto");
            xset_set_cb("arc_def_exto", (GFunc)on_archive_default, set);
            xset_set_ob2(set, nullptr, set_radio);

            set = xset_get("arc_def_list");
            xset_set_cb("arc_def_list", (GFunc)on_archive_default, set);
            xset_set_ob2(set, nullptr, set_radio);

            set = xset_get("arc_def_write");
            set->disable = geteuid() == 0 || !xset_get_b("arc_def_parent");

            // temp remove unwanted items from Archive Defaults submenu
            set = xset_get("arc_default");
            char* old_desc = set->desc;
            set->desc = ztd::strdup("arc_def_open arc_def_ex arc_def_exto arc_def_list separator "
                                    "arc_def_parent arc_def_write");
            xset_add_menuitem(hnd->browser, popup, accel_group, set);
            free(set->desc);
            set->desc = old_desc;
        }
        else if (hnd->mode == HANDLER_MODE_FS)
        {
            // Device handler options
            xset_context_new();
            gtk_container_add(GTK_CONTAINER(popup), gtk_separator_menu_item_new());
            xset_add_menuitem(hnd->browser, popup, accel_group, xset_get("dev_mount_options"));
        }
    }

    // show menu
    gtk_widget_show_all(GTK_WIDGET(popup));
    g_signal_connect(popup, "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);
    gtk_menu_popup_at_pointer(GTK_MENU(popup), nullptr);
}

void
ptk_handler_show_config(int mode, PtkFileBrowser* file_browser, XSet* def_handler_set)
{
    char* str;

    HandlerData* hnd = g_slice_new0(HandlerData);
    hnd->mode = mode;

    /* Create handlers dialog
     * Extra nullptr on the nullptr-terminated list to placate an irrelevant
     * compilation warning */
    if (file_browser)
        hnd->parent = gtk_widget_get_toplevel(GTK_WIDGET(file_browser->main_window));
    else
        hnd->parent = nullptr;
    hnd->browser = file_browser;
    hnd->dlg = gtk_dialog_new_with_buttons(
        dialog_titles[mode],
        hnd->parent ? GTK_WINDOW(hnd->parent) : nullptr,
        GtkDialogFlags(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        nullptr,
        nullptr);
    gtk_container_set_border_width(GTK_CONTAINER(hnd->dlg), 5);
    g_signal_connect(G_OBJECT(hnd->dlg), "key-press-event", G_CALLBACK(on_textview_keypress), hnd);
    g_object_set_data(G_OBJECT(hnd->dlg), "hnd", hnd);

    // Debug code
    // LOG_INFO("Parent window title: {}", gtk_window_get_title(GTK_WINDOW(hnd->parent)));

    // Forcing dialog icon
    xset_set_window_icon(GTK_WINDOW(hnd->dlg));

    // Setting saved dialog size
    int width = xset_get_int(handler_conf_xset[HANDLER_MODE_ARC], "x");
    int height = xset_get_int(handler_conf_xset[HANDLER_MODE_ARC], "y");
    if (width && height)
        gtk_window_set_default_size(GTK_WINDOW(hnd->dlg), width, height);

    // Adding standard buttons and saving references in the dialog
    // 'Restore defaults' button has custom text but a stock image
    hnd->btn_defaults = gtk_dialog_add_button(GTK_DIALOG(hnd->dlg), "Opt_ions", GTK_RESPONSE_NONE);
    gtk_widget_set_focus_on_click(GTK_WIDGET(hnd->btn_defaults), false);
    // use clicked event because menu only shown once from dialog run???
    g_signal_connect(G_OBJECT(hnd->btn_defaults),
                     "clicked",
                     G_CALLBACK(on_options_button_clicked),
                     hnd);

    hnd->btn_defaults0 = gtk_dialog_add_button(GTK_DIALOG(hnd->dlg), "Defa_ults", GTK_RESPONSE_NO);
    gtk_widget_set_focus_on_click(GTK_WIDGET(hnd->btn_defaults0), false);

    hnd->btn_cancel = gtk_dialog_add_button(GTK_DIALOG(hnd->dlg), "Cancel", GTK_RESPONSE_CANCEL);
    hnd->btn_ok = gtk_dialog_add_button(GTK_DIALOG(hnd->dlg), "OK", GTK_RESPONSE_OK);

    // Generating left-hand side of dialog
    GtkWidget* lbl_handlers = gtk_label_new(nullptr);
    str = g_strdup_printf("<b>%s</b>", dialog_mnemonics[mode]);
    gtk_label_set_markup_with_mnemonic(GTK_LABEL(lbl_handlers), str);
    free(str);
    gtk_widget_set_halign(GTK_WIDGET(lbl_handlers), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(lbl_handlers), GTK_ALIGN_START);

    // Generating the main manager list
    // Creating model - xset name then handler name
    hnd->list = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);

    // Creating treeview - setting single-click mode (normally this
    // widget is used for file lists, where double-clicking is the norm
    // for doing an action)
    hnd->view_handlers = gtk_tree_view_new();
    gtk_tree_view_set_model(GTK_TREE_VIEW(hnd->view_handlers), GTK_TREE_MODEL(hnd->list));
    // gtk_tree_view_set_model adds a ref
    g_object_unref(hnd->list);

    /*igcr probably doesn't need to be single click, as you're not using row
     * activation, only selection changed? */
    // gtk_tree_view_set_single_click(GTK_TREE_VIEW(hnd->view_handlers)), true);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(hnd->view_handlers), false);

    // Turning the treeview into a scrollable widget
    GtkWidget* view_scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(view_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(view_scroll), hnd->view_handlers);

    // Enabling item reordering (GTK-handled drag'n'drop)
    gtk_tree_view_set_reorderable(GTK_TREE_VIEW(hnd->view_handlers), true);

    // Connecting treeview callbacks
    g_signal_connect(G_OBJECT(hnd->view_handlers),
                     "drag-end",
                     G_CALLBACK(on_configure_drag_end),
                     hnd);
    g_signal_connect(G_OBJECT(gtk_tree_view_get_selection(GTK_TREE_VIEW(hnd->view_handlers))),
                     "changed",
                     G_CALLBACK(on_configure_changed),
                     hnd);
    g_signal_connect(hnd->view_handlers,
                     "button-press-event",
                     G_CALLBACK(on_handlers_button_press),
                     hnd);
    g_signal_connect(hnd->view_handlers, "key-press-event", G_CALLBACK(on_handlers_key_press), hnd);

    // Adding column to the treeview
    GtkTreeViewColumn* col = gtk_tree_view_column_new();

    // Change columns to optimal size whenever the model changes
    gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

    GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, true);

    // Tie model data to the column
    gtk_tree_view_column_add_attribute(col, renderer, "text", COL_HANDLER_NAME);

    gtk_tree_view_append_column(GTK_TREE_VIEW(hnd->view_handlers), col);

    // Set column to take all available space - false by default
    gtk_tree_view_column_set_expand(col, true);

    // Mnemonically attaching treeview to main label
    gtk_label_set_mnemonic_widget(GTK_LABEL(lbl_handlers), GTK_WIDGET(hnd->view_handlers));

    // Treeview widgets
    hnd->btn_remove = gtk_button_new_with_mnemonic("_Remove");
    gtk_widget_set_focus_on_click(GTK_WIDGET(hnd->btn_remove), false);
    gtk_widget_set_sensitive(hnd->btn_remove, false);
    g_signal_connect(G_OBJECT(hnd->btn_remove),
                     "clicked",
                     G_CALLBACK(on_configure_button_press),
                     hnd);

    hnd->btn_add = gtk_button_new_with_mnemonic("A_dd");
    gtk_widget_set_focus_on_click(GTK_WIDGET(hnd->btn_add), false);
    g_signal_connect(G_OBJECT(hnd->btn_add), "clicked", G_CALLBACK(on_configure_button_press), hnd);

    hnd->btn_apply = gtk_button_new_with_mnemonic("Appl_y");
    gtk_widget_set_focus_on_click(GTK_WIDGET(hnd->btn_apply), false);
    gtk_widget_set_sensitive(hnd->btn_apply, false);
    g_signal_connect(G_OBJECT(hnd->btn_apply),
                     "clicked",
                     G_CALLBACK(on_configure_button_press),
                     hnd);

    hnd->btn_up = gtk_button_new_with_mnemonic("U_p");
    gtk_widget_set_focus_on_click(GTK_WIDGET(hnd->btn_up), false);
    gtk_widget_set_sensitive(hnd->btn_up, false);
    g_signal_connect(G_OBJECT(hnd->btn_up), "clicked", G_CALLBACK(on_configure_button_press), hnd);

    hnd->btn_down = gtk_button_new_with_mnemonic("Do_wn");
    gtk_widget_set_focus_on_click(GTK_WIDGET(hnd->btn_down), false);
    gtk_widget_set_sensitive(hnd->btn_down, false);
    g_signal_connect(G_OBJECT(hnd->btn_down),
                     "clicked",
                     G_CALLBACK(on_configure_button_press),
                     hnd);

    // Generating right-hand side of dialog
    hnd->chkbtn_handler_enabled = gtk_check_button_new_with_mnemonic(
        mode == HANDLER_MODE_FILE ? "Ena_ble as a default opener" : "Ena_ble Handler");
    gtk_widget_set_focus_on_click(GTK_WIDGET(hnd->chkbtn_handler_enabled), false);
    g_signal_connect(G_OBJECT(hnd->chkbtn_handler_enabled),
                     "toggled",
                     G_CALLBACK(on_configure_handler_enabled_check),
                     hnd);
    GtkWidget* lbl_handler_name = gtk_label_new(nullptr);
    gtk_label_set_markup_with_mnemonic(GTK_LABEL(lbl_handler_name), "_Name:");
    gtk_widget_set_halign(GTK_WIDGET(lbl_handler_name), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(lbl_handler_name), GTK_ALIGN_CENTER);
    GtkWidget* lbl_handler_mime = gtk_label_new(nullptr);
    gtk_label_set_markup_with_mnemonic(
        GTK_LABEL(lbl_handler_mime),
        mode == HANDLER_MODE_ARC || mode == HANDLER_MODE_FILE ? "MIM_E Type:" : "Whit_elist:");
    gtk_widget_set_halign(GTK_WIDGET(lbl_handler_mime), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(lbl_handler_mime), GTK_ALIGN_CENTER);
    GtkWidget* lbl_handler_extension = gtk_label_new(nullptr);
    gtk_label_set_markup_with_mnemonic(
        GTK_LABEL(lbl_handler_extension),
        mode == HANDLER_MODE_ARC || mode == HANDLER_MODE_FILE ? "P_athname:" : "Bl_acklist:");
    gtk_widget_set_halign(GTK_WIDGET(lbl_handler_extension), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(lbl_handler_extension), GTK_ALIGN_END);
    GtkWidget* lbl_handler_icon;
    if (mode == HANDLER_MODE_FILE)
    {
        lbl_handler_icon = gtk_label_new(nullptr);
        gtk_label_set_markup_with_mnemonic(GTK_LABEL(lbl_handler_icon), "_Icon:");
        gtk_widget_set_halign(GTK_WIDGET(lbl_handler_icon), GTK_ALIGN_START);
        gtk_widget_set_valign(GTK_WIDGET(lbl_handler_icon), GTK_ALIGN_END);
    }
    else
        lbl_handler_icon = nullptr;

    GtkWidget* lbl_handler_compress = gtk_label_new(nullptr);
    if (mode == HANDLER_MODE_ARC)
        str = ztd::strdup("<b>Co_mpress:</b>");
    else if (mode == HANDLER_MODE_FILE)
        str = ztd::strdup("<b>Open Co_mmand:</b>");
    else
        str = ztd::strdup("<b>_Mount:</b>");
    gtk_label_set_markup_with_mnemonic(GTK_LABEL(lbl_handler_compress), str);
    gtk_widget_set_halign(GTK_WIDGET(lbl_handler_compress), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(lbl_handler_compress), GTK_ALIGN_END);
    GtkWidget* lbl_handler_extract = gtk_label_new(nullptr);
    gtk_label_set_markup_with_mnemonic(GTK_LABEL(lbl_handler_extract),
                                       mode == HANDLER_MODE_ARC ? "<b>Ex_tract:</b>"
                                                                : "<b>Unmoun_t:</b>");
    gtk_widget_set_halign(GTK_WIDGET(lbl_handler_extract), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(lbl_handler_extract), GTK_ALIGN_END);
    GtkWidget* lbl_handler_list = gtk_label_new(nullptr);
    gtk_label_set_markup_with_mnemonic(GTK_LABEL(lbl_handler_list),
                                       mode == HANDLER_MODE_ARC ? "<b>Li_st:</b>"
                                                                : "<b>Propertie_s:</b>");
    gtk_widget_set_halign(GTK_WIDGET(lbl_handler_list), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(lbl_handler_list), GTK_ALIGN_END);
    hnd->entry_handler_name = gtk_entry_new();
    hnd->entry_handler_mime = gtk_entry_new();
    hnd->entry_handler_extension = gtk_entry_new();
    if (mode == HANDLER_MODE_FILE)
    {
        hnd->entry_handler_icon = gtk_entry_new();
        hnd->icon_choose_btn = gtk_button_new_with_mnemonic("C_hoose");
        gtk_widget_set_focus_on_click(GTK_WIDGET(hnd->icon_choose_btn), false);

        // keep this
        gtk_button_set_always_show_image(GTK_BUTTON(hnd->icon_choose_btn), true);
    }
    else
        hnd->entry_handler_icon = hnd->icon_choose_btn = nullptr;

    g_signal_connect(G_OBJECT(gtk_entry_get_buffer(GTK_ENTRY(hnd->entry_handler_name))),
                     "inserted-text",
                     G_CALLBACK(on_entry_text_insert),
                     hnd);
    g_signal_connect(G_OBJECT(gtk_entry_get_buffer(GTK_ENTRY(hnd->entry_handler_name))),
                     "deleted-text",
                     G_CALLBACK(on_entry_text_delete),
                     hnd);
    g_signal_connect(G_OBJECT(gtk_entry_get_buffer(GTK_ENTRY(hnd->entry_handler_mime))),
                     "inserted-text",
                     G_CALLBACK(on_entry_text_insert),
                     hnd);
    g_signal_connect(G_OBJECT(gtk_entry_get_buffer(GTK_ENTRY(hnd->entry_handler_mime))),
                     "deleted-text",
                     G_CALLBACK(on_entry_text_delete),
                     hnd);
    g_signal_connect(G_OBJECT(gtk_entry_get_buffer(GTK_ENTRY(hnd->entry_handler_extension))),
                     "inserted-text",
                     G_CALLBACK(on_entry_text_insert),
                     hnd);
    g_signal_connect(G_OBJECT(gtk_entry_get_buffer(GTK_ENTRY(hnd->entry_handler_extension))),
                     "deleted-text",
                     G_CALLBACK(on_entry_text_delete),
                     hnd);
    if (hnd->entry_handler_icon)
    {
        g_signal_connect(G_OBJECT(gtk_entry_get_buffer(GTK_ENTRY(hnd->entry_handler_icon))),
                         "inserted-text",
                         G_CALLBACK(on_entry_text_insert),
                         hnd);
        g_signal_connect(G_OBJECT(gtk_entry_get_buffer(GTK_ENTRY(hnd->entry_handler_icon))),
                         "deleted-text",
                         G_CALLBACK(on_entry_text_delete),
                         hnd);
        g_signal_connect(G_OBJECT(hnd->icon_choose_btn),
                         "clicked",
                         G_CALLBACK(on_icon_choose_button_clicked),
                         hnd);
    }

    /* Creating new textviews in scrolled windows */
    hnd->view_handler_compress = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(hnd->view_handler_compress), GTK_WRAP_WORD_CHAR);
    GtkWidget* view_handler_compress_scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(view_handler_compress_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(view_handler_compress_scroll), hnd->view_handler_compress);
    g_signal_connect(G_OBJECT(hnd->view_handler_compress),
                     "key-press-event",
                     G_CALLBACK(on_textview_keypress),
                     hnd);
    hnd->buf_handler_compress = gtk_text_view_get_buffer(GTK_TEXT_VIEW(hnd->view_handler_compress));
    g_signal_connect(G_OBJECT(hnd->buf_handler_compress),
                     "changed",
                     G_CALLBACK(on_textview_buffer_changed),
                     hnd);

    hnd->view_handler_extract = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(hnd->view_handler_extract), GTK_WRAP_WORD_CHAR);
    GtkWidget* view_handler_extract_scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(view_handler_extract_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(view_handler_extract_scroll), hnd->view_handler_extract);
    g_signal_connect(G_OBJECT(hnd->view_handler_extract),
                     "key-press-event",
                     G_CALLBACK(on_textview_keypress),
                     hnd);
    hnd->buf_handler_extract = gtk_text_view_get_buffer(GTK_TEXT_VIEW(hnd->view_handler_extract));
    g_signal_connect(G_OBJECT(hnd->buf_handler_extract),
                     "changed",
                     G_CALLBACK(on_textview_buffer_changed),
                     hnd);

    hnd->view_handler_list = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(hnd->view_handler_list), GTK_WRAP_WORD_CHAR);
    GtkWidget* view_handler_list_scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(view_handler_list_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(view_handler_list_scroll), hnd->view_handler_list);
    g_signal_connect(G_OBJECT(hnd->view_handler_list),
                     "key-press-event",
                     G_CALLBACK(on_textview_keypress),
                     hnd);
    hnd->buf_handler_list = gtk_text_view_get_buffer(GTK_TEXT_VIEW(hnd->view_handler_list));
    g_signal_connect(G_OBJECT(hnd->buf_handler_list),
                     "changed",
                     G_CALLBACK(on_textview_buffer_changed),
                     hnd);

    // set textview popup menu event handlers
    g_signal_connect_after(G_OBJECT(hnd->view_handler_compress),
                           "populate-popup",
                           G_CALLBACK(on_textview_popup),
                           hnd);
    g_signal_connect_after(G_OBJECT(hnd->view_handler_extract),
                           "populate-popup",
                           G_CALLBACK(on_textview_popup),
                           hnd);
    g_signal_connect_after(G_OBJECT(hnd->view_handler_list),
                           "populate-popup",
                           G_CALLBACK(on_textview_popup),
                           hnd);

    // Setting widgets to be activated associated with label mnemonics
    gtk_label_set_mnemonic_widget(GTK_LABEL(lbl_handler_name), hnd->entry_handler_name);
    gtk_label_set_mnemonic_widget(GTK_LABEL(lbl_handler_mime), hnd->entry_handler_mime);
    gtk_label_set_mnemonic_widget(GTK_LABEL(lbl_handler_extension), hnd->entry_handler_extension);
    gtk_label_set_mnemonic_widget(GTK_LABEL(lbl_handler_compress), hnd->view_handler_compress);
    gtk_label_set_mnemonic_widget(GTK_LABEL(lbl_handler_extract), hnd->view_handler_extract);
    gtk_label_set_mnemonic_widget(GTK_LABEL(lbl_handler_list), hnd->view_handler_list);

    hnd->chkbtn_handler_compress_term = gtk_check_button_new_with_label("Run In Terminal");
    hnd->chkbtn_handler_extract_term = gtk_check_button_new_with_label(
        mode == HANDLER_MODE_FILE ? "Run As Task" : "Run In Terminal");
    hnd->chkbtn_handler_list_term = gtk_check_button_new_with_label("Run In Terminal");
    gtk_widget_set_focus_on_click(GTK_WIDGET(hnd->chkbtn_handler_compress_term), false);
    g_signal_connect(G_OBJECT(hnd->chkbtn_handler_compress_term),
                     "toggled",
                     G_CALLBACK(on_terminal_toggled),
                     hnd);
    gtk_widget_set_focus_on_click(GTK_WIDGET(hnd->chkbtn_handler_extract_term), false);
    g_signal_connect(G_OBJECT(hnd->chkbtn_handler_extract_term),
                     "toggled",
                     G_CALLBACK(on_terminal_toggled),
                     hnd);
    gtk_widget_set_focus_on_click(GTK_WIDGET(hnd->chkbtn_handler_list_term), false);
    g_signal_connect(G_OBJECT(hnd->chkbtn_handler_list_term),
                     "toggled",
                     G_CALLBACK(on_terminal_toggled),
                     hnd);

    GtkWidget* lbl_edit0 = gtk_label_new(nullptr);
    str = g_strdup_printf("<a href=\"%d\">%s</a>", 0, "Edit");
    gtk_label_set_markup_with_mnemonic(GTK_LABEL(lbl_edit0), str);
    free(str);
    g_signal_connect(G_OBJECT(lbl_edit0), "activate-link", G_CALLBACK(on_activate_link), hnd);
    GtkWidget* lbl_edit1 = gtk_label_new(nullptr);
    str = g_strdup_printf("<a href=\"%d\">%s</a>", 1, "Edit");
    gtk_label_set_markup_with_mnemonic(GTK_LABEL(lbl_edit1), str);
    free(str);
    g_signal_connect(G_OBJECT(lbl_edit1), "activate-link", G_CALLBACK(on_activate_link), hnd);
    GtkWidget* lbl_edit2 = gtk_label_new(nullptr);
    str = g_strdup_printf("<a href=\"%d\">%s</a>", 2, "Edit");
    gtk_label_set_markup_with_mnemonic(GTK_LABEL(lbl_edit2), str);
    free(str);
    g_signal_connect(G_OBJECT(lbl_edit2), "activate-link", G_CALLBACK(on_activate_link), hnd);

    /* Creating container boxes - at this point the dialog already comes
     * with one GtkVBox then inside that a GtkHButtonBox
     * For the right side of the dialog, standard GtkBox approach fails
     * to allow precise padding of labels to allow all entries to line up
     *  - so reimplementing with GtkTable. Would many GtkAlignments have
     * worked? */
    GtkWidget* hbox_main = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* vbox_handlers = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget* hbox_view_buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* hbox_move_buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* vbox_settings = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget* hbox_compress_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* hbox_extract_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* hbox_list_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

    GtkGrid* grid = GTK_GRID(gtk_grid_new());

    /* Packing widgets into boxes
     * Remember, start and end-ness is broken
     * vbox_handlers packing must not expand so that the right side can
     * take the space */
    gtk_box_pack_start(GTK_BOX(hbox_main), GTK_WIDGET(vbox_handlers), false, false, 4);
    gtk_box_pack_start(GTK_BOX(vbox_handlers), GTK_WIDGET(lbl_handlers), false, false, 4);
    gtk_box_pack_start(GTK_BOX(hbox_main), GTK_WIDGET(vbox_settings), true, true, 4);
    gtk_box_pack_start(GTK_BOX(vbox_settings), hnd->chkbtn_handler_enabled, false, false, 4);
    gtk_box_pack_start(GTK_BOX(vbox_settings), GTK_WIDGET(grid), false, false, 4);

    /* view_handlers isn't added but view_scroll is - view_handlers is
     * inside view_scroll. No padding added to get it to align with the
     * enabled widget on the right side */
    gtk_box_pack_start(GTK_BOX(vbox_handlers), GTK_WIDGET(view_scroll), true, true, 0);
    gtk_box_pack_start(GTK_BOX(vbox_handlers), GTK_WIDGET(hbox_view_buttons), false, false, 0);
    gtk_box_pack_start(GTK_BOX(vbox_handlers), GTK_WIDGET(hbox_move_buttons), false, false, 0);
    gtk_box_pack_start(GTK_BOX(hbox_view_buttons), GTK_WIDGET(hnd->btn_remove), true, true, 4);
    gtk_box_pack_start(GTK_BOX(hbox_view_buttons),
                       GTK_WIDGET(gtk_separator_new(GTK_ORIENTATION_VERTICAL)),
                       true,
                       true,
                       4);
    gtk_box_pack_start(GTK_BOX(hbox_view_buttons), GTK_WIDGET(hnd->btn_add), true, true, 4);
    gtk_box_pack_start(GTK_BOX(hbox_view_buttons), GTK_WIDGET(hnd->btn_apply), true, true, 4);
    gtk_box_pack_start(GTK_BOX(hbox_move_buttons), GTK_WIDGET(hnd->btn_up), true, true, 4);
    gtk_box_pack_start(GTK_BOX(hbox_move_buttons), GTK_WIDGET(hnd->btn_down), true, true, 4);

    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);

    gtk_grid_attach(grid, GTK_WIDGET(lbl_handler_name), 0, 0, 1, 1);
    gtk_grid_attach(grid, GTK_WIDGET(hnd->entry_handler_name), 1, 0, 1, 1);
    gtk_grid_attach(grid, GTK_WIDGET(lbl_handler_mime), 0, 1, 1, 1);
    gtk_grid_attach(grid, GTK_WIDGET(hnd->entry_handler_mime), 1, 1, 1, 1);
    gtk_grid_attach(grid, GTK_WIDGET(lbl_handler_extension), 0, 2, 1, 1);
    gtk_grid_attach(grid, GTK_WIDGET(hnd->entry_handler_extension), 1, 2, 1, 1);

    if (mode == HANDLER_MODE_FILE)
    {
        gtk_grid_attach(grid, GTK_WIDGET(lbl_handler_icon), 0, 3, 1, 1);
        GtkWidget* hbox_icon = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        gtk_box_pack_start(GTK_BOX(hbox_icon), GTK_WIDGET(hnd->entry_handler_icon), true, true, 0);
        gtk_box_pack_start(GTK_BOX(hbox_icon), GTK_WIDGET(hnd->icon_choose_btn), false, true, 0);
        gtk_grid_attach(grid, GTK_WIDGET(hbox_icon), 1, 3, 1, 1);
    }

    // Make sure widgets do not separate too much vertically
    gtk_box_set_spacing(GTK_BOX(vbox_settings), 1);

    // pack_end widgets must not expand to be flush up against the side
    gtk_box_pack_start(GTK_BOX(vbox_settings), GTK_WIDGET(hbox_compress_header), false, false, 4);
    gtk_box_pack_start(GTK_BOX(hbox_compress_header),
                       GTK_WIDGET(lbl_handler_compress),
                       true,
                       true,
                       4);
    if (mode == HANDLER_MODE_FILE)
        // for file handlers, extract_term is used for Run As Task
        gtk_box_pack_start(GTK_BOX(hbox_compress_header),
                           hnd->chkbtn_handler_extract_term,
                           false,
                           true,
                           4);
    gtk_box_pack_start(GTK_BOX(hbox_compress_header),
                       hnd->chkbtn_handler_compress_term,
                       false,
                       true,
                       4);
    gtk_box_pack_end(GTK_BOX(hbox_compress_header), GTK_WIDGET(lbl_edit0), false, false, 4);
    gtk_box_pack_start(GTK_BOX(vbox_settings),
                       GTK_WIDGET(view_handler_compress_scroll),
                       true,
                       true,
                       4);

    gtk_box_pack_start(GTK_BOX(vbox_settings), GTK_WIDGET(hbox_extract_header), false, false, 4);
    gtk_box_pack_start(GTK_BOX(hbox_extract_header),
                       GTK_WIDGET(lbl_handler_extract),
                       true,
                       true,
                       4);
    if (mode != HANDLER_MODE_FILE)
        gtk_box_pack_start(GTK_BOX(hbox_extract_header),
                           hnd->chkbtn_handler_extract_term,
                           false,
                           true,
                           4);
    gtk_box_pack_end(GTK_BOX(hbox_extract_header), GTK_WIDGET(lbl_edit1), false, false, 4);
    gtk_box_pack_start(GTK_BOX(vbox_settings),
                       GTK_WIDGET(view_handler_extract_scroll),
                       true,
                       true,
                       4);

    gtk_box_pack_start(GTK_BOX(vbox_settings), GTK_WIDGET(hbox_list_header), false, false, 4);
    gtk_box_pack_start(GTK_BOX(hbox_list_header), GTK_WIDGET(lbl_handler_list), true, true, 4);
    gtk_box_pack_start(GTK_BOX(hbox_list_header), hnd->chkbtn_handler_list_term, false, true, 4);
    gtk_box_pack_end(GTK_BOX(hbox_list_header), GTK_WIDGET(lbl_edit2), false, false, 4);
    gtk_box_pack_start(GTK_BOX(vbox_settings), GTK_WIDGET(view_handler_list_scroll), true, true, 4);

    /* Packing boxes into dialog with padding to separate from dialog's
     * standard buttons at the bottom */
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(hnd->dlg))),
                       GTK_WIDGET(hbox_main),
                       true,
                       true,
                       4);

    // Adding archive handlers to list
    populate_archive_handlers(hnd, def_handler_set);

    // Show All
    gtk_widget_show_all(GTK_WIDGET(hnd->dlg));
    if (mode == HANDLER_MODE_FILE)
    {
        gtk_widget_hide(hbox_extract_header);
        gtk_widget_hide(hbox_list_header);
        gtk_widget_hide(view_handler_extract_scroll);
        gtk_widget_hide(view_handler_list_scroll);
    }

    /* Rendering dialog - while loop is used to deal with standard
     * buttons that should not cause the dialog to exit */
    /*igcr need to handle dialog delete event? */
    int response;
    while ((response = gtk_dialog_run(GTK_DIALOG(hnd->dlg))))
    {
        bool exit_loop = false;
        // const char* help = nullptr;
        switch (response)
        {
            case GTK_RESPONSE_OK:
                if (hnd->changed)
                    on_configure_button_press(GTK_BUTTON(hnd->btn_apply), hnd);
                exit_loop = true;
                break;
            case GTK_RESPONSE_CANCEL:
                exit_loop = true;
                break;
            case GTK_RESPONSE_NONE:
                // Options menu requested
                break;
            case GTK_RESPONSE_NO:
                // Restore defaults requested
                restore_defaults(hnd, false);
                break;
            default:
                exit_loop = true;
                break;
        }
        if (exit_loop)
            break;
    }

    // Fetching dialog dimensions
    GtkAllocation allocation;
    gtk_widget_get_allocation(GTK_WIDGET(hnd->dlg), &allocation);
    width = allocation.width;
    height = allocation.height;

    // Checking if they are valid
    if (width && height)
    {
        // They are - saving
        str = g_strdup_printf("%d", width);
        xset_set(handler_conf_xset[HANDLER_MODE_ARC], "x", str);
        free(str);
        str = g_strdup_printf("%d", height);
        xset_set(handler_conf_xset[HANDLER_MODE_ARC], "y", str);
        free(str);
    }

    // Clearing up dialog
    gtk_widget_destroy(hnd->dlg);
    g_slice_free(HandlerData, hnd);
}
