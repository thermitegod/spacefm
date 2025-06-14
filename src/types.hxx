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

// using a signed type because negative values are used as control codes
using panel_t = i32;

inline constexpr panel_t panel_control_code_prev = -1_i32;
inline constexpr panel_t panel_control_code_next = -2_i32;
inline constexpr panel_t panel_control_code_hide = -3_i32;

inline constexpr panel_t panel_1 = 1_i32;
inline constexpr panel_t panel_2 = 2_i32;
inline constexpr panel_t panel_3 = 3_i32;
inline constexpr panel_t panel_4 = 4_i32;

inline constexpr panel_t MIN_PANELS = panel_1;
inline constexpr panel_t MAX_PANELS = panel_4;

inline constexpr panel_t INVALID_PANEL = 0_i32; // 0 is unused for sanity reasons

inline constexpr std::array<panel_t, MAX_PANELS.data()> PANELS{panel_1, panel_2, panel_3, panel_4};

bool is_valid_panel(panel_t p) noexcept;
bool is_valid_panel_code(panel_t p) noexcept;

// using a signed type because negative values are used as control codes
using tab_t = i32;

inline constexpr tab_t tab_control_code_prev    = -1_i32;
inline constexpr tab_t tab_control_code_next    = -2_i32;
inline constexpr tab_t tab_control_code_close   = -3_i32;
inline constexpr tab_t tab_control_code_restore = -4_i32;

// More than 10 tabs are supported, but keyboard shorcuts
// are only available for the first 10 tabs
inline constexpr tab_t tab_1  = 1_i32;
inline constexpr tab_t tab_2  = 2_i32;
inline constexpr tab_t tab_3  = 3_i32;
inline constexpr tab_t tab_4  = 4_i32;
inline constexpr tab_t tab_5  = 5_i32;
inline constexpr tab_t tab_6  = 6_i32;
inline constexpr tab_t tab_7  = 7_i32;
inline constexpr tab_t tab_8  = 8_i32;
inline constexpr tab_t tab_9  = 9_i32;
inline constexpr tab_t tab_10 = 10_i32;

inline constexpr tab_t MIN_TABS = tab_1;
inline constexpr tab_t MAX_TABS = tab_10; // this is only the max tabs with keybinding support

inline constexpr tab_t INVALID_TAB = 0_i32; // 0 is unused for sanity reasons

inline constexpr std::array<panel_t, MAX_TABS.data()> TABS{tab_1, tab_2, tab_3, tab_4, tab_5, tab_6, tab_7, tab_8, tab_9, tab_10};

bool is_valid_tab(tab_t t) noexcept;
bool is_valid_tab_code(tab_t t) noexcept;

// clang-format on
