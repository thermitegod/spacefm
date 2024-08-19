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

#include <malloc.h>

#include <ztd/ztd.hxx>

#include "logger.hxx"

#include "utils/memory.hxx"

void
utils::memory_trim() noexcept
{
    const auto ret = malloc_trim(0);
    if (ret == 0)
    {
        logger::warn("Bad malloc_trim()");
    }
}
