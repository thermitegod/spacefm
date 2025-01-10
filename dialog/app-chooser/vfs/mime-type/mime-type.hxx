/**
 * Copyright 2007 Houng Jen Yee (PCMan) <pcman.tw@gmail.com>
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

namespace vfs::detail::mime_type
{
/*
 * Get mime-type info of the specified file. To determine the mime-type
 * of the file, lookup the mime-type of file extension from mime.cache.
 * If the mime-type could not be determined, the content of
 * the file will be checked, which is much more time-consuming.
 */
[[nodiscard]] std::string get_by_file(const std::filesystem::path& path) noexcept;

[[nodiscard]] bool is_text(const std::string_view mime_type) noexcept;
[[nodiscard]] bool is_executable(const std::string_view mime_type) noexcept;
[[nodiscard]] bool is_archive(const std::string_view mime_type) noexcept;
[[nodiscard]] bool is_video(const std::string_view mime_type) noexcept;
[[nodiscard]] bool is_audio(const std::string_view mime_type) noexcept;
[[nodiscard]] bool is_image(const std::string_view mime_type) noexcept;
[[nodiscard]] bool is_unknown(const std::string_view mime_type) noexcept;

/* Get human-readable description and icon name of the mime-type.
 *
 * Note: Spec is not followed for icon.  If icon tag is found in .local
 * xml file, it is used.  Otherwise vfs_mime_type_get_icon guesses the icon.
 * The Freedesktop spec /usr/share/mime/generic-icons is NOT parsed.
 */
[[nodiscard]] std::array<std::string, 2> get_desc_icon(const std::string_view type) noexcept;
} // namespace vfs::detail::mime_type
