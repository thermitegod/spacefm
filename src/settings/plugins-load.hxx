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

#include "xset/xset-plugins.hxx"

using plugin_func_t = void(std::string_view, PluginUse*, std::string_view, std::string_view,
                           std::string_view);

bool load_user_plugin(std::string_view plug_dir, PluginUse* use, std::string_view plugin,
                      plugin_func_t plugin_func);
