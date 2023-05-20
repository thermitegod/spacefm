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

#include <filesystem>

#include <exception>

namespace spacefm
{
    enum class script
    {
        spacefm_auth,
        config_update,
        config_update_git,
    };
}

bool script_exists(spacefm::script script) noexcept;
bool script_exists(const std::filesystem::path& script) noexcept;

const std::filesystem::path get_script_path(spacefm::script script) noexcept;
