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

#include <format>

#include <filesystem>

#include <span>

#include <algorithm>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "settings/app.hxx"

#include "vfs/vfs-app-desktop.hxx"
#include "vfs/vfs-thumbnail-loader.hxx"
#include "vfs/vfs-time.hxx"
#include "vfs/vfs-utils.hxx"
#include "vfs/vfs-user-dirs.hxx"

#include "vfs/vfs-file-info.hxx"

vfs::file_info
vfs_file_info_new(const std::filesystem::path& file_path)
{
    const auto fi = new VFSFileInfo(file_path);
    fi->ref_inc();
    return fi;
}

vfs::file_info
vfs_file_info_ref(vfs::file_info file)
{
    assert(file != nullptr);
    file->ref_inc();
    return file;
}

void
vfs_file_info_unref(vfs::file_info file)
{
    assert(file != nullptr);
    file->ref_dec();
    if (file->ref_count() == 0)
    {
        delete file;
    }
}

VFSFileInfo::VFSFileInfo(const std::filesystem::path& file_path)
{
    this->update(file_path);
}

VFSFileInfo::~VFSFileInfo()
{
    if (this->big_thumbnail_)
    {
        g_object_unref(this->big_thumbnail_);
    }
    if (this->small_thumbnail_)
    {
        g_object_unref(this->small_thumbnail_);
    }
}

bool
VFSFileInfo::update(const std::filesystem::path& file_path) noexcept
{
    if (std::filesystem::equivalent(file_path, "/"))
    {
        // special case, using std::filesystem::path::filename() on the root
        // directory returns an empty string. that causes subtle bugs
        // so hard code "/" as the value for root.
        this->name_ = "/";
        this->display_name_ = "/";
    }
    else
    {
        this->name_ = file_path.filename();
        this->display_name_ = file_path.filename();
    }

    this->path_ = file_path;

    this->file_stat_ = ztd::statx(this->path_, ztd::statx::symlink::no_follow);
    if (!this->file_stat_)
    {
        this->mime_type_ = vfs_mime_type_get_from_type(XDG_MIME_TYPE_UNKNOWN);
        return false;
    }

    // ztd::logger::debug("VFSFileInfo name={}  size={}", this->name, this->file_stat.size());

    // this->status = std::filesystem::status(file_path);
    this->status_ = std::filesystem::symlink_status(this->path_);

    this->mime_type_ = vfs_mime_type_get_from_file(this->path_);

    // file size formated
    this->display_size_ = vfs_file_size_format(this->size());
    this->display_size_bytes_ = std::format("{:L}", this->size());

    // disk file size formated
    const std::string disk_size = vfs_file_size_format(this->size_on_disk());
    this->display_disk_size_ = disk_size;

    // hidden
    this->is_hidden_ = ztd::startswith(this->name_, ".");

    // collate keys
    this->collate_key_ = g_utf8_collate_key_for_filename(this->display_name_.data(), -1);
    const std::string str = g_utf8_casefold(this->display_name_.data(), -1);
    this->collate_icase_key_ = g_utf8_collate_key_for_filename(str.data(), -1);

    // this->collate_key_ = Glib::ustring(this->display_name_).collate_key();
    // this->collate_icase_key_ = Glib::ustring(this->display_name_).casefold_collate_key();

    // owner
    const auto pw = ztd::passwd(this->file_stat_.uid());
    this->display_owner_ = pw.name();

    // group
    const auto gr = ztd::group(this->file_stat_.gid());
    this->display_group_ = gr.name();

    // time
    this->display_atime_ = vfs_create_display_date(this->atime());
    this->display_btime_ = vfs_create_display_date(this->btime());
    this->display_ctime_ = vfs_create_display_date(this->ctime());
    this->display_mtime_ = vfs_create_display_date(this->mtime());

    return true;
}

const std::string_view
VFSFileInfo::name() const noexcept
{
    return this->name_;
}

// Get displayed name encoded in UTF-8
const std::string_view
VFSFileInfo::display_name() const noexcept
{
    return this->display_name_;
}

void
VFSFileInfo::update_display_name(const std::string_view new_display_name) noexcept
{
    this->display_name_ = new_display_name.data();
    // sfm get new collate keys

    this->collate_key_ = g_utf8_collate_key_for_filename(this->display_name_.data(), -1);
    const std::string str = g_utf8_casefold(this->display_name_.data(), -1);
    this->collate_icase_key_ = g_utf8_collate_key_for_filename(str.data(), -1);

    //this->collate_key_ = Glib::ustring(this->display_name_).collate_key();
    //this->collate_icase_key_ = Glib::ustring(this->display_name_).casefold_collate_key();
}

const std::filesystem::path&
VFSFileInfo::path() const noexcept
{
    return this->path_;
}

const std::string_view
VFSFileInfo::collate_key() const noexcept
{
    return this->collate_key_;
}

const std::string_view
VFSFileInfo::collate_icase_key() const noexcept
{
    return this->collate_icase_key_;
}

u64
VFSFileInfo::size() const noexcept
{
    return this->file_stat_.size();
}

u64
VFSFileInfo::size_on_disk() const noexcept
{
    return this->file_stat_.size_on_disk();
}

const std::string_view
VFSFileInfo::display_size() const noexcept
{
    return this->display_size_;
}

const std::string_view
VFSFileInfo::display_size_in_bytes() const noexcept
{
    return this->display_size_bytes_;
}

const std::string_view
VFSFileInfo::display_size_on_disk() const noexcept
{
    return this->display_disk_size_;
}

u64
VFSFileInfo::blocks() const noexcept
{
    return this->file_stat_.blocks();
}

vfs::mime_type
VFSFileInfo::mime_type() const noexcept
{
    return this->mime_type_;
}

void
VFSFileInfo::reload_mime_type(const std::filesystem::path& full_path) noexcept
{
    // In current implementation, only st_mode is used in
    // mime-type detection, so let's save some CPU cycles
    // and do not copy unused fields.

    this->mime_type_ = vfs_mime_type_get_from_file(full_path);
    this->load_special_info(full_path);
}

/* Icons */
#define ICON_FOLDER              "folder-symbolic"
#define ICON_FOLDER_DOCUMENTS    "folder-documents-symbolic"
#define ICON_FOLDER_DOWNLOAD     "folder-download-symbolic"
#define ICON_FOLDER_MUSIC        "folder-music-symbolic"
#define ICON_FOLDER_PICTURES     "folder-pictures-symbolic"
#define ICON_FOLDER_PUBLIC_SHARE "folder-publicshare-symbolic"
#define ICON_FOLDER_TEMPLATES    "folder-templates-symbolic"
#define ICON_FOLDER_VIDEOS       "folder-videos-symbolic"
#define ICON_FOLDER_HOME         "user-home-symbolic"
#define ICON_FOLDER_DESKTOP      "user-desktop-symbolic"

/* Fullcolor icons */
#define ICON_FULLCOLOR_FOLDER              "folder"
#define ICON_FULLCOLOR_FOLDER_DOCUMENTS    "folder-documents"
#define ICON_FULLCOLOR_FOLDER_DOWNLOAD     "folder-download"
#define ICON_FULLCOLOR_FOLDER_MUSIC        "folder-music"
#define ICON_FULLCOLOR_FOLDER_PICTURES     "folder-pictures"
#define ICON_FULLCOLOR_FOLDER_PUBLIC_SHARE "folder-publicshare"
#define ICON_FULLCOLOR_FOLDER_TEMPLATES    "folder-templates"
#define ICON_FULLCOLOR_FOLDER_VIDEOS       "folder-videos"
#define ICON_FULLCOLOR_FOLDER_HOME         "user-home"
#define ICON_FULLCOLOR_FOLDER_DESKTOP      "user-desktop"

const std::string
VFSFileInfo::special_directory_get_icon_name() const noexcept
{
    const bool symbolic = this->is_symlink();

    if (vfs::user_dirs->home_dir() == this->path_)
    {
        return (symbolic) ? ICON_FOLDER_HOME : ICON_FULLCOLOR_FOLDER_HOME;
    }
    else if (vfs::user_dirs->desktop_dir() == this->path_)
    {
        return (symbolic) ? ICON_FOLDER_DESKTOP : ICON_FULLCOLOR_FOLDER_DESKTOP;
    }
    else if (vfs::user_dirs->documents_dir() == this->path_)
    {
        return (symbolic) ? ICON_FOLDER_DOCUMENTS : ICON_FULLCOLOR_FOLDER_DOCUMENTS;
    }
    else if (vfs::user_dirs->download_dir() == this->path_)
    {
        return (symbolic) ? ICON_FOLDER_DOWNLOAD : ICON_FULLCOLOR_FOLDER_DOWNLOAD;
    }
    else if (vfs::user_dirs->music_dir() == this->path_)
    {
        return (symbolic) ? ICON_FOLDER_MUSIC : ICON_FULLCOLOR_FOLDER_MUSIC;
    }
    else if (vfs::user_dirs->pictures_dir() == this->path_)
    {
        return (symbolic) ? ICON_FOLDER_PICTURES : ICON_FULLCOLOR_FOLDER_PICTURES;
    }
    else if (vfs::user_dirs->public_share_dir() == this->path_)
    {
        return (symbolic) ? ICON_FOLDER_PUBLIC_SHARE : ICON_FULLCOLOR_FOLDER_PUBLIC_SHARE;
    }
    else if (vfs::user_dirs->template_dir() == this->path_)
    {
        return (symbolic) ? ICON_FOLDER_TEMPLATES : ICON_FULLCOLOR_FOLDER_TEMPLATES;
    }
    else if (vfs::user_dirs->videos_dir() == this->path_)
    {
        return (symbolic) ? ICON_FOLDER_VIDEOS : ICON_FULLCOLOR_FOLDER_VIDEOS;
    }
    else
    {
        return (symbolic) ? ICON_FOLDER : ICON_FULLCOLOR_FOLDER;
    }
}

GdkPixbuf*
VFSFileInfo::big_icon() noexcept
{
    if (this->flags() & vfs::file_info_flags::desktop_entry && this->big_thumbnail_)
    {
        return g_object_ref(this->big_thumbnail_);
    }

    if (this->is_directory())
    {
        const auto icon_name = this->special_directory_get_icon_name();
        return vfs_load_icon(icon_name, app_settings.icon_size_big());
    }

    if (!this->mime_type_)
    {
        return nullptr;
    }
    return this->mime_type_->icon(true);
}

GdkPixbuf*
VFSFileInfo::small_icon() noexcept
{
    if (this->flags() & vfs::file_info_flags::desktop_entry && this->small_thumbnail_)
    {
        return g_object_ref(this->small_thumbnail_);
    }

    if (this->is_directory())
    {
        const auto icon_name = this->special_directory_get_icon_name();
        return vfs_load_icon(icon_name, app_settings.icon_size_small());
    }

    if (!this->mime_type_)
    {
        return nullptr;
    }
    return this->mime_type_->icon(false);
}

GdkPixbuf*
VFSFileInfo::big_thumbnail() const noexcept
{
    return this->big_thumbnail_ ? g_object_ref(this->big_thumbnail_) : nullptr;
}

GdkPixbuf*
VFSFileInfo::small_thumbnail() const noexcept
{
    return this->small_thumbnail_ ? g_object_ref(this->small_thumbnail_) : nullptr;
}

void
VFSFileInfo::unload_big_thumbnail() noexcept
{
    if (this->big_thumbnail_)
    {
        g_object_unref(this->big_thumbnail_);
        this->big_thumbnail_ = nullptr;
    }
}

void
VFSFileInfo::unload_small_thumbnail() noexcept
{
    if (this->small_thumbnail_)
    {
        g_object_unref(this->small_thumbnail_);
        this->small_thumbnail_ = nullptr;
    }
}

const std::string_view
VFSFileInfo::display_owner() const noexcept
{
    return this->display_owner_;
}

const std::string_view
VFSFileInfo::display_group() const noexcept
{
    return this->display_group_;
}

const std::string_view
VFSFileInfo::display_atime() const noexcept
{
    return this->display_atime_;
}

const std::string_view
VFSFileInfo::display_btime() const noexcept
{
    return this->display_btime_;
}

const std::string_view
VFSFileInfo::display_ctime() const noexcept
{
    return this->display_ctime_;
}

const std::string_view
VFSFileInfo::display_mtime() const noexcept
{
    return this->display_mtime_;
}

std::time_t
VFSFileInfo::atime() const noexcept
{
    return this->file_stat_.atime().tv_sec;
}

std::time_t
VFSFileInfo::btime() const noexcept
{
    return this->file_stat_.btime().tv_sec;
}

std::time_t
VFSFileInfo::ctime() const noexcept
{
    return this->file_stat_.ctime().tv_sec;
}

std::time_t
VFSFileInfo::mtime() const noexcept
{
    return this->file_stat_.mtime().tv_sec;
}

vfs::file_info_flags
VFSFileInfo::flags() const noexcept
{
    return this->flags_;
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
    {
        perm[file_type] = '-';
    }
    else if (std::filesystem::is_directory(status))
    {
        perm[file_type] = 'd';
    }
    else if (std::filesystem::is_symlink(status))
    {
        perm[file_type] = 'l';
    }
    else if (std::filesystem::is_character_file(status))
    {
        perm[file_type] = 'c';
    }
    else if (std::filesystem::is_block_file(status))
    {
        perm[file_type] = 'b';
    }
    else if (std::filesystem::is_fifo(status))
    {
        perm[file_type] = 'p';
    }
    else if (std::filesystem::is_socket(status))
    {
        perm[file_type] = 's';
    }

    const std::filesystem::perms p = status.permissions();

    // Owner
    if ((p & std::filesystem::perms::owner_read) != std::filesystem::perms::none)
    {
        perm[owner_read] = 'r';
    }

    if ((p & std::filesystem::perms::owner_write) != std::filesystem::perms::none)
    {
        perm[owner_write] = 'w';
    }

    if ((p & std::filesystem::perms::set_uid) != std::filesystem::perms::none)
    {
        if ((p & std::filesystem::perms::owner_exec) != std::filesystem::perms::none)
        {
            perm[owner_exec] = 's';
        }
        else
        {
            perm[owner_exec] = 'S';
        }
    }
    else
    {
        if ((p & std::filesystem::perms::owner_exec) != std::filesystem::perms::none)
        {
            perm[owner_exec] = 'x';
        }
    }

    // Group
    if ((p & std::filesystem::perms::group_read) != std::filesystem::perms::none)
    {
        perm[group_read] = 'r';
    }

    if ((p & std::filesystem::perms::group_write) != std::filesystem::perms::none)
    {
        perm[group_write] = 'w';
    }

    if ((p & std::filesystem::perms::set_gid) != std::filesystem::perms::none)
    {
        if ((p & std::filesystem::perms::group_exec) != std::filesystem::perms::none)
        {
            perm[group_exec] = 's';
        }
        else
        {
            perm[group_exec] = 'S';
        }
    }
    else
    {
        if ((p & std::filesystem::perms::group_exec) != std::filesystem::perms::none)
        {
            perm[group_exec] = 'x';
        }
    }

    // Other
    if ((p & std::filesystem::perms::others_read) != std::filesystem::perms::none)
    {
        perm[other_read] = 'r';
    }

    if ((p & std::filesystem::perms::others_write) != std::filesystem::perms::none)
    {
        perm[other_write] = 'w';
    }

    if ((p & std::filesystem::perms::sticky_bit) != std::filesystem::perms::none)
    {
        if ((p & std::filesystem::perms::others_exec) != std::filesystem::perms::none)
        {
            perm[other_exec] = 't';
        }
        else
        {
            perm[other_exec] = 'T';
        }
    }
    else
    {
        if ((p & std::filesystem::perms::others_exec) != std::filesystem::perms::none)
        {
            perm[other_exec] = 'x';
        }
    }

    return perm;
}

const std::string_view
VFSFileInfo::display_permissions() noexcept
{
    if (this->display_perm_.empty())
    {
        this->display_perm_ = get_file_perm_string(this->status_);
    }
    return this->display_perm_;
}

bool
VFSFileInfo::is_directory() const noexcept
{
    // return std::filesystem::is_directory(this->status);

    if (std::filesystem::is_symlink(this->status_))
    {
        std::error_code ec;
        const auto symlink_path = std::filesystem::read_symlink(this->path_, ec);
        if (!ec)
        {
            return std::filesystem::is_directory(symlink_path);
        }
    }
    return std::filesystem::is_directory(this->status_);
}

bool
VFSFileInfo::is_regular_file() const noexcept
{
    return std::filesystem::is_regular_file(this->status_);
}

bool
VFSFileInfo::is_symlink() const noexcept
{
    return std::filesystem::is_symlink(this->status_);
}

bool
VFSFileInfo::is_socket() const noexcept
{
    return std::filesystem::is_socket(this->status_);
}

bool
VFSFileInfo::is_fifo() const noexcept
{
    return std::filesystem::is_fifo(this->status_);
}

bool
VFSFileInfo::is_block_file() const noexcept
{
    return std::filesystem::is_block_file(this->status_);
}

bool
VFSFileInfo::is_character_file() const noexcept
{
    return std::filesystem::is_character_file(this->status_);
}

bool
VFSFileInfo::is_other() const noexcept
{
    return (!this->is_directory() && !this->is_regular_file() && !this->is_symlink());
}

bool
VFSFileInfo::is_hidden() const noexcept
{
    return this->is_hidden_;
}

bool
VFSFileInfo::is_image() const noexcept
{
    // FIXME: We had better use functions of xdg_mime to check this
    return ztd::startswith(this->mime_type_->type(), "image/");
}

bool
VFSFileInfo::is_video() const noexcept
{
    // FIXME: We had better use functions of xdg_mime to check this
    return ztd::startswith(this->mime_type_->type(), "video/");
}

bool
VFSFileInfo::is_desktop_entry() const noexcept
{
    return 0 != (this->flags() & vfs::file_info_flags::desktop_entry);
}

bool
VFSFileInfo::is_unknown_type() const noexcept
{
    return ztd::same(this->mime_type_->type(), XDG_MIME_TYPE_UNKNOWN);
}

// full path of the file is required by this function
bool
VFSFileInfo::is_executable() const noexcept
{
    return mime_type_is_executable_file(this->path(), this->mime_type_->type());
}

// full path of the file is required by this function
bool
VFSFileInfo::is_text() const noexcept
{
    return mime_type_is_text_file(this->path(), this->mime_type_->type());
}

bool
VFSFileInfo::is_compressed() const noexcept
{
    return this->file_stat_.is_compressed();
}

bool
VFSFileInfo::is_immutable() const noexcept
{
    return this->file_stat_.is_immutable();
}

bool
VFSFileInfo::is_append() const noexcept
{
    return this->file_stat_.is_append();
}

bool
VFSFileInfo::is_nodump() const noexcept
{
    return this->file_stat_.is_nodump();
}

bool
VFSFileInfo::is_encrypted() const noexcept
{
    return this->file_stat_.is_encrypted();
}

bool
VFSFileInfo::is_verity() const noexcept
{
    return this->file_stat_.is_verity();
}

bool
VFSFileInfo::is_dax() const noexcept
{
    return this->file_stat_.is_dax();
}

std::filesystem::perms
VFSFileInfo::permissions() const noexcept
{
    return this->status_.permissions();
}

bool
VFSFileInfo::is_thumbnail_loaded(bool big) const noexcept
{
    if (big)
    {
        return (this->big_thumbnail_ != nullptr);
    }
    return (this->small_thumbnail_ != nullptr);
}

void
VFSFileInfo::load_thumbnail(const std::filesystem::path& full_path, bool big) noexcept
{
    if (big)
    {
        load_thumbnail_big(full_path);
    }
    else
    {
        load_thumbnail_small(full_path);
    }
}

void
VFSFileInfo::load_thumbnail_small(const std::filesystem::path& full_path) noexcept
{
    if (this->small_thumbnail_)
    {
        return;
    }

    std::error_code ec;
    const bool exists = std::filesystem::exists(full_path, ec);
    if (ec || !exists)
    {
        return;
    }

    GdkPixbuf* thumbnail = vfs_thumbnail_load_for_file(full_path, app_settings.icon_size_small());
    if (thumbnail)
    {
        this->small_thumbnail_ = thumbnail;
    }
    else // fallback to mime_type icon
    {
        this->small_thumbnail_ = this->small_icon();
    }
}

void
VFSFileInfo::load_thumbnail_big(const std::filesystem::path& full_path) noexcept
{
    if (this->big_thumbnail_)
    {
        return;
    }

    std::error_code ec;
    const bool exists = std::filesystem::exists(full_path, ec);
    if (ec || !exists)
    {
        return;
    }

    GdkPixbuf* thumbnail = vfs_thumbnail_load_for_file(full_path, app_settings.icon_size_big());
    if (thumbnail)
    {
        this->big_thumbnail_ = thumbnail;
    }
    else // fallback to mime_type icon
    {
        this->big_thumbnail_ = this->big_icon();
    }
}

void
VFSFileInfo::load_special_info(const std::filesystem::path& file_path) noexcept
{
    if (!ztd::endswith(this->name_, ".desktop"))
    {
        return;
    }

    const auto file_dir = file_path.parent_path();

    this->flags_ = (vfs::file_info_flags)(this->flags_ | vfs::file_info_flags::desktop_entry);
    const vfs::desktop desktop = vfs_get_desktop(file_path);

    // MOD  display real filenames of .desktop files not in desktop directory
    if (std::filesystem::equivalent(file_dir, vfs::user_dirs->desktop_dir()))
    {
        this->update_display_name(desktop->display_name());
    }

    if (desktop->icon_name().empty())
    {
        return;
    }

    const i32 big_size = app_settings.icon_size_big();
    const i32 small_size = app_settings.icon_size_small();
    if (this->big_thumbnail_ == nullptr)
    {
        GdkPixbuf* icon = desktop->icon(big_size);
        if (icon)
        {
            this->big_thumbnail_ = icon;
        }
    }
    if (this->small_thumbnail_ == nullptr)
    {
        GdkPixbuf* icon = desktop->icon(small_size);
        if (icon)
        {
            this->small_thumbnail_ = icon;
        }
    }
}

void
vfs_file_info_list_free(const std::span<const vfs::file_info> list)
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
