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

#include <chrono>
#include <filesystem>
#include <format>
#include <memory>
#include <string>

#include <gdkmm.h>
#include <glibmm.h>
#include <gtkmm.h>

#include <glaze/glaze.hpp>

#include <magic_enum/magic_enum.hpp>

#include <botan/hash.h>
#include <botan/hex.h>

#include <ztd/extra/glaze.hxx>
#include <ztd/ztd.hxx>

#include "vfs/execute.hxx"
#include "vfs/file.hxx"
#include "vfs/user-dirs.hxx"

#include "vfs/thumbnails/thumbnails.hxx"

#include "logger.hxx"

// Based on spec v0.9.0
// https://specifications.freedesktop.org/thumbnail-spec/thumbnail-spec-latest.html
// Not implemented
// - No thumbnail delete <https://specifications.freedesktop.org/thumbnail-spec/latest/delete.html>
// - No shared thumbnail <https://specifications.freedesktop.org/thumbnail-spec/latest/shared.html>

struct fail final
{
    std::string uri;
    std::time_t mtime;
    u64 size;
    std::string mimetype;
};

static void
create_fail(const std::shared_ptr<vfs::file>& file, const std::filesystem::path& path) noexcept
{
    if (!std::filesystem::is_directory(path.parent_path()))
    {
        std::filesystem::create_directories(path.parent_path());
    }

    const auto fail_data = fail{
        file->uri().data(),
        std::chrono::system_clock::to_time_t(file->mtime()),
        file->size(),
        file->mime_type()->type().data(),
    };

    std::string buffer;
    const auto ec =
        glz::write_file_json<glz::opts{.prettify = true}>(fail_data, path.c_str(), buffer);

    logger::error_if(ec, "Failed to create thumbnail fail file: {}", glz::format_error(ec, buffer));
}

enum class thumbnail_size : std::uint16_t
{
    normal = 128,
    large = 256,
    x_large = 512,
    xx_large = 1024,
};

enum class thumbnail_mode : std::uint8_t
{
    image,
    video,
};

[[nodiscard]] static bool
is_metadata_valid(const std::shared_ptr<vfs::file>& file,
                  std::chrono::system_clock::time_point mtime, std::string_view uri,
                  usize size) noexcept
{
    (void)uri;

    if (std::chrono::time_point_cast<std::chrono::seconds>(file->mtime()) !=
        std::chrono::time_point_cast<std::chrono::seconds>(mtime))
    {
        return false;
    }

#if 0 // TODO CJK problems with on disk uri in thumbnails metadata
    if (uri.starts_with("file://"))
    {
        if (file->uri() != uri)
        {
            return false;
        }
    }
    else
    {
        if (file->path() != uri)
        {
            return false;
        }
    }
#endif

    if (file->size() != size)
    {
        return false;
    }

    return true;
}

#if (GTK_MAJOR_VERSION == 4)
static Glib::RefPtr<Gdk::Paintable>
#elif (GTK_MAJOR_VERSION == 3)
static GdkPixbuf*
#endif
thumbnail_create(const std::shared_ptr<vfs::file>& file, const i32 thumb_size,
                 const thumbnail_mode mode) noexcept
{
    static thread_local auto cache_dirs = vfs::user::thumbnail_cache();

    const auto [thumbnail_create_size, thumbnail_cache] = std::invoke(
        [](const auto size) -> std::pair<std::uint32_t, std::filesystem::path>
        {
            if (size <= 128)
            {
                return {magic_enum::enum_integer(thumbnail_size::normal), cache_dirs.normal};
            }
            else if (size <= 256)
            {
                return {magic_enum::enum_integer(thumbnail_size::large), cache_dirs.large};
            }
            else if (size <= 512)
            {
                return {magic_enum::enum_integer(thumbnail_size::x_large), cache_dirs.x_large};
            }
            else // if (size <= 1024)
            {
                return {magic_enum::enum_integer(thumbnail_size::xx_large), cache_dirs.xx_large};
            }
        },
        thumb_size);

    const auto hash = std::invoke(
        [](const auto& file)
        {
            const auto md5 = Botan::HashFunction::create("MD5");
            md5->update(file->uri());
            return Botan::hex_encode(md5->final(), false);
        },
        file);

    const auto thumbnail_file = thumbnail_cache / std::format("{}.png", hash);
    const auto fail_file = cache_dirs.fail / std::format("{}.json", hash);
    if (std::filesystem::exists(fail_file))
    {
        logger::trace<logger::vfs>("failed to create thumbnail in the past: {}",
                                   file->path().string());
        return nullptr;
    }

    // logger::debug<logger::vfs>("thumbnail_load()={} | uri={} | thumb_size={}", file->path().string(), file->uri(), thumb_size);

    // if the mtime of the file being thumbnailed is less than 5 sec ago,
    // do not create a thumbnail. This means that newly created files
    // will not have a thumbnail until a refresh
    const auto mtime = file->mtime();
    const auto now = std::chrono::system_clock::now();
    if (now - mtime < std::chrono::seconds(5))
    {
        return nullptr;
    }

    // thumbnail metadata
    std::chrono::system_clock::time_point metadata_mtime;
    std::string metadata_uri;
    usize metadata_size;

#if (GTK_MAJOR_VERSION == 4)
    Glib::RefPtr<Gdk::Pixbuf> thumbnail = nullptr;
#elif (GTK_MAJOR_VERSION == 3)
    GdkPixbuf* thumbnail = nullptr;
#endif

    if (std::filesystem::is_regular_file(thumbnail_file))
    {
        // logger::debug<logger::vfs>("Existing thumb: {}", thumbnail_file);
#if (GTK_MAJOR_VERSION == 4)
        try
        {
            thumbnail = Gdk::Pixbuf::create_from_file(thumbnail_file);
        }
        catch (const Glib::Error& e)
        { // broken/corrupt/empty thumbnail
            logger::error<logger::vfs>("Loading existing thumbnail for file '{}' failed with: {}",
                                       file->path().string(),
                                       e.what());
            std::filesystem::remove(thumbnail_file);
        }

        if (thumbnail)
        {
            const auto metadata_mtime_raw = thumbnail->get_option("tEXt::Thumb::MTime");
            if (!metadata_mtime_raw.empty())
            {
                metadata_mtime = std::chrono::system_clock::from_time_t(
                    ztd::from_string<std::time_t>(metadata_mtime_raw.data()).value_or(0));
            }

            const auto metadata_uri_raw = thumbnail->get_option("tEXt::Thumb::URI");
            if (!metadata_uri_raw.empty())
            {
                metadata_uri = metadata_uri_raw.data();
            }

            const auto metadata_size_raw = thumbnail->get_option("tEXt::Thumb::Size");
            if (!metadata_size_raw.empty())
            {
                metadata_size = usize::create(metadata_size_raw.data()).value_or(0);
            }
        }
#elif (GTK_MAJOR_VERSION == 3)
        GError* error = nullptr;
        thumbnail = gdk_pixbuf_new_from_file(thumbnail_file.c_str(), &error);
        if (thumbnail)
        {
            const char* metadata_mtime_raw = gdk_pixbuf_get_option(thumbnail, "tEXt::Thumb::MTime");
            if (metadata_mtime_raw != nullptr)
            {
                const auto result = ztd::from_string<std::time_t>(metadata_mtime_raw);
                metadata_mtime = std::chrono::system_clock::from_time_t(result.value_or(0));
            }

            const char* metadata_uri_raw = gdk_pixbuf_get_option(thumbnail, "tEXt::Thumb::URI");
            if (metadata_uri_raw != nullptr)
            {
                metadata_uri = metadata_uri_raw;
            }

            const char* metadata_size_raw = gdk_pixbuf_get_option(thumbnail, "tEXt::Thumb::Size");
            if (metadata_size_raw != nullptr)
            {
                metadata_size = usize::create(metadata_size_raw).value_or(0);
            }
        }
        else
        { // broken/corrupt/empty thumbnail
            logger::error<logger::vfs>("Loading existing thumbnail for file '{}' failed with: {}",
                                       file->path().string(),
                                       error->message);
            g_error_free(error);

            // delete the bad thumbnail file
            std::filesystem::remove(thumbnail_file);
        }
#endif
    }

    if (!thumbnail || !is_metadata_valid(file, metadata_mtime, metadata_uri, metadata_size))
    {
        // logger::debug<logger::vfs>("New thumb: {}", thumbnail_file);

#if (GTK_MAJOR_VERSION == 3)
        if (thumbnail)
        {
            g_object_unref(thumbnail);
        }
#endif

        // Need to create thumbnail directory if it is missing,
        // ffmpegthumbnailer will not create missing directories.
        // Have this check run everytime because if the cache is
        // deleted while running then thumbnail loading will break.
        // TODO - have a monitor watch this directory and recreate if deleted.
        if (!std::filesystem::is_directory(thumbnail_cache))
        {
            std::filesystem::create_directories(thumbnail_cache);
        }

        // create new thumbnail
        std::string command;
        switch (mode)
        {
            case thumbnail_mode::image:
            {
                command = std::format("ffmpegthumbnailer -s {} -i {} -o {}",
                                      thumbnail_create_size,
                                      vfs::execute::quote(file->path().string()),
                                      vfs::execute::quote(thumbnail_file.string()));
                break;
            }
            case thumbnail_mode::video:
            {
                command = std::format("ffmpegthumbnailer -f -s {} -i {} -o {}",
                                      thumbnail_create_size,
                                      vfs::execute::quote(file->path().string()),
                                      vfs::execute::quote(thumbnail_file.string()));
                break;
            }
        }
        const auto result = vfs::execute::command_line_sync(command);

        if (result.exit_status != 0 || !std::filesystem::exists(thumbnail_file))
        {
            logger::error<logger::vfs>("Failed to create thumbnail for '{}'",
                                       file->path().string());

            create_fail(file, fail_file);

            return nullptr;
        }

#if (GTK_MAJOR_VERSION == 4)
        try
        {
            thumbnail = Gdk::Pixbuf::create_from_file(thumbnail_file);
        }
        catch (const Glib::Error& e)
        {
            logger::error<logger::vfs>("Loading new thumbnail for file '{}' failed with: {}",
                                       file->path().string(),
                                       e.what());

            create_fail(file, fail_file);
            std::filesystem::remove(thumbnail_file);

            return nullptr;
        }
#elif (GTK_MAJOR_VERSION == 3)
        GError* error = nullptr;
        thumbnail = gdk_pixbuf_new_from_file(thumbnail_file.c_str(), &error);
        if (!thumbnail)
        {
            logger::error<logger::vfs>("Loading new thumbnail for file '{}' failed with: {}",
                                       file->path().string(),
                                       error->message);
            g_error_free(error);

            create_fail(file, fail_file);
            std::filesystem::remove(thumbnail_file);

            return nullptr;
        }
#endif
    }

    // Scale thumbnail to requested size from cached thumbnail
#if (GTK_MAJOR_VERSION == 4)
    const i32 original_width = thumbnail->get_width();
    const i32 original_height = thumbnail->get_height();
#elif (GTK_MAJOR_VERSION == 3)
    const i32 original_width = gdk_pixbuf_get_width(thumbnail);
    const i32 original_height = gdk_pixbuf_get_height(thumbnail);
#endif
    i32 new_width = thumb_size;
    i32 new_height = thumb_size;
    if (original_width > original_height)
    { // Scale by width
        new_height = (thumb_size * original_height) / original_width;
    }
    else
    { // Scale by height
        new_width = (thumb_size * original_width) / original_height;
    }

#if (GTK_MAJOR_VERSION == 4)
    return Gdk::Texture::create_for_pixbuf(
        thumbnail->scale_simple(new_width.data(), new_height.data(), Gdk::InterpType::BILINEAR));
#elif (GTK_MAJOR_VERSION == 3)
    GdkPixbuf* scaled = gdk_pixbuf_scale_simple(thumbnail,
                                                new_width.data(),
                                                new_height.data(),
                                                GdkInterpType::GDK_INTERP_BILINEAR);
    g_object_unref(thumbnail);
    return scaled;
#endif
}

#if (GTK_MAJOR_VERSION == 4)
Glib::RefPtr<Gdk::Paintable>
#elif (GTK_MAJOR_VERSION == 3)
GdkPixbuf*
#endif
vfs::detail::thumbnail::image(const std::shared_ptr<vfs::file>& file, const i32 thumb_size) noexcept
{
    return thumbnail_create(file, thumb_size, thumbnail_mode::image);
}

#if (GTK_MAJOR_VERSION == 4)
Glib::RefPtr<Gdk::Paintable>
#elif (GTK_MAJOR_VERSION == 3)
GdkPixbuf*
#endif
vfs::detail::thumbnail::video(const std::shared_ptr<vfs::file>& file, const i32 thumb_size) noexcept
{
    return thumbnail_create(file, thumb_size, thumbnail_mode::video);
}
