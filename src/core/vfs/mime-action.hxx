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

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vfs::mime_action
{
/*
 *  Get a list of applications supporting this mime-type
 */
[[nodiscard]] std::vector<std::string> actions(std::string_view mime_type) noexcept;

/*
 * Add an applications used to open this mime-type
 * desktop_id is the name of *.desktop file.
 */
[[nodiscard]] std::optional<std::string> add_action(std::string_view mime_type,
                                                    std::string_view desktop_id) noexcept;

/*
 * Get default applications used to open this mime-type
 *
 * If std::nullopt is returned, that means a default app is not set for this mime-type.
 */
[[nodiscard]] std::optional<std::string> default_action(std::string_view mime_type) noexcept;

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
void set_default_action(std::string_view mime_type, std::string_view desktop_id) noexcept;
} // namespace vfs::mime_action
