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

// using a signed type because negative values are used as control codes
using panel_t = i64;

inline constexpr panel_t panel_control_code_prev = -1;
inline constexpr panel_t panel_control_code_next = -2;
inline constexpr panel_t panel_control_code_hide = -3;

inline constexpr panel_t panel_1 = 1;
inline constexpr panel_t panel_2 = 2;
inline constexpr panel_t panel_3 = 3;
inline constexpr panel_t panel_4 = 4;

inline constexpr panel_t MIN_PANELS = panel_1;
inline constexpr panel_t MAX_PANELS = panel_4;

inline constexpr panel_t INVALID_PANEL = 0; // 0 is unused for sanity reasons

inline constexpr std::array<panel_t, MAX_PANELS> PANELS{panel_1, panel_2, panel_3, panel_4};

bool is_valid_panel(panel_t p) noexcept;
bool is_valid_panel_code(panel_t p) noexcept;

// using a signed type because negative values are used as control codes
using tab_t = i64;

inline constexpr tab_t tab_control_code_prev    = -1;
inline constexpr tab_t tab_control_code_next    = -2;
inline constexpr tab_t tab_control_code_close   = -3;
inline constexpr tab_t tab_control_code_restore = -4;

// More than 10 tabs are supported, but keyboard shorcuts
// are only available for the first 10 tabs
inline constexpr tab_t tab_1  = 1;
inline constexpr tab_t tab_2  = 2;
inline constexpr tab_t tab_3  = 3;
inline constexpr tab_t tab_4  = 4;
inline constexpr tab_t tab_5  = 5;
inline constexpr tab_t tab_6  = 6;
inline constexpr tab_t tab_7  = 7;
inline constexpr tab_t tab_8  = 8;
inline constexpr tab_t tab_9  = 9;
inline constexpr tab_t tab_10 = 10;

inline constexpr tab_t MIN_TABS = tab_1;
inline constexpr tab_t MAX_TABS = tab_10; // this is only the max tabs with keybinding support

inline constexpr tab_t INVALID_TAB = 0; // 0 is unused for sanity reasons

inline constexpr std::array<panel_t, MAX_TABS> TABS{tab_1, tab_2, tab_3, tab_4, tab_5, tab_6, tab_7, tab_8, tab_9, tab_10};

bool is_valid_tab(tab_t t) noexcept;
bool is_valid_tab_code(tab_t t) noexcept;

// clang-format on
