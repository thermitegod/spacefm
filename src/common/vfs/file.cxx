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
#include <utility>

#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "vfs/file.hxx"
#include "vfs/mime-type.hxx"
#if (GTK_MAJOR_VERSION == 3)
#include "vfs/settings.hxx"
#endif
#include "vfs/user-dirs.hxx"

#include "vfs/thumbnails/thumbnails.hxx"
#include "vfs/utils/icon.hxx"
#include "vfs/utils/permissions.hxx"
#include "vfs/utils/utils.hxx"

#include "logger.hxx"

#if (GTK_MAJOR_VERSION == 4)
std::shared_ptr<vfs::file>
vfs::file::create(const std::filesystem::path& path) noexcept
{
    struct hack : public vfs::file
    {
        hack(const std::filesystem::path& path) : file(path) {}
    };

    return std::make_shared<hack>(path);
}
#elif (GTK_MAJOR_VERSION == 3)
std::shared_ptr<vfs::file>
vfs::file::create(const std::filesystem::path& path,
                  const std::shared_ptr<vfs::settings>& settings) noexcept
{
    struct hack : public vfs::file
    {
        hack(const std::filesystem::path& path, const std::shared_ptr<vfs::settings>& settings)
            : file(path, settings)
        {
        }
    };

    return std::make_shared<hack>(path, settings);
}
#endif

#if (GTK_MAJOR_VERSION == 4)
vfs::file::file(const std::filesystem::path& path) noexcept : path_(path)
#elif (GTK_MAJOR_VERSION == 3)
vfs::file::file(const std::filesystem::path& path,
                const std::shared_ptr<vfs::settings>& settings) noexcept
    : path_(path), settings_(settings)
#endif
{
    // logger::debug<logger::vfs>("vfs::file::file({})    {}", logger::utils::ptr(this), path_);
    uri_ = Glib::filename_to_uri(path_.string());

    if (path_ == "/")
    {
        // special case, using std::filesystem::path::filename() on the root
        // directory returns an empty string. that causes subtle bugs
        // so hard code "/" as the value for root.
        name_ = "/";
    }
    else
    {
        name_ = path_.filename();
    }

    // Is a hidden file
    is_hidden_ = name_.starts_with('.');

    const auto result = update();

    logger::error_if<logger::vfs>(!result, "Failed to create vfs::file for {}", path.string());
}

vfs::file::~file() noexcept
{
// logger::debug<logger::vfs>("vfs::file::~file({})   {}", logger::utils::ptr(this), path_);
#if (GTK_MAJOR_VERSION == 3)
    if (thumbnail_.big)
    {
        g_object_unref(thumbnail_.big);
    }
    if (thumbnail_.small)
    {
        g_object_unref(thumbnail_.small);
    }
#endif
}

bool
vfs::file::update() noexcept
{
    const auto stat = ztd::statx::create(path_, ztd::statx::symlink::no_follow);
    if (!stat)
    {
#if (GTK_MAJOR_VERSION == 4)
        mime_type_ = vfs::mime_type::create_from_type(vfs::constants::mime_type::unknown);
#elif (GTK_MAJOR_VERSION == 3)
        mime_type_ =
            vfs::mime_type::create_from_type(vfs::constants::mime_type::unknown, settings_);
#endif
        return false;
    }
    stat_ = stat.value();

    // logger::debug<logger::vfs>("vfs::file::update({})    {}  size={}", logger::utils::ptr(this), name, file_stat.size());

#if (GTK_MAJOR_VERSION == 4)
    mime_type_ = vfs::mime_type::create_from_file(path_);
#elif (GTK_MAJOR_VERSION == 3)
    mime_type_ = vfs::mime_type::create_from_file(path_, settings_);
#endif

    // file size formated
    display_size_ = vfs::utils::format_file_size(size());
    display_size_bytes_ = std::format("{:L}", size());

    // disk file size formated
    display_disk_size_ = vfs::utils::format_file_size(size_on_disk());

    // owner
    const auto pw = ztd::passwd::create(stat_.uid().data());
    if (pw)
    {
        display_owner_ = pw->name();
    }

    // group
    const auto gr = ztd::group::create(stat_.gid().data());
    if (gr)
    {
        display_group_ = gr->name();
    }

    // time
    display_atime_ = std::format("{}", std::chrono::floor<std::chrono::seconds>(atime()));
    display_btime_ = std::format("{}", std::chrono::floor<std::chrono::seconds>(btime()));
    display_ctime_ = std::format("{}", std::chrono::floor<std::chrono::seconds>(ctime()));
    display_mtime_ = std::format("{}", std::chrono::floor<std::chrono::seconds>(mtime()));

    // Cause file prem string to be regenerated as needed
    display_perm_.clear();

    return true;
}

std::string_view
vfs::file::name() const noexcept
{
    return name_;
}

const std::filesystem::path&
vfs::file::path() const noexcept
{
    return path_;
}

std::string_view
vfs::file::uri() const noexcept
{
    return uri_;
}

u64
vfs::file::size() const noexcept
{
    return stat_.size();
}

u64
vfs::file::size_on_disk() const noexcept
{
    return stat_.size_on_disk();
}

std::string_view
vfs::file::display_size() const noexcept
{
    return display_size_;
}

std::string_view
vfs::file::display_size_in_bytes() const noexcept
{
    return display_size_bytes_;
}

std::string_view
vfs::file::display_size_on_disk() const noexcept
{
    return display_disk_size_;
}

u64
vfs::file::blocks() const noexcept
{
    return stat_.blocks();
}

const std::shared_ptr<vfs::mime_type>&
vfs::file::mime_type() const noexcept
{
    return mime_type_;
}

std::string_view
vfs::file::special_directory_get_icon_name(const bool symbolic) const noexcept
{
    if (vfs::user::home() == path_)
    {
        return symbolic ? "user-home-symbolic" : "user-home";
    }
    else if (vfs::user::desktop() == path_)
    {
        return symbolic ? "user-desktop-symbolic" : "user-desktop";
    }
    else if (vfs::user::documents() == path_)
    {
        return symbolic ? "folder-documents-symbolic" : "folder-documents";
    }
    else if (vfs::user::download() == path_)
    {
        return symbolic ? "folder-download-symbolic" : "folder-download";
    }
    else if (vfs::user::music() == path_)
    {
        return symbolic ? "folder-music-symbolic" : "folder-music";
    }
    else if (vfs::user::pictures() == path_)
    {
        return symbolic ? "folder-pictures-symbolic" : "folder-pictures";
    }
    else if (vfs::user::public_share() == path_)
    {
        return symbolic ? "folder-publicshare-symbolic" : "folder-publicshare";
    }
    else if (vfs::user::templates() == path_)
    {
        return symbolic ? "folder-templates-symbolic" : "folder-templates";
    }
    else if (vfs::user::videos() == path_)
    {
        return symbolic ? "folder-videos-symbolic" : "folder-videos";
    }
    else
    {
        return symbolic ? "folder-symbolic" : "folder";
    }
}

std::string_view
vfs::file::display_owner() const noexcept
{
    return display_owner_;
}

std::string_view
vfs::file::display_group() const noexcept
{
    return display_group_;
}

std::string_view
vfs::file::display_atime() const noexcept
{
    return display_atime_;
}

std::string_view
vfs::file::display_btime() const noexcept
{
    return display_btime_;
}

std::string_view
vfs::file::display_ctime() const noexcept
{
    return display_ctime_;
}

std::string_view
vfs::file::display_mtime() const noexcept
{
    return display_mtime_;
}

std::chrono::system_clock::time_point
vfs::file::atime() const noexcept
{
    return stat_.atime();
}

std::chrono::system_clock::time_point
vfs::file::btime() const noexcept
{
    return stat_.btime();
}

std::chrono::system_clock::time_point
vfs::file::ctime() const noexcept
{
    return stat_.ctime();
}

std::chrono::system_clock::time_point
vfs::file::mtime() const noexcept
{
    return stat_.mtime();
}

std::string_view
vfs::file::display_permissions() noexcept
{
    if (display_perm_.empty())
    {
        display_perm_ = stat_.perms_fancy();
    }
    return display_perm_;
}

bool
vfs::file::is_directory() const noexcept
{
    if (stat_.is_symlink())
    {
        std::error_code ec;
        const auto symlink_path = std::filesystem::read_symlink(path_, ec);
        if (!ec)
        {
            return std::filesystem::is_directory(symlink_path);
        }
    }
    return stat_.is_directory();
}

bool
vfs::file::is_regular_file() const noexcept
{
    return stat_.is_regular_file();
}

bool
vfs::file::is_symlink() const noexcept
{
    return stat_.is_symlink();
}

bool
vfs::file::is_socket() const noexcept
{
    return stat_.is_socket();
}

bool
vfs::file::is_fifo() const noexcept
{
    return stat_.is_fifo();
}

bool
vfs::file::is_block_file() const noexcept
{
    return stat_.is_block_file();
}

bool
vfs::file::is_character_file() const noexcept
{
    return stat_.is_character_file();
}

bool
vfs::file::is_other() const noexcept
{
    return (!is_directory() && !is_regular_file() && !is_symlink());
}

bool
vfs::file::is_readable() const noexcept
{
    return vfs::utils::has_read_permission(path_);
}

bool
vfs::file::is_writable() const noexcept
{
    return vfs::utils::has_write_permission(path_);
}

bool
vfs::file::is_executable() const noexcept
{
    return mime_type()->is_executable() && vfs::utils::has_execute_permission(path());
}

bool
vfs::file::is_hidden() const noexcept
{
    return is_hidden_;
}

bool
vfs::file::is_desktop_entry() const noexcept
{
    return is_special_desktop_entry_;
}

bool
vfs::file::is_compressed() const noexcept
{
    return stat_.is_compressed();
}

bool
vfs::file::is_immutable() const noexcept
{
    return stat_.is_immutable();
}

bool
vfs::file::is_append() const noexcept
{
    return stat_.is_append();
}

bool
vfs::file::is_nodump() const noexcept
{
    return stat_.is_nodump();
}

bool
vfs::file::is_encrypted() const noexcept
{
    return stat_.is_encrypted();
}

bool
vfs::file::is_automount() const noexcept
{
    return stat_.is_automount();
}

bool
vfs::file::is_mount_root() const noexcept
{
    return stat_.is_mount_root();
}

bool
vfs::file::is_verity() const noexcept
{
    return stat_.is_verity();
}

bool
vfs::file::is_dax() const noexcept
{
    return stat_.is_dax();
}

#if (GTK_MAJOR_VERSION == 4)

Glib::RefPtr<Gtk::IconPaintable>
vfs::file::icon(const std::int32_t size) const noexcept
{
    if (is_directory())
    {
        return vfs::utils::load_icon(special_directory_get_icon_name(), size);
    }
    return mime_type_->icon(size);
}

Glib::RefPtr<Gdk::Paintable>
vfs::file::thumbnail(const std::int32_t size) const noexcept
{
    auto thumbnail = thumbnail_.get(size);
    if (!thumbnail)
    {
        return icon(size);
    }
    return thumbnail;
}

void
vfs::file::load_thumbnail(const std::int32_t size, bool force_reload) noexcept
{
    static const auto thumbnail_cache = vfs::user::thumbnail_cache();
    if (path_.string().starts_with(thumbnail_cache.parent.string()))
    {
        // TODO use cache images directly
        logger::debug<logger::vfs>("Not generating thumbnails in cache path: {}", path_.string());
        return;
    }

    if (thumbnail_.is_loaded(size) && !force_reload)
    {
        return;
    }

    const auto raw = thumbnail_data::get_raw_size(size);

    Glib::RefPtr<Gdk::Texture> thumbnail;
    if (mime_type_->is_image())
    {
        thumbnail = vfs::detail::thumbnail::image(shared_from_this(), std::to_underlying(raw));
    }
    else if (mime_type_->is_video())
    {
        thumbnail = vfs::detail::thumbnail::video(shared_from_this(), std::to_underlying(raw));
    }

    if (thumbnail)
    {
        thumbnail_.set(raw, thumbnail);
    }
}

bool
vfs::file::is_thumbnail_loaded(const std::int32_t size) const noexcept
{
    return thumbnail_.is_loaded(size);
}

void
vfs::file::thumbnail_data::set(const raw_size size,
                               const Glib::RefPtr<Gdk::Texture>& texture) noexcept
{
    if (size == raw_size::normal)
    {
        normal = texture;
    }
    else if (size == raw_size::large)
    {
        large = texture;
    }
    else if (size == raw_size::x_large)
    {
        x_large = texture;
    }
    else if (size == raw_size::xx_large)
    {
        xx_large = texture;
    }
    else
    {
        std::unreachable();
    }
}

Glib::RefPtr<Gdk::Paintable>
vfs::file::thumbnail_data::get(const std::int32_t size) const noexcept
{
    Glib::RefPtr<Gdk::Texture> thumbnail;
    if (size <= 128)
    {
        thumbnail = normal;
    }
    else if (size <= 256)
    {
        thumbnail = large;
    }
    else if (size <= 512)
    {
        thumbnail = x_large;
    }
    else if (size <= 1024)
    {
        thumbnail = xx_large;
    }
    else
    {
        std::unreachable();
    }

    if (!thumbnail)
    {
        return nullptr;
    }

    const auto src_width = static_cast<std::float_t>(thumbnail->get_width());
    const auto src_height = static_cast<std::float_t>(thumbnail->get_height());

    const auto scale = std::min(static_cast<std::float_t>(size) / src_width,
                                static_cast<std::float_t>(size) / src_height);

    const auto scaled_width = src_width * scale;
    const auto scaled_height = src_height * scale;

    const auto final_width = scaled_width;
    const auto final_height = scaled_height;

    auto snapshot = Gtk::Snapshot::create();
    snapshot->scale(scale, scale);
    snapshot->append_texture(thumbnail, Gdk::Graphene::Rect(0.0f, 0.0f, src_width, src_height));

    return snapshot->to_paintable(Gdk::Graphene::Size(final_width, final_height));
}

bool
vfs::file::thumbnail_data::is_loaded(const std::int32_t size) const noexcept
{
    if (size <= 128)
    {
        return normal != nullptr;
    }
    else if (size <= 256)
    {
        return large != nullptr;
    }
    else if (size <= 512)
    {
        return x_large != nullptr;
    }
    else if (size <= 1024)
    {
        return xx_large != nullptr;
    }
    else
    {
        std::unreachable();
    }
}

vfs::file::thumbnail_data::raw_size
vfs::file::thumbnail_data::get_raw_size(const std::int32_t size) noexcept
{
    if (size <= 128)
    {
        return raw_size::normal;
    }
    else if (size <= 256)
    {
        return raw_size::large;
    }
    else if (size <= 512)
    {
        return raw_size::x_large;
    }
    else if (size <= 1024)
    {
        return raw_size::xx_large;
    }
    else
    {
        std::unreachable();
    }
}

void
vfs::file::thumbnail_data::clear() noexcept
{
    normal = nullptr;
    large = nullptr;
    x_large = nullptr;
    xx_large = nullptr;
}

#elif (GTK_MAJOR_VERSION == 3)

GdkPixbuf*
vfs::file::icon(const thumbnail_size size) noexcept
{
    ztd::panic_if(settings_ == nullptr, "Function disabled");

    if (size == thumbnail_size::big)
    {
        if (is_desktop_entry() && thumbnail_.big)
        {
            return g_object_ref(thumbnail_.big);
        }

        if (is_directory())
        {
            const auto icon_name = special_directory_get_icon_name();
            return vfs::utils::load_icon(icon_name, settings_->icon_size_big);
        }

        if (!mime_type_)
        {
            return nullptr;
        }
        return mime_type_->icon(true);
    }
    else
    {
        if (is_desktop_entry() && thumbnail_.small)
        {
            return g_object_ref(thumbnail_.small);
        }

        if (is_directory())
        {
            const auto icon_name = special_directory_get_icon_name();
            return vfs::utils::load_icon(icon_name, settings_->icon_size_small);
        }

        if (!mime_type_)
        {
            return nullptr;
        }
        return mime_type_->icon(false);
    }
}

GdkPixbuf*
vfs::file::thumbnail(const thumbnail_size size) const noexcept
{
    if (size == thumbnail_size::big)
    {
        return thumbnail_.big ? g_object_ref(thumbnail_.big) : nullptr;
    }
    else
    {
        return thumbnail_.small ? g_object_ref(thumbnail_.small) : nullptr;
    }
}

void
vfs::file::load_thumbnail(const thumbnail_size size) noexcept
{
    ztd::panic_if(settings_ == nullptr, "Function disabled");

    static const auto thumbnail_cache = vfs::user::thumbnail_cache();
    if (path_.string().starts_with(thumbnail_cache.parent.string()))
    {
        // TODO use cache images directly
        logger::debug<logger::vfs>("Not generating thumbnails in cache path: {}", path_.string());
        return;
    }

    if (size == thumbnail_size::big)
    {
        if (thumbnail_.big)
        {
            return;
        }

        std::error_code ec;
        const bool exists = std::filesystem::exists(path_, ec);
        if (ec || !exists)
        {
            return;
        }

        GdkPixbuf* thumbnail = nullptr;
        if (mime_type_->is_image())
        {
            thumbnail =
                vfs::detail::thumbnail::image(shared_from_this(), settings_->icon_size_big.data());
        }
        else if (mime_type_->is_video())
        {
            thumbnail =
                vfs::detail::thumbnail::video(shared_from_this(), settings_->icon_size_big.data());
        }

        if (thumbnail)
        {
            thumbnail_.big = thumbnail;
        }
        else
        {
            // fallback to mime_type icon
            thumbnail_.big = icon(thumbnail_size::big);
        }
    }
    else
    {
        if (thumbnail_.small)
        {
            return;
        }

        std::error_code ec;
        const bool exists = std::filesystem::exists(path_, ec);
        if (ec || !exists)
        {
            return;
        }

        GdkPixbuf* thumbnail = nullptr;
        if (mime_type_->is_image())
        {
            thumbnail = vfs::detail::thumbnail::image(shared_from_this(),
                                                      settings_->icon_size_small.data());
        }
        else if (mime_type_->is_video())
        {
            thumbnail = vfs::detail::thumbnail::video(shared_from_this(),
                                                      settings_->icon_size_small.data());
        }

        if (thumbnail)
        {
            thumbnail_.small = thumbnail;
        }
        else
        {
            // fallback to mime_type icon
            thumbnail_.small = icon(thumbnail_size::small);
        }
    }
}

void
vfs::file::unload_thumbnail(const thumbnail_size size) noexcept
{
    if (size == thumbnail_size::big)
    {
        if (thumbnail_.big)
        {
            g_object_unref(thumbnail_.big);
            thumbnail_.big = nullptr;
        }
    }
    else
    {
        if (thumbnail_.small)
        {
            g_object_unref(thumbnail_.small);
            thumbnail_.small = nullptr;
        }
    }
}

bool
vfs::file::is_thumbnail_loaded(const thumbnail_size size) const noexcept
{
    if (size == thumbnail_size::big)
    {
        return (thumbnail_.big != nullptr);
    }
    else
    {
        return (thumbnail_.small != nullptr);
    }
}

#endif
