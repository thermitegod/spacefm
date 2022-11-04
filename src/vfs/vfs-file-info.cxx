/**
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
#include <string_view>

#include <filesystem>

#include <vector>

#include <algorithm>
#include <ranges>

#include <grp.h>
#include <pwd.h>

#include <fmt/format.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "settings/app.hxx"

#include "settings.hxx"

#include "vfs/vfs-app-desktop.hxx"
#include "vfs/vfs-thumbnail-loader.hxx"
#include "vfs/vfs-utils.hxx"
#include "vfs/vfs-user-dir.hxx"

#include "vfs/vfs-file-info.hxx"

static i32 big_thumb_size = 48, small_thumb_size = 20;

vfs::file_info
vfs_file_info_new()
{
    vfs::file_info fi = g_slice_new0(VFSFileInfo);
    fi->ref_inc();
    return fi;
}

static void
vfs_file_info_clear(vfs::file_info fi)
{
    if (!fi->disp_name.empty() && !ztd::same(fi->disp_name, fi->name))
        fi->disp_name.clear();
    if (!fi->name.empty())
        fi->name.clear();
    if (!fi->collate_key.empty())
        fi->collate_key.clear();
    if (!fi->collate_icase_key.empty())
        fi->collate_icase_key.clear();
    if (!fi->disp_size.empty())
        fi->disp_size.clear();
    if (!fi->disp_owner.empty())
        fi->disp_owner.clear();
    if (!fi->disp_mtime.empty())
        fi->disp_mtime.clear();
    if (!fi->disp_perm.empty())
        fi->disp_perm.clear();
    if (fi->big_thumbnail)
    {
        g_object_unref(fi->big_thumbnail);
        fi->big_thumbnail = nullptr;
    }
    if (fi->small_thumbnail)
    {
        g_object_unref(fi->small_thumbnail);
        fi->small_thumbnail = nullptr;
    }
    if (fi->mime_type)
    {
        vfs_mime_type_unref(fi->mime_type);
        fi->mime_type = nullptr;
    }
    fi->flags = VFSFileInfoFlag::VFS_FILE_INFO_NONE;
}

vfs::file_info
vfs_file_info_ref(vfs::file_info fi)
{
    fi->ref_inc();
    return fi;
}

void
vfs_file_info_unref(vfs::file_info fi)
{
    fi->ref_dec();
    if (fi->ref_count() == 0)
    {
        vfs_file_info_clear(fi);
        g_slice_free(VFSFileInfo, fi);
    }
}

bool
vfs_file_info_get(vfs::file_info fi, std::string_view file_path)
{
    vfs_file_info_clear(fi);

    const std::string name = Glib::path_get_basename(file_path.data());
    const std::string disp_name = Glib::filename_display_basename(file_path.data());
    fi->name = name;
    fi->disp_name = disp_name;

    struct stat file_stat;
    if (lstat(file_path.data(), &file_stat) == 0)
    {
        // LOG_INFO("VFSFileInfo name={}  size={}", fi->name, fi->name);
        // This is time-consuming but can save much memory
        fi->mode = file_stat.st_mode;
        fi->dev = file_stat.st_dev;
        fi->uid = file_stat.st_uid;
        fi->gid = file_stat.st_gid;
        fi->size = file_stat.st_size;
        fi->mtime = file_stat.st_mtime;
        fi->atime = file_stat.st_atime;
        fi->blksize = file_stat.st_blksize;
        fi->blocks = file_stat.st_blocks;

        // fi->status = std::filesystem::status(file_path);
        fi->status = std::filesystem::symlink_status(file_path);

        fi->mime_type =
            vfs_mime_type_get_from_file(file_path.data(), fi->disp_name.c_str(), &file_stat);

        // sfm get collate keys
        fi->collate_key = g_utf8_collate_key_for_filename(fi->disp_name.c_str(), -1);
        const std::string str = g_utf8_casefold(fi->disp_name.c_str(), -1);
        fi->collate_icase_key = g_utf8_collate_key_for_filename(str.c_str(), -1);

        return true;
    }
    else
    {
        fi->mime_type = vfs_mime_type_get_from_type(XDG_MIME_TYPE_UNKNOWN);
    }

    return false;
}

const char*
vfs_file_info_get_name(vfs::file_info fi)
{
    return fi->name.c_str();
}

/* Get displayed name encoded in UTF-8 */
const char*
vfs_file_info_get_disp_name(vfs::file_info fi)
{
    return fi->disp_name.c_str();
}

void
vfs_file_info_set_disp_name(vfs::file_info fi, const char* name)
{
    if (fi->disp_name.c_str() != fi->name.c_str())
        fi->disp_name.clear();

    fi->disp_name = name;
    // sfm get new collate keys
    fi->collate_key = g_utf8_collate_key_for_filename(fi->disp_name.c_str(), -1);
    const std::string str = g_utf8_casefold(fi->disp_name.c_str(), -1);
    fi->collate_icase_key = g_utf8_collate_key_for_filename(str.c_str(), -1);
}

off_t
vfs_file_info_get_size(vfs::file_info fi)
{
    return fi->size;
}

const char*
vfs_file_info_get_disp_size(vfs::file_info fi)
{
    if (fi->disp_size.empty())
    {
        const std::string size_str = vfs_file_size_to_string_format(fi->size, true);
        fi->disp_size = size_str;
    }
    return fi->disp_size.c_str();
}

off_t
vfs_file_info_get_blocks(vfs::file_info fi)
{
    return fi->blocks;
}

VFSMimeType*
vfs_file_info_get_mime_type(vfs::file_info fi)
{
    vfs_mime_type_ref(fi->mime_type);
    return fi->mime_type;
}

void
vfs_file_info_reload_mime_type(vfs::file_info fi, const char* full_path)
{
    VFSMimeType* old_mime_type;
    struct stat file_stat;

    // convert VFSFileInfo to struct stat

    // In current implementation, only st_mode is used in
    // mime-type detection, so let's save some CPU cycles
    // and do not copy unused fields.
    file_stat.st_mode = fi->mode;

    old_mime_type = fi->mime_type;
    fi->mime_type = vfs_mime_type_get_from_file(full_path, fi->name.c_str(), &file_stat);
    vfs_file_info_load_special_info(fi, full_path);
    vfs_mime_type_unref(old_mime_type); /* FIXME: is vfs_mime_type_unref needed ?*/
}

const char*
vfs_file_info_get_mime_type_desc(vfs::file_info fi)
{
    return vfs_mime_type_get_description(fi->mime_type);
}

GdkPixbuf*
vfs_file_info_get_big_icon(vfs::file_info fi)
{
    /* get special icons for special files, especially for
       some desktop icons */

    if (fi->flags != VFSFileInfoFlag::VFS_FILE_INFO_NONE)
    {
        i32 w, h;
        const i32 icon_size = vfs_mime_type_get_icon_size_big();
        if (fi->big_thumbnail)
        {
            w = gdk_pixbuf_get_width(fi->big_thumbnail);
            h = gdk_pixbuf_get_height(fi->big_thumbnail);
        }
        else
        {
            w = h = 0;
        }

        if (std::abs(std::max(w, h) - icon_size) > 2)
        {
            char* icon_name = nullptr;
            if (fi->big_thumbnail)
            {
                icon_name = (char*)g_object_steal_data(G_OBJECT(fi->big_thumbnail), "name");
                g_object_unref(fi->big_thumbnail);
                fi->big_thumbnail = nullptr;
            }
            if (icon_name)
            {
                if (icon_name[0] == '/')
                    fi->big_thumbnail = gdk_pixbuf_new_from_file(icon_name, nullptr);
                else
                    fi->big_thumbnail = vfs_load_icon(icon_name, icon_size);
            }
            if (fi->big_thumbnail)
                g_object_set_data_full(G_OBJECT(fi->big_thumbnail), "name", icon_name, free);
            else
                free(icon_name);
        }
        return fi->big_thumbnail ? g_object_ref(fi->big_thumbnail) : nullptr;
    }
    if (!fi->mime_type)
        return nullptr;
    return vfs_mime_type_get_icon(fi->mime_type, true);
}

GdkPixbuf*
vfs_file_info_get_small_icon(vfs::file_info fi)
{
    if (fi->flags & VFSFileInfoFlag::VFS_FILE_INFO_DESKTOP_ENTRY && fi->small_thumbnail) // sfm
        return g_object_ref(fi->small_thumbnail);

    if (!fi->mime_type)
        return nullptr;
    return vfs_mime_type_get_icon(fi->mime_type, false);
}

GdkPixbuf*
vfs_file_info_get_big_thumbnail(vfs::file_info fi)
{
    return fi->big_thumbnail ? g_object_ref(fi->big_thumbnail) : nullptr;
}

GdkPixbuf*
vfs_file_info_get_small_thumbnail(vfs::file_info fi)
{
    return fi->small_thumbnail ? g_object_ref(fi->small_thumbnail) : nullptr;
}

const char*
vfs_file_info_get_disp_owner(vfs::file_info fi)
{
    struct passwd* puser;
    struct group* pgroup;

    /* FIXME: user names should be cached */
    if (fi->disp_owner.empty())
    {
        std::string user_name;
        std::string group_name;

        puser = getpwuid(fi->uid);
        if (puser && puser->pw_name && *puser->pw_name)
            user_name = puser->pw_name;
        else
            user_name = fmt::format("{}", fi->uid);

        pgroup = getgrgid(fi->gid);
        if (pgroup && pgroup->gr_name && *pgroup->gr_name)
            group_name = pgroup->gr_name;
        else
            group_name = fmt::format("{}", fi->gid);

        const std::string str = fmt::format("{}:{}", user_name, group_name);
        fi->disp_owner = str;
    }
    return fi->disp_owner.c_str();
}

const char*
vfs_file_info_get_disp_mtime(vfs::file_info fi)
{
    if (fi->disp_mtime.empty())
    {
        char buf[64];
        strftime(buf,
                 sizeof(buf),
                 app_settings.get_date_format().c_str(), //"%Y-%m-%d %H:%M",
                 std::localtime(&fi->mtime));
        fi->disp_mtime = buf;
    }
    return fi->disp_mtime.c_str();
}

time_t*
vfs_file_info_get_mtime(vfs::file_info fi)
{
    return &fi->mtime;
}

time_t*
vfs_file_info_get_atime(vfs::file_info fi)
{
    return &fi->atime;
}

static const std::string
get_file_perm_string(std::filesystem::file_status status)
{
    static constexpr u8 file_type{0};

    static constexpr u8 owner_read{1};
    static constexpr u8 owner_write{2};
    static constexpr u8 owner_exec{3};

    static constexpr u8 group_read{4};
    static constexpr u8 group_write{5};
    static constexpr u8 group_exec{6};

    static constexpr u8 other_read{7};
    static constexpr u8 other_write{8};
    static constexpr u8 other_exec{9};

    // blank permissions
    std::string perm = "----------";

    // File Type Permissions
    if (std::filesystem::is_regular_file(status))
        perm[file_type] = '-';
    else if (std::filesystem::is_directory(status))
        perm[file_type] = 'd';
    else if (std::filesystem::is_symlink(status))
        perm[file_type] = 'l';
    else if (std::filesystem::is_character_file(status))
        perm[file_type] = 'c';
    else if (std::filesystem::is_block_file(status))
        perm[file_type] = 'b';
    else if (std::filesystem::is_fifo(status))
        perm[file_type] = 'p';
    else if (std::filesystem::is_socket(status))
        perm[file_type] = 's';

    std::filesystem::perms p = status.permissions();

    // Owner
    if ((p & std::filesystem::perms::owner_read) != std::filesystem::perms::none)
        perm[owner_read] = 'r';

    if ((p & std::filesystem::perms::owner_write) != std::filesystem::perms::none)
        perm[owner_write] = 'w';

    if ((p & std::filesystem::perms::set_uid) != std::filesystem::perms::none)
    {
        if ((p & std::filesystem::perms::owner_exec) != std::filesystem::perms::none)
            perm[owner_exec] = 's';
        else
            perm[owner_exec] = 's';
    }
    else
    {
        if ((p & std::filesystem::perms::owner_exec) != std::filesystem::perms::none)
            perm[owner_exec] = 'x';
    }

    // Group
    if ((p & std::filesystem::perms::group_read) != std::filesystem::perms::none)
        perm[group_read] = 'r';

    if ((p & std::filesystem::perms::group_write) != std::filesystem::perms::none)
        perm[group_write] = 'w';

    if ((p & std::filesystem::perms::set_gid) != std::filesystem::perms::none)
    {
        if ((p & std::filesystem::perms::group_exec) != std::filesystem::perms::none)
            perm[group_exec] = 's';
        else
            perm[group_exec] = 'S';
    }
    else
    {
        if ((p & std::filesystem::perms::group_exec) != std::filesystem::perms::none)
            perm[group_exec] = 'x';
    }

    // Other
    if ((p & std::filesystem::perms::others_read) != std::filesystem::perms::none)
        perm[other_read] = 'r';

    if ((p & std::filesystem::perms::others_write) != std::filesystem::perms::none)
        perm[other_write] = 'w';

    if ((p & std::filesystem::perms::sticky_bit) != std::filesystem::perms::none)
    {
        if ((p & std::filesystem::perms::others_exec) != std::filesystem::perms::none)
            perm[other_exec] = 't';
        else
            perm[other_exec] = 'T';
    }
    else
    {
        if ((p & std::filesystem::perms::others_exec) != std::filesystem::perms::none)
            perm[other_exec] = 'x';
    }

    return perm;
}

const char*
vfs_file_info_get_disp_perm(vfs::file_info fi)
{
    if (fi->disp_perm.empty())
        fi->disp_perm = get_file_perm_string(fi->status);
    return fi->disp_perm.c_str();
}

bool
vfs_file_info_is_dir(vfs::file_info fi)
{
    if (std::filesystem::is_directory(fi->status))
        return true;
    else if (std::filesystem::is_symlink(fi->status))
        return std::filesystem::is_directory(std::filesystem::read_symlink(fi->name));
    return false;
}

bool
vfs_file_info_is_regular_file(vfs::file_info fi)
{
    return std::filesystem::is_regular_file(fi->status);
}

bool
vfs_file_info_is_symlink(vfs::file_info fi)
{
    return std::filesystem::is_symlink(fi->status);
}

bool
vfs_file_info_is_socket(vfs::file_info fi)
{
    return std::filesystem::is_socket(fi->status);
}

bool
vfs_file_info_is_named_pipe(vfs::file_info fi)
{
    return std::filesystem::is_fifo(fi->status);
}

bool
vfs_file_info_is_block_device(vfs::file_info fi)
{
    return std::filesystem::is_block_file(fi->status);
}

bool
vfs_file_info_is_char_device(vfs::file_info fi)
{
    return std::filesystem::is_character_file(fi->status);
}

bool
vfs_file_info_is_image(vfs::file_info fi)
{
    // FIXME: We had better use functions of xdg_mime to check this
    return ztd::startswith(vfs_mime_type_get_type(fi->mime_type), "image/");
}

bool
vfs_file_info_is_video(vfs::file_info fi)
{
    // FIXME: We had better use functions of xdg_mime to check this
    return ztd::startswith(vfs_mime_type_get_type(fi->mime_type), "video/");
}

bool
vfs_file_info_is_desktop_entry(vfs::file_info fi)
{
    return 0 != (fi->flags & VFSFileInfoFlag::VFS_FILE_INFO_DESKTOP_ENTRY);
}

bool
vfs_file_info_is_unknown_type(vfs::file_info fi)
{
    return ztd::same(vfs_mime_type_get_type(fi->mime_type), XDG_MIME_TYPE_UNKNOWN);
}

/* full path of the file is required by this function */
bool
vfs_file_info_is_executable(vfs::file_info fi, std::string_view file_path)
{
    return mime_type_is_executable_file(file_path, fi->mime_type->type);
}

/* full path of the file is required by this function */
bool
vfs_file_info_is_text(vfs::file_info fi, std::string_view file_path)
{
    return mime_type_is_text_file(file_path, fi->mime_type->type);
}

std::filesystem::perms
vfs_file_info_get_mode(vfs::file_info fi)
{
    return fi->status.permissions();
}

bool
vfs_file_info_is_thumbnail_loaded(vfs::file_info fi, bool big)
{
    if (big)
        return (fi->big_thumbnail != nullptr);
    return (fi->small_thumbnail != nullptr);
}

bool
vfs_file_info_load_thumbnail(vfs::file_info fi, std::string_view full_path, bool big)
{
    if (big)
    {
        if (fi->big_thumbnail)
            return true;
    }
    else
    {
        if (fi->small_thumbnail)
            return true;
    }
    GdkPixbuf* thumbnail =
        vfs_thumbnail_load_for_file(full_path, big ? big_thumb_size : small_thumb_size, fi->mtime);
    if (thumbnail)
    {
        if (big)
            fi->big_thumbnail = thumbnail;
        else
            fi->small_thumbnail = thumbnail;
    }
    else /* fallback to mime_type icon */
    {
        if (big)
            fi->big_thumbnail = vfs_file_info_get_big_icon(fi);
        else
            fi->small_thumbnail = vfs_file_info_get_small_icon(fi);
    }
    return (thumbnail != nullptr);
}

void
vfs_file_info_set_thumbnail_size_big(i32 size)
{
    big_thumb_size = size;
}

void
vfs_file_info_set_thumbnail_size_small(i32 size)
{
    small_thumb_size = size;
}

void
vfs_file_info_load_special_info(vfs::file_info fi, std::string_view file_path)
{
    /*if (fi->type && fi->type->name, "application/x-desktop") */
    if (!ztd::endswith(fi->name, ".desktop"))
        return;

    const std::string file_dir = Glib::path_get_dirname(file_path.data());

    fi->flags = (VFSFileInfoFlag)(fi->flags | VFSFileInfoFlag::VFS_FILE_INFO_DESKTOP_ENTRY);

    vfs::desktop desktop(file_path);

    // MOD  display real filenames of .desktop files not in desktop directory
    if (ztd::same(file_dir, vfs_user_desktop_dir()))
    {
        if (desktop.get_disp_name())
            vfs_file_info_set_disp_name(fi, desktop.get_disp_name());
    }

    if (desktop.get_icon_name())
    {
        GdkPixbuf* icon;
        const i32 big_size = vfs_mime_type_get_icon_size_big();
        const i32 small_size = vfs_mime_type_get_icon_size_small();
        if (!fi->big_thumbnail)
        {
            icon = desktop.get_icon(big_size);
            if (icon)
                fi->big_thumbnail = icon;
        }
        if (!fi->small_thumbnail)
        {
            icon = desktop.get_icon(small_size);
            if (icon)
                fi->small_thumbnail = icon;
        }
    }
}

void
vfs_file_info_list_free(const std::vector<vfs::file_info>& list)
{
    std::ranges::for_each(list, vfs_file_info_unref);
}

void
VFSFileInfo::ref_inc()
{
    ++n_ref;
}

void
VFSFileInfo::ref_dec()
{
    --n_ref;
}

u32
VFSFileInfo::ref_count()
{
    return n_ref;
}
