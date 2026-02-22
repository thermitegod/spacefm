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

#include <cstdint>

#include <gtkmm.h>

// VFS only settings, used to support both gtk3/gtk4 builds
// should be replaced with the regular settings struct when the
// gtk4 build is the only one left.

namespace vfs
{
struct settings
{
#if (GTK_MAJOR_VERSION == 4)
    std::int32_t icon_size_grid;
    std::int32_t icon_size_list;
#elif (GTK_MAJOR_VERSION == 3)
    i32 icon_size_big;
    i32 icon_size_small;
    i32 icon_size_tool;
#endif
};
} // namespace vfs
