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

#include <string>

#include <ztd/ztd.hxx>

#include "vfs/utils/vfs-utils.hxx"

std::string
vfs::utils::format_file_size(u64 size_in_bytes, bool decimal) noexcept
{
    return ztd::format_filesize(size_in_bytes, ztd::base::iec, decimal ? 1 : 0);
}

vfs::utils::split_basename_extension_data
vfs::utils::split_basename_extension(const std::filesystem::path& filename) noexcept
{
    if (std::filesystem::is_directory(filename))
    {
        return {filename.string(), "", false};
    }

    // Find the last dot in the filename
    const auto dot_pos = filename.string().find_last_of('.');

    // Check if the dot is not at the beginning or end of the filename
    if (dot_pos != std::string::npos && dot_pos != 0 && dot_pos != filename.string().length() - 1)
    {
        const auto split = ztd::rpartition(filename.string(), ".");

        // Check if the extension is a compressed tar archive
        if (split[0].ends_with(".tar"))
        {
            // Find the second last dot in the filename
            const auto split_second = ztd::rpartition(split[0], ".");

            return {split_second[0], std::format(".{}.{}", split_second[2], split[2]), true};
        }
        else
        {
            // Return the basename and the extension
            return {split[0], std::format(".{}", split[2]), false};
        }
    }

    // No valid extension found, return the whole filename as the basename
    return {filename.string(), "", false};
}

std::filesystem::path
vfs::utils::unique_path(const std::filesystem::path& path, const std::filesystem::path& filename,
                        const std::string_view tag) noexcept
{
    assert(!path.empty());
    assert(!filename.empty());

    const auto parts = split_basename_extension(filename);

    u32 n = 1;
    auto unique_path = path / std::format("{}{}", parts.basename, parts.extension);
    while (std::filesystem::exists(unique_path))
    { // need to see broken symlinks
        unique_path = path / std::format("{}{}{}{}", parts.basename, tag, n, parts.extension);
        n += 1;
    }

    return unique_path;
}
