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
#include <string_view>

#include <vector>

#include <glib.h>

#include "vfs/vfs-file-info.hxx"

/**
 * std::string
 */

std::vector<std::string>
glist_t_char_to_vector_t_string(GList* list)
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
glist_to_vector_VFSFileInfo(GList* list)
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
vector_to_glist_VFSFileInfo(const std::vector<vfs::file_info>& list)
{
    GList* l = nullptr;
    for (vfs::file_info file : list)
    {
        l = g_list_append(l, file);
    }
    return l;
}
