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

#include <array>
#include <vector>

#include <iostream>

#include <filesystem>

#include <fmt/format.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-user-dir.hxx"

#include "write.hxx"
#include "bookmarks.hxx"

// Bookmark Path, Bookmark Name
static all_bookmarks_t bookmarks;

static bool bookmarks_changed = false;
static std::string bookmark_file;

const all_bookmarks_t&
get_all_bookmarks() noexcept
{
    return bookmarks;
}

static void
parse_bookmarks(std::string_view raw_line) noexcept
{
    const std::string line = ztd::strip(raw_line); // remove newline

    const auto book_parts = ztd::rpartition(line, " ");

    const std::string book_path = book_parts[0];
    const std::string book_name = book_parts[2];

    if (book_path.empty())
        return;

    // LOG_INFO("Bookmark: Path={} | Name={}", book_path, book_name);

    bookmarks.push_back({ztd::removeprefix(book_path, "file://"), book_name});
}

void
load_bookmarks() noexcept
{
    // If not the first time calling then need to remove loaded bookmarks
    if (!bookmarks.empty())
        bookmarks.clear();

    if (bookmark_file.empty())
        bookmark_file = Glib::build_filename(vfs_user_config_dir(), "gtk-3.0", "bookmarks");

    // no bookmark file
    if (!std::filesystem::exists(bookmark_file))
        return;

    std::string line;
    std::ifstream file(bookmark_file);
    if (file.is_open())
    {
        while (std::getline(file, line))
        {
            parse_bookmarks(line);
        }
    }
    file.close();
}

void
save_bookmarks() noexcept
{
    if (!bookmarks_changed)
        return;
    bookmarks_changed = false;

    std::string book_entry;
    for (auto [book_path, book_name]: bookmarks)
    {
        book_entry.append(fmt::format("file://{} {}\n", book_path, book_name));
    }

    write_file(bookmark_file, book_entry);
}

void
add_bookmarks(std::string_view book_path) noexcept
{
    bookmarks_changed = true;

    const std::string book_name = ztd::rpartition(book_path, "/")[2];

    bookmarks.push_back({book_path.data(), book_name});
}

void
remove_bookmarks() noexcept
{
    bookmarks_changed = true;

    // TODO
}
