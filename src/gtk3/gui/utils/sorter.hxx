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

#include <string_view>

#include <ztd/ztd.hxx>

#include "natsort/strnatcmp.hxx"

namespace sorter
{
struct natural
{
    bool
    operator()(const std::string_view& lhs, const std::string_view& rhs) const
    {
        return strnatcmp(lhs, rhs, false) < 0;
    }
};

struct natural_fold
{
    bool
    operator()(const std::string_view& lhs, const std::string_view& rhs) const
    {
        return strnatcmp(lhs, rhs, true) < 0;
    }
};

struct random
{
    bool
    operator()(const std::string_view& lhs, const std::string_view& rhs) const
    {
        (void)lhs;
        (void)rhs;
        return ztd::random<bool>();
    }
};
} // namespace sorter
