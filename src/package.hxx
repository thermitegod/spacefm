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

namespace spacefm
{
struct package_t final
{
    std::string_view name = PACKAGE_NAME;
    std::string_view name_fancy = PACKAGE_NAME_FANCY;
    std::string_view version = PACKAGE_VERSION;
    std::string_view build_root = PACKAGE_BUILD_ROOT;
    struct dialog_t final
    {
        // Standalone dialog program names
        std::string_view about = "spacefm_about";
        std::string_view app_chooser = "spacefm_app_chooser";
        std::string_view bookmarks = "spacefm_bookmarks";
        std::string_view error = "spacefm_error";
        std::string_view file_action = "spacefm_action";
        std::string_view file_batch_rename = "spacefm_batch_rename";
        std::string_view file_chooser = "spacefm_chooser";
        std::string_view file_create = "spacefm_create";
        std::string_view file_rename = "spacefm_rename";
        std::string_view keybindings = "spacefm_keybindings";
        std::string_view keybinding_set = "spacefm_keybinding_set_key";
        std::string_view message = "spacefm_message";
        std::string_view pattern = "spacefm_pattern";
        std::string_view preference = "spacefm_preference";
        std::string_view properties = "spacefm_properties";
        std::string_view text = "spacefm_text";

        std::string_view build_root = DIALOG_BUILD_ROOT;
    };
    dialog_t dialog;
};

/**
 * @brief instance for package information
 */
inline constexpr package_t package{};
} // namespace spacefm
