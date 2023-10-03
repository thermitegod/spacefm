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

#include <string>
#include <string_view>

#include <filesystem>

#include <gtkmm.h>

#include <ztd/ztd.hxx>

// clang-format off

/* Icons */
inline constexpr std::string_view ICON_FOLDER              = "folder-symbolic";
inline constexpr std::string_view ICON_FOLDER_DOCUMENTS    = "folder-documents-symbolic";
inline constexpr std::string_view ICON_FOLDER_DOWNLOAD     = "folder-download-symbolic";
inline constexpr std::string_view ICON_FOLDER_MUSIC        = "folder-music-symbolic";
inline constexpr std::string_view ICON_FOLDER_PICTURES     = "folder-pictures-symbolic";
inline constexpr std::string_view ICON_FOLDER_PUBLIC_SHARE = "folder-publicshare-symbolic";
inline constexpr std::string_view ICON_FOLDER_TEMPLATES    = "folder-templates-symbolic";
inline constexpr std::string_view ICON_FOLDER_VIDEOS       = "folder-videos-symbolic";
inline constexpr std::string_view ICON_FOLDER_HOME         = "user-home-symbolic";
inline constexpr std::string_view ICON_FOLDER_DESKTOP      = "user-desktop-symbolic";

/* Fullcolor icons */
inline constexpr std::string_view ICON_FULLCOLOR_FOLDER              = "folder";
inline constexpr std::string_view ICON_FULLCOLOR_FOLDER_DOCUMENTS    = "folder-documents";
inline constexpr std::string_view ICON_FULLCOLOR_FOLDER_DOWNLOAD     = "folder-download";
inline constexpr std::string_view ICON_FULLCOLOR_FOLDER_MUSIC        = "folder-music";
inline constexpr std::string_view ICON_FULLCOLOR_FOLDER_PICTURES     = "folder-pictures";
inline constexpr std::string_view ICON_FULLCOLOR_FOLDER_PUBLIC_SHARE = "folder-publicshare";
inline constexpr std::string_view ICON_FULLCOLOR_FOLDER_TEMPLATES    = "folder-templates";
inline constexpr std::string_view ICON_FULLCOLOR_FOLDER_VIDEOS       = "folder-videos";
inline constexpr std::string_view ICON_FULLCOLOR_FOLDER_HOME         = "user-home";
inline constexpr std::string_view ICON_FULLCOLOR_FOLDER_DESKTOP      = "user-desktop";

// clang-format on

GdkPixbuf* vfs_load_icon(const std::string_view icon_name, i32 icon_size);

const std::string vfs_file_size_format(u64 size_in_bytes, bool decimal = true);

const std::filesystem::path vfs_get_unique_name(const std::filesystem::path& dest_dir,
                                                const std::string_view base_name,
                                                const std::string_view ext);
