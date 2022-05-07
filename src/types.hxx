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

#include <array>

#include <cstdint>

#define INT(obj)        (static_cast<int>(obj))
#define UINT(obj)       (static_cast<unsigned int>(obj))
#define CHAR(obj)       (static_cast<char*>(obj))
#define CONST_CHAR(obj) (static_cast<const char*>(obj))

#define MAX_PANELS 4

// using signed type because neg values are treated as
// control codes and not a tab/panel number
using tab_t = int64_t;
using panel_t = int64_t;

const std::array<panel_t, 4> PANELS{1, 2, 3, 4};
