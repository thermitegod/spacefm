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

static u32 big_thumb_size = 48;
static u32 small_thumb_size = 20;

vfs::file_info
vfs_file_info_new()
{
    vfs::file_info fi = g_slice_new0(VFSFileInfo);
    fi->ref_inc();
    return fi;
}

static void
vfs_file_info_clear(vfs::file_info file)
{
    if (!file->disp_name.empty() && !ztd::same(file->disp_name, file->name))
        file->disp_name.clear();
    if (!file->name.empty())
        file->name.clear();
    if (!file->collate_key.empty())
        file->collate_key.clear();
    if (!file->collate_icase_key.empty())
        file->collate_icase_key.clear();
    if (!file->disp_size.empty())
        file->disp_size.clear();
    if (!file->disp_owner.empty())
        file->disp_owner.clear();
    if (!file->disp_mtime.empty())
        file->disp_mtime.clear();
    if (!file->disp_perm.empty())
        file->disp_perm.clear();
    if (file->big_thumbnail)
    {
        g_object_unref(file->big_thumbnail);
        file->big_thumbnail = nullptr;
    }
    if (file->small_thumbnail)
    {
        g_object_unref(file->small_thumbnail);
        file->small_thumbnail = nullptr;
    }
    if (file->mime_type)
    {
        vfs_mime_type_unref(file->mime_type);
        file->mime_type = nullptr;
    }
    file->flags = VFSFileInfoFlag::VFS_FILE_INFO_NONE;
}

vfs::file_info
vfs_file_info_ref(vfs::file_info file)
{
    file->ref_inc();
    return file;
}

void
vfs_file_info_unref(vfs::file_info file)
{
    file->ref_dec();
    if (file->ref_count() == 0)
    {
        vfs_file_info_clear(file);
        g_slice_free(VFSFileInfo, file);
    }
}

bool
vfs_file_info_get(vfs::file_info file, std::string_view file_path)
{
    vfs_file_info_clear(file);

    const std::string name = Glib::path_get_basename(file_path.data());
    const std::string disp_name = Glib::filename_display_basename(file_path.data());
    file->name = name;
    file->disp_name = disp_name;

    struct stat file_stat;
    if (lstat(file_path.data(), &file_stat) == 0)
    {
        // LOG_INFO("VFSFileInfo {}", fi->name);

        // This is time-consuming but can save much memory
        file->mode = file_stat.st_mode;
        file->dev = file_stat.st_dev;
        file->uid = file_stat.st_uid;
        file->gid = file_stat.st_gid;
        file->size = file_stat.st_size;
        file->mtime = file_stat.st_mtime;
        file->atime = file_stat.st_atime;
        file->blksize = file_stat.st_blksize;
        file->blocks = file_stat.st_blocks;

        // LOG_INFO("size {} {}", fi->name, fi->size);

        // file->status = std::filesystem::status(file_path);
        file->status = std::filesystem::symlink_status(file_path);

        file->mime_type = vfs_mime_type_get_from_file(file_path, file->disp_name, &file_stat);

        // file size formated
        const std::string size_str = vfs_file_size_to_string_format(file->size);
        file->disp_size = size_str;

        // sfm get collate keys
        file->collate_key = g_utf8_collate_key_for_filename(file->disp_name.c_str(), -1);
        const std::string str = g_utf8_casefold(file->disp_name.c_str(), -1);
        file->collate_icase_key = g_utf8_collate_key_for_filename(str.c_str(), -1);

        return true;
    }
    else
    {
        file->mime_type = vfs_mime_type_get_from_type(XDG_MIME_TYPE_UNKNOWN);
    }

    return false;
}

const std::string&
VFSFileInfo::get_name() const noexcept
{
    return this->name;
}

// Get displayed name encoded in UTF-8
const std::string&
VFSFileInfo::get_disp_name() const noexcept
{
    return this->disp_name;
}

void
VFSFileInfo::set_disp_name(std::string_view new_disp_name) noexcept
{
    this->disp_name = new_disp_name.data();
    // sfm get new collate keys
    this->collate_key = g_utf8_collate_key_for_filename(this->disp_name.c_str(), -1);
    const std::string str = g_utf8_casefold(this->disp_name.c_str(), -1);
    this->collate_icase_key = g_utf8_collate_key_for_filename(str.c_str(), -1);
}

off_t
VFSFileInfo::get_size() const noexcept
{
    return this->size;
}

const std::string&
VFSFileInfo::get_disp_size() const noexcept
{
    return this->disp_size;
}

blkcnt_t
VFSFileInfo::get_blocks() const noexcept
{
    return this->blocks;
}

vfs::mime_type
VFSFileInfo::get_mime_type() const noexcept
{
    vfs_mime_type_ref(this->mime_type);
    return this->mime_type;
}

void
VFSFileInfo::reload_mime_type(std::string_view full_path) noexcept
{
    vfs::mime_type old_mime_type;
    struct stat file_stat;

    // convert VFSFileInfo to struct stat

    // In current implementation, only st_mode is used in
    // mime-type detection, so let's save some CPU cycles
    // and do not copy unused fields.

    file_stat.st_mode = this->mode;

    old_mime_type = this->mime_type;
    this->mime_type = vfs_mime_type_get_from_file(full_path, this->name, &file_stat);
    this->load_special_info(full_path);
    vfs_mime_type_unref(old_mime_type); /* FIXME: is vfs_mime_type_unref needed ?*/
}

const std::string
VFSFileInfo::get_mime_type_desc() const noexcept
{
    return vfs_mime_type_get_description(this->mime_type);
}

GdkPixbuf*
VFSFileInfo::get_big_icon() noexcept
{
    /* get special icons for special files, especially for
       some desktop icons */

    if (this->flags != VFSFileInfoFlag::VFS_FILE_INFO_NONE)
    {
        i32 w;
        i32 h;
        const i32 icon_size = vfs_mime_type_get_icon_size_big();

        if (this->big_thumbnail)
        {
            w = gdk_pixbuf_get_width(this->big_thumbnail);
            h = gdk_pixbuf_get_height(this->big_thumbnail);
        }
        else
        {
            w = h = 0;
        }

        if (std::abs(std::max(w, h) - icon_size) > 2)
        {
            char* icon_name = nullptr;
            if (this->big_thumbnail)
            {
                icon_name = (char*)g_object_steal_data(G_OBJECT(this->big_thumbnail), "name");
                g_object_unref(this->big_thumbnail);
                this->big_thumbnail = nullptr;
            }
            if (icon_name)
            {
                if (icon_name[0] == '/')
                    this->big_thumbnail = gdk_pixbuf_new_from_file(icon_name, nullptr);
                else
                    this->big_thumbnail = vfs_load_icon(icon_name, icon_size);
            }
            if (this->big_thumbnail)
                g_object_set_data_full(G_OBJECT(this->big_thumbnail), "name", icon_name, free);
            else
                free(icon_name);
        }
        return this->big_thumbnail ? g_object_ref(this->big_thumbnail) : nullptr;
    }
    if (!this->mime_type)
        return nullptr;
    return vfs_mime_type_get_icon(this->mime_type, true);
}

GdkPixbuf*
VFSFileInfo::get_small_icon() noexcept
{
    if (this->flags & VFSFileInfoFlag::VFS_FILE_INFO_DESKTOP_ENTRY && this->small_thumbnail)
        return g_object_ref(this->small_thumbnail);

    if (!this->mime_type)
        return nullptr;
    return vfs_mime_type_get_icon(this->mime_type, false);
}

GdkPixbuf*
VFSFileInfo::get_big_thumbnail() const noexcept
{
    return this->big_thumbnail ? g_object_ref(this->big_thumbnail) : nullptr;
}

GdkPixbuf*
VFSFileInfo::get_small_thumbnail() const noexcept
{
    return this->small_thumbnail ? g_object_ref(this->small_thumbnail) : nullptr;
}

const std::string&
VFSFileInfo::get_disp_owner() noexcept
{
    struct passwd* puser;
    struct group* pgroup;

    std::string user_name;
    std::string group_name;

    // FIXME: user names should be cached
    if (this->disp_owner.empty())
    {
        puser = getpwuid(this->uid);
        if (puser && puser->pw_name && *puser->pw_name)
            user_name = puser->pw_name;
        else
            user_name = fmt::format("{}", this->uid);

        pgroup = getgrgid(this->gid);
        if (pgroup && pgroup->gr_name && *pgroup->gr_name)
            group_name = pgroup->gr_name;
        else
            group_name = fmt::format("{}", this->gid);

        const std::string str = fmt::format("{}:{}", user_name, group_name);
        this->disp_owner = str;
    }
    return this->disp_owner;
}

const std::string&
VFSFileInfo::get_disp_mtime() noexcept
{
    if (this->disp_mtime.empty())
    {
        char buf[64];
        strftime(buf,
                 sizeof(buf),
                 app_settings.get_date_format().c_str(), //"%Y-%m-%d %H:%M",
                 std::localtime(&this->mtime));
        this->disp_mtime = buf;
    }
    return this->disp_mtime;
}

std::time_t*
VFSFileInfo::get_mtime() noexcept
{
    return &this->mtime;
}

std::time_t*
VFSFileInfo::get_atime() noexcept
{
    return &this->atime;
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

const std::string&
VFSFileInfo::get_disp_perm() noexcept
{
    if (this->disp_perm.empty())
        this->disp_perm = get_file_perm_string(this->status);
    return this->disp_perm;
}

bool
VFSFileInfo::is_directory() const noexcept
{
    // return std::filesystem::is_directory(this->status);

    if (std::filesystem::is_directory(this->status))
    {
        return true;
    }
    else if (std::filesystem::is_symlink(this->status))
    {
        std::string symlink_path;
        try
        {
            symlink_path = std::filesystem::read_symlink(this->name);
        }
        catch (const std::exception& e)
        {
            return false;
        }
        return std::filesystem::is_directory(symlink_path);
    }
    return false;
}

bool
VFSFileInfo::is_regular_file() const noexcept
{
    return std::filesystem::is_regular_file(this->status);
}

bool
VFSFileInfo::is_symlink() const noexcept
{
    return std::filesystem::is_symlink(this->status);
}

bool
VFSFileInfo::is_socket() const noexcept
{
    return std::filesystem::is_socket(this->status);
}

bool
VFSFileInfo::is_fifo() const noexcept
{
    return std::filesystem::is_fifo(this->status);
}

bool
VFSFileInfo::is_block_file() const noexcept
{
    return std::filesystem::is_block_file(this->status);
}

bool
VFSFileInfo::is_character_file() const noexcept
{
    return std::filesystem::is_character_file(this->status);
}

bool
VFSFileInfo::is_image() const noexcept
{
    // FIXME: We had better use functions of xdg_mime to check this
    return ztd::startswith(vfs_mime_type_get_type(this->mime_type), "image/");
}

bool
VFSFileInfo::is_video() const noexcept
{
    // FIXME: We had better use functions of xdg_mime to check this
    return ztd::startswith(vfs_mime_type_get_type(this->mime_type), "video/");
}

bool
VFSFileInfo::is_desktop_entry() const noexcept
{
    return 0 != (this->flags & VFSFileInfoFlag::VFS_FILE_INFO_DESKTOP_ENTRY);
}

bool
VFSFileInfo::is_unknown_type() const noexcept
{
    return ztd::same(vfs_mime_type_get_type(this->mime_type), XDG_MIME_TYPE_UNKNOWN);
}

// full path of the file is required by this function
bool
VFSFileInfo::is_executable(std::string_view file_path) const noexcept
{
    return mime_type_is_executable_file(file_path, this->mime_type->type);
}

// full path of the file is required by this function
bool
VFSFileInfo::is_text(std::string_view file_path) const noexcept
{
    return mime_type_is_text_file(file_path, this->mime_type->type);
}

std::filesystem::perms
VFSFileInfo::get_permissions() const noexcept
{
    return this->status.permissions();
}

bool
VFSFileInfo::is_thumbnail_loaded(bool big) const noexcept
{
    if (big)
        return (this->big_thumbnail != nullptr);
    return (this->small_thumbnail != nullptr);
}

bool
VFSFileInfo::load_thumbnail(std::string_view full_path, bool big) noexcept
{
    if (big)
    {
        if (this->big_thumbnail)
            return true;
    }
    else
    {
        if (this->small_thumbnail)
            return true;
    }

    GdkPixbuf* thumbnail =
        vfs_thumbnail_load_for_file(full_path, big ? big_thumb_size : small_thumb_size);
    if (thumbnail)
    {
        if (big)
            this->big_thumbnail = thumbnail;
        else
            this->small_thumbnail = thumbnail;
    }
    else /* fallback to mime_type icon */
    {
        if (big)
            this->big_thumbnail = this->get_big_icon();
        else
            this->small_thumbnail = this->get_small_icon();
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
VFSFileInfo::load_special_info(std::string_view file_path) noexcept
{
    if (!ztd::endswith(this->name, ".desktop"))
        return;

    const std::string file_dir = Glib::path_get_dirname(file_path.data());

    this->flags = (VFSFileInfoFlag)(this->flags | VFSFileInfoFlag::VFS_FILE_INFO_DESKTOP_ENTRY);
    vfs::desktop desktop(file_path);

    // MOD  display real filenames of .desktop files not in desktop directory
    if (ztd::same(file_dir, vfs_user_desktop_dir()))
        this->set_disp_name(desktop.get_disp_name());

    if (desktop.get_icon_name().empty())
        return;

    GdkPixbuf* icon;
    const i32 big_size = vfs_mime_type_get_icon_size_big();
    const i32 small_size = vfs_mime_type_get_icon_size_small();
    if (!this->big_thumbnail)
    {
        icon = desktop.get_icon(big_size);
        if (icon)
            this->big_thumbnail = icon;
    }
    if (!this->small_thumbnail)
    {
        icon = desktop.get_icon(small_size);
        if (icon)
            this->small_thumbnail = icon;
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
