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

#include <gtk/gtk.h>

inline constexpr const std::string_view PTK_DLG_FIND_FILES("find-files3.ui");
inline constexpr const std::string_view PTK_DLG_ABOUT("about-dlg3.ui");
inline constexpr const std::string_view PTK_DLG_APP_CHOOSER("appchooserdlg3.ui");
inline constexpr const std::string_view PTK_DLG_FILE_PROPERTIES("file_properties3.ui");

GtkBuilder* ptk_gtk_builder_new_from_file(const std::filesystem::path& file);
