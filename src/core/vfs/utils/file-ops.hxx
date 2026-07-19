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

#include <expected>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>

#include "vfs/error.hxx"

namespace vfs::utils
{
/**
 * Read an entire file into a string. Does not support no partial reads, either
 * the entire contents of a file are returned or an error. Works with zero size
 * files in /sys and /proc.
 *
 * @param[in] path - file to read
 * @param[in] max_size - max size that will be read from a file, if the file is
 *                       larger than this the error file_too_large is returned.
 *                       The default max size is 10MiB.
 *
 * @return the files contents in a string or an error_code
 */
[[nodiscard]] inline std::expected<std::string, std::error_code>
read_file(const std::filesystem::path& path,
          const std::size_t max_size = (10uz * 1024uz * 1024uz)) noexcept
{
    std::error_code ec;
    const std::size_t file_size = std::filesystem::file_size(path, ec);

    if (!ec && file_size > max_size)
    {
        return std::unexpected{vfs::error_code::file_too_large};
    }

    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file)
    {
        return std::unexpected{vfs::error_code::file_open_failure};
    }

    constexpr std::size_t CHUNK_SIZE = 4096;
    std::array<char, CHUNK_SIZE> buffer;

    std::string result;
    if (!ec && file_size > 0)
    {
        result.reserve(file_size);
    }

    while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0)
    {
        const auto bytes_read = static_cast<std::size_t>(file.gcount());

        if (result.size() + bytes_read > max_size)
        {
            return std::unexpected{vfs::error_code::file_too_large};
        }

        result.append(buffer.data(), bytes_read);
    }

    if (file.bad())
    {
        return std::unexpected{vfs::error_code::file_read_failure};
    }

    return result;
}

/**
 * Write a file from a text buffer.
 *
 * @param[in] path - file to write
 * @param[in] buffer - text that is written into a file
 *
 * @return the result of the write as an error_code
 */
[[nodiscard]] inline std::error_code
write_file(const std::filesystem::path& path, std::span<const char> buffer) noexcept
{
    std::ofstream file(path, std::ios::out | std::ios::binary);
    if (!file.is_open())
    {
        return vfs::error_code::file_open_failure;
    }

    file.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));

    if (file.fail())
    {
        return vfs::error_code::file_write_failure;
    }

    file.close();
    if (file.fail())
    {
        return vfs::error_code::file_close_failure;
    }

    return {};
}
} // namespace vfs::utils
