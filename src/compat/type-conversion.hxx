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

#include <filesystem>

#include <span>
#include <vector>

#include <glibmm.h>

#include "vfs/vfs-file.hxx"

// std::filesystem::path
const std::vector<std::filesystem::path> glist_to_vector_path(GList* list);

// std::string
const std::vector<std::string> glist_to_vector_string(GList* list);

// vfs::file
const std::vector<std::shared_ptr<vfs::file>> glist_to_vector_vfs_file(GList* list);
GList* vector_to_glist_vfs_file(const std::span<const std::shared_ptr<vfs::file>> list);
