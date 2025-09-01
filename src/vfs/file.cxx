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

#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "vfs/file.hxx"
#include "vfs/mime-type.hxx"
#include "vfs/settings.hxx"
#include "vfs/user-dirs.hxx"

#include "vfs/thumbnails/thumbnails.hxx"
#include "vfs/utils/icon.hxx"
#include "vfs/utils/utils.hxx"

#include "logger.hxx"

std::shared_ptr<vfs::file>
vfs::file::create(const std::filesystem::path& path,
                  const std::shared_ptr<vfs::settings>& settings) noexcept
{
    return std::make_shared<vfs::file>(path, settings);
}

vfs::file::file(const std::filesystem::path& path,
                const std::shared_ptr<vfs::settings>& settings) noexcept
    : path_(path), settings_(settings)
{
    // logger::debug<logger::domain::vfs>("vfs::file::file({})    {}", logger::utils::ptr(this), this->path_);
    this->uri_ = Glib::filename_to_uri(this->path_.string());

    if (this->path_ == "/")
    {
        // special case, using std::filesystem::path::filename() on the root
        // directory returns an empty string. that causes subtle bugs
        // so hard code "/" as the value for root.
        this->name_ = "/";
    }
    else
    {
        this->name_ = this->path_.filename();
    }

    // Is a hidden file
    this->is_hidden_ = this->name_.starts_with('.');

    const auto result = this->update();

    logger::error_if<logger::domain::vfs>(!result,
                                          "Failed to create vfs::file for {}",
                                          path.string());
}

vfs::file::~file() noexcept
{
// logger::debug<logger::domain::vfs>("vfs::file::~file({})   {}", logger::utils::ptr(this), this->path_);
#if (GTK_MAJOR_VERSION == 3)
    if (this->thumbnail_.big)
    {
        g_object_unref(this->thumbnail_.big);
    }
    if (this->thumbnail_.small)
    {
        g_object_unref(this->thumbnail_.small);
    }
#endif
}

bool
vfs::file::update() noexcept
{
    const auto stat = ztd::statx::create(this->path_, ztd::statx::symlink::no_follow);
    if (!stat)
    {
        this->mime_type_ =
            vfs::mime_type::create_from_type(vfs::constants::mime_type::unknown, this->settings_);
        return false;
    }
    this->stat_ = stat.value();

    // logger::debug<logger::domain::vfs>("vfs::file::update({})    {}  size={}", logger::utils::ptr(this), this->name, this->file_stat.size());

    this->mime_type_ = vfs::mime_type::create_from_file(this->path_, this->settings_);

    // file size formated
    this->display_size_ = vfs::utils::format_file_size(this->size());
    this->display_size_bytes_ = std::format("{:L}", this->size());

    // disk file size formated
    this->display_disk_size_ = vfs::utils::format_file_size(this->size_on_disk());

    // owner
    const auto pw = ztd::passwd::create(this->stat_.uid().data());
    if (pw)
    {
        this->display_owner_ = pw->name();
    }

    // group
    const auto gr = ztd::group::create(this->stat_.gid().data());
    if (gr)
    {
        this->display_group_ = gr->name();
    }

    // time
    this->display_atime_ =
        std::format("{}", std::chrono::floor<std::chrono::seconds>(this->atime()));
    this->display_btime_ =
        std::format("{}", std::chrono::floor<std::chrono::seconds>(this->btime()));
    this->display_ctime_ =
        std::format("{}", std::chrono::floor<std::chrono::seconds>(this->ctime()));
    this->display_mtime_ =
        std::format("{}", std::chrono::floor<std::chrono::seconds>(this->mtime()));

    // Cause file prem string to be regenerated as needed
    this->display_perm_.clear();

    return true;
}

std::string_view
vfs::file::name() const noexcept
{
    return this->name_;
}

const std::filesystem::path&
vfs::file::path() const noexcept
{
    return this->path_;
}

std::string_view
vfs::file::uri() const noexcept
{
    return this->uri_;
}

u64
vfs::file::size() const noexcept
{
    return this->stat_.size();
}

u64
vfs::file::size_on_disk() const noexcept
{
    return this->stat_.size_on_disk();
}

std::string_view
vfs::file::display_size() const noexcept
{
    return this->display_size_;
}

std::string_view
vfs::file::display_size_in_bytes() const noexcept
{
    return this->display_size_bytes_;
}

std::string_view
vfs::file::display_size_on_disk() const noexcept
{
    return this->display_disk_size_;
}

u64
vfs::file::blocks() const noexcept
{
    return this->stat_.blocks();
}

const std::shared_ptr<vfs::mime_type>&
vfs::file::mime_type() const noexcept
{
    return this->mime_type_;
}

std::string_view
vfs::file::special_directory_get_icon_name(const bool symbolic) const noexcept
{
    if (vfs::user::home() == this->path_)
    {
        return symbolic ? "user-home-symbolic" : "user-home";
    }
    else if (vfs::user::desktop() == this->path_)
    {
        return symbolic ? "user-desktop-symbolic" : "user-desktop";
    }
    else if (vfs::user::documents() == this->path_)
    {
        return symbolic ? "folder-documents-symbolic" : "folder-documents";
    }
    else if (vfs::user::download() == this->path_)
    {
        return symbolic ? "folder-download-symbolic" : "folder-download";
    }
    else if (vfs::user::music() == this->path_)
    {
        return symbolic ? "folder-music-symbolic" : "folder-music";
    }
    else if (vfs::user::pictures() == this->path_)
    {
        return symbolic ? "folder-pictures-symbolic" : "folder-pictures";
    }
    else if (vfs::user::public_share() == this->path_)
    {
        return symbolic ? "folder-publicshare-symbolic" : "folder-publicshare";
    }
    else if (vfs::user::templates() == this->path_)
    {
        return symbolic ? "folder-templates-symbolic" : "folder-templates";
    }
    else if (vfs::user::videos() == this->path_)
    {
        return symbolic ? "folder-videos-symbolic" : "folder-videos";
    }
    else
    {
        return symbolic ? "folder-symbolic" : "folder";
    }
}

#if (GTK_MAJOR_VERSION == 4)

Glib::RefPtr<Gtk::IconPaintable>
vfs::file::icon(const thumbnail_size size) noexcept
{
    ztd::panic_if(this->settings_ == nullptr, "Function disabled");

    if (size == thumbnail_size::big)
    {
        if (this->is_directory())
        {
            return vfs::utils::load_icon(this->special_directory_get_icon_name(),
                                         this->settings_->icon_size_big)
                .value_or(nullptr);
        }

        return this->mime_type_->icon(true);
    }
    else
    {
        if (this->is_directory())
        {
            return vfs::utils::load_icon(this->special_directory_get_icon_name(),
                                         this->settings_->icon_size_small)
                .value_or(nullptr);
        }

        return this->mime_type_->icon(false);
    }
}

Glib::RefPtr<Gdk::Texture>
vfs::file::thumbnail(const thumbnail_size size) const noexcept
{
    if (size == thumbnail_size::big)
    {
        return this->thumbnail_.big;
    }
    else
    {
        return this->thumbnail_.small;
    }
}

#elif (GTK_MAJOR_VERSION == 3)

GdkPixbuf*
vfs::file::icon(const thumbnail_size size) noexcept
{
    ztd::panic_if(this->settings_ == nullptr, "Function disabled");

    if (size == thumbnail_size::big)
    {
        if (this->is_desktop_entry() && this->thumbnail_.big)
        {
            return g_object_ref(this->thumbnail_.big);
        }

        if (this->is_directory())
        {
            const auto icon_name = this->special_directory_get_icon_name();
            return vfs::utils::load_icon(icon_name, this->settings_->icon_size_big);
        }

        if (!this->mime_type_)
        {
            return nullptr;
        }
        return this->mime_type_->icon(true);
    }
    else
    {
        if (this->is_desktop_entry() && this->thumbnail_.small)
        {
            return g_object_ref(this->thumbnail_.small);
        }

        if (this->is_directory())
        {
            const auto icon_name = this->special_directory_get_icon_name();
            return vfs::utils::load_icon(icon_name, this->settings_->icon_size_small);
        }

        if (!this->mime_type_)
        {
            return nullptr;
        }
        return this->mime_type_->icon(false);
    }
}

GdkPixbuf*
vfs::file::thumbnail(const thumbnail_size size) const noexcept
{
    if (size == thumbnail_size::big)
    {
        return this->thumbnail_.big ? g_object_ref(this->thumbnail_.big) : nullptr;
    }
    else
    {
        return this->thumbnail_.small ? g_object_ref(this->thumbnail_.small) : nullptr;
    }
}

#endif

void
vfs::file::unload_thumbnail(const thumbnail_size size) noexcept
{
    if (size == thumbnail_size::big)
    {
#if (GTK_MAJOR_VERSION == 4)
        this->thumbnail_.big = nullptr;
#elif (GTK_MAJOR_VERSION == 3)
        if (this->thumbnail_.big)
        {
            g_object_unref(this->thumbnail_.big);
            this->thumbnail_.big = nullptr;
        }
#endif
    }
    else
    {
#if (GTK_MAJOR_VERSION == 4)
        this->thumbnail_.small = nullptr;
#elif (GTK_MAJOR_VERSION == 3)
        if (this->thumbnail_.small)
        {
            g_object_unref(this->thumbnail_.small);
            this->thumbnail_.small = nullptr;
        }
#endif
    }
}

std::string_view
vfs::file::display_owner() const noexcept
{
    return this->display_owner_;
}

std::string_view
vfs::file::display_group() const noexcept
{
    return this->display_group_;
}

std::string_view
vfs::file::display_atime() const noexcept
{
    return this->display_atime_;
}

std::string_view
vfs::file::display_btime() const noexcept
{
    return this->display_btime_;
}

std::string_view
vfs::file::display_ctime() const noexcept
{
    return this->display_ctime_;
}

std::string_view
vfs::file::display_mtime() const noexcept
{
    return this->display_mtime_;
}

std::chrono::system_clock::time_point
vfs::file::atime() const noexcept
{
    return this->stat_.atime();
}

std::chrono::system_clock::time_point
vfs::file::btime() const noexcept
{
    return this->stat_.btime();
}

std::chrono::system_clock::time_point
vfs::file::ctime() const noexcept
{
    return this->stat_.ctime();
}

std::chrono::system_clock::time_point
vfs::file::mtime() const noexcept
{
    return this->stat_.mtime();
}

std::string
vfs::file::create_file_perm_string() const noexcept
{
    static constexpr std::uint8_t file_type{0};

    static constexpr std::uint8_t owner_read{1};
    static constexpr std::uint8_t owner_write{2};
    static constexpr std::uint8_t owner_exec{3};

    static constexpr std::uint8_t group_read{4};
    static constexpr std::uint8_t group_write{5};
    static constexpr std::uint8_t group_exec{6};

    static constexpr std::uint8_t other_read{7};
    static constexpr std::uint8_t other_write{8};
    static constexpr std::uint8_t other_exec{9};

    // blank permissions
    std::string perm = "----------";

    // File Type Permissions
    if (this->is_regular_file())
    {
        perm[file_type] = '-';
    }
    else if (this->is_directory())
    {
        perm[file_type] = 'd';
    }
    else if (this->is_symlink())
    {
        perm[file_type] = 'l';
    }
    else if (this->is_character_file())
    {
        perm[file_type] = 'c';
    }
    else if (this->is_block_file())
    {
        perm[file_type] = 'b';
    }
    else if (this->is_fifo())
    {
        perm[file_type] = 'p';
    }
    else if (this->is_socket())
    {
        perm[file_type] = 's';
    }

    const auto p = std::filesystem::symlink_status(this->path_).permissions();

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

std::string_view
vfs::file::display_permissions() noexcept
{
    if (this->display_perm_.empty())
    {
        this->display_perm_ = this->create_file_perm_string();
    }
    return this->display_perm_;
}

bool
vfs::file::is_directory() const noexcept
{
    if (this->stat_.is_symlink())
    {
        std::error_code ec;
        const auto symlink_path = std::filesystem::read_symlink(this->path_, ec);
        if (!ec)
        {
            return std::filesystem::is_directory(symlink_path);
        }
    }
    return this->stat_.is_directory();
}

bool
vfs::file::is_regular_file() const noexcept
{
    return this->stat_.is_regular_file();
}

bool
vfs::file::is_symlink() const noexcept
{
    return this->stat_.is_symlink();
}

bool
vfs::file::is_socket() const noexcept
{
    return this->stat_.is_socket();
}

bool
vfs::file::is_fifo() const noexcept
{
    return this->stat_.is_fifo();
}

bool
vfs::file::is_block_file() const noexcept
{
    return this->stat_.is_block_file();
}

bool
vfs::file::is_character_file() const noexcept
{
    return this->stat_.is_character_file();
}

bool
vfs::file::is_other() const noexcept
{
    return (!this->is_directory() && !this->is_regular_file() && !this->is_symlink());
}

bool
vfs::file::is_hidden() const noexcept
{
    return this->is_hidden_;
}

bool
vfs::file::is_desktop_entry() const noexcept
{
    return this->is_special_desktop_entry_;
}

bool
vfs::file::is_compressed() const noexcept
{
    return this->stat_.is_compressed();
}

bool
vfs::file::is_immutable() const noexcept
{
    return this->stat_.is_immutable();
}

bool
vfs::file::is_append() const noexcept
{
    return this->stat_.is_append();
}

bool
vfs::file::is_nodump() const noexcept
{
    return this->stat_.is_nodump();
}

bool
vfs::file::is_encrypted() const noexcept
{
    return this->stat_.is_encrypted();
}

bool
vfs::file::is_automount() const noexcept
{
    return this->stat_.is_automount();
}

bool
vfs::file::is_mount_root() const noexcept
{
    return this->stat_.is_mount_root();
}

bool
vfs::file::is_verity() const noexcept
{
    return this->stat_.is_verity();
}

bool
vfs::file::is_dax() const noexcept
{
    return this->stat_.is_dax();
}

bool
vfs::file::is_thumbnail_loaded(const thumbnail_size size) const noexcept
{
    if (size == thumbnail_size::big)
    {
        return (this->thumbnail_.big != nullptr);
    }
    else
    {
        return (this->thumbnail_.small != nullptr);
    }
}

void
vfs::file::load_thumbnail(const thumbnail_size size) noexcept
{
    ztd::panic_if(this->settings_ == nullptr, "Function disabled");

    static const auto thumbnail_cache = vfs::user::thumbnail_cache();
    if (this->path_.string().starts_with(thumbnail_cache.parent.string()))
    {
        // TODO use cache images directly
        logger::debug<logger::domain::vfs>("Not generating thumbnails in cache path: {}",
                                           this->path_.string());
        return;
    }

#if (GTK_MAJOR_VERSION == 4)
    // TODO do something better
    auto icon_to_pixbuf = [](const Glib::RefPtr<Gtk::IconPaintable>& icon)
    {
        return Gdk::Texture::create_for_pixbuf(
            Gdk::Pixbuf::create_from_file(icon->get_file()->get_path(),
                                          icon->get_intrinsic_width(),
                                          icon->get_intrinsic_height()));
    };

    if (size == thumbnail_size::big)
    {
        if (this->thumbnail_.big)
        {
            return;
        }

        std::error_code ec;
        const bool exists = std::filesystem::exists(this->path_, ec);
        if (ec || !exists)
        {
            return;
        }

        Glib::RefPtr<Gdk::Texture> thumbnail = nullptr;
        if (this->mime_type_->is_image())
        {
            thumbnail = vfs::detail::thumbnail::image(this->shared_from_this(),
                                                      this->settings_->icon_size_big);
        }
        else if (this->mime_type_->is_video())
        {
            thumbnail = vfs::detail::thumbnail::video(this->shared_from_this(),
                                                      this->settings_->icon_size_big);
        }

        if (thumbnail)
        {
            this->thumbnail_.big = thumbnail;
        }
        else
        {
            // fallback to mime_type icon
            this->thumbnail_.big = icon_to_pixbuf(this->icon(thumbnail_size::big));
        }
    }
    else
    {
        if (this->thumbnail_.small)
        {
            return;
        }

        std::error_code ec;
        const bool exists = std::filesystem::exists(this->path_, ec);
        if (ec || !exists)
        {
            return;
        }

        Glib::RefPtr<Gdk::Texture> thumbnail = nullptr;
        if (this->mime_type_->is_image())
        {
            thumbnail = vfs::detail::thumbnail::image(this->shared_from_this(),
                                                      this->settings_->icon_size_small);
        }
        else if (this->mime_type_->is_video())
        {
            thumbnail = vfs::detail::thumbnail::video(this->shared_from_this(),
                                                      this->settings_->icon_size_small);
        }

        if (thumbnail)
        {
            this->thumbnail_.small = thumbnail;
        }
        else
        {
            // fallback to mime_type icon
            this->thumbnail_.small = icon_to_pixbuf(this->icon(thumbnail_size::small));
        }
    }

#elif (GTK_MAJOR_VERSION == 3)

    if (size == thumbnail_size::big)
    {
        if (this->thumbnail_.big)
        {
            return;
        }

        std::error_code ec;
        const bool exists = std::filesystem::exists(this->path_, ec);
        if (ec || !exists)
        {
            return;
        }

        GdkPixbuf* thumbnail = nullptr;
        if (this->mime_type_->is_image())
        {
            thumbnail = vfs::detail::thumbnail::image(this->shared_from_this(),
                                                      this->settings_->icon_size_big);
        }
        else if (this->mime_type_->is_video())
        {
            thumbnail = vfs::detail::thumbnail::video(this->shared_from_this(),
                                                      this->settings_->icon_size_big);
        }

        if (thumbnail)
        {
            this->thumbnail_.big = thumbnail;
        }
        else
        {
            // fallback to mime_type icon
            this->thumbnail_.big = this->icon(thumbnail_size::big);
        }
    }
    else
    {
        if (this->thumbnail_.small)
        {
            return;
        }

        std::error_code ec;
        const bool exists = std::filesystem::exists(this->path_, ec);
        if (ec || !exists)
        {
            return;
        }

        GdkPixbuf* thumbnail = nullptr;
        if (this->mime_type_->is_image())
        {
            thumbnail = vfs::detail::thumbnail::image(this->shared_from_this(),
                                                      this->settings_->icon_size_small);
        }
        else if (this->mime_type_->is_video())
        {
            thumbnail = vfs::detail::thumbnail::video(this->shared_from_this(),
                                                      this->settings_->icon_size_small);
        }

        if (thumbnail)
        {
            this->thumbnail_.small = thumbnail;
        }
        else
        {
            // fallback to mime_type icon
            this->thumbnail_.small = this->icon(thumbnail_size::small);
        }
    }
#endif
}
