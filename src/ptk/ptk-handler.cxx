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

#include <magic_enum.hpp>
#include <string>
#include <string_view>

#include <format>

#include <filesystem>

#include <span>

#include <array>
#include <vector>

#include <algorithm>
#include <ranges>

#include <optional>

#include <fstream>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-user-dirs.hxx"

#include "xset/xset.hxx"
#include "xset/xset-context.hxx"
#include "xset/xset-custom.hxx"
#include "xset/xset-dialog.hxx"

#include "ptk/ptk-dialog.hxx"

#include "ptk/ptk-handler.hxx"
#include "ptk/ptk-keyboard.hxx"

#include "autosave.hxx"
#include "write.hxx"

#define INFO_EXAMPLE                                                                            \
    "# Enter command to show properties or leave blank for auto:\n\n\n# # Example:\n\n# echo "  \
    "MOUNT\n# mount | grep \" on %a \"\n# echo\n# echo PROCESSES\n# /usr/bin/lsof -w \"%a\" | " \
    "head -n 500\n"

namespace ptk::handler
{
    enum class job
    {
        restore_all,
        remove,
    };

    enum class column
    { // Archive handlers treeview model enum
        xset_name,
        handler_name,
    };
} // namespace ptk::handler

// xset name prefixes of default handlers
inline constexpr std::array<const std::string_view, 4> handler_def_prefixs{
    "handler_archive_",
    "handler_filesystem_",
    "handler_network_",
    "handler_file_",
};

// xset name prefixes of custom handlers
inline constexpr std::array<const std::string_view, 4> handler_cust_prefixs{
    "custom_handler_archive_",
    "custom_handler_filesystem_",
    "custom_handler_network_",
    "custom_handler_file_",
};

inline constexpr std::array<xset::name, 4> handler_conf_xsets{
    xset::name::arc_conf2,
    xset::name::dev_fs_cnf,
    xset::name::dev_net_cnf,
    xset::name::open_hand,
};

inline constexpr std::array<const std::string_view, 4> dialog_titles{
    "Archive Handlers",
    "Device Handlers",
    "Protocol Handlers",
    "File Handlers",
};

inline constexpr std::array<const std::string_view, 4> dialog_mnemonics{
    "Archive Hand_lers",
    "Device Hand_lers",
    "Protocol Hand_lers",
    "File Hand_lers",
};

inline constexpr std::array<const std::string_view, 4> modes{
    "archive",
    "device",
    "protocol",
    "file",
};

inline constexpr std::array<const std::string_view, 3> cmds_arc{
    "compress",
    "extract",
    "list",
};

inline constexpr std::array<const std::string_view, 3> cmds_mnt{
    "mount",
    "unmount",
    "info",
};

struct HandlerData
{
    HandlerData() = default;
    ~HandlerData();

    GtkWidget* dlg{nullptr};
    GtkWidget* parent{nullptr};
    i32 mode{0};
    bool changed{false};
    PtkFileBrowser* browser{nullptr};

    GtkWidget* view_handlers{nullptr};
    GtkListStore* list{nullptr};

    GtkWidget* chkbtn_handler_enabled{nullptr};
    GtkWidget* entry_handler_name{nullptr};
    GtkWidget* entry_handler_mime{nullptr};
    GtkWidget* entry_handler_extension{nullptr};
    GtkWidget* entry_handler_icon{nullptr};
    GtkWidget* view_handler_compress{nullptr};
    GtkWidget* view_handler_extract{nullptr};
    GtkWidget* view_handler_list{nullptr};
    GtkTextBuffer* buf_handler_compress{nullptr};
    GtkTextBuffer* buf_handler_extract{nullptr};
    GtkTextBuffer* buf_handler_list{nullptr};

    bool compress_changed{false};
    bool extract_changed{false};
    bool list_changed{false};

    GtkWidget* chkbtn_handler_compress_term{nullptr};
    GtkWidget* chkbtn_handler_extract_term{nullptr};
    GtkWidget* chkbtn_handler_list_term{nullptr};
    GtkWidget* btn_remove{nullptr};
    GtkWidget* btn_add{nullptr};
    GtkWidget* btn_apply{nullptr};
    GtkWidget* btn_up{nullptr};
    GtkWidget* btn_down{nullptr};
    GtkWidget* btn_ok{nullptr};
    GtkWidget* btn_cancel{nullptr};
    GtkWidget* btn_defaults{nullptr};
    GtkWidget* btn_defaults0{nullptr};
    GtkWidget* icon_choose_btn{nullptr};
};

HandlerData::~HandlerData()
{
    gtk_widget_destroy(this->dlg);
}

struct Handler
{
    // enabled                                        set->b
    const char* setname{nullptr};      //                      set->name
    xset::name xset_name;              //                      set->xset_name
    const char* handler_name{nullptr}; //                      set->menu_label
    const char* type{nullptr};         // or whitelist         set->s
    const char* ext{nullptr};          // or blacklist         set->x
    const char* compress_cmd{nullptr}; // or mount             (script)
    bool compress_term{false};         //                      set->in_terminal
    const char* extract_cmd{nullptr};  // or unmount           (script)
    bool extract_term{false};          // or run task file     set->keep_terminal
    const char* list_cmd{nullptr};     // or info              (script)
    bool list_term{false};             //                      set->scroll_lock
    // save as custom item                            set->lock = false
    // if handler equals default, do not save         set->disable = true
    // icon (file handlers only)                      set->icon
};

/* If you add a new handler, add it to (end of ) existing session file handler
 * list so existing users see the new handler. */
inline constexpr std::array<Handler, 13> handlers_arc{
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
     * Plus standard fish variables are accepted.
     */
    Handler{
        "handler_archive_7z",
        xset::name::handler_archive_7z,
        "7-Zip",
        "application/x-7z-compressed",
        "*.7z",
        "7z a %o %N",
        true,
        "7z x %x",
        true,
        "7z l %x",
        false,
    },
    Handler{
        "handler_archive_rar",
        xset::name::handler_archive_rar,
        "RAR",
        "application/x-rar",
        "*.rar *.RAR",
        "command rar a -r %o %N",
        true,
        "unrar -o- x %x",
        true,
        "unrar lt %x",
        false,
    },
    Handler{
        "handler_archive_tar",
        xset::name::handler_archive_tar,
        "Tar",
        "application/x-tar",
        "*.tar",
        "tar -cvf %o %N",
        false,
        "tar -xvf %x",
        false,
        "tar -tvf %x",
        false,
    },
    Handler{
        "handler_archive_tar_bz2",
        xset::name::handler_archive_tar_bz2,
        "Tar bzip2",
        "application/x-bzip-compressed-tar",
        "*.tar.bz2",
        "tar -cvjf %o %N",
        false,
        "tar -xvjf %x",
        false,
        "tar -tvf %x",
        false,
    },
    Handler{
        "handler_archive_tar_gz",
        xset::name::handler_archive_tar_gz,
        "Tar Gzip",
        "application/x-compressed-tar",
        "*.tar.gz *.tgz",
        "tar -cvzf %o %N",
        false,
        "tar -xvzf %x",
        false,
        "tar -tvf %x",
        false,
    },
    Handler{
        "handler_archive_tar_xz",
        xset::name::handler_archive_tar_xz,
        "Tar xz",
        "application/x-xz-compressed-tar",
        "*.tar.xz *.txz",
        "tar -cvJf %o %N",
        false,
        "tar -xvJf %x",
        false,
        "tar -tvf %x",
        false,
    },
    Handler{
        "handler_archive_zip",
        xset::name::handler_archive_zip,
        "Zip",
        "application/x-zip application/zip",
        "*.zip *.ZIP",
        "zip -r %o %N",
        true,
        "unzip %x",
        true,
        "unzip -l %x",
        false,
    },
    Handler{
        "handler_archive_gz",
        xset::name::handler_archive_gz,
        "Gzip",
        "application/x-gzip application/x-gzpdf application/gzip",
        "*.gz",
        "gzip -c %N >| %O",
        false,
        "gzip -cd %x >| %G",
        false,
        "gunzip -l %x",
        false,
    },
    Handler{
        "handler_archive_xz",
        xset::name::handler_archive_xz,
        "XZ",
        "application/x-xz",
        "*.xz",
        "xz -cz %N >| %O",
        false,
        "xz -cd %x >| %G",
        false,
        "xz -tv %x",
        false,
    },
    Handler{
        "handler_archive_tar_lz4",
        xset::name::handler_archive_tar_lz4,
        "Tar Lz4",
        "application/x-lz4-compressed-tar",
        "*.tar.lz4",
        "tar -I lz4 -cvf %o %N",
        false,
        "tar -I lz4 -xvf %x",
        false,
        "lz4 -dc %x | tar tvf -",
        false,
    },
    Handler{
        "handler_archive_lz4",
        xset::name::handler_archive_lz4,
        "Lz4",
        "application/x-lz4",
        "*.lz4",
        "lz4 -c %N >| %O",
        false,
        "lz4 -d -c %x >| %G",
        false,
        "lz4 -tv %x",
        false,
    },
    Handler{
        "handler_archive_tar_zst",
        xset::name::handler_archive_tar_zst,
        "Tar Zstd",
        "application/x-zstd-compressed-tar",
        "*.tar.zst",
        "tar -I 'zstd --long=31' -cvf %o %N",
        false,
        "zstd -dc --long=31 %x | tar xvf -",
        false,
        "zstd -dc --long=31 %x | tar tvf -",
        false,
    },
    Handler{
        "handler_archive_zst",
        xset::name::handler_archive_zip,
        "Zstd",
        "application/zstd",
        "*.zst",
        "zstd -c --long=31 %N >| %O",
        false,
        "zstd -dc --long=31 -d %x >| %G",
        false,
        "zstd -dc --long=31 -tlv %x",
        false,
    },
};

inline constexpr std::array<Handler, 3> handlers_fs{
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
    Handler{
        "handler_filesystem_fuseiso",
        xset::name::handler_filesystem_fuseiso,
        "fuseiso unmount",
        "*fuseiso",
        "",
        "# Mounting of iso files is performed by fuseiso in a file handler,\n"
        "# not this device handler.  Right-click on any file and select\n"
        "# Open|File Handlers, and select Mount ISO to see this command.",
        false,
        "fusermount -u \"%a\"",
        false,
        "grep \"%a\" ~/.mtab.fuseiso",
        false,
    },
    Handler{
        "handler_filesystem_udiso",
        xset::name::handler_filesystem_udiso,
        "udevil iso unmount",
        "+iso9660 +dev=/dev/loop*",
        "optical=1 removable=1",
        "# Mounting of iso files is performed by udevil in a file handler,\n"
        "# not this device handler.  Right-click on any file and select\n"
        "# Open|File Handlers, and select Mount ISO to see this command.",
        false,
        "# Note: non-iso9660 types will fall through to Default unmount handler\n"
        "udevil umount \"%a\"\n",
        false,
        INFO_EXAMPLE,
        false,
    },
    Handler{
        "handler_filesystem_default",
        xset::name::handler_filesystem_default,
        "Default",
        "*",
        "",
        // clang-format off
        "# Enter mount command or leave blank for auto:\n\n\n"
        "# # Examples: (remove # to "
        "enable a mount command)\n"
        "#\n"
        "# # udevil:\n"
        "#     udevil mount -o '%o' %v\n"
        "#\n"
        "# # pmount: (does not accept mount options)\n"
        "#     pmount %v\n"
        "#\n"
        "# # udisks v2:\n"
        "#     udisksctl mount -b %v -o '%o'\n",
        false,
        "# Enter unmount command or leave blank for auto:\n\n\n"
        "# # Examples: (remove # to enable an unmount command)\n"
        "#\n"
        "# # udevil:\n"
        "#     udevil umount %v\n"
        "#\n"
        "# # pmount:\n"
        "#     pumount %v\n"
        "#\n"
        "# # udisks v2:\n"
        "#     udisksctl unmount -b %v\n"
        "#\n",
        // clang-format on
        false,
        INFO_EXAMPLE,
        false,
    },
};

inline constexpr std::array<Handler, 10> handlers_net{
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
    Handler{
        "handler_network_http",
        xset::name::handler_network_http,
        "http & webdav",
        "http https webdav davfs davs dav mtab_fs=davfs*",
        "",
        // clang-format off
        "# This handler opens http:// and webdav://\n\n"
        "# Set your web browser in Help|Options|Browser\n\n"
        "# set missing_davfs=1 if you always want to open http in web browser\n"
        "# set missing_davfs=0 if you always want to mount http with davfs\n"
        "missing_davfs=\n\n"
        "if [ -z \"$missing_davfs\" ];then\n"
        "    grep -qs '^[[:space:]]*allowed_types[[:space:]]*=[^#]*davfs' \\\n"
        "                                    /etc/udevil/udevil.conf 2>/dev/null\n"
        "    missing_davfs=$status\n"
        "fi\n"
        "if [ \"$fm_url_proto\" = \"webdav\" ] || [ \"$fm_url_proto\" = \"davfs\" ] || \\\n"
        "   [ \"$fm_url_proto\" = \"dav\" ]    || [ \"$fm_url_proto\" = \"davs\" ] || \\\n"
        "   [ $missing_davfs -eq 0 ];then\n"
        "    fm_url_proto=\"${fm_url_proto/webdav/http}\"\n"
        "    fm_url_proto=\"${fm_url_proto/davfs/http}\"\n"
        "    fm_url_proto=\"${fm_url_proto/davs/https}\"\n"
        "    fm_url_proto=\"${fm_url_proto/dav/http}\"\n"
        "    url=\"${fm_url_proto}://${fm_url_host}${fm_url_port:+:}${fm_url_port}${fm_url_path:-/}\"\n"
        "    [[ -z \"$fm_url_user$fm_url_password\" ]] && msg=\"\" || \\\n"
        "            msg=\"Warning: user:password in URL is not supported by davfs.\"\n"
        "    # attempt davfs mount in terminal\n"
        "    spacefm socket run-task cmd --terminal \\\n"
        "        \"echo $msg; echo 'udevil mount $url'; udevil mount '$url' || \" \\\n"
        "                        \"( echo; echo 'Press Enter to close:'; read )\"\n"
        "    exit\n"
        "fi\n"
        "# open in web browser\n"
        "spacefm socket run-task web \"$fm_url\"\n",
        // clang-format on
        false,
        "# Note: Unmount is usually performed by the 'fuse unmount' handler.\n\nudevil umount "
        "\"%a\"",
        false,
        INFO_EXAMPLE,
        false,
    },
    Handler{
        "handler_network_ftp",
        xset::name::handler_network_ftp,
        "ftp",
        "ftp",
        "",
        // clang-format off
        "options=\"nonempty\"\n"
        "if [ -n \"%user%\" ];then\n"
        "    user=\",user=%user%\"\n"
        "    [[ -n \"%pass%\" ]] && user=\"$user:%pass%\"\n"
        "fi\n"
        "[[ -n \"%port%\" ]] && portcolon=:\n"
        "echo \">>> curlftpfs -o $options$user ftp://%host%${portcolon}%port%%path% %a\"\n"
        "echo\n"
        "curlftpfs -o $options$user ftp://%host%${portcolon}%port%%path% \"%a\"\n"
        "[[ $status -eq 0 ]] && sleep 1 && ls \"%a\"  # set error status or wait until ready\n",
        // clang-format on
        true,
        "fusermount -u \"%a\"",
        false,
        INFO_EXAMPLE,
        false,
    },
    Handler{
        "handler_network_ssh",
        xset::name::handler_network_ssh,
        "ssh",
        "ssh sftp mtab_fs=fuse.sshfs",
        "",
        // clang-format off
        "[[ -n \"$fm_url_user\" ]] && fm_url_user=\"${fm_url_user}@\"\n"
        "[[ -z \"$fm_url_port\" ]] && fm_url_port=22\n"
        "echo \">>> sshfs -p $fm_url_port $fm_url_user$fm_url_host:$fm_url_path %a\"\n"
        "echo\n"
        "# Run sshfs through nohup to prevent disconnect on terminal close\n"
        "sshtmp=\"$(mktemp --tmpdir spacefm-ssh-output-XXXXXXXX.tmp)\" || exit 1\n"
        "nohup sshfs -p $fm_url_port $fm_url_user$fm_url_host:$fm_url_path %a &> \"$sshtmp\"\n"
        "err=$status\n"
        "[[ -e \"$sshtmp\" ]] && cat \"$sshtmp\" ; rm -f \"$sshtmp\"\n"
        "[[ $err -eq 0 ]]  # set error status\n\n"
        "# Alternate Method - if enabled, disable nohup line above and\n"
        "#                    uncheck Run In Terminal\n"
        "# # Run sshfs in a terminal without SpaceFM task.  sshfs disconnects when the\n"
        "# # terminal is closed\n"
        "# spacefm socket run-task cmd --terminal \"echo 'Connecting to $fm_url'; echo; sshfs -p $fm_url_port $fm_url_user$fm_url_host:$fm_url_path %a; if [ $status -ne 0 ];then echo; echo '[ Finished ] Press Enter to close'; else echo; echo 'Press Enter to close (closing this window may unmount sshfs)'; fi; read\" & sleep 1\n",
        // clang-format on
        true,
        "fusermount -u \"%a\"",
        false,
        INFO_EXAMPLE,
        false,
    },
    Handler{
        "handler_network_mtp",
        xset::name::handler_network_mtp,
        "mtp",
        "mtp mtab_fs=fuse.jmtpfs mtab_fs=fuse.simple-mtpfs mtab_fs=fuse.mtpfs "
        "mtab_fs=fuse.DeviceFs(*",
        "",
        // clang-format off
        "mtpmount=\"$(which jmtpfs || which simple-mtpfs || which mtpfs || which go-mtpfs)\"\n"
        "if [ -z \"$mtpmount\" ];then\n"
        "    echo \"To mount mtp:// you must install jmtpfs, simple-mtpfs, mtpfs, or go-mtpfs,\"\n"
        "    echo \"or add a custom protocol handler.\"\n"
        "    exit 1\n"
        "elif [ \"${mtpmount##*/}\" = \"go-mtpfs\" ];then\n"
        "    # Run go-mtpfs in background, as it does not exit after mount\n"
        "    outputtmp=\"$(mktemp --tmpdir spacefm-go-mtpfs-output-XXXXXXXX)\" || exit 1\n"
        "    go-mtpfs \"%a\" &> \"$outputtmp\" &\n"
        "    sleep 2s\n"
        "    [[ -e \"$outputtmp\" ]] && cat \"$outputtmp\" ; rm -f \"$outputtmp\"\n"
        "    # set success status only if positive that mountpoint is mountpoint\n"
        "    mountpoint \"%a\"\n"
        "else\n"
        "    $mtpmount \"%a\"\n"
        "fi\n",
        // clang-format on
        false,
        "fusermount -u \"%a\"",
        false,
        INFO_EXAMPLE,
        false,
    },
    Handler{
        "handler_network_gphoto",
        xset::name::handler_network_gphoto,
        "ptp",
        "ptp gphoto mtab_fs=fuse.gphotofs",
        "",
        "gphotofs \"%a\"",
        false,
        "fusermount -u \"%a\"",
        false,
        INFO_EXAMPLE,
        false,
    },
    Handler{
        "handler_network_ifuse",
        xset::name::handler_network_ifuse,
        "ifuse",
        "ifuse ios mtab_fs=fuse.ifuse",
        "",
        "ifuse \"%a\"",
        false,
        "fusermount -u \"%a\"",
        false,
        INFO_EXAMPLE,
        false,
    },
    Handler{
        "handler_network_udevil",
        xset::name::handler_network_udevil,
        "udevil",
        "ftp http https nfs ssh mtab_fs=fuse.sshfs mtab_fs=davfs*",
        "",
        "udevil mount \"$fm_url\"",
        true,
        "udevil umount \"%a\"",
        false,
        INFO_EXAMPLE,
        false,
    },
    Handler{
        "handler_network_udevilsmb",
        xset::name::handler_network_udevilsmb,
        "udevil-smb",
        "smb mtab_fs=cifs",
        "",
        "UDEVIL_RESULT=\"$(udevil mount \"$fm_url\" | grep Mounted)\"\n"
        "[ -n \"$UDEVIL_RESULT\" ] && spacefm socket set new_tab \"${UDEVIL_RESULT#* at }\"",
        true,
        "udevil umount \"%a\"",
        false,
        INFO_EXAMPLE,
        false,
    },
    Handler{
        "handler_network_fusesmb",
        xset::name::handler_network_fusesmb,
        "fusesmb",
        "smb mtab_fs=fuse.fusesmb",
        "",
        "fusesmb \"%a\"",
        true,
        "fusermount -u \"%a\"",
        false,
        INFO_EXAMPLE,
        false,
    },
    Handler{
        "handler_network_fuse",
        xset::name::handler_network_fuse,
        "fuse unmount",
        "mtab_fs=fuse.* mtab_fs=fuse",
        "",
        "",
        false,
        "fusermount -u \"%a\"",
        false,
        INFO_EXAMPLE,
        false,
    },
};

inline constexpr std::array<Handler, 1> handlers_file{
    /* %a custom mount point
     * Plus standard fish variables are accepted.
     * For file handlers, extract_term is used for Run As Task. */
    Handler{
        "handler_file_iso",
        xset::name::handler_file_iso,
        "Mount ISO",
        "application/x-iso9660-image application/x-iso-image application/x-cd-image",
        "*.img *.iso *.mdf *.nrg",
        // clang-format off
        "# Note: Unmounting of iso files is performed by the fuseiso or udevil device\n"
        "# handler, not this file handler.\n\n"
        "# Use fuseiso or udevil ?\n"
        "fuse=\"$(which fuseiso)\"  # remove this line to use udevil only\n"
        "if [[ -z \"$fuse\" ]];then\n"
        "    udevil=\"$(which udevil)\"\n"
        "    if [[ -z \"$udevil\" ]];then\n"
        "         echo \"You must install fuseiso or udevil to mount ISOs with this handler.\"\n"
        "        exit 1\n"
        "    fi\n"
        "    # use udevil - attempt mount\n"
        "    uout=\"$($udevil mount \"$fm_file\" 2>&1)\"\n"
        "    err=$status; echo \"$uout\"\n"
        "    if [ $err -eq 2 ];then\n"
        "        # is file already mounted? (english only)\n"
        "        point=\"${uout#* is already mounted at }\"\n"
        "        if [ \"$point\" != \"$uout\" ];then\n"
        "            point=\"${point% (*}\"\n"
        "            if [ -x \"$point\" ];then\n"
        "                spacefm -t \"$point\"\n"
        "                exit 0\n"
        "            fi\n"
        "        fi\n"
        "    fi\n"
        "    [[ $err -ne 0 ]] && exit 1\n"
        "    point=\"${uout#Mounted }\"\n"
        "    [[ \"$point\" = \"$uout\" ]] && exit 0\n"
        "    point=\"${point##* at }\"\n"
        "    [[ -d \"$point\" ]] && spacefm \"$point\" &\n"
        "    exit 0\n"
        "fi\n"
        "# use fuseiso - is file already mounted?\n"
        "canon=\"$(readlink -f \"$fm_file\" 2>/dev/null)\"\n"
        "if [ -n \"$canon\" ];then\n"
        "    canon_enc=\"${canon// /\\\\040}\" # encode spaces for mtab+grep\n"
        "    if grep -q \"^$canon_enc \" ~/.mtab.fuseiso 2>/dev/null;then\n"
        "        # file is mounted - get mount point\n"
        "        point=\"$(grep -m 1 \"^$canon_enc \" ~/.mtab.fuseiso \\\n"
        "                 | sed 's/.* \\(.*\\) fuseiso .*/\\1/' )\"\n"
        "    if [ -x \"$point\" ];then\n"
        "            spacefm \"$point\" &\n"
        "            exit\n"
        "        fi\n"
        "    fi\n"
        "fi\n"
        "# mount & open\n"
        "fuseiso %f %a && spacefm %a &\n",
        // clang-format on
        false,
        "",
        true, // Run As Task
        "",
        false,
    },
};

// Function prototypes
static void on_configure_handler_enabled_check(GtkToggleButton* togglebutton, HandlerData* hnd);
static void restore_defaults(HandlerData* hnd, bool all);
static bool validate_archive_handler(HandlerData* hnd);
static void on_options_button_clicked(GtkWidget* btn, HandlerData* hnd);

bool
ptk_handler_command_is_empty(const std::string_view command)
{
    if (command.empty())
    {
        return true;
    }

    const auto cmd_parts = ztd::split(command, "\n");
    for (const std::string_view cmd_line : cmd_parts)
    {
        if (ztd::strip(cmd_line).empty())
        {
            continue;
        }

        if (!ztd::startswith(ztd::strip(cmd_line), "#"))
        {
            return false;
        }
    }
    return true;
}

static void
ptk_handler_load_text_view(GtkTextView* view, const std::string_view text = "")
{
    if (!view)
    {
        return;
    }

    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
    gtk_text_buffer_set_text(buf, text.data(), -1);
}

static char*
ptk_handler_get_text_view(GtkTextView* view)
{
    if (!view)
    {
        return ztd::strdup("");
    }
    GtkTextBuffer* buf = gtk_text_view_get_buffer(view);
    GtkTextIter iter, siter;
    gtk_text_buffer_get_start_iter(buf, &siter);
    gtk_text_buffer_get_end_iter(buf, &iter);
    char* text = gtk_text_buffer_get_text(buf, &siter, &iter, false);
    if (!text)
    {
        return ztd::strdup("");
    }
    return text;
}

char*
ptk_handler_get_command(i32 mode, i32 cmd, xset_t handler_set)
{ /* if handler->disable, get const command, else get script path if exists */
    if (!handler_set)
    {
        return nullptr;
    }
    if (handler_set->disable)
    {
        // is default handler - get command from const char
        const Handler* handler;
        usize nelements;
        const char* command;

        switch (mode)
        {
            case ptk::handler::mode::arc:
                nelements = handlers_arc.size();
                break;
            case ptk::handler::mode::fs:
                nelements = handlers_fs.size();
                break;
            case ptk::handler::mode::net:
                nelements = handlers_net.size();
                break;
            case ptk::handler::mode::file:
                nelements = handlers_file.size();
                break;
        }

        for (usize i = 0; i < nelements; ++i)
        {
            switch (mode)
            {
                case ptk::handler::mode::arc:
                    handler = &handlers_arc[i];
                    break;
                case ptk::handler::mode::fs:
                    handler = &handlers_fs[i];
                    break;
                case ptk::handler::mode::net:
                    handler = &handlers_net[i];
                    break;
                case ptk::handler::mode::file:
                    handler = &handlers_file[i];
                    break;
            }

            if (handler->xset_name == handler_set->xset_name)
            {
                // found default handler
                switch (cmd)
                {
                    case ptk::handler::archive::compress:
                        command = handler->compress_cmd;
                        break;
                    case ptk::handler::archive::extract:
                        command = handler->extract_cmd;
                        break;
                    case ptk::handler::archive::list:
                        command = handler->list_cmd;
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
        ztd::logger::warn("ptk_handler_get_command unable to get script for custom {}",
                          handler_set->name);
        return nullptr;
    }
    // name script
    const std::string str =
        std::format("/hand-{}-{}.fish",
                    modes.at(mode),
                    mode == ptk::handler::mode::arc ? cmds_arc.at(cmd) : cmds_mnt.at(cmd));
    const std::string script = ztd::replace(def_script, "/exec.fish", str);
    std::free(def_script);
    if (std::filesystem::exists(script))
    {
        return ztd::strdup(script);
    }

    ztd::logger::warn("ptk_handler_get_command missing script for custom {}", handler_set->name);
    return nullptr;
}

bool
ptk_handler_load_script(i32 mode, i32 cmd, xset_t handler_set, GtkTextView* view,
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
        {
            gtk_text_buffer_insert_at_cursor(buf, command, -1);
        }
        else
        {
            script = command;
        }

        // success
        return false;
    }

    // get default script path
    char* def_script = xset_custom_get_script(handler_set, false);
    if (!def_script)
    {
        error_message =
            std::format("get_handler_script unable to get script for custom {}", handler_set->name);
        return true;
    }
    // name script
    const std::string script_name =
        std::format("/hand-{}-{}.fish",
                    modes.at(mode),
                    mode == ptk::handler::mode::arc ? cmds_arc.at(cmd) : cmds_mnt.at(cmd));
    script = ztd::replace(def_script, "/exec.fish", script_name);
    std::free(def_script);
    // load script

    if (!std::filesystem::exists(script))
    {
        return true;
    }

    std::ifstream file(script);
    if (!file)
    {
        ztd::logger::error("Failed to open the file: {}", script);
        return true;
    }

    bool start = true;
    std::string line;
    while (std::getline(file, line))
    {
        if (start)
        {
            // skip script header
            if (ztd::same(line, std::format("#!{}\n", FISH_PATH)))
            {
                continue;
            }

            start = false;
        }
        // add line to buffer
        if (view)
        {
            gtk_text_buffer_insert_at_cursor(buf, line.data(), -1);
        }
        else
        {
            script.append(line);
        }
    }
    file.close();

    // success
    return false;
}

bool
ptk_handler_save_script(i32 mode, i32 cmd, xset_t handler_set, GtkTextView* view,
                        const std::string& command, std::string& error_message)
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
        ztd::logger::warn("save_handler_script unable to get script for custom {}",
                          handler_set->name);
        error_message = "Error: unable to save command (cannot get script path?)";
        return true;
    }
    // create parent dir
    const auto parent_dir = std::filesystem::path(def_script).parent_path();
    if (!std::filesystem::is_directory(parent_dir))
    {
        std::filesystem::create_directories(parent_dir);
        std::filesystem::permissions(parent_dir, std::filesystem::perms::owner_all);
    }
    // name script
    const std::string str =
        std::format("/hand-{}-{}.fish",
                    modes.at(mode),
                    mode == ptk::handler::mode::arc ? cmds_arc.at(cmd) : cmds_mnt.at(cmd));
    const std::string script = ztd::replace(def_script, "/exec.fish", str);
    std::free(def_script);
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
    {
        text = command;
    }

    // ztd::logger::info("WRITE {}", script);
    const std::string data = std::format("#!{}\nsource {}\n\n{}\n", FISH_PATH, FISH_FMLIB, text);
    const bool result = write_file(script, data);
    if (!result)
    {
        const std::string errno_msg = std::strerror(errno);
        error_message = std::format("Error writing to file: {}\n\nerrno: {}", script, errno_msg);
        return true;
    }

    // success
    return false;
}

bool
ptk_handler_values_in_list(const std::string_view list, const std::span<const std::string> values,
                           std::string& msg)
{ /* test for the presence of values in list, using wildcards.
   *  list is space-separated, plus sign (+) indicates required. */
    if (values.empty())
    {
        return false;
    }

    // get elements of list
    const std::vector<std::string> elements = ztd::split(list, " ");
    if (elements.empty())
    {
        return false;
    }

    // test each element for match
    bool required, match;
    bool ret = false;
    for (const std::string_view element : elements)
    {
        if (element.empty())
        {
            continue;
        }
        std::string_view match_element = element;
        if (ztd::startswith(element, "+"))
        {                                      // plus prefix indicates this element is required
            match_element = element.substr(1); // shift right of '+'
            required = true;
        }
        else
        {
            required = false;
        }
        match = false;
        for (const std::string_view handler : values)
        {
            if (ztd::fnmatch(match_element, handler))
            {
                // match
                ret = match = true;
                break;
            }
        }
        if (required && !match)
        {
            // no match of required
            return false;
        }

        msg = std::format("{}{}{}", match ? "[" : "", element, match ? "]" : "");
    }

    return ret;
}

static bool
value_in_list(const std::string_view list, const std::string_view value)
{ // this function must be FAST - is run multiple times on menu popup
    // value in space-separated list with wildcards
    for (const std::string_view key : ztd::split(list, " "))
    {
        if (ztd::fnmatch(key, value))
        {
            return true;
        }
    }
    return false;
}

const std::vector<xset_t>
ptk_handler_file_has_handlers(i32 mode, i32 cmd, const std::filesystem::path& path,
                              const vfs::mime_type& mime_type, bool test_cmd, bool multiple,
                              bool enabled_only)
{ /* this function must be FAST - is run multiple times on menu popup
   * command must be non-empty if test_cmd */

    std::vector<xset_t> xset_handlers;

    if (path.empty() && !mime_type)
    {
        return xset_handlers;
    }

    // Fetching and validating MIME type if provided
    std::string type;
    if (mime_type)
    {
        type = mime_type->type();
    }

    // replace spaces in path with underscores for matching
    std::string under_path;
    if (ztd::contains(path.string(), " "))
    {
        under_path = ztd::replace(path.string(), " ", "_");
    }
    else
    {
        under_path = path;
    }

    // parsing handlers space-separated list
    const auto archive_handlers_s = xset_get_s(handler_conf_xsets.at(mode));
    if (!archive_handlers_s)
    {
        ztd::logger::warn("File handlers are empty for {}",
                          magic_enum::enum_name(handler_conf_xsets.at(mode)));
        return xset_handlers;
    }
    const auto handlers = ztd::split(archive_handlers_s.value(), " ");
    if (handlers.empty())
    {
        return xset_handlers;
    }

    for (const std::string_view handler : handlers)
    {
        xset_t handler_set = xset_is(handler);
        if (!handler_set)
        {
            continue;
        }

        if (!(!enabled_only || handler_set->b))
        {
            continue;
        }

        // handler supports type or path ?
        if (value_in_list(handler_set->s.value(), type) ||
            value_in_list(handler_set->x.value(), under_path))
        {
            // test command
            if (test_cmd)
            {
                std::string command;
                std::string error_message;
                const bool error = ptk_handler_load_script(mode,
                                                           cmd,
                                                           handler_set,
                                                           nullptr,
                                                           command,
                                                           error_message);
                if (error)
                {
                    ztd::logger::error(error_message);
                }
                else if (!ptk_handler_command_is_empty(command))
                {
                    xset_handlers.emplace_back(handler_set);
                    if (!multiple)
                    {
                        break;
                    }
                }
            }
            else
            {
                xset_handlers.emplace_back(handler_set);
                if (!multiple)
                {
                    break;
                }
            }
        }
    }

    std::ranges::reverse(xset_handlers);

    return xset_handlers;
}

void
ptk_handler_add_defaults(i32 mode, bool overwrite, bool add_missing)
{
    usize nelements;
    char* list;
    char* str;
    xset_t set;
    xset_t set_conf;
    const Handler* handler;

    switch (mode)
    {
        case ptk::handler::mode::arc:
            nelements = handlers_arc.size();
            break;
        case ptk::handler::mode::fs:
            nelements = handlers_fs.size();
            break;
        case ptk::handler::mode::net:
            nelements = handlers_net.size();
            break;
        case ptk::handler::mode::file:
            nelements = handlers_file.size();
            break;
    }

    set_conf = xset_get(handler_conf_xsets.at(mode));
    if (!set_conf->s)
    {
        return;
    }
    list = ztd::strdup(set_conf->s.value());

    if (!list)
    {
        // create default list - eg sets arc_conf2 ->s
        list = ztd::strdup("");
        overwrite = add_missing = true;
    }

    for (const auto i : ztd::range(nelements))
    {
        switch (mode)
        {
            case ptk::handler::mode::arc:
                handler = &handlers_arc[i];
                break;
            case ptk::handler::mode::fs:
                handler = &handlers_fs[i];
                break;
            case ptk::handler::mode::net:
                handler = &handlers_net[i];
                break;
            case ptk::handler::mode::file:
                handler = &handlers_file[i];
                break;
        }

        // add a space to end of list and end of name before testing to avoid
        // substring false positive
        std::string testlist = std::format("{} ", list);
        std::string const testname = std::format("{} ", handler->setname);
        if (add_missing && !ztd::contains(testlist, testname))
        {
            // add a missing default handler to the list
            str = list;
            list = g_strconcat(list, list[0] ? " " : "", handler->setname, nullptr);
            std::free(str);
        }
        testlist = std::format("{} ", list);
        if (add_missing || ztd::contains(testlist, testname))
        {
            set = xset_is(handler->xset_name);
            if (!set || overwrite)
            {
                // create xset if missing
                if (!set)
                {
                    set = xset_get(handler->xset_name);
                }
                // set handler values to defaults
                set->menu_label = handler->handler_name;
                set->s = handler->type;
                set->x = handler->ext;
                set->in_terminal = handler->compress_term;
                // extract in terminal or (file handler) run as task
                set->keep_terminal = handler->extract_term;
                if (mode != ptk::handler::mode::file)
                {
                    set->scroll_lock = handler->list_term;
                }
                set->b = xset::b::xtrue;
                set->lock = false;
                // handler equals default, so do not save in session
                set->disable = true;
            }
        }
    }
    // update handler list
    set_conf->s = list;
}

static xset_t
add_new_handler(i32 mode)
{ // creates a new xset for a custom handler type
    // get a unique new xset name
    while (true)
    {
        const std::string setname =
            std::format("{}{}", handler_cust_prefixs.at(mode), ztd::randhex());
        if (!xset_is(setname))
        {
            // create and return the xset
            xset_t set = xset_get(setname);
            set->lock = false;
            return set;
        }
    };
}

static void
config_load_handler_settings(xset_t handler_xset, char* handler_xset_name, const Handler* handler,
                             HandlerData* hnd)
{
    // handler_xset_name optional if handler_xset passed
    // Fetching actual xset if only the name has been passed
    if (!handler_xset)
    {
        handler_xset = xset_is(handler_xset_name);
        if (!handler_xset)
        {
            return;
        }
    }

    /* At this point a handler exists, so making remove and apply buttons
     * sensitive as well as the enabled checkbutton */
    gtk_widget_set_sensitive(GTK_WIDGET(hnd->btn_remove), true);
    gtk_widget_set_sensitive(GTK_WIDGET(hnd->btn_apply), false);
    gtk_widget_set_sensitive(GTK_WIDGET(hnd->btn_up), true);
    gtk_widget_set_sensitive(GTK_WIDGET(hnd->btn_down), true);
    gtk_widget_set_sensitive(GTK_WIDGET(hnd->chkbtn_handler_enabled), true);
    gtk_widget_set_sensitive(
        GTK_WIDGET(hnd->btn_defaults0),
        ztd::startswith(handler_xset->name, handler_def_prefixs.at(hnd->mode)));

    /* Configuring widgets with handler settings. Only name, MIME and
     * extension warrant a warning
     * Commands are prefixed with '+' when they are ran in a terminal */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hnd->chkbtn_handler_enabled),
                                 handler_xset->b == xset::b::xtrue);

    gtk_entry_set_text(GTK_ENTRY(hnd->entry_handler_name),
                       handler_xset->menu_label ? handler_xset->menu_label.value().data() : "");
    gtk_entry_set_text(GTK_ENTRY(hnd->entry_handler_mime),
                       handler_xset->s ? handler_xset->s.value().data() : "");
    gtk_entry_set_text(GTK_ENTRY(hnd->entry_handler_extension),
                       handler_xset->x ? handler_xset->x.value().data() : "");

    if (handler)
    {
        // load commands from const handler
        ptk_handler_load_text_view(GTK_TEXT_VIEW(hnd->view_handler_compress),
                                   handler->compress_cmd);
        if (hnd->mode != ptk::handler::mode::file)
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
                                        ptk::handler::archive::compress,
                                        handler_xset,
                                        GTK_TEXT_VIEW(hnd->view_handler_compress),
                                        command,
                                        error_message);
        if (hnd->mode != ptk::handler::mode::file)
        {
            if (!error)
            {
                error = ptk_handler_load_script(hnd->mode,
                                                ptk::handler::archive::extract,
                                                handler_xset,
                                                GTK_TEXT_VIEW(hnd->view_handler_extract),
                                                command,
                                                error_message);
            }
            if (!error)
            {
                error = ptk_handler_load_script(hnd->mode,
                                                ptk::handler::archive::list,
                                                handler_xset,
                                                GTK_TEXT_VIEW(hnd->view_handler_list),
                                                command,
                                                error_message);
            }
        }
        if (error)
        {
            ptk_show_message(GTK_WINDOW(hnd->dlg),
                             GtkMessageType::GTK_MESSAGE_ERROR,
                             "Error Loading Handler",
                             GtkButtonsType::GTK_BUTTONS_OK,
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
    if (hnd->mode != ptk::handler::mode::file)
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hnd->chkbtn_handler_enabled), false);
    }

    // Resetting all widgets
    gtk_entry_set_text(GTK_ENTRY(hnd->entry_handler_name), "");
    gtk_entry_set_text(GTK_ENTRY(hnd->entry_handler_mime), "");
    gtk_entry_set_text(GTK_ENTRY(hnd->entry_handler_extension), "");
    ptk_handler_load_text_view(GTK_TEXT_VIEW(hnd->view_handler_compress));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hnd->chkbtn_handler_compress_term), false);
    ptk_handler_load_text_view(GTK_TEXT_VIEW(hnd->view_handler_extract));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hnd->chkbtn_handler_extract_term), false);
    ptk_handler_load_text_view(GTK_TEXT_VIEW(hnd->view_handler_list));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hnd->chkbtn_handler_list_term), false);

    gtk_widget_set_sensitive(GTK_WIDGET(hnd->btn_apply), false);
    hnd->changed = hnd->compress_changed = hnd->extract_changed = hnd->list_changed = false;
}

static void
populate_archive_handlers(HandlerData* hnd, xset_t def_handler_set)
{
    /* Fetching available archive handlers (literally gets member s from
     * the xset) - user-defined order has already been set */
    const auto archive_handlers_s = xset_get_s(handler_conf_xsets.at(hnd->mode));

    // Making sure archive handlers are available
    if (!archive_handlers_s)
    {
        return;
    }

    const std::vector<std::string> archive_handlers = ztd::split(archive_handlers_s.value(), " ");

    // Debug code
    // ztd::logger::info("archive_handlers_s: {}", archive_handlers_s);

    // Looping for handlers (nullptr-terminated list)
    GtkTreeIter iter;
    GtkTreeIter def_handler_iter;
    def_handler_iter.stamp = 0;
    for (const std::string_view archive_handler : archive_handlers)
    {
        if (ztd::startswith(archive_handler, handler_cust_prefixs.at(hnd->mode)))
        {
            // Fetching handler  - ignoring invalid handler xset names
            xset_t handler_xset = xset_is(archive_handler);
            if (handler_xset)
            {
                // Obtaining appending iterator for treeview model
                gtk_list_store_append(GTK_LIST_STORE(hnd->list), &iter);
                // Adding handler to model
                const std::string disabled =
                    hnd->mode == ptk::handler::mode::file ? "(optional)" : "(disabled)";
                const std::string dis_name =
                    std::format("{} {}",
                                handler_xset->menu_label.value(),
                                handler_xset->b == xset::b::xtrue ? "" : disabled);
                gtk_list_store_set(GTK_LIST_STORE(hnd->list),
                                   &iter,
                                   ptk::handler::column::xset_name,
                                   archive_handler.data(),
                                   ptk::handler::column::handler_name,
                                   dis_name.data(),
                                   -1);
                if (def_handler_set == handler_xset)
                {
                    def_handler_iter = iter;
                }
            }
        }
    }

    // Fetching selection from treeview
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(hnd->view_handlers));

    // Loading first or default archive handler if there is one and no selection is present
    if (!gtk_tree_selection_get_selected(GTK_TREE_SELECTION(selection), nullptr, nullptr))
    {
        GtkTreePath* tree_path;
        if (def_handler_set && def_handler_iter.stamp)
        {
            tree_path = gtk_tree_model_get_path(GTK_TREE_MODEL(hnd->list), &def_handler_iter);
        }
        else
        {
            tree_path = gtk_tree_path_new_first();
        }
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
        ztd::logger::warn("Drag'n'drop end event detected, but unable to get an"
                          " iterator to the start of the model!");
        return;
    }

    // Looping for all handlers
    char* xset_name;
    std::string archive_handlers;
    do
    {
        gtk_tree_model_get(GTK_TREE_MODEL(hnd->list),
                           &iter,
                           ptk::handler::column::xset_name,
                           &xset_name,
                           -1);

        if (ztd::same(archive_handlers, ""))
        {
            archive_handlers = xset_name;
        }
        else
        {
            archive_handlers = std::format("{} {}", archive_handlers, xset_name);
        }
        std::free(xset_name);
    } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(hnd->list), &iter));

    // Saving the new archive handlers list
    xset_set(handler_conf_xsets.at(hnd->mode), xset::var::s, archive_handlers);

    // Saving settings
    autosave_request_add();
}

static void
on_configure_button_press(GtkButton* widget, HandlerData* hnd)
{
    std::string const command;
    std::string error_message;
    bool error = false;

    const char* handler_name = gtk_entry_get_text(GTK_ENTRY(hnd->entry_handler_name));
    const char* handler_mime = gtk_entry_get_text(GTK_ENTRY(hnd->entry_handler_mime));
    const char* handler_extension = gtk_entry_get_text(GTK_ENTRY(hnd->entry_handler_extension));

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
    xset_t handler_xset = nullptr;

    // Fetching selection from treeview
    GtkTreeSelection* selection = nullptr;
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(hnd->view_handlers));

    // Getting selection fails if there are no handlers
    if (gtk_tree_selection_get_selected(GTK_TREE_SELECTION(selection), &model, &it))
    {
        // Succeeded - handlers present
        // Fetching data from the model based on the iterator.
        gtk_tree_model_get(model,
                           &it,
                           ptk::handler::column::xset_name,
                           &xset_name,
                           ptk::handler::column::handler_name,
                           &handler_name_from_model,
                           -1);

        // Fetching the xset now I have the xset name
        handler_xset = xset_is(xset_name);

        // Making sure it has been fetched
        if (!handler_xset)
        {
            ztd::logger::warn("Unable to fetch the xset for the archive handler '%s'",
                              handler_name);
            std::free(xset_name);
            std::free(handler_name_from_model);
            return;
        }
    }

    if (GTK_WIDGET(widget) == GTK_WIDGET(hnd->btn_add))
    {
        // Exiting if there is no handler to add
        if (!(handler_name && handler_name[0]))
        {
            std::free(xset_name);
            std::free(handler_name_from_model);
            return;
        }

        // Adding new handler as a copy of the current active handler
        xset_t new_handler_xset = add_new_handler(hnd->mode);
        new_handler_xset->b =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(hnd->chkbtn_handler_enabled))
                ? xset::b::xtrue
                : xset::b::xfalse;
        new_handler_xset->disable = false; // not default - save in session
        xset_set_var(new_handler_xset, xset::var::menu_label, handler_name);
        xset_set_var(new_handler_xset, xset::var::s,
                     handler_mime); // Mime Type(s) or whitelist
        xset_set_var(new_handler_xset,
                     xset::var::x,
                     handler_extension); // Extension(s) or blacklist
        new_handler_xset->in_terminal = handler_compress_term;
        new_handler_xset->keep_terminal = handler_extract_term;

        error = ptk_handler_save_script(hnd->mode,
                                        ptk::handler::archive::compress,
                                        new_handler_xset,
                                        GTK_TEXT_VIEW(hnd->view_handler_compress),
                                        command,
                                        error_message);
        if (hnd->mode != ptk::handler::mode::file)
        {
            new_handler_xset->scroll_lock = handler_list_term;
            if (!error)
            {
                error = ptk_handler_save_script(hnd->mode,
                                                ptk::handler::archive::extract,
                                                new_handler_xset,
                                                GTK_TEXT_VIEW(hnd->view_handler_extract),
                                                command,
                                                error_message);
            }
            if (!error)
            {
                error = ptk_handler_save_script(hnd->mode,
                                                ptk::handler::archive::list,
                                                new_handler_xset,
                                                GTK_TEXT_VIEW(hnd->view_handler_list),
                                                command,
                                                error_message);
            }
        }

        // Obtaining appending iterator for treeview model
        gtk_list_store_prepend(GTK_LIST_STORE(hnd->list), &iter);

        // Adding handler to model
        const char* disabled = hnd->mode == ptk::handler::mode::file ? "(optional)" : "(disabled)";
        const std::string dis_name =
            std::format("{} {}",
                        handler_name,
                        new_handler_xset->b == xset::b::xtrue ? "" : disabled);
        gtk_list_store_set(GTK_LIST_STORE(hnd->list),
                           &iter,
                           ptk::handler::column::xset_name,
                           new_handler_xset->name.c_str(),
                           ptk::handler::column::handler_name,
                           dis_name.data(),
                           -1);

        // Updating available archive handlers list
        const auto archive_handlers_s = xset_get_s(handler_conf_xsets.at(hnd->mode));
        if (ztd::compare(archive_handlers_s.value(), "") <= 0)
        {
            // No handlers present - adding new handler
            xset_set(handler_conf_xsets.at(hnd->mode), xset::var::s, new_handler_xset->name);
        }
        else
        {
            // Adding new handler to handlers
            const std::string new_handlers_list =
                std::format("{} {}", new_handler_xset->name, archive_handlers_s.value());
            xset_set(handler_conf_xsets.at(hnd->mode), xset::var::s, new_handlers_list);
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
        {
            std::free(xset_name);
            std::free(handler_name_from_model);
            return;
        }

        // Validating - to be saved even with invalid commands
        validate_archive_handler(hnd);

        // Determining current handler enabled state
        const bool handler_enabled =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(hnd->chkbtn_handler_enabled));

        // Checking if the handler has been renamed
        if (ztd::compare(handler_name_from_model, handler_name) != 0)
        {
            // It has - updating model
            const char* disabled =
                hnd->mode == ptk::handler::mode::file ? "(optional)" : "(disabled)";
            const std::string dis_name =
                std::format("{} {}", handler_name, handler_enabled ? "" : disabled);
            gtk_list_store_set(GTK_LIST_STORE(model),
                               &it,
                               ptk::handler::column::xset_name,
                               xset_name,
                               ptk::handler::column::handler_name,
                               dis_name.data(),
                               -1);
        }

        // Saving archive handler
        handler_xset->b = handler_enabled ? xset::b::xtrue : xset::b::unset;
        const bool was_default = handler_xset->disable;
        handler_xset->disable = false; // not default - save in session
        xset_set_var(handler_xset, xset::var::menu_label, handler_name);
        xset_set_var(handler_xset, xset::var::s, handler_mime);
        xset_set_var(handler_xset, xset::var::x, handler_extension);
        handler_xset->in_terminal = handler_compress_term;
        handler_xset->keep_terminal = handler_extract_term;
        if (hnd->compress_changed || was_default)
        {
            error = ptk_handler_save_script(hnd->mode,
                                            ptk::handler::archive::compress,
                                            handler_xset,
                                            GTK_TEXT_VIEW(hnd->view_handler_compress),
                                            command,
                                            error_message);
        }
        if (hnd->mode != ptk::handler::mode::file)
        {
            handler_xset->scroll_lock = handler_list_term;
            if (hnd->extract_changed || was_default)
            {
                error = ptk_handler_save_script(hnd->mode,
                                                ptk::handler::archive::extract,
                                                handler_xset,
                                                GTK_TEXT_VIEW(hnd->view_handler_extract),
                                                command,
                                                error_message);
            }
            if (hnd->list_changed || was_default)
            {
                error = ptk_handler_save_script(hnd->mode,
                                                ptk::handler::archive::list,
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
        {
            std::free(xset_name);
            std::free(handler_name_from_model);
            return;
        }

        const auto response = ptk_show_message(GTK_WINDOW(hnd->dlg),
                                               GtkMessageType::GTK_MESSAGE_WARNING,
                                               "Confirm Remove",
                                               GtkButtonsType::GTK_BUTTONS_YES_NO,
                                               "Permanently remove the selected handler?");

        if (response != GtkResponseType::GTK_RESPONSE_YES)
        {
            std::free(xset_name);
            std::free(handler_name_from_model);
            return;
        }

        // Updating available archive handlers list - fetching current handlers
        const auto archive_handlers_s = xset_get_s(handler_conf_xsets.at(hnd->mode));
        const std::vector<std::string> archive_handlers =
            ztd::split(archive_handlers_s.value(), " ");
        std::string new_archive_handlers_s;

        // Looping for handlers (nullptr-terminated list)
        for (const std::string_view archive_handler : archive_handlers)
        {
            // Appending to new archive handlers list when it isnt the
            // deleted handler - remember that archive handlers are
            // referred to by their xset names, not handler names!!
            if (ztd::compare(archive_handler, xset_name) != 0)
            {
                // Debug code
                // ztd::logger::info("archive_handler     : {}", archive_handler)
                // ztd::logger::info("xset_name           : {}", xset_name);

                if (new_archive_handlers_s.empty())
                {
                    new_archive_handlers_s = archive_handler.data();
                }
                else
                {
                    new_archive_handlers_s =
                        std::format("{} {}", new_archive_handlers_s, archive_handler);
                }
            }
        }

        // Finally updating handlers
        xset_set(handler_conf_xsets.at(hnd->mode), xset::var::s, new_archive_handlers_s);

        // Deleting xset
        xset_custom_delete(handler_xset, false);
        handler_xset = nullptr;

        // Removing handler from the list
        gtk_list_store_remove(GTK_LIST_STORE(model), &it);

        if (ztd::same(new_archive_handlers_s, ""))
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
    }
    else if (GTK_WIDGET(widget) == GTK_WIDGET(hnd->btn_up) ||
             GTK_WIDGET(widget) == GTK_WIDGET(hnd->btn_down))
    {
        if (!handler_xset)
        {
            // no row selected
            std::free(xset_name);
            std::free(handler_name_from_model);
            return;
        }

        // Note: gtk_tree_model_iter_previous requires GTK3, so not using
        GtkTreeIter iter_prev;
        if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
        {
            std::free(xset_name);
            std::free(handler_name_from_model);
            return;
        }
        iter_prev = iter;
        do
        {
            // find my it (stamp is NOT unique - compare whole struct)
            if (iter.stamp == it.stamp && iter.user_data == it.user_data &&
                iter.user_data2 == it.user_data2 && iter.user_data3 == it.user_data3)
            {
                if (GTK_WIDGET(widget) == GTK_WIDGET(hnd->btn_up))
                {
                    iter = iter_prev;
                }
                else if (!gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter))
                {
                    // was last row
                    std::free(xset_name);
                    std::free(handler_name_from_model);
                    return;
                }
                break;
            }
            iter_prev = iter;
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter));
        gtk_list_store_swap(GTK_LIST_STORE(model), &it, &iter);
        // save the new list
        on_configure_drag_end(nullptr, nullptr, hnd);
    }

    // Saving settings
    autosave_request_add();

    if (error)
    {
        ptk_show_message(GTK_WINDOW(hnd->dlg),
                         GtkMessageType::GTK_MESSAGE_ERROR,
                         "Error Saving Handler",
                         GtkButtonsType::GTK_BUTTONS_OK,
                         error_message);
    }

    std::free(xset_name);
    std::free(handler_name_from_model);
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
    gtk_tree_model_get(model, &it, ptk::handler::column::xset_name, &xset_name, -1);

    // Loading new archive handler values
    config_load_handler_settings(nullptr, xset_name, nullptr, hnd);
    std::free(xset_name);
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

    if (hnd->mode == ptk::handler::mode::file)
    {
        return;
    }

    // Fetching selection from treeview
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(hnd->view_handlers));

    // Exiting if no handler is selected
    GtkTreeIter it;
    GtkTreeModel* model;
    if (!gtk_tree_selection_get_selected(selection, &model, &it))
    {
        return;
    }

    // Fetching current status
    const bool enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(togglebutton));

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
    // Current handler has not been changed?
    if (!hnd->changed /* was !gtk_widget_get_sensitive( hnd->btn_apply )*/)
    {
        return false;
    }

    const auto response = ptk_show_message(GTK_WINDOW(hnd->dlg),
                                           GtkMessageType::GTK_MESSAGE_QUESTION,
                                           "Apply Changes ?",
                                           GtkButtonsType::GTK_BUTTONS_YES_NO,
                                           "Apply changes to the current handler?");

    if (response == GtkResponseType::GTK_RESPONSE_YES)
    {
        on_configure_button_press(GTK_BUTTON(hnd->btn_apply), hnd);
    }
    else
    {
        hnd->changed = false;
    }
    return true; // false does not retain key after dialog shown
}

static bool
on_handlers_button_press(GtkWidget* view, GdkEventButton* event, HandlerData* hnd)
{
    GtkTreeModel* model;
    GtkTreePath* tree_path = nullptr;
    GtkTreeIter it;
    bool item_clicked = false;
    bool ret = false;

    // get clicked item
    if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view),
                                      static_cast<i32>(event->x),
                                      static_cast<i32>(event->y),
                                      &tree_path,
                                      nullptr,
                                      nullptr,
                                      nullptr))
    {
        model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
        if (gtk_tree_model_get_iter(model, &it, tree_path))
        {
            item_clicked = true;
        }
    }

    // Current handler has been changed?
    if (gtk_widget_get_sensitive(hnd->btn_apply))
    {
        // Query apply changes
        const auto response = ptk_show_message(GTK_WINDOW(hnd->dlg),
                                               GtkMessageType::GTK_MESSAGE_QUESTION,
                                               "Apply Changes ?",
                                               GtkButtonsType::GTK_BUTTONS_YES_NO,
                                               "Apply changes to the current handler?");

        if (response == GtkResponseType::GTK_RESPONSE_YES)
        {
            on_configure_button_press(GTK_BUTTON(hnd->btn_apply), hnd);
        }

        // Move cursor or unselect
        if (item_clicked)
        {
            // select clicked item
            gtk_tree_view_set_cursor(GTK_TREE_VIEW(hnd->view_handlers), tree_path, nullptr, false);
        }
        else
        {
            GtkTreeSelection* selection =
                gtk_tree_view_get_selection(GTK_TREE_VIEW(hnd->view_handlers));
            if (selection)
            { // unselect all
                gtk_tree_selection_unselect_all(selection);
            }
        }
        ret = true;
    }
    else if (event->button == 3)
    {
        // right click - Move cursor or unselect
        if (item_clicked)
        {
            // select clicked item
            gtk_tree_view_set_cursor(GTK_TREE_VIEW(hnd->view_handlers), tree_path, nullptr, false);
        }
        else
        {
            GtkTreeSelection* selection =
                gtk_tree_view_get_selection(GTK_TREE_VIEW(hnd->view_handlers));
            if (selection)
            { // unselect all
                gtk_tree_selection_unselect_all(selection);
            }
        }

        // show menu
        on_options_button_clicked(nullptr, hnd);
        ret = true;
    }

    if (tree_path)
    {
        gtk_tree_path_free(tree_path);
    }
    return ret;
}

static void
restore_defaults(HandlerData* hnd, bool all)
{
    if (all)
    {
        const auto response = ptk_show_message(GTK_WINDOW(hnd->dlg),
                                               GtkMessageType::GTK_MESSAGE_WARNING,
                                               "Restore Default Handlers",
                                               GtkButtonsType::GTK_BUTTONS_YES_NO,
                                               "Missing default handlers will be restored.\n\nAlso "
                                               "OVERWRITE ALL EXISTING default handlers?");
        if (response != GtkResponseType::GTK_RESPONSE_YES &&
            response != GtkResponseType::GTK_RESPONSE_NO)
        {
            // dialog was closed with no button pressed - cancel
            return;
        }
        ptk_handler_add_defaults(hnd->mode, response == GtkResponseType::GTK_RESPONSE_YES, true);

        /* Reset archive handlers list (this also selects
         * the first handler and therefore populates the handler widgets) */
        gtk_list_store_clear(GTK_LIST_STORE(hnd->list));
        populate_archive_handlers(hnd, nullptr);
    }
    else
    {
        // Fetching selection from treeview
        GtkTreeSelection* selection = nullptr;
        selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(hnd->view_handlers));

        // Exiting if no handler is selected
        GtkTreeIter it;
        GtkTreeModel* model;
        if (!gtk_tree_selection_get_selected(selection, &model, &it))
        {
            return;
        }

        char* xset_name;
        gtk_tree_model_get(model, &it, ptk::handler::column::xset_name, &xset_name, -1);
        // a default handler is selected?
        if (!(xset_name && ztd::startswith(xset_name, handler_def_prefixs.at(hnd->mode))))
        {
            std::free(xset_name);
            return;
        }

        // get default handler
        usize nelements;
        const Handler* handler = nullptr;

        switch (hnd->mode)
        {
            case ptk::handler::mode::arc:
                nelements = handlers_arc.size();
                break;
            case ptk::handler::mode::fs:
                nelements = handlers_fs.size();
                break;
            case ptk::handler::mode::net:
                nelements = handlers_net.size();
                break;
            case ptk::handler::mode::file:
                nelements = handlers_file.size();
                break;
        }

        bool found_handler = false;
        for (const auto i : ztd::range(nelements))
        {
            switch (hnd->mode)
            {
                case ptk::handler::mode::arc:
                    handler = &handlers_arc[i];
                    break;
                case ptk::handler::mode::fs:
                    handler = &handlers_fs[i];
                    break;
                case ptk::handler::mode::net:
                    handler = &handlers_net[i];
                    break;
                case ptk::handler::mode::file:
                    handler = &handlers_file[i];
                    return;
            }

            if (ztd::same(handler->setname, xset_name))
            {
                found_handler = true;
                break;
            }
        }

        std::free(xset_name);
        if (!found_handler)
        {
            return;
        }

        // create fake xset
        const auto set = new xset::XSet(handler->setname, xset::name::custom);
        set->menu_label = handler->handler_name;
        set->s = handler->type;
        set->x = handler->ext;
        set->in_terminal = handler->compress_term;
        set->keep_terminal = handler->extract_term;
        if (hnd->mode != ptk::handler::mode::file)
        {
            set->scroll_lock = handler->list_term;
        }
        set->b = xset::b::xtrue;
        set->icon = std::nullopt;

        // show fake xset values
        config_load_handler_settings(set, nullptr, handler, hnd);

        delete set;
    }
}

static bool
validate_archive_handler(HandlerData* hnd)
{
    if (hnd->mode != ptk::handler::mode::arc)
    {
        // only archive handlers currently have validity checks
        return true;
    }

    const char* handler_name = gtk_entry_get_text(GTK_ENTRY(hnd->entry_handler_name));
    const char* handler_mime = gtk_entry_get_text(GTK_ENTRY(hnd->entry_handler_mime));
    const char* handler_extension = gtk_entry_get_text(GTK_ENTRY(hnd->entry_handler_extension));

    /* Validating data. Note that data straight from widgets shouldnt
     * be modified or stored
     * Note that archive creation also allows for a command to be
     * saved */
    if (ztd::compare(handler_name, "") <= 0)
    {
        /* Handler name not set - warning user and exiting. Note
         * that the created dialog does not have an icon set */
        ptk_show_message(GTK_WINDOW(hnd->dlg),
                         GtkMessageType::GTK_MESSAGE_WARNING,
                         dialog_titles.at(hnd->mode),
                         GtkButtonsType::GTK_BUTTONS_OK,
                         "Please enter a valid handler name.");
        gtk_widget_grab_focus(hnd->entry_handler_name);
        return false;
    }

    // MIME and Pathname cannot both be empty
    if (ztd::compare(handler_mime, "") <= 0 && ztd::compare(handler_extension, "") <= 0)
    {
        ptk_show_message(GTK_WINDOW(hnd->dlg),
                         GtkMessageType::GTK_MESSAGE_WARNING,
                         dialog_titles.at(hnd->mode),
                         GtkButtonsType::GTK_BUTTONS_OK,
                         "Please enter a valid MIME Type or Pathname pattern.");
        gtk_widget_grab_focus(hnd->entry_handler_mime);
        return false;
    }

    char* handler_compress = ptk_handler_get_text_view(GTK_TEXT_VIEW(hnd->view_handler_compress));
    char* handler_extract = ptk_handler_get_text_view(GTK_TEXT_VIEW(hnd->view_handler_extract));
    char* handler_list = ptk_handler_get_text_view(GTK_TEXT_VIEW(hnd->view_handler_list));
    const bool ret = true;

    /* Other settings are commands to run in different situations -
     * since different handlers may or may not need different
     * commands, empty commands are allowed but if something is given,
     * relevant substitution characters should be in place */

    /* Compression handler validation - remember to maintain this code
     * in ptk_file_archiver_create too
     * Checking if a compression command has been entered */
    if (ztd::compare(handler_compress, "") != 0)
    {
        /* It has - making sure all substitution characters are in
         * place - not mandatory to only have one of the particular
         * type */
        if ((!ztd::contains(handler_compress, "%o") && !ztd::contains(handler_compress, "%O")) ||
            (!ztd::contains(handler_compress, "%n") && !ztd::contains(handler_compress, "%N")))
        {
            ptk_show_message(GTK_WINDOW(hnd->dlg),
                             GtkMessageType::GTK_MESSAGE_WARNING,
                             dialog_titles.at(hnd->mode),
                             GtkButtonsType::GTK_BUTTONS_OK,
                             "The following substitution variables should probably be in the "
                             "compression command:\n\nOne of the following:\n\n%%n: First selected "
                             "file/directory to archive\n%%N: All selected files/directories to "
                             "archive\n\nand one of the following:\n\n%%o: Resulting single "
                             "archive\n%%O: Resulting archive per source file/directory");
            gtk_widget_grab_focus(hnd->view_handler_compress);

            std::free(handler_compress);
            std::free(handler_extract);
            std::free(handler_list);
            return false;
        }
    }

    if (ztd::compare(handler_extract, "") != 0 && (!g_strstr_len(handler_extract, -1, "%x")))
    {
        /* Not all substitution characters are in place - warning
         * user and exiting. Note that the created dialog does not
         * have an icon set */
        ptk_show_message(GTK_WINDOW(hnd->dlg),
                         GtkMessageType::GTK_MESSAGE_WARNING,
                         dialog_titles.at(hnd->mode),
                         GtkButtonsType::GTK_BUTTONS_OK,
                         "The following variables should probably be in the extraction "
                         "command:\n\n%%x: Archive to extract");
        gtk_widget_grab_focus(hnd->view_handler_extract);

        std::free(handler_compress);
        std::free(handler_extract);
        std::free(handler_list);
        return false;
    }

    if (ztd::compare(handler_list, "") != 0 && (!g_strstr_len(handler_list, -1, "%x")))
    {
        /* Not all substitution characters are in place  - warning
         * user and exiting. Note that the created dialog does not
         * have an icon set */
        ptk_show_message(GTK_WINDOW(hnd->dlg),
                         GtkMessageType::GTK_MESSAGE_WARNING,
                         dialog_titles.at(hnd->mode),
                         GtkButtonsType::GTK_BUTTONS_OK,
                         "The following variables should probably be in the list command:\n\n%%x: "
                         "Archive to list");
        gtk_widget_grab_focus(hnd->view_handler_list);

        std::free(handler_compress);
        std::free(handler_extract);
        std::free(handler_list);
        return false;
    }

    std::free(handler_compress);
    std::free(handler_extract);
    std::free(handler_list);

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
    xset_t set = xset_get(xset::name::separator);
    set->menu_style = xset::menu::sep;
    set->browser = nullptr;
    xset_add_menuitem(nullptr, GTK_WIDGET(menu), accel_group, set);

    gtk_widget_show_all(GTK_WIDGET(menu));
}

static bool
on_activate_link(GtkLabel* label, const char* uri, HandlerData* hnd)
{
    (void)label;
    // click apply to save handler
    on_configure_button_press(GTK_BUTTON(hnd->btn_apply), hnd);
    // open in editor
    const i32 action = std::stoi(uri);
    if (action > ptk::handler::archive::list || action < 0)
    {
        return true;
    }

    // Fetching selection from treeview
    GtkTreeSelection* selection = nullptr;
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(hnd->view_handlers));

    // Exiting if no handler is selected
    GtkTreeIter it;
    GtkTreeModel* model;
    if (!gtk_tree_selection_get_selected(selection, &model, &it))
    {
        return true;
    }

    char* xset_name = nullptr;
    gtk_tree_model_get(model, &it, ptk::handler::column::xset_name, &xset_name, -1);
    xset_t set = xset_is(xset_name);
    std::free(xset_name);
    if (!(set && !set->disable && set->b == xset::b::xtrue))
    {
        return true;
    }
    char* script = ptk_handler_get_command(hnd->mode, action, set);
    if (!script)
    {
        return true;
    }
    xset_edit(hnd->dlg, script, false, false);
    std::free(script);
    return true;
}

static bool
on_textview_keypress(GtkWidget* widget, GdkEventKey* event, HandlerData* hnd)
{ // also used on dlg keypress
    u32 keymod = ptk_get_keymod(event->state);
    switch (event->keyval)
    {
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
            if (keymod == GdkModifierType::GDK_MOD1_MASK)
            {
                // Alt+Enter == Open Handler Command In Editor
                if (widget == hnd->view_handler_compress)
                {
                    keymod = 0;
                }
                else if (widget == hnd->view_handler_extract)
                {
                    keymod = 1;
                }
                else if (widget == hnd->view_handler_list)
                {
                    keymod = 2;
                }
                else
                {
                    return false;
                }
                on_activate_link(nullptr, std::to_string(keymod).data(), hnd);
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
    {
        hnd->compress_changed = true;
    }
    else if (buf == hnd->buf_handler_extract && !hnd->extract_changed)
    {
        hnd->extract_changed = true;
    }
    if (buf == hnd->buf_handler_list && !hnd->list_changed)
    {
        hnd->list_changed = true;
    }
    if (!hnd->changed)
    {
        hnd->changed = true;
        gtk_widget_set_sensitive(hnd->btn_apply, gtk_widget_get_sensitive(hnd->btn_remove));
    }
}

static void
on_entry_text_insert(GtkEntryBuffer* buffer, u32 position, char* chars, u32 n_chars,
                     HandlerData* hnd)
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
on_entry_text_delete(GtkEntryBuffer* buffer, u32 position, u32 n_chars, HandlerData* hnd)
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
on_option_cb(GtkMenuItem* item, HandlerData* hnd)
{
    if (hnd->changed)
    {
        on_configure_button_press(GTK_BUTTON(hnd->btn_apply), hnd);
    }

    const ptk::handler::job job =
        ptk::handler::job(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "job")));

    // Determine handler selected
    char* xset_name;
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(hnd->view_handlers));
    GtkTreeIter it;
    GtkTreeModel* model;
    if (gtk_tree_selection_get_selected(selection, &model, &it))
    {
        gtk_tree_model_get(model, &it, ptk::handler::column::xset_name, &xset_name, -1);
        std::free(xset_name);
    }

    // determine job
    std::optional<std::filesystem::path> file;
    switch (job)
    {
        case ptk::handler::job::restore_all:
            restore_defaults(hnd, true);
            break;
        case ptk::handler::job::remove:
            on_configure_button_press(GTK_BUTTON(hnd->btn_remove), hnd);
            break;
    }
}

static void
on_archive_default(GtkMenuItem* menuitem, xset_t set)
{
    (void)menuitem;
    static constexpr std::array<xset::name, 4> arcnames{
        xset::name::arc_def_open,
        xset::name::arc_def_ex,
        xset::name::arc_def_exto,
        xset::name::arc_def_list,
    };

    for (const xset::name arcname : arcnames)
    {
        if (set->xset_name == arcname)
        {
            set->b = xset::b::xtrue;
        }
        else
        {
            xset_set_b(arcname, false);
        }
    }
}

static GtkWidget*
add_popup_menuitem(GtkWidget* popup, GtkAccelGroup* accel_group, const std::string_view label,
                   ptk::handler::job job, HandlerData* hnd)
{
    (void)accel_group;
    GtkWidget* item = gtk_menu_item_new_with_mnemonic(label.data());
    gtk_container_add(GTK_CONTAINER(popup), item);
    g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_option_cb), (void*)hnd);
    g_object_set_data(G_OBJECT(item), "job", GINT_TO_POINTER(magic_enum::enum_integer(job)));
    return item;
}

static void
on_options_button_clicked(GtkWidget* btn, HandlerData* hnd)
{
    GtkWidget* item;

    // Determine if a handler is selected
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(hnd->view_handlers));
    const bool handler_selected = gtk_tree_selection_get_selected(selection, nullptr, nullptr);

    // build menu
    GtkWidget* popup = gtk_menu_new();
    GtkAccelGroup* accel_group = gtk_accel_group_new();

    if (!btn)
    {
        // menu is shown from right-click on list
        item = add_popup_menuitem(popup, accel_group, "_Remove", ptk::handler::job::remove, hnd);
        gtk_widget_set_sensitive(item, handler_selected);
    }

    add_popup_menuitem(popup,
                       accel_group,
                       "Restore _Default Handlers",
                       ptk::handler::job::restore_all,
                       hnd);
    if (btn)
    {
        // menu is shown from Options button
        if (hnd->mode == ptk::handler::mode::arc)
        {
            xset_t set;

            // Archive options
            xset_context_new();
            gtk_container_add(GTK_CONTAINER(popup), gtk_separator_menu_item_new());
            set = xset_get(xset::name::arc_def_open);
            // do NOT use set = xset_set_cb here or wrong set is passed
            xset_set_cb(xset::name::arc_def_open, (GFunc)on_archive_default, set);
            xset_set_ob2(set, nullptr, nullptr);
            xset_t set_radio = set;

            set = xset_get(xset::name::arc_def_ex);
            xset_set_cb(xset::name::arc_def_ex, (GFunc)on_archive_default, set);
            xset_set_ob2(set, nullptr, set_radio->name.data());

            set = xset_get(xset::name::arc_def_exto);
            xset_set_cb(xset::name::arc_def_exto, (GFunc)on_archive_default, set);
            xset_set_ob2(set, nullptr, set_radio->name.data());

            set = xset_get(xset::name::arc_def_list);
            xset_set_cb(xset::name::arc_def_list, (GFunc)on_archive_default, set);
            xset_set_ob2(set, nullptr, set_radio->name.data());

            set = xset_get(xset::name::arc_def_write);
            set->disable = geteuid() == 0 || !xset_get_b(xset::name::arc_def_parent);

            // temp remove unwanted items from Archive Defaults submenu
            set = xset_get(xset::name::arc_default);
            const auto old_desc = set->desc;
            set->desc = "arc_def_open arc_def_ex arc_def_exto arc_def_list separator "
                        "arc_def_parent arc_def_write";
            xset_add_menuitem(hnd->browser, popup, accel_group, set);
            set->desc = old_desc;
        }
        else if (hnd->mode == ptk::handler::mode::fs)
        {
            // Device handler options
            xset_context_new();
            gtk_container_add(GTK_CONTAINER(popup), gtk_separator_menu_item_new());
            xset_add_menuitem(hnd->browser,
                              popup,
                              accel_group,
                              xset_get(xset::name::dev_mount_options));
        }
    }

    // show menu
    gtk_widget_show_all(GTK_WIDGET(popup));
    g_signal_connect(popup, "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);
    gtk_menu_popup_at_pointer(GTK_MENU(popup), nullptr);
}

void
ptk_handler_show_config(i32 mode, PtkFileBrowser* file_browser, xset_t def_handler_set)
{
    const auto hnd = new HandlerData;
    hnd->mode = mode;

    /* Create handlers dialog
     * Extra nullptr on the nullptr-terminated list to placate an irrelevant
     * compilation warning */
    if (file_browser)
    {
        hnd->parent = gtk_widget_get_toplevel(GTK_WIDGET(file_browser->main_window()));
    }

    hnd->browser = file_browser;
    hnd->dlg =
        gtk_dialog_new_with_buttons(dialog_titles.at(mode).data(),
                                    hnd->parent ? GTK_WINDOW(hnd->parent) : nullptr,
                                    GtkDialogFlags(GtkDialogFlags::GTK_DIALOG_MODAL |
                                                   GtkDialogFlags::GTK_DIALOG_DESTROY_WITH_PARENT),
                                    nullptr,
                                    nullptr);
    gtk_container_set_border_width(GTK_CONTAINER(hnd->dlg), 5);
    g_signal_connect(G_OBJECT(hnd->dlg), "key-press-event", G_CALLBACK(on_textview_keypress), hnd);
    g_object_set_data(G_OBJECT(hnd->dlg), "hnd", hnd);

    // Debug code
    // ztd::logger::info("Parent window title: {}", gtk_window_get_title(GTK_WINDOW(hnd->parent)));

    // Forcing dialog icon
    xset_set_window_icon(GTK_WINDOW(hnd->dlg));

    // Setting saved dialog size
    i32 width = xset_get_int(handler_conf_xsets.at(ptk::handler::mode::arc), xset::var::x);
    i32 height = xset_get_int(handler_conf_xsets.at(ptk::handler::mode::arc), xset::var::y);
    if (width && height)
    {
        gtk_window_set_default_size(GTK_WINDOW(hnd->dlg), width, height);
    }

    // Adding standard buttons and saving references in the dialog
    // 'Restore defaults' button has custom text but a stock image
    hnd->btn_defaults =
        gtk_dialog_add_button(GTK_DIALOG(hnd->dlg), "Opt_ions", GtkResponseType::GTK_RESPONSE_NONE);
    gtk_widget_set_focus_on_click(GTK_WIDGET(hnd->btn_defaults), false);
    // use clicked event because menu only shown once from dialog run???
    g_signal_connect(G_OBJECT(hnd->btn_defaults),
                     "clicked",
                     G_CALLBACK(on_options_button_clicked),
                     hnd);

    hnd->btn_defaults0 =
        gtk_dialog_add_button(GTK_DIALOG(hnd->dlg), "Defa_ults", GtkResponseType::GTK_RESPONSE_NO);
    gtk_widget_set_focus_on_click(GTK_WIDGET(hnd->btn_defaults0), false);

    hnd->btn_cancel =
        gtk_dialog_add_button(GTK_DIALOG(hnd->dlg), "Cancel", GtkResponseType::GTK_RESPONSE_CANCEL);
    hnd->btn_ok =
        gtk_dialog_add_button(GTK_DIALOG(hnd->dlg), "OK", GtkResponseType::GTK_RESPONSE_OK);

    // Generating left-hand side of dialog
    GtkWidget* lbl_handlers = gtk_label_new(nullptr);
    const std::string markup = std::format("<b>{}</b>", dialog_mnemonics.at(mode));
    gtk_label_set_markup_with_mnemonic(GTK_LABEL(lbl_handlers), markup.data());
    gtk_widget_set_halign(GTK_WIDGET(lbl_handlers), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(lbl_handlers), GtkAlign::GTK_ALIGN_START);

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

    /*igcr probably does not need to be single click, as you are not using row
     * activation, only selection changed? */
    // gtk_tree_view_set_single_click(GTK_TREE_VIEW(hnd->view_handlers)), true);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(hnd->view_handlers), false);

    // Turning the treeview into a scrollable widget
    GtkWidget* view_scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(view_scroll),
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);
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
    gtk_tree_view_column_set_sizing(col, GtkTreeViewColumnSizing::GTK_TREE_VIEW_COLUMN_AUTOSIZE);

    GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, true);

    // Tie model data to the column
    gtk_tree_view_column_add_attribute(
        col,
        renderer,
        "text",
        magic_enum::enum_integer(ptk::handler::column::handler_name));

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
        mode == ptk::handler::mode::file ? "Ena_ble as a default opener" : "Ena_ble Handler");
    gtk_widget_set_focus_on_click(GTK_WIDGET(hnd->chkbtn_handler_enabled), false);
    g_signal_connect(G_OBJECT(hnd->chkbtn_handler_enabled),
                     "toggled",
                     G_CALLBACK(on_configure_handler_enabled_check),
                     hnd);
    GtkWidget* lbl_handler_name = gtk_label_new(nullptr);
    gtk_label_set_markup_with_mnemonic(GTK_LABEL(lbl_handler_name), "_Name:");
    gtk_widget_set_halign(GTK_WIDGET(lbl_handler_name), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(lbl_handler_name), GtkAlign::GTK_ALIGN_CENTER);
    GtkWidget* lbl_handler_mime = gtk_label_new(nullptr);
    gtk_label_set_markup_with_mnemonic(
        GTK_LABEL(lbl_handler_mime),
        mode == ptk::handler::mode::arc || mode == ptk::handler::mode::file ? "MIM_E Type:"
                                                                            : "Whit_elist:");
    gtk_widget_set_halign(GTK_WIDGET(lbl_handler_mime), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(lbl_handler_mime), GtkAlign::GTK_ALIGN_CENTER);
    GtkWidget* lbl_handler_extension = gtk_label_new(nullptr);
    gtk_label_set_markup_with_mnemonic(
        GTK_LABEL(lbl_handler_extension),
        mode == ptk::handler::mode::arc || mode == ptk::handler::mode::file ? "P_athname:"
                                                                            : "Bl_acklist:");
    gtk_widget_set_halign(GTK_WIDGET(lbl_handler_extension), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(lbl_handler_extension), GtkAlign::GTK_ALIGN_END);
    GtkWidget* lbl_handler_icon;
    if (mode == ptk::handler::mode::file)
    {
        lbl_handler_icon = gtk_label_new(nullptr);
        gtk_label_set_markup_with_mnemonic(GTK_LABEL(lbl_handler_icon), "_Icon:");
        gtk_widget_set_halign(GTK_WIDGET(lbl_handler_icon), GtkAlign::GTK_ALIGN_START);
        gtk_widget_set_valign(GTK_WIDGET(lbl_handler_icon), GtkAlign::GTK_ALIGN_END);
    }
    else
    {
        lbl_handler_icon = nullptr;
    }

    std::string str;
    GtkWidget* lbl_handler_compress = gtk_label_new(nullptr);
    if (mode == ptk::handler::mode::arc)
    {
        str = "<b>Co_mpress:</b>";
    }
    else if (mode == ptk::handler::mode::file)
    {
        str = "<b>Open Co_mmand:</b>";
    }
    else
    {
        str = "<b>_Mount:</b>";
    }
    gtk_label_set_markup_with_mnemonic(GTK_LABEL(lbl_handler_compress), str.data());
    gtk_widget_set_halign(GTK_WIDGET(lbl_handler_compress), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(lbl_handler_compress), GtkAlign::GTK_ALIGN_END);
    GtkWidget* lbl_handler_extract = gtk_label_new(nullptr);
    gtk_label_set_markup_with_mnemonic(GTK_LABEL(lbl_handler_extract),
                                       mode == ptk::handler::mode::arc ? "<b>Ex_tract:</b>"
                                                                       : "<b>Unmoun_t:</b>");
    gtk_widget_set_halign(GTK_WIDGET(lbl_handler_extract), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(lbl_handler_extract), GtkAlign::GTK_ALIGN_END);
    GtkWidget* lbl_handler_list = gtk_label_new(nullptr);
    gtk_label_set_markup_with_mnemonic(GTK_LABEL(lbl_handler_list),
                                       mode == ptk::handler::mode::arc ? "<b>Li_st:</b>"
                                                                       : "<b>Propertie_s:</b>");
    gtk_widget_set_halign(GTK_WIDGET(lbl_handler_list), GtkAlign::GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(lbl_handler_list), GtkAlign::GTK_ALIGN_END);
    hnd->entry_handler_name = gtk_entry_new();
    hnd->entry_handler_mime = gtk_entry_new();
    hnd->entry_handler_extension = gtk_entry_new();

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

    /* Creating new textviews in scrolled windows */
    hnd->view_handler_compress = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(hnd->view_handler_compress),
                                GtkWrapMode::GTK_WRAP_WORD_CHAR);
    GtkWidget* view_handler_compress_scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(view_handler_compress_scroll),
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);
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
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(hnd->view_handler_extract),
                                GtkWrapMode::GTK_WRAP_WORD_CHAR);
    GtkWidget* view_handler_extract_scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(view_handler_extract_scroll),
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);
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
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(hnd->view_handler_list),
                                GtkWrapMode::GTK_WRAP_WORD_CHAR);
    GtkWidget* view_handler_list_scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(view_handler_list_scroll),
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);
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
        mode == ptk::handler::mode::file ? "Run As Task" : "Run In Terminal");
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
    str = std::format("<a href=\"{}\">{}</a>", 0, "Edit");
    gtk_label_set_markup_with_mnemonic(GTK_LABEL(lbl_edit0), str.data());
    g_signal_connect(G_OBJECT(lbl_edit0), "activate-link", G_CALLBACK(on_activate_link), hnd);
    GtkWidget* lbl_edit1 = gtk_label_new(nullptr);
    str = std::format("<a href=\"{}\">{}</a>", 1, "Edit");
    gtk_label_set_markup_with_mnemonic(GTK_LABEL(lbl_edit1), str.data());
    g_signal_connect(G_OBJECT(lbl_edit1), "activate-link", G_CALLBACK(on_activate_link), hnd);
    GtkWidget* lbl_edit2 = gtk_label_new(nullptr);
    str = std::format("<a href=\"{}\">{}</a>", 2, "Edit");
    gtk_label_set_markup_with_mnemonic(GTK_LABEL(lbl_edit2), str.data());
    g_signal_connect(G_OBJECT(lbl_edit2), "activate-link", G_CALLBACK(on_activate_link), hnd);

    /* Creating container boxes - at this point the dialog already comes
     * with one GtkVBox then inside that a GtkHButtonBox
     * For the right side of the dialog, standard GtkBox approach fails
     * to allow precise padding of labels to allow all entries to line up
     *  - so reimplementing with GtkTable. Would many GtkAlignments have
     * worked? */
    GtkWidget* hbox_main = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* vbox_handlers = gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget* hbox_view_buttons = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* hbox_move_buttons = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* vbox_settings = gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget* hbox_compress_header = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* hbox_extract_header = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* hbox_list_header = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 4);

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

    /* view_handlers is not added but view_scroll is - view_handlers is
     * inside view_scroll. No padding added to get it to align with the
     * enabled widget on the right side */
    gtk_box_pack_start(GTK_BOX(vbox_handlers), GTK_WIDGET(view_scroll), true, true, 0);
    gtk_box_pack_start(GTK_BOX(vbox_handlers), GTK_WIDGET(hbox_view_buttons), false, false, 0);
    gtk_box_pack_start(GTK_BOX(vbox_handlers), GTK_WIDGET(hbox_move_buttons), false, false, 0);
    gtk_box_pack_start(GTK_BOX(hbox_view_buttons), GTK_WIDGET(hnd->btn_remove), true, true, 4);
    gtk_box_pack_start(GTK_BOX(hbox_view_buttons),
                       GTK_WIDGET(gtk_separator_new(GtkOrientation::GTK_ORIENTATION_VERTICAL)),
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

    // Make sure widgets do not separate too much vertically
    gtk_box_set_spacing(GTK_BOX(vbox_settings), 1);

    // pack_end widgets must not expand to be flush up against the side
    gtk_box_pack_start(GTK_BOX(vbox_settings), GTK_WIDGET(hbox_compress_header), false, false, 4);
    gtk_box_pack_start(GTK_BOX(hbox_compress_header),
                       GTK_WIDGET(lbl_handler_compress),
                       true,
                       true,
                       4);
    if (mode == ptk::handler::mode::file)
    {
        // for file handlers, extract_term is used for Run As Task
        gtk_box_pack_start(GTK_BOX(hbox_compress_header),
                           hnd->chkbtn_handler_extract_term,
                           false,
                           true,
                           4);
    }
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
    if (mode != ptk::handler::mode::file)
    {
        gtk_box_pack_start(GTK_BOX(hbox_extract_header),
                           hnd->chkbtn_handler_extract_term,
                           false,
                           true,
                           4);
    }
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
    if (mode == ptk::handler::mode::file)
    {
        gtk_widget_hide(hbox_extract_header);
        gtk_widget_hide(hbox_list_header);
        gtk_widget_hide(view_handler_extract_scroll);
        gtk_widget_hide(view_handler_list_scroll);
    }

    /* Rendering dialog - while loop is used to deal with standard
     * buttons that should not cause the dialog to exit */
    /*igcr need to handle dialog delete event? */
    i32 response;
    while ((response = gtk_dialog_run(GTK_DIALOG(hnd->dlg))))
    {
        bool exit_loop = false;
        switch (response)
        {
            case GtkResponseType::GTK_RESPONSE_OK:
                if (hnd->changed)
                {
                    on_configure_button_press(GTK_BUTTON(hnd->btn_apply), hnd);
                }
                exit_loop = true;
                break;
            case GtkResponseType::GTK_RESPONSE_CANCEL:
                exit_loop = true;
                break;
            case GtkResponseType::GTK_RESPONSE_NONE:
                // Options menu requested
                break;
            case GtkResponseType::GTK_RESPONSE_NO:
                // Restore defaults requested
                restore_defaults(hnd, false);
                break;
            default:
                exit_loop = true;
                break;
        }
        if (exit_loop)
        {
            break;
        }
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
        xset_set(handler_conf_xsets.at(ptk::handler::mode::arc),
                 xset::var::x,
                 std::to_string(width));
        xset_set(handler_conf_xsets.at(ptk::handler::mode::arc),
                 xset::var::y,
                 std::to_string(height));
    }

    // Clearing up dialog
    delete hnd;
}
