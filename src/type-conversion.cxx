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
#include <vector>

#include <glib.h>

std::vector<std::string>
glist_t_char_to_vector_t_string(GList* list)
{
    std::vector<std::string> vec;
    GList* l;
    for (l = list; l; l = l->next)
    {
        std::string open_file = (const char*)(l->data);
        vec.push_back(open_file);
    }
    return vec;
}
