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

#include <string>
#include <string_view>

#include <filesystem>

#include <tuple>

#include <optional>

#include <gtkmm.h>

GtkTextView* multi_input_new(GtkScrolledWindow* scrolled, const char* text);
const std::optional<std::string> multi_input_get_text(GtkWidget* input);

const std::tuple<bool, std::string>
xset_text_dialog(GtkWidget* parent, const std::string_view title, const std::string_view msg1,
                 const std::string_view msg2, const std::string_view defstring,
                 const std::string_view defreset, bool edit_care);

const std::optional<std::filesystem::path>
xset_file_dialog(GtkWidget* parent, GtkFileChooserAction action, const std::string_view title,
                 const std::optional<std::filesystem::path>& deffolder,
                 const std::optional<std::filesystem::path>& deffile);
