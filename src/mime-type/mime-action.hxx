/**
 * Copyright (C) 2007 PCMan <pcman.tw@gmail.com>
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

#include <string>
#include <string_view>

#include <filesystem>

#include <vector>

#include <optional>

namespace mime_type
{
    enum class action
    {
        DEFAULT,
        append,
        remove,
    };
}

/*
 *  Get a list of applications supporting this mime-type
 */
const std::vector<std::string> mime_type_get_actions(const std::string_view type);

/*
 * Add an applications used to open this mime-type
 * desktop_id is the name of *.desktop file.
 *
 * custom_desktop: used to store name of the newly created user-custom desktop file, can be nullptr.
 */
const std::string mime_type_add_action(const std::string_view type,
                                       const std::string_view desktop_id);

/*
 * Get default applications used to open this mime-type
 *
 * The returned string was newly allocated, and should be freed when no longer
 * used.  If nullptr is returned, that means a default app is not set for this
 * mime-type.  This is very roughly based on specs:
 * http://standards.freedesktop.org/mime-apps-spec/mime-apps-spec-latest.html
 *
 * The old defaults.list is also checked.
 */
const char* mime_type_get_default_action(const std::string_view type);

/*
 * Set applications used to open or never used to open this mime-type
 * desktop_id is the name of *.desktop file.
 * action ==
 *     mime_type::action::DEFAULT - make desktop_id the default app
 *     mime_type::action::APPEND  - add desktop_id to Default and Added apps
 *     mime_type::action::REMOVE  - add desktop id to Removed apps
 *
 * http://standards.freedesktop.org/mime-apps-spec/mime-apps-spec-latest.html
 */
void mime_type_update_association(const std::string_view type, const std::string_view desktop_id,
                                  mime_type::action action);

/* Locate the file path of desktop file by desktop_id */
const std::optional<std::filesystem::path>
mime_type_locate_desktop_file(const std::string_view desktop_id);
const std::optional<std::filesystem::path>
mime_type_locate_desktop_file(const std::filesystem::path& dir, const std::string_view desktop_id);
