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
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "vfs/error.hxx"

namespace vfs::thumbnail
{
class thumbnailer
{
  private:
    explicit thumbnailer(const std::filesystem::path& path);

  public:
    [[nodiscard]] static std::shared_ptr<vfs::thumbnail::thumbnailer>
    create(const std::filesystem::path& path) noexcept;

    [[nodiscard]] const std::filesystem::path&
    path() const noexcept
    {
        return path_;
    }

    [[nodiscard]] std::string_view
    exec() const noexcept
    {
        return thumbnailer_entry_.exec;
    }

    [[nodiscard]] std::string_view
    try_exec() const noexcept
    {
        return thumbnailer_entry_.try_exec;
    }

    [[nodiscard]] std::span<const std::string>
    mime_types() const noexcept
    {
        return thumbnailer_entry_.mime_types;
    }

  private:
    std::filesystem::path path_;

    vfs::error_code parse_thumbnailer_file() noexcept;

    struct thumbnailer_entry_data final
    {
        std::string exec;
        std::string try_exec;
        std::vector<std::string> mime_types;
    };
    thumbnailer_entry_data thumbnailer_entry_;
};
} // namespace vfs::thumbnail
