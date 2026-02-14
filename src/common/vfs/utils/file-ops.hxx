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
#include <string>
#include <vector>

#include "vfs/error.hxx"

#include "logger.hxx"

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
    constexpr std::size_t CHUNK_SIZE = 4096;

    auto file = std::ifstream(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) [[unlikely]]
    {
        logger::error<logger::vfs>("Failed to open file for reading: {}", path.string());
        return std::unexpected{vfs::error_code::file_open_failure};
    }

    std::string result;

    while (file.good())
    {
        std::vector<char> buffer(CHUNK_SIZE, 0);

        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        result.append(buffer.data(), static_cast<std::string::size_type>(file.gcount()));
        if (result.size() > max_size) [[unlikely]]
        {
            return std::unexpected{vfs::error_code::file_too_large};
        }
    }

    if (file.fail() && !file.eof()) [[unlikely]]
    {
        logger::error<logger::vfs>("Failed to read file: {}", path.string());
        return std::unexpected{vfs::error_code::file_read_failure};
    }

    file.close();

    if (file.is_open()) [[unlikely]]
    {
        logger::error<logger::vfs>("Failed to close file: {}", path.string());
        return std::unexpected{vfs::error_code::file_close_failure};
    }

    return result;
}

/**
 * Read part of a file into a string.
 *
 * @param[in] path - file to read
 * @param[in] size - number of bytes to read from a file
 *
 * @return the files contents in a string or an error_code
 */
[[nodiscard]] inline std::expected<std::string, std::error_code>
read_file_partial(const std::filesystem::path& path, const std::size_t size) noexcept
{
    auto file = std::ifstream(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) [[unlikely]]
    {
        logger::error<logger::vfs>("Failed to open file for reading: {}", path.string());
        return std::unexpected{vfs::error_code::file_open_failure};
    }

    std::string result;
    std::vector<char> buffer(size, 0);

    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    result.append(buffer.data(), static_cast<std::string::size_type>(file.gcount()));

    if (file.fail() && !file.eof()) [[unlikely]]
    {
        logger::error<logger::vfs>("Failed to read file: {}", path.string());
        return std::unexpected{vfs::error_code::file_read_failure};
    }

    file.close();

    if (file.is_open()) [[unlikely]]
    {
        logger::error<logger::vfs>("Failed to close file: {}", path.string());
        return std::unexpected{vfs::error_code::file_close_failure};
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
write_file(const std::filesystem::path& path, auto&& buffer) noexcept
{
    auto file = std::ofstream(path.c_str(), std::ios::out);
    if (!file.is_open()) [[unlikely]]
    {
        logger::error<logger::vfs>("Failed to open file for writing: {}", path.string());
        return {vfs::error_code::file_open_failure};
    }

    file.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));

    if (file.fail()) [[unlikely]]
    {
        logger::error<logger::vfs>("Failed to write file: {}", path.string());
        return {vfs::error_code::file_write_failure};
    }

    file.close();

    if (file.fail()) [[unlikely]]
    {
        logger::error<logger::vfs>("Failed to close file: {}", path.string());
        return {vfs::error_code::file_close_failure};
    }

    return {};
}
} // namespace vfs::utils
