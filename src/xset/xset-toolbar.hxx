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

#include <optional>

#include <gtkmm.h>
#include <gdkmm.h>

#include <ztd/ztd.hxx>

#include "xset/xset.hxx"

void xset_fill_toolbar(GtkWidget* parent, ptk::browser* file_browser, GtkToolbar* toolbar,
                       const xset_t& set_parent, bool show_tooltips) noexcept;

const std::string xset_get_builtin_toolitem_label(xset::tool tool_type) noexcept;
const xset_t xset_new_builtin_toolitem(xset::tool tool_type) noexcept;
void xset_builtin_tool_activate(xset::tool tool_type, const xset_t& set, GdkEvent* event) noexcept;

struct builtin_tool_data
{
    std::optional<std::string> name;
    std::optional<std::string> icon;
    std::optional<xset::name> shared_key;
};
const ztd::map<xset::tool, builtin_tool_data, 19>& xset_toolbar_builtin_tools() noexcept;
