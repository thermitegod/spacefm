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

#include <string>

#include <format>

#include <filesystem>

#include <chrono>

#include <memory>

#include <glibmm.h>

#include <libffmpegthumbnailer/imagetypes.h>
#include <libffmpegthumbnailer/videothumbnailer.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "utils/shell-quote.hxx"

#include "settings/settings.hxx"

#include "vfs/vfs-file.hxx"
#include "vfs/vfs-user-dirs.hxx"

#include "vfs/thumbnails/thumbnails.hxx"

GdkPixbuf*
vfs::detail::thumbnail_load(const std::shared_ptr<vfs::file>& file, const i32 thumb_size) noexcept
{
    const std::string file_hash = ztd::compute_checksum(ztd::checksum::type::md5, file->uri());
    const std::string thumbnail_filename = std::format("{}.png", file_hash);

    const auto thumbnail_cache = vfs::user::cache() / "thumbnails/normal";
    const auto thumbnail_file = thumbnail_cache / thumbnail_filename;

    // ztd::logger::debug("thumbnail_load()={} | uri={} | thumb_size={}", file->path().string(), file->uri(), thumb_size);

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
    i32 w = 0;
    i32 h = 0;
    std::chrono::system_clock::time_point embeded_mtime;
    GdkPixbuf* thumbnail = nullptr;
    if (std::filesystem::is_regular_file(thumbnail_file))
    {
        // ztd::logger::debug("Existing thumb: {}", thumbnail_file);
        thumbnail = gdk_pixbuf_new_from_file(thumbnail_file.c_str(), nullptr);
        if (thumbnail)
        { // need to check for broken thumbnail images
            w = gdk_pixbuf_get_width(thumbnail);
            h = gdk_pixbuf_get_height(thumbnail);
            const char* thumb_mtime = gdk_pixbuf_get_option(thumbnail, "tEXt::Thumb::MTime");
            if (thumb_mtime != nullptr)
            {
                embeded_mtime = std::chrono::system_clock::from_time_t(std::stol(thumb_mtime));
            }
        }
    }

    if (!thumbnail || (w < thumb_size && h < thumb_size) ||
        // TODO? on disk thumbnail mtime metadata does not store nanoseconds
        std::chrono::time_point_cast<std::chrono::seconds>(embeded_mtime) !=
            std::chrono::time_point_cast<std::chrono::seconds>(mtime))
    {
        // ztd::logger::debug("New thumb: {}", thumbnail_file);

        if (thumbnail)
        {
            g_object_unref(thumbnail);
        }

        // Need to create thumbnail directory if it is missing,
        // ffmpegthumbnailer will not create missing directories.
        // Have this check run everytime because if the cache is
        // deleted while running then thumbnail loading will break.
        // TODO - have a monitor watch this directory and recreate if deleted.
        if (!std::filesystem::is_directory(vfs::user::cache() / "thumbnails/normal"))
        {
            std::filesystem::create_directories(thumbnail_cache);
        }

        // create new thumbnail
        if (config::settings.thumbnailer_use_api)
        {
            try
            {
                ffmpegthumbnailer::VideoThumbnailer video_thumb;
                // video_thumb.setLogCallback(nullptr);
                // video_thumb.clearFilters();
                video_thumb.setSeekPercentage(25);
                video_thumb.setThumbnailSize(thumb_size);
                video_thumb.setMaintainAspectRatio(true);
                video_thumb.generateThumbnail(file->path(),
                                              ThumbnailerImageType::Png,
                                              thumbnail_file,
                                              nullptr);
            }
            catch (const std::logic_error& e)
            {
                // file cannot be opened
                return nullptr;
            }
        }
        else
        {
            const auto command = std::format("ffmpegthumbnailer -s {} -i {} -o {}",
                                             thumb_size,
                                             ::utils::shell_quote(file->path().string()),
                                             ::utils::shell_quote(thumbnail_file.string()));
            // ztd::logger::info("COMMAND({})", command);
            Glib::spawn_command_line_sync(command);

            if (!std::filesystem::exists(thumbnail_file))
            {
                return nullptr;
            }
        }

        thumbnail = gdk_pixbuf_new_from_file(thumbnail_file.c_str(), nullptr);
    }

    GdkPixbuf* result = nullptr;
    if (thumbnail)
    {
        w = gdk_pixbuf_get_width(thumbnail);
        h = gdk_pixbuf_get_height(thumbnail);

        if (w > h)
        {
            h = h * thumb_size / w;
            w = thumb_size;
        }
        else if (h > w)
        {
            w = w * thumb_size / h;
            h = thumb_size;
        }
        else
        {
            w = h = thumb_size;
        }

        if (w > 0 && h > 0)
        {
            result = gdk_pixbuf_scale_simple(thumbnail, w, h, GdkInterpType::GDK_INTERP_BILINEAR);
        }

        g_object_unref(thumbnail);
    }

    return result;
}
