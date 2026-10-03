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

#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <glibmm.h>

#include "vfs/thumbnails/thumbnailer.hxx"

#include "logger.hxx"

vfs::thumbnail::thumbnailer::thumbnailer(const std::filesystem::path& path) : path_(path)
{
    auto result = parse_thumbnailer_file();
    if (result != vfs::error_code::none)
    {
        throw std::runtime_error("Failed to parse");
    }
}

std::shared_ptr<vfs::thumbnail::thumbnailer>
vfs::thumbnail::thumbnailer::create(const std::filesystem::path& path) noexcept
{
    struct hack : public vfs::thumbnail::thumbnailer
    {
        hack(const std::filesystem::path& path) : thumbnailer(path) {}
    };

    try
    {
        return std::make_shared<hack>(path);
    }
    catch (...)
    {
        return nullptr;
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
