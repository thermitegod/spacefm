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
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "vfs/thumbnails/thumbnailer.hxx"

#include "logger.hxx"

namespace
{
struct thumbnailer_cache_data final
{
    std::shared_ptr<vfs::thumbnail::thumbnailer> thumbnailer;
    std::chrono::system_clock::time_point mtime;
};

std::mutex thumbnailers_cache_mutex;
std::unordered_map<std::filesystem::path, thumbnailer_cache_data> thumbnailers_cache;
} // namespace

std::shared_ptr<vfs::thumbnail::thumbnailer>
vfs::thumbnail::thumbnailer::create(const std::filesystem::path& path) noexcept
{
    std::scoped_lock lock(thumbnailers_cache_mutex);

    {
        const auto it = thumbnailers_cache.find(path);
        if (it != thumbnailers_cache.cend())
        {
            // logger::trace<logger::vfs>("vfs::thumbnail::thumbnailer({})  cache   {}", logger::utils::ptr(it->second.thumbnailer), path);
            const auto stat = ztd::stat::create(it->second.thumbnailer->path());
            if (stat && stat->mtime() == it->second.mtime)
            {
                return it->second.thumbnailer;
            }
            // logger::trace<logger::vfs>("vfs::thumbnail::thumbnailer({})  changed on disk, reloading", logger::utils::ptr(it->second.thumbnailer));
        }
    }

    struct hack : public vfs::thumbnail::thumbnailer
    {
        hack(const std::filesystem::path& path) : thumbnailer(path) {}
    };

    std::shared_ptr<vfs::thumbnail::thumbnailer> thumbnailer;
    try
    {
        thumbnailer = std::make_shared<hack>(path);
    }
    catch (...)
    {
        return nullptr;
    }

    const auto stat = ztd::stat::create(thumbnailer->path());
    if (!stat)
    {
        return nullptr;
    }

    const auto [it, _] = thumbnailers_cache.insert_or_assign(path,
                                                             thumbnailer_cache_data{
                                                                 std::move(thumbnailer),
                                                                 stat->mtime(),
                                                             });
    // logger::trace<logger::vfs>("vfs::thumbnail::thumbnailer({})  new     {}", logger::utils::ptr(it->second.thumbnailer), path);
    return it->second.thumbnailer;
}

vfs::thumbnail::thumbnailer::thumbnailer(const std::filesystem::path& path) : path_(path)
{
    // logger::info<logger::vfs>("vfs::thumbnail::thumbnailer({})", logger::utils::ptr(this));

    auto result = parse_thumbnailer_file();
    if (result != vfs::error_code::none)
    {
        throw std::runtime_error("Failed to parse");
    }
}

vfs::error_code
vfs::thumbnail::thumbnailer::parse_thumbnailer_file() noexcept
{
    static constexpr auto THUMBNAILER_ENTRY_GROUP = "Thumbnailer Entry";
    static constexpr auto THUMBNAILER_ENTRY_KEY_EXEC = "Exec";
    static constexpr auto THUMBNAILER_ENTRY_KEY_TRYEXEC = "TryExec";
    static constexpr auto THUMBNAILER_ENTRY_KEY_MIMETYPE = "MimeType";

    const auto kf = Glib::KeyFile::create();
    const auto loaded = kf->load_from_file(path_, Glib::KeyFile::Flags::NONE);

    if (!loaded)
    {
        logger::error<logger::vfs>("Failed to load thumbnailer file: {}", path_);
        return vfs::error_code::parse_error;
    }

    if (kf->has_key(THUMBNAILER_ENTRY_GROUP, THUMBNAILER_ENTRY_KEY_EXEC))
    {
        thumbnailer_entry_.exec =
            kf->get_string(THUMBNAILER_ENTRY_GROUP, THUMBNAILER_ENTRY_KEY_EXEC);
    }
    else
    {
        return vfs::error_code::key_not_found;
    }

    if (kf->has_key(THUMBNAILER_ENTRY_GROUP, THUMBNAILER_ENTRY_KEY_MIMETYPE))
    {
        try
        {
            auto mime_types =
                kf->get_string_list(THUMBNAILER_ENTRY_GROUP, THUMBNAILER_ENTRY_KEY_MIMETYPE);
            for (const auto& mime_type : mime_types)
            {
                thumbnailer_entry_.mime_types.push_back(mime_type);
            }
        }
        catch (...)
        {
            return vfs::error_code::parse_error;
        }
    }
    else
    {
        return vfs::error_code::key_not_found;
    }

    if (kf->has_key(THUMBNAILER_ENTRY_GROUP, THUMBNAILER_ENTRY_KEY_TRYEXEC))
    {
        thumbnailer_entry_.try_exec =
            kf->get_string(THUMBNAILER_ENTRY_GROUP, THUMBNAILER_ENTRY_KEY_TRYEXEC);
    }

    return loaded ? vfs::error_code::none : vfs::error_code::parse_error;
}
