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

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

#include <gtkmm.h>

#include <ztd/ztd.hxx>

namespace ptk::dialog
{
std::tuple<bool, std::string> text(GtkWidget* parent, const std::string_view title,
                                   const std::string_view message, const std::string_view defstring,
                                   const std::string_view defreset) noexcept;

std::optional<std::filesystem::path>
file_chooser(GtkWidget* parent, GtkFileChooserAction action, const std::string_view title,
             const std::optional<std::filesystem::path>& deffolder,
             const std::optional<std::filesystem::path>& deffile) noexcept;

GtkResponseType message(GtkWindow* parent, GtkMessageType action, const std::string_view title,
                        GtkButtonsType buttons, const std::string_view message,
                        const std::string_view secondary_message = "") noexcept;

void error(GtkWindow* parent, const std::string_view title,
           const std::string_view message) noexcept;
} // namespace ptk::dialog
