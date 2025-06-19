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
#include <format>
#include <span>
#include <vector>

#include <glaze/glaze.hpp>

#include <ztd/ztd.hxx>

#include "datatypes/datatypes.hxx"

#include "vfs/bookmarks.hxx"
#include "vfs/user-dirs.hxx"

#include "vfs/utils/file-ops.hxx"

#include "logger.hxx"

namespace
{
datatype::bookmarks::bookmarks bookmarks_{};
}

std::span<datatype::bookmarks::bookmark>
vfs::bookmarks::bookmarks() noexcept
{
    return bookmarks_.bookmarks;
}

void
vfs::bookmarks::bookmarks(const datatype::bookmarks::bookmarks& bookmarks) noexcept
{
    bookmarks_.bookmarks = bookmarks.bookmarks;
}

void
vfs::bookmarks::load() noexcept
{
    const auto file = vfs::program::config() / "bookmarks.json";
    if (!std::filesystem::exists(file))
    {
        return;
    }

    const auto buffer = vfs::utils::read_file(file);
    if (!buffer)
    {
        logger::error("Failed to read bookmark file: {} {}",
                      file.string(),
                      buffer.error().message());
        return;
    }

    const auto result = glz::read_json<datatype::bookmarks::bookmarks>(buffer.value());
    if (!result)
    {
        std::println("Failed to decode json: {}",
                     glz::format_error(result.error(), buffer.value()));
    }
    bookmarks_ = result.value();
}

void
vfs::bookmarks::save() noexcept
{
    const auto file = vfs::program::config() / "bookmarks.json";

    std::string buffer;
    const auto ec =
        glz::write_file_json<glz::opts{.prettify = true}>(bookmarks_, file.c_str(), buffer);
    if (ec)
    {
        logger::error("Failed to bookmark file: {}", glz::format_error(ec, buffer));
    }
}

void
vfs::bookmarks::add(const std::filesystem::path& path) noexcept
{
    auto data = datatype::bookmarks::bookmark{path.filename(), path};

    for (const auto& [book_name, book_path] : bookmarks_.bookmarks)
    {
        if (book_name == data.name && book_path == data.path)
        {
            logger::info("Path already has a bookmark: {}", book_path.string());
            return;
        }
    }

    bookmarks_.bookmarks.push_back({path.filename(), path});

    vfs::bookmarks::save();
}

void
vfs::bookmarks::remove(const std::filesystem::path& path) noexcept
{
    std::int64_t idx = 0;
    for (const auto& [book_name, book_path] : bookmarks_.bookmarks)
    {
        if (book_name == path.filename() && book_path == path)
        {
            break;
        }
        idx += 1;
    }
    bookmarks_.bookmarks.erase(bookmarks_.bookmarks.cbegin() + idx);
}
