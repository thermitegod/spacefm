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

#include <cstdint>

namespace vfs::thumbnail
{
enum class thumbnail_size : std::int32_t
{
    normal = 128,
    large = 256,
    x_large = 512,
    xx_large = 1024,
};
[[nodiscard]] thumbnail_size get_spec_size(const std::int32_t size) noexcept;

struct cache_paths final
{
    // https://specifications.freedesktop.org/thumbnail-spec/latest/directory.html
    std::filesystem::path normal;
    std::filesystem::path large;
    std::filesystem::path x_large;
    std::filesystem::path xx_large;
    std::filesystem::path fail;
};
[[nodiscard]] cache_paths thumbnail_cache() noexcept;

[[nodiscard]] std::filesystem::path cache_parent() noexcept;
[[nodiscard]] std::filesystem::path cache_normal() noexcept;
[[nodiscard]] std::filesystem::path cache_large() noexcept;
[[nodiscard]] std::filesystem::path cache_x_large() noexcept;
[[nodiscard]] std::filesystem::path cache_xx_large() noexcept;
[[nodiscard]] std::filesystem::path cache_fail() noexcept;

[[nodiscard]] std::filesystem::path get_cache_path(const thumbnail_size size) noexcept;
} // namespace vfs::thumbnail
