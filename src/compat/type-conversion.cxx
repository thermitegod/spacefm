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

#include <string>

#include <filesystem>

#include <vector>

#include <glibmm.h>

#include "vfs/vfs-file-info.hxx"

/**
 * std::filesystem::path
 */

std::vector<std::filesystem::path>
glist_to_vector_path(GList* list)
{
    std::vector<std::filesystem::path> vec;
    vec.reserve(g_list_length(list));
    for (GList* l = list; l; l = g_list_next(l))
    {
        const std::filesystem::path open_file = (const char*)(l->data);
        vec.emplace_back(open_file);
    }
    return vec;
}

/**
 * std::string
 */

std::vector<std::string>
glist_to_vector_string(GList* list)
{
    std::vector<std::string> vec;
    vec.reserve(g_list_length(list));
    for (GList* l = list; l; l = g_list_next(l))
    {
        const std::string open_file = (const char*)(l->data);
        vec.emplace_back(open_file);
    }
    return vec;
}

/**
 * VFSFileInfo
 */

std::vector<vfs::file_info>
glist_to_vector_vfs_file_info(GList* list)
{
    std::vector<vfs::file_info> vec;
    vec.reserve(g_list_length(list));
    for (GList* l = list; l; l = g_list_next(l))
    {
        vfs::file_info file = VFS_FILE_INFO(l->data);
        vec.emplace_back(file);
    }
    return vec;
}

GList*
vector_to_glist_vfs_file_info(const std::span<const vfs::file_info> list)
{
    GList* l = nullptr;
    for (const vfs::file_info file : list)
    {
        l = g_list_append(l, file);
    }
    return l;
}
