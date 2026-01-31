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

#include <gtkmm.h>

#if (GTK_MAJOR_VERSION == 4)

#include <chrono>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include <glaze/glaze.hpp>

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
        bookmark_disk_format::bookmark_data{bookmark_disk_format::version, this->bookmarks_};

    std::string buffer;
    const auto ec =
        glz::write_file_json<glz::opts{.prettify = true}>(data,
                                                          bookmark_disk_format::path.c_str(),
                                                          buffer);

    if (ec)
    {
        // logger::error("Failed to write bookmark file: {}", glz::format_error(ec, buffer));

        this->signal_save_error().emit(glz::format_error(ec, buffer));
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
    if (statx->mtime() == this->bookmark_mtime_)
    { // Bookmark file has not been modified since last read
        return;
    }
    this->bookmark_mtime_ = statx->mtime();

    bookmark_disk_format::bookmark_data config_data;
    std::string buffer;
    const auto ec = glz::read_file_json<glz::opts{.error_on_unknown_keys = false}>(
        config_data,
        bookmark_disk_format::path.c_str(),
        buffer);

    if (ec)
    {
        // logger::error("Failed to load bookmark file: {}", glz::format_error(ec, buffer));

        this->signal_load_error().emit(glz::format_error(ec, buffer));
        return;
    }

    this->bookmarks_ = config_data.bookmarks;
}

void
vfs::bookmarks::add(const std::filesystem::path& path) noexcept
{
    this->load();

    const auto now = std::chrono::system_clock::now();

    for (auto& bookmark : this->bookmarks_)
    {
        if (bookmark.path == path)
        {
            bookmark.created = now;
            this->save();
            return;
        }
    }

    this->bookmarks_.push_back({path.filename().string(), path, now});
    this->save();
}

void
vfs::bookmarks::remove(const std::filesystem::path& path) noexcept
{
    this->load();

    std::erase_if(this->bookmarks_,
                  [&path](const bookmark_data& bookmark) { return bookmark.path == path; });

    this->save();
}

void
vfs::bookmarks::remove_all() noexcept
{
    this->bookmarks_.clear();
    this->save();
}

std::span<const vfs::bookmarks::bookmark_data>
vfs::bookmarks::get_bookmarks() noexcept
{
    this->load();

    return this->bookmarks_;
}

#else

#include <chrono>
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

    logger::error_if(ec, "Failed to bookmark file: {}", glz::format_error(ec, buffer));
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
#endif
