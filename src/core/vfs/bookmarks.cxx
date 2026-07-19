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
#include <span>
#include <string>
#include <vector>

#include <gtkmm.h>

#include <glaze/json.hpp>

#include <ztd/extra/glaze.hxx>
#include <ztd/ztd.hxx>

#include "vfs/bookmarks.hxx"
#include "vfs/user-dirs.hxx"

namespace bookmark_disk_format
{
constexpr u64 version = 1_u64;
const std::filesystem::path path = vfs::program::data() / "bookmarks.json";

struct bookmark_data final
{
    u64 version;
    std::vector<vfs::bookmarks::bookmark_data> bookmarks;
};
} // namespace bookmark_disk_format

void
vfs::bookmarks::save() noexcept
{
    if (!std::filesystem::exists(vfs::program::data()))
    {
        std::filesystem::create_directory(vfs::program::data());
    }

    const auto data =
        bookmark_disk_format::bookmark_data{bookmark_disk_format::version, bookmarks_};

    std::string buffer;
    const auto ec =
        glz::write_file_json<glz::opts{.prettify = true}>(data,
                                                          bookmark_disk_format::path.c_str(),
                                                          buffer);

    if (ec)
    {
        // logger::error("Failed to write bookmark file: {}", glz::format_error(ec, buffer));

        signal_save_error().emit(glz::format_error(ec, buffer));
    }
}

void
vfs::bookmarks::load() noexcept
{
    if (!std::filesystem::exists(bookmark_disk_format::path))
    {
        return;
    }

    const auto statx = ztd::stat::create(bookmark_disk_format::path);
    if (statx->mtime() == bookmark_mtime_)
    { // Bookmark file has not been modified since last read
        return;
    }
    bookmark_mtime_ = statx->mtime();

    bookmark_disk_format::bookmark_data config_data;
    std::string buffer;
    const auto ec = glz::read_file_json<glz::opts{.error_on_unknown_keys = false}>(
        config_data,
        bookmark_disk_format::path.c_str(),
        buffer);

    if (ec)
    {
        // logger::error("Failed to load bookmark file: {}", glz::format_error(ec, buffer));

        signal_load_error().emit(glz::format_error(ec, buffer));
        return;
    }

    bookmarks_ = config_data.bookmarks;
}

void
vfs::bookmarks::add(const std::filesystem::path& path) noexcept
{
    load();

    const auto now = std::chrono::system_clock::now();

    for (auto& bookmark : bookmarks_)
    {
        if (bookmark.path == path)
        {
            bookmark.created = now;
            save();
            return;
        }
    }

    bookmarks_.push_back({path.filename().string(), path, now});
    save();
}

void
vfs::bookmarks::remove(const std::filesystem::path& path) noexcept
{
    load();

    std::erase_if(bookmarks_,
                  [&path](const bookmark_data& bookmark) { return bookmark.path == path; });

    save();
}

void
vfs::bookmarks::remove_all() noexcept
{
    bookmarks_.clear();
    save();
}

std::span<const vfs::bookmarks::bookmark_data>
vfs::bookmarks::get_bookmarks() noexcept
{
    load();

    return bookmarks_;
}
