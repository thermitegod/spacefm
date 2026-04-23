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

#include "glycin-wrapper.hxx"
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
is_metadata_valid(Glib::RefPtr<Gly::Image> image, const std::shared_ptr<vfs::file>& file) noexcept
{
    if (!image || !file)
    {
        return false;
    }

#if 1
    // TODO
    // glycin-thumbnailer does not set Thumb metadata keys.
    // What a waste of time to figure this out.
    // $ exiftool -list <path> | grep Thumb
    return true;
#else
    // MTime
    const auto mtime_val = image->get_metadata_key_value("Thumb::MTime");
    auto mtime = mtime_val ? std::chrono::system_clock::from_time_t(
                                 ztd::from_string<std::time_t>(*mtime_val).value_or(0))
                           : std::chrono::system_clock::time_point{};

    // URI
    const auto uri_val = image->get_metadata_key_value("Thumb::URI");
    std::string uri = uri_val.value_or("");

    // Size
    const auto size_val = image->get_metadata_key_value("Thumb::Size");
    usize size = size_val ? usize::create(*size_val).value_or(0) : usize{0};

    // logger::trace<logger::vfs>("uri={}, mtime={}, size={}", uri, std::chrono::duration_cast<std::chrono::seconds>(mtime.time_since_epoch()).count(), size);

    // validate //

    // MTime
    const auto file_mtime = std::chrono::time_point_cast<std::chrono::seconds>(file->mtime());
    const auto meta_mtime = std::chrono::time_point_cast<std::chrono::seconds>(mtime);
    if (file_mtime != meta_mtime)
    {
        // logger::trace<logger::vfs>("MTime mismatch for '{}': expected {}, got {}", uri, meta_mtime.time_since_epoch().count(), file_mtime.time_since_epoch().count());
        return false;
    }

    // Size
    if (file->size() != size)
    {
        // logger::trace<logger::vfs>("Size mismatch for '{}': expected {}, got {}", uri, size, file->size());
        return false;
    }

    return true;
#endif
}

static Glib::RefPtr<Gdk::Texture>
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
            else if (size <= 1024)
            {
                return {magic_enum::enum_integer(thumbnail_size::xx_large), cache_dirs.xx_large};
            }
            else
            {
                std::unreachable();
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

    // logger::debug<logger::vfs>("path={}, uri={}, thumb_size={}", file->path().string(), file->uri(), thumb_size);

    // if the mtime of the file being thumbnailed is less than 5 sec ago,
    // do not create a thumbnail. This means that newly created files
    // will not have a thumbnail until a refresh
    const auto mtime = file->mtime();
    const auto now = std::chrono::system_clock::now();
    if (now - mtime < std::chrono::seconds(5))
    {
        return nullptr;
    }

    auto glycin_load_image = [](const std::filesystem::path& filepath) -> Glib::RefPtr<Gly::Image>
    {
        auto file = Gio::File::create_for_path(filepath);

        try
        {
            auto loader = Gly::Loader::create(file);
            auto image = loader->load();

            return image;
        }
        catch (const Glib::Error& e)
        {
            logger::error<logger::vfs>("Loading '{}' failed with: {}", file->get_path(), e.what());
            return nullptr;
        }
    };

    auto glycin_get_texture = [](Glib::RefPtr<Gly::Image> image) -> Glib::RefPtr<Gdk::Texture>
    {
        try
        {
            auto frame = image->next_frame();
            auto texture = frame->get_texture();

            return texture;
        }
        catch (const Glib::Error& e)
        {
            logger::error<logger::vfs>("Texture loading failed with: {}", e.what());
            return nullptr;
        }
    };

    Glib::RefPtr<Gly::Image> thumbnail;

    if (std::filesystem::is_regular_file(thumbnail_file))
    {
        // logger::debug<logger::vfs>("Existing thumb: {}", thumbnail_file);
        thumbnail = glycin_load_image(thumbnail_file);
        if (!thumbnail)
        {
            std::filesystem::remove(thumbnail_file);
        }
    }

    if (!thumbnail || !is_metadata_valid(thumbnail, file))
    {
        // logger::debug<logger::vfs>("New thumb for '{}', {}", file->path().string(), thumbnail_file.string());

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
                command = std::format("glycin-thumbnailer -s {} -i {} -o {}",
                                      thumbnail_create_size,
                                      vfs::execute::quote(file->uri()),
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

        thumbnail = glycin_load_image(thumbnail_file);
        if (!thumbnail)
        {
            create_fail(file, fail_file);
            std::filesystem::remove(thumbnail_file);
            return nullptr;
        }
    }

    return glycin_get_texture(thumbnail);
}

Glib::RefPtr<Gdk::Texture>
vfs::detail::thumbnail::image(const std::shared_ptr<vfs::file>& file,
                              const std::int32_t thumb_size) noexcept
{
    return thumbnail_create(file, thumb_size, thumbnail_mode::image);
}

Glib::RefPtr<Gdk::Texture>
vfs::detail::thumbnail::video(const std::shared_ptr<vfs::file>& file,
                              const std::int32_t thumb_size) noexcept
{
    return thumbnail_create(file, thumb_size, thumbnail_mode::video);
}
