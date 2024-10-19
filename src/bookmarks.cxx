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
#include <string_view>

#include <format>

#include <filesystem>

#include <vector>
#include <span>

#include <fstream>

#include <ztd/ztd.hxx>

#include "logger.hxx"

#include "vfs/vfs-user-dirs.hxx"

#include "utils/write.hxx"

#include "bookmarks.hxx"

// Bookmark Path, Bookmark Name
namespace global
{
static std::vector<bookmark_t> bookmarks;
static bool bookmarks_changed = false;
static std::filesystem::path bookmark_file;
} // namespace global

const std::span<bookmark_t>
get_all_bookmarks() noexcept
{
    return global::bookmarks;
}

static void
parse_bookmarks(const std::string_view raw_line) noexcept
{
    const auto line = ztd::strip(raw_line); // remove newline

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

void
load_bookmarks() noexcept
{
    // If not the first time calling then need to remove loaded bookmarks
    if (!global::bookmarks.empty())
    {
        global::bookmarks.clear();
    }

    if (global::bookmark_file.empty())
    {
        global::bookmark_file = vfs::user::config() / "gtk-3.0" / "bookmarks";
    }

    // no bookmark file
    if (!std::filesystem::exists(global::bookmark_file))
    {
        return;
    }

    std::ifstream file(global::bookmark_file);
    if (!file)
    {
        logger::error("Failed to open the file: {}", global::bookmark_file.string());
        return;
    }

    std::string line;
    while (std::getline(file, line))
    {
        parse_bookmarks(line);
    }
    file.close();
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

    ::utils::write_file(global::bookmark_file, book_entry);
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
