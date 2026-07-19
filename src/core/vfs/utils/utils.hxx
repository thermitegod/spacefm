/**
 * Copyright 2008 PCMan <pcman.tw@gmail.com>
 *
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

#include <array>
#include <filesystem>
#include <string>
#include <string_view>

#include <ztd/ztd.hxx>

namespace vfs::utils
{
[[nodiscard]] std::string format_file_size(u64 size_in_bytes, bool decimal = true) noexcept;

/**
 * Split a filename into its stem and extension,
 * unlike using std::filesystem::path::stem/std::filesystem::path::extension
 * this will support multi part extensions such as tar archives, i.e. .tar.gz,.tar.zst,etc...
 * does not check if the filename is a directory, that should be checked by the caller.
 */
[[nodiscard]] std::array<std::string, 2>
filename_stem_and_extension(const std::filesystem::path& filename) noexcept;

/**
 * @brief unique_path
 *
 * - Create a unique path given a base path and a filename. If the filename already exists
 *   then the filename will be modified with an integer counter.
 *
 * @param[in] path The path the filename will be in, does not get modified. Must be a directory.
 * @param[in] filename The filename, if an file already exists with this name it will be modified.
 * @param[in] tag String to be appended before the numeric counter if a duplicate files exist.
 *
 * @return A unique path
 */
[[nodiscard]] std::filesystem::path unique_path(const std::filesystem::path& path,
                                                const std::filesystem::path& filename,
                                                const std::string_view tag = "") noexcept;
} // namespace vfs::utils
