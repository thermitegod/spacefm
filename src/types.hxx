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

#include <ztd/ztd.hxx>

#define INT(obj)        (static_cast<i32>(obj))
#define UINT(obj)       (static_cast<u32>(obj))
#define CHAR(obj)       (static_cast<char*>(obj))
#define CONST_CHAR(obj) (static_cast<const char*>(obj))

// clang-format off

// better type names
// using i8    = int8_t;
// using i16   = int16_t;
// using i32   = int32_t;
// using i64   = int64_t;
// using i128  = __int128_t;
//
// using u8    = uint8_t;
// using u16   = uint16_t;
// using u32   = uint32_t;
// using u64   = uint64_t;
// using u128  = __uint128_t;
//
// using f32   = float;
// using f64   = double;
//
// using usize = size_t;
// using isize = ssize_t;

// need too use signed type because neg values are treated
// as control codes and not a tab/panel number
using tab_t   = i64;
using panel_t = i64;

const usize MAX_PANELS = 4;
const std::array<panel_t, MAX_PANELS> PANELS{1, 2, 3, 4};

// clang-format on
