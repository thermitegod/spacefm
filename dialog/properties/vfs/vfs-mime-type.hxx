/**
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#include <string>
#include <string_view>

#include <filesystem>

#include <memory>

#include <gdkmm.h>

#include <ztd/ztd.hxx>

namespace vfs
{
namespace constants::mime_type
{
inline constexpr std::string_view unknown{"application/octet-stream"};
inline constexpr std::string_view directory{"inode/directory"};
inline constexpr std::string_view executable{"application/x-executable"};
inline constexpr std::string_view plain_text{"text/plain"};
} // namespace constants::mime_type

struct mime_type
{
  public:
    mime_type() = delete;
    explicit mime_type(const std::string_view type) noexcept;
    ~mime_type() noexcept;
    mime_type(const mime_type& other) = delete;
    mime_type(mime_type&& other) = delete;
    mime_type& operator=(const mime_type& other) = delete;
    mime_type& operator=(mime_type&& other) = delete;

    [[nodiscard]] static std::shared_ptr<vfs::mime_type>
    create_from_file(const std::filesystem::path& path) noexcept;

    [[nodiscard]] static std::shared_ptr<vfs::mime_type>
    create_from_type(const std::string_view type) noexcept;

    // Get mime-type string
    [[nodiscard]] std::string_view type() const noexcept;

    // Get human-readable description of mime-type
    [[nodiscard]] std::string_view description() const noexcept;

    [[nodiscard]] bool is_archive() const noexcept;
    [[nodiscard]] bool is_executable() const noexcept;
    [[nodiscard]] bool is_text() const noexcept;
    [[nodiscard]] bool is_image() const noexcept;
    [[nodiscard]] bool is_video() const noexcept;
    [[nodiscard]] bool is_audio() const noexcept;

  private:
    [[nodiscard]] static std::shared_ptr<vfs::mime_type>
    create(const std::string_view type) noexcept;

    std::string type_;
    std::string description_;
};
} // namespace vfs
