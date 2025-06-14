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

#include <glibmm.h>

#include <magic_enum/magic_enum.hpp>

#include <botan/hash.h>
#include <botan/hex.h>

#include <ztd/ztd.hxx>

#include "utils/shell-quote.hxx"

#include "vfs/file.hxx"
#include "vfs/user-dirs.hxx"

#include "vfs/thumbnails/thumbnails.hxx"

#include "logger.hxx"

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

static GdkPixbuf*
thumbnail_create(const std::shared_ptr<vfs::file>& file, const i32 thumb_size,
                 const thumbnail_mode mode) noexcept
{
    const auto [thumbnail_create_size, thumbnail_cache] =
        [](const auto thumb_size) -> std::pair<std::uint32_t, std::filesystem::path>
    {
        assert(thumb_size <= 1024);

        if (thumb_size <= 128)
        {
            return {magic_enum::enum_integer(thumbnail_size::normal),
                    vfs::user::cache() / "thumbnails/normal"};
        }
        else if (thumb_size <= 256)
        {
            return {magic_enum::enum_integer(thumbnail_size::large),
                    vfs::user::cache() / "thumbnails/large"};
        }
        else if (thumb_size <= 512)
        {
            return {magic_enum::enum_integer(thumbnail_size::x_large),
                    vfs::user::cache() / "thumbnails/x-large"};
        }
        else // if (thumb_size <= 1024)
        {
            return {magic_enum::enum_integer(thumbnail_size::xx_large),
                    vfs::user::cache() / "thumbnails/xx-large"};
        }
    }(thumb_size);

    const auto md5 = Botan::HashFunction::create("MD5");
    md5->update(file->uri());
    const auto hash = Botan::hex_encode(md5->final(), false);

    const auto thumbnail_file = thumbnail_cache / std::format("{}.png", hash);

    // logger::debug<logger::domain::vfs>("thumbnail_load()={} | uri={} | thumb_size={}", file->path().string(), file->uri(), thumb_size);

    // if the mtime of the file being thumbnailed is less than 5 sec ago,
    // do not create a thumbnail. This means that newly created files
    // will not have a thumbnail until a refresh
    const auto mtime = file->mtime();
    const auto now = std::chrono::system_clock::now();
    if (now - mtime < std::chrono::seconds(5))
    {
        return nullptr;
    }

    // load existing thumbnail
    std::chrono::system_clock::time_point embeded_mtime;
    GdkPixbuf* thumbnail = nullptr;
    if (std::filesystem::is_regular_file(thumbnail_file))
    {
        // logger::debug<logger::domain::vfs>("Existing thumb: {}", thumbnail_file);
        GError* error = nullptr;
        thumbnail = gdk_pixbuf_new_from_file(thumbnail_file.c_str(), &error);
        if (thumbnail)
        {
            const char* thumb_mtime = gdk_pixbuf_get_option(thumbnail, "tEXt::Thumb::MTime");
            if (thumb_mtime != nullptr)
            {
                const auto result = ztd::from_string<std::time_t>(thumb_mtime);
                embeded_mtime = std::chrono::system_clock::from_time_t(result.value_or(0));
            }
        }
        else
        { // broken/corrupt/empty thumbnail
            logger::error<logger::domain::vfs>(
                "Loading existing thumbnail for file '{}' failed with: {}",
                file->name(),
                error->message);
            g_error_free(error);

            // delete the bad thumbnail file
            std::filesystem::remove(thumbnail_file);
        }
    }

    if (!thumbnail ||
        // on disk thumbnail metadata does not store mtime nanoseconds
        std::chrono::time_point_cast<std::chrono::seconds>(embeded_mtime) !=
            std::chrono::time_point_cast<std::chrono::seconds>(mtime))
    {
        // logger::debug<logger::domain::vfs>("New thumb: {}", thumbnail_file);

        if (thumbnail)
        {
            g_object_unref(thumbnail);
        }

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
                                      ::utils::shell_quote(file->path().string()),
                                      ::utils::shell_quote(thumbnail_file.string()));
                break;
            }
            case thumbnail_mode::video:
            {
                command = std::format("ffmpegthumbnailer -f -s {} -i {} -o {}",
                                      thumbnail_create_size,
                                      ::utils::shell_quote(file->path().string()),
                                      ::utils::shell_quote(thumbnail_file.string()));
                break;
            }
        }

        // logger::info<logger::domain::vfs>("COMMAND({})", command);
        Glib::spawn_command_line_sync(command);

        if (!std::filesystem::exists(thumbnail_file))
        {
            logger::error<logger::domain::vfs>("Failed to create thumbnail for '{}'", file->name());
            return nullptr;
        }

        GError* error = nullptr;
        thumbnail = gdk_pixbuf_new_from_file(thumbnail_file.c_str(), &error);
        if (!thumbnail)
        {
            logger::error<logger::domain::vfs>(
                "Loading existing thumbnail for file '{}' failed with: {}",
                file->name(),
                error->message);
            g_error_free(error);
            return nullptr;
        }
    }

    // Scale thumbnail to requested size from cached thumbnail
    const i32 fixed_size = thumb_size;
    const i32 original_width = gdk_pixbuf_get_width(thumbnail);
    const i32 original_height = gdk_pixbuf_get_height(thumbnail);
    i32 new_width = fixed_size;
    i32 new_height = fixed_size;
    if (original_width > original_height)
    { // Scale by width
        new_height = (fixed_size * original_height) / original_width;
    }
    else
    { // Scale by height
        new_width = (fixed_size * original_width) / original_height;
    }

    GdkPixbuf* thumbnail_scaled = gdk_pixbuf_scale_simple(thumbnail,
                                                          new_width.data(),
                                                          new_height.data(),
                                                          GdkInterpType::GDK_INTERP_BILINEAR);
    g_object_unref(thumbnail);

    return thumbnail_scaled;
}

GdkPixbuf*
vfs::detail::thumbnail::image(const std::shared_ptr<vfs::file>& file, const i32 thumb_size) noexcept
{
    return thumbnail_create(file, thumb_size, thumbnail_mode::image);
}

GdkPixbuf*
vfs::detail::thumbnail::video(const std::shared_ptr<vfs::file>& file, const i32 thumb_size) noexcept
{
    return thumbnail_create(file, thumb_size, thumbnail_mode::video);
}
