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

#include <optional>
#include <string>
#include <string_view>

#include <gtkmm.h>

namespace ptk::utils
{
GtkTextView* multi_input_new(GtkScrolledWindow* scrolled,
                             const std::string_view text = "") noexcept;

// returns a string or nullopt if input is empty
std::optional<std::string> multi_input_get_text(GtkWidget* input) noexcept;
} // namespace ptk::utils
