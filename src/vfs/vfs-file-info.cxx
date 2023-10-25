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

#include <memory>

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

const std::shared_ptr<vfs::file_info>
vfs_file_info_new(const std::filesystem::path& path)
{
    return std::make_shared<vfs::file_info>(path);
}

vfs::file_info::file_info(const std::filesystem::path& path) : path_(path)
{
    // ztd::logger::debug("vfs::file_info::file_info({})    {}", fmt::ptr(this), this->path);
    this->uri_ = Glib::filename_to_uri(this->path_.string());
    this->update();
}

vfs::file_info::~file_info()
{
    // ztd::logger::debug("vfs::file_info::~file_info({})   {}", fmt::ptr(this), this->path);
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
vfs::file_info::update() noexcept
{
    if (std::filesystem::equivalent(this->path_, "/"))
    {
        // special case, using std::filesystem::path::filename() on the root
        // directory returns an empty string. that causes subtle bugs
        // so hard code "/" as the value for root.
        this->name_ = "/";
        this->display_name_ = "/";
    }
    else
    {
        this->name_ = this->path_.filename();
        this->display_name_ = this->path_.filename();
    }

    this->file_stat_ = ztd::statx(this->path_, ztd::statx::symlink::no_follow);
    if (!this->file_stat_)
    {
        this->mime_type_ = vfs_mime_type_get_from_type(XDG_MIME_TYPE_UNKNOWN);
        return false;
    }

    // ztd::logger::debug("vfs::file::update({})    {}  size={}", fmt::ptr(this), this->name, this->file_stat.size());

    // this->status = std::filesystem::status(file_path);
    this->status_ = std::filesystem::symlink_status(this->path_);

    this->mime_type_ = vfs_mime_type_get_from_file(this->path_);

    // file size formated
    this->display_size_ = vfs_file_size_format(this->size());
    this->display_size_bytes_ = std::format("{:L}", this->size());

    // disk file size formated
    this->display_disk_size_ = vfs_file_size_format(this->size_on_disk());

    // hidden
    this->is_hidden_ = this->name_.starts_with('.');

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

    this->load_special_info();

    return true;
}

const std::string_view
vfs::file_info::name() const noexcept
{
    return this->name_;
}

// Get displayed name encoded in UTF-8
const std::string_view
vfs::file_info::display_name() const noexcept
{
    return this->display_name_;
}

void
vfs::file_info::update_display_name(const std::string_view new_display_name) noexcept
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
vfs::file_info::path() const noexcept
{
    return this->path_;
}

const std::string_view
vfs::file_info::uri() const noexcept
{
    return this->uri_;
}

const std::string_view
vfs::file_info::collate_key() const noexcept
{
    return this->collate_key_;
}

const std::string_view
vfs::file_info::collate_icase_key() const noexcept
{
    return this->collate_icase_key_;
}

u64
vfs::file_info::size() const noexcept
{
    return this->file_stat_.size();
}

u64
vfs::file_info::size_on_disk() const noexcept
{
    return this->file_stat_.size_on_disk();
}

const std::string_view
vfs::file_info::display_size() const noexcept
{
    return this->display_size_;
}

const std::string_view
vfs::file_info::display_size_in_bytes() const noexcept
{
    return this->display_size_bytes_;
}

const std::string_view
vfs::file_info::display_size_on_disk() const noexcept
{
    return this->display_disk_size_;
}

u64
vfs::file_info::blocks() const noexcept
{
    return this->file_stat_.blocks();
}

vfs::mime_type
vfs::file_info::mime_type() const noexcept
{
    return this->mime_type_;
}

void
vfs::file_info::reload_mime_type() noexcept
{
    this->mime_type_ = vfs_mime_type_get_from_file(this->path_);
    this->load_special_info();
}

const std::string_view
vfs::file_info::special_directory_get_icon_name() const noexcept
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
vfs::file_info::big_icon() noexcept
{
    if (this->is_desktop_entry() && this->big_thumbnail_)
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
vfs::file_info::small_icon() noexcept
{
    if (this->is_desktop_entry() && this->small_thumbnail_)
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
vfs::file_info::big_thumbnail() const noexcept
{
    return this->big_thumbnail_ ? g_object_ref(this->big_thumbnail_) : nullptr;
}

GdkPixbuf*
vfs::file_info::small_thumbnail() const noexcept
{
    return this->small_thumbnail_ ? g_object_ref(this->small_thumbnail_) : nullptr;
}

void
vfs::file_info::unload_big_thumbnail() noexcept
{
    if (this->big_thumbnail_)
    {
        g_object_unref(this->big_thumbnail_);
        this->big_thumbnail_ = nullptr;
    }
}

void
vfs::file_info::unload_small_thumbnail() noexcept
{
    if (this->small_thumbnail_)
    {
        g_object_unref(this->small_thumbnail_);
        this->small_thumbnail_ = nullptr;
    }
}

const std::string_view
vfs::file_info::display_owner() const noexcept
{
    return this->display_owner_;
}

const std::string_view
vfs::file_info::display_group() const noexcept
{
    return this->display_group_;
}

const std::string_view
vfs::file_info::display_atime() const noexcept
{
    return this->display_atime_;
}

const std::string_view
vfs::file_info::display_btime() const noexcept
{
    return this->display_btime_;
}

const std::string_view
vfs::file_info::display_ctime() const noexcept
{
    return this->display_ctime_;
}

const std::string_view
vfs::file_info::display_mtime() const noexcept
{
    return this->display_mtime_;
}

std::time_t
vfs::file_info::atime() const noexcept
{
    return this->file_stat_.atime().tv_sec;
}

std::time_t
vfs::file_info::btime() const noexcept
{
    return this->file_stat_.btime().tv_sec;
}

std::time_t
vfs::file_info::ctime() const noexcept
{
    return this->file_stat_.ctime().tv_sec;
}

std::time_t
vfs::file_info::mtime() const noexcept
{
    return this->file_stat_.mtime().tv_sec;
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
vfs::file_info::display_permissions() noexcept
{
    if (this->display_perm_.empty())
    {
        this->display_perm_ = get_file_perm_string(this->status_);
    }
    return this->display_perm_;
}

bool
vfs::file_info::is_directory() const noexcept
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
vfs::file_info::is_regular_file() const noexcept
{
    return std::filesystem::is_regular_file(this->status_);
}

bool
vfs::file_info::is_symlink() const noexcept
{
    return std::filesystem::is_symlink(this->status_);
}

bool
vfs::file_info::is_socket() const noexcept
{
    return std::filesystem::is_socket(this->status_);
}

bool
vfs::file_info::is_fifo() const noexcept
{
    return std::filesystem::is_fifo(this->status_);
}

bool
vfs::file_info::is_block_file() const noexcept
{
    return std::filesystem::is_block_file(this->status_);
}

bool
vfs::file_info::is_character_file() const noexcept
{
    return std::filesystem::is_character_file(this->status_);
}

bool
vfs::file_info::is_other() const noexcept
{
    return (!this->is_directory() && !this->is_regular_file() && !this->is_symlink());
}

bool
vfs::file_info::is_hidden() const noexcept
{
    return this->is_hidden_;
}

bool
vfs::file_info::is_image() const noexcept
{
    // FIXME: We had better use functions of xdg_mime to check this
    return this->mime_type_->type().starts_with("image/");
}

bool
vfs::file_info::is_video() const noexcept
{
    // FIXME: We had better use functions of xdg_mime to check this
    return this->mime_type_->type().starts_with("video/");
}

bool
vfs::file_info::is_desktop_entry() const noexcept
{
    return this->is_special_desktop_entry_;
}

bool
vfs::file_info::is_unknown_type() const noexcept
{
    return ztd::same(this->mime_type_->type(), XDG_MIME_TYPE_UNKNOWN);
}

// full path of the file is required by this function
bool
vfs::file_info::is_executable() const noexcept
{
    return mime_type_is_executable_file(this->path(), this->mime_type_->type());
}

bool
vfs::file_info::is_text() const noexcept
{
    return mime_type_is_text_file(this->path(), this->mime_type_->type());
}

bool
vfs::file_info::is_archive() const noexcept
{
    return mime_type_is_archive_file(this->path(), this->mime_type_->type());
}

bool
vfs::file_info::is_compressed() const noexcept
{
    return this->file_stat_.is_compressed();
}

bool
vfs::file_info::is_immutable() const noexcept
{
    return this->file_stat_.is_immutable();
}

bool
vfs::file_info::is_append() const noexcept
{
    return this->file_stat_.is_append();
}

bool
vfs::file_info::is_nodump() const noexcept
{
    return this->file_stat_.is_nodump();
}

bool
vfs::file_info::is_encrypted() const noexcept
{
    return this->file_stat_.is_encrypted();
}

bool
vfs::file_info::is_verity() const noexcept
{
    return this->file_stat_.is_verity();
}

bool
vfs::file_info::is_dax() const noexcept
{
    return this->file_stat_.is_dax();
}

std::filesystem::perms
vfs::file_info::permissions() const noexcept
{
    return this->status_.permissions();
}

bool
vfs::file_info::is_thumbnail_loaded(bool big) const noexcept
{
    if (big)
    {
        return (this->big_thumbnail_ != nullptr);
    }
    return (this->small_thumbnail_ != nullptr);
}

void
vfs::file_info::load_thumbnail(bool big) noexcept
{
    if (big)
    {
        load_thumbnail_big();
    }
    else
    {
        load_thumbnail_small();
    }
}

void
vfs::file_info::load_thumbnail_small() noexcept
{
    if (this->small_thumbnail_)
    {
        return;
    }

    std::error_code ec;
    const bool exists = std::filesystem::exists(this->path_, ec);
    if (ec || !exists)
    {
        return;
    }

    if (this->is_image() || this->is_video())
    {
        GdkPixbuf* thumbnail =
            vfs_thumbnail_load(this->shared_from_this(), app_settings.icon_size_small());
        if (thumbnail)
        {
            this->big_thumbnail_ = thumbnail;
            return;
        }
    }

    // fallback to mime_type icon
    // ztd::logger::debug("mime={}", this->mime_type_->type());
    this->big_thumbnail_ = this->small_icon();
}

void
vfs::file_info::load_thumbnail_big() noexcept
{
    if (this->big_thumbnail_)
    {
        return;
    }

    std::error_code ec;
    const bool exists = std::filesystem::exists(this->path_, ec);
    if (ec || !exists)
    {
        return;
    }

    if (this->is_image() || this->is_video())
    {
        GdkPixbuf* thumbnail =
            vfs_thumbnail_load(this->shared_from_this(), app_settings.icon_size_big());
        if (thumbnail)
        {
            this->big_thumbnail_ = thumbnail;
            return;
        }
    }

    // fallback to mime_type icon
    // ztd::logger::debug("mime={}", this->mime_type_->type());
    this->big_thumbnail_ = this->big_icon();
}

void
vfs::file_info::load_special_info() noexcept
{
    if (!this->name_.ends_with(".desktop"))
    {
        return;
    }

    this->is_special_desktop_entry_ = true;
    const auto desktop = vfs::desktop::create(this->path_);

    // MOD  display real filenames of .desktop files not in desktop directory
    // if (std::filesystem::equivalent(this->path_.parent_path(), vfs::user_dirs->desktop_dir()))
    // {
    //     this->update_display_name(desktop->display_name());
    // }

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
