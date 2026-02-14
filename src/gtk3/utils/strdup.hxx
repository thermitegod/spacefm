/**
 * Copyright (C) 2023 Brandon Zorn <brandonzorn@cock.li>
 *
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

namespace utils
{
/**
 * @brief strdup
 *
 * - Returns a pointer to a null-terminated byte string.
 *
 * @param[in] str string to duplicate
 *
 * @return A pointer to the newly allocated string.
 * New string must be freed by caller.
 */
[[nodiscard]] char* strdup(const char* str) noexcept;

/**
 * @brief strdup
 *
 * - Returns a pointer to a null-terminated byte string.
 *
 * @param[in] str string to duplicate
 *
 * @return A pointer to the newly allocated string.
 * New string must be freed by caller.
 */
[[nodiscard]] char* strdup(const std::string_view str) noexcept;
} // namespace utils
