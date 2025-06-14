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
#include <string>
#include <string_view>
#include <vector>

#include <ztd/ztd.hxx>

#include "vfs/user-dirs.hxx"

#include "vfs/utils/file-ops.hxx"

#include "bookmarks.hxx"
#include "logger.hxx"

// Bookmark Path, Bookmark Name
namespace global
{
static std::vector<bookmark_t> bookmarks;
static bool bookmarks_changed = false;
} // namespace global

std::span<bookmark_t>
get_all_bookmarks() noexcept
{
    return global::bookmarks;
}

void
load_bookmarks() noexcept
{
    // If not the first time calling then need to remove loaded bookmarks
    if (!global::bookmarks.empty())
    {
        global::bookmarks.clear();
    }

    const auto bookmark_file = vfs::user::config() / "gtk-3.0" / "bookmarks";

    // no bookmark file
    if (!std::filesystem::exists(bookmark_file))
    {
        return;
    }

    const auto buffer = vfs::utils::read_file(bookmark_file);
    if (!buffer)
    {
        logger::error("Failed to read bookmark file: {} {}",
                      bookmark_file.string(),
                      buffer.error().message());
        return;
    }

    for (const auto& line : ztd::split(*buffer, "\n"))
    {
        const auto book_parts = ztd::rpartition(line, " ");

        const auto& book_path = book_parts[0];
        const auto& book_name = book_parts[2];

        if (book_path.empty())
        {
            return;
        }

        // logger::info("Bookmark: Path={} | Name={}", book_path, book_name);

        global::bookmarks.push_back({ztd::removeprefix(book_path, "file://"), book_name});
    }
}

void
save_bookmarks() noexcept
{
    if (!global::bookmarks_changed)
    {
        return;
    }
    global::bookmarks_changed = false;

    std::string book_entry;
    for (const auto& [book_path, book_name] : global::bookmarks)
    {
        book_entry.append(std::format("file://{} {}\n", book_path.string(), book_name.string()));
    }

    const auto bookmark_file = vfs::user::config() / "gtk-3.0" / "bookmarks";

    const auto ec = vfs::utils::write_file(bookmark_file, book_entry);
    if (ec)
    {
        logger::error("Failed to write bookmark file: {} {}", bookmark_file.string(), ec.message());
    }
}

void
add_bookmarks(const std::filesystem::path& book_path) noexcept
{
    global::bookmarks_changed = true;

    const auto book_name = book_path.filename();

    global::bookmarks.push_back({book_path, book_name});
}

void
remove_bookmarks() noexcept
{
    global::bookmarks_changed = true;

    // TODO
}
