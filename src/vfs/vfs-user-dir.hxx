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

#include <string>
#include <string_view>

#include <vector>

const std::string& vfs_user_desktop_dir() noexcept;
const std::string& vfs_user_documents_dir() noexcept;
const std::string& vfs_user_download_dir() noexcept;
const std::string& vfs_user_music_dir() noexcept;
const std::string& vfs_user_pictures_dir() noexcept;
const std::string& vfs_user_public_share_dir() noexcept;
const std::string& vfs_user_template_dir() noexcept;
const std::string& vfs_user_videos_dir() noexcept;

const std::string& vfs_user_home_dir() noexcept;
const std::string& vfs_user_cache_dir() noexcept;
const std::string& vfs_user_data_dir() noexcept;
const std::string& vfs_user_config_dir() noexcept;
const std::string& vfs_user_runtime_dir() noexcept;

const std::vector<std::string>& vfs_system_data_dir() noexcept;

const std::string& vfs_current_dir() noexcept;

const std::string& vfs_user_get_config_dir() noexcept;
void vfs_user_set_config_dir(const char* config_dir) noexcept;
