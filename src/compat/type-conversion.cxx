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

#include <memory>

#include <vector>

#include <glibmm.h>

#include "vfs/vfs-file.hxx"

const std::vector<std::shared_ptr<vfs::file>>
glist_to_vector_vfs_file(GList* list)
{
    std::vector<std::shared_ptr<vfs::file>> vec;
    vec.reserve(g_list_length(list));
    for (GList* l = list; l; l = g_list_next(l))
    {
        vec.push_back(static_cast<vfs::file*>(l->data)->shared_from_this());
    }
    return vec;
}

GList*
vector_to_glist_vfs_file(const std::span<const std::shared_ptr<vfs::file>> list)
{
    GList* l = nullptr;
    for (const auto& file : list)
    {
        l = g_list_append(l, file.get());
    }
    return l;
}
