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

#include <ztd/ztd.hxx>

namespace config
{
namespace detail
{
struct settings
{
    // General Settings
    bool show_thumbnails{false};
    bool thumbnail_size_limit{true};
    u32 thumbnail_max_size{8 << 20}; // 8 MiB

    i32 icon_size_big{48};
    i32 icon_size_small{22};
    i32 icon_size_tool{22};

    bool single_click{false};
    bool single_hover{false};

    bool click_executes{false};

    bool confirm{true};
    bool confirm_delete{true};
    bool confirm_trash{true};

    bool load_saved_tabs{true};

    // Window State
    u64 width{640};
    u64 height{480};
    bool maximized{false};

    // Interface
    bool always_show_tabs{true};
    bool show_close_tab_buttons{false};
    bool new_tab_here{false};

    // Units
    bool use_si_prefix{false};

    // thumbnailer backend cli/api
    bool thumbnailer_use_api{true};

    // Git
    bool git_backed_settings{true};
};
} // namespace detail

extern detail::settings settings;
} // namespace config
