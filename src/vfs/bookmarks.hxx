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

#pragma once

#include <filesystem>
#include <span>

#include <gtkmm.h>

#if (GTK_MAJOR_VERSION == 4)

namespace vfs
{
class bookmarks
{
  public:
    struct bookmark_data final
    {
        std::string name;
        std::filesystem::path path;
        std::chrono::system_clock::time_point created;
    };

    void save() noexcept;
    void load() noexcept;

    void add(const std::filesystem::path& path) noexcept;
    void remove(const std::filesystem::path& path) noexcept;
    void remove_all() noexcept;

    [[nodiscard]] std::span<const bookmark_data> get_bookmarks() noexcept;

  private:
    std::vector<bookmark_data> bookmarks_;
    std::chrono::system_clock::time_point bookmark_mtime_;

  public:
    [[nodiscard]] auto
    signal_load_error() noexcept
    {
        return this->signal_load_error_;
    }

    [[nodiscard]] auto
    signal_save_error() noexcept
    {
        return this->signal_save_error_;
    }

  private:
    sigc::signal<void(std::string)> signal_load_error_;
    sigc::signal<void(std::string)> signal_save_error_;
};
} // namespace vfs

#else

#include "datatypes/datatypes.hxx"

namespace vfs::bookmarks
{
[[nodiscard]] std::span<datatype::bookmarks::bookmark> bookmarks() noexcept;

void bookmarks(const datatype::bookmarks::bookmarks& bookmarks) noexcept;

void load() noexcept;
void save() noexcept;

void add(const std::filesystem::path& path) noexcept;
void remove(const std::filesystem::path& path) noexcept;
} // namespace vfs::bookmarks

#endif
