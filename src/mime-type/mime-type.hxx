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

#include <string>
#include <string_view>

#include <vector>

#include "mime-type/mime-cache.hxx"

inline constexpr std::string_view XDG_MIME_TYPE_UNKNOWN{"application/octet-stream"};
inline constexpr std::string_view XDG_MIME_TYPE_DIRECTORY{"inode/directory"};
inline constexpr std::string_view XDG_MIME_TYPE_EXECUTABLE{"application/x-executable"};
inline constexpr std::string_view XDG_MIME_TYPE_PLAIN_TEXT{"text/plain"};

/* Initialize the library */
void mime_type_init();

/* Finalize the library and free related resources */
void mime_type_finalize();

/*
 * Get mime-type info of the specified file (slow, but more accurate):
 * To determine the mime-type of the file, mime_type_get_by_filename() is
 * tried first.  If the mime-type could not be determined, the content of
 * the file will be checked, which is much more time-consuming.
 * If statbuf is not nullptr, it will be used to determine if the file is a directory,
 * or if the file is an executable file; otherwise, the function will call stat()
 * to gather this info itself. So if you already have stat info of the file,
 * pass it to the function to prevent checking the file stat again.
 * If you have basename of the file, pass it to the function can improve the
 * efifciency, too. Otherwise, the function will try to get the basename of
 * the specified file again.
 */
const std::string mime_type_get_by_file(std::string_view filepath);

bool mime_type_is_text_file(std::string_view file_path, std::string_view mime_type = "");
bool mime_type_is_executable_file(std::string_view file_path, std::string_view mime_type = "");

/* Get human-readable description and icon name of the mime-type
 * If locale is nullptr, current locale will be used.
 * The returned string should be freed when no longer used.
 * The icon_name will only be set if points to nullptr, and must be freed. */
const std::string mime_type_get_desc_icon(std::string_view type, std::string_view locale,
                                          char** icon_name);

/*
 * Get mime caches
 */
const std::vector<mime_cache_t>& mime_type_get_caches();

void mime_type_regen_all_caches();
