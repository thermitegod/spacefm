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

#include <ztd/ztd.hxx>

struct AppSettings
{
  public:
    // AppSettings();
    // ~AppSettings();

    bool get_show_thumbnail() const noexcept;
    void set_show_thumbnail(bool val) noexcept;

    u64 get_max_thumb_size() const noexcept;
    void set_max_thumb_size(u64 val) noexcept;

    u64 get_icon_size_big() const noexcept;
    void set_icon_size_big(u64 val) noexcept;

    u64 get_icon_size_small() const noexcept;
    void set_icon_size_small(u64 val) noexcept;

    u64 get_icon_size_tool() const noexcept;
    void set_icon_size_tool(u64 val) noexcept;

    bool get_single_click() const noexcept;
    void set_single_click(bool val) noexcept;

    bool get_single_hover() const noexcept;
    void set_single_hover(bool val) noexcept;

    bool get_click_executes() const noexcept;
    void set_click_executes(bool val) noexcept;

    bool get_confirm() const noexcept;
    void set_confirm(bool val) noexcept;

    bool get_confirm_delete() const noexcept;
    void set_confirm_delete(bool val) noexcept;

    bool get_confirm_trash() const noexcept;
    void set_confirm_trash(bool val) noexcept;

    bool get_load_saved_tabs() const noexcept;
    void set_load_saved_tabs(bool val) noexcept;

    const std::string& get_date_format() const noexcept;
    void set_date_format(const std::string_view val) noexcept;

    u64 get_sort_order() const noexcept;
    void set_sort_order(u64 val) noexcept;

    u64 get_sort_type() const noexcept;
    void set_sort_type(u64 val) noexcept;

    u64 get_width() const noexcept;
    void set_width(u64 val) noexcept;

    u64 get_height() const noexcept;
    void set_height(u64 val) noexcept;

    bool get_maximized() const noexcept;
    void set_maximized(bool val) noexcept;

    bool get_always_show_tabs() const noexcept;
    void set_always_show_tabs(bool val) noexcept;

    bool get_show_close_tab_buttons() const noexcept;
    void set_show_close_tab_buttons(bool val) noexcept;

    bool get_use_si_prefix() const noexcept;
    void set_use_si_prefix(bool val) noexcept;

    bool get_git_backed_settings() const noexcept;
    void set_git_backed_settings(bool val) noexcept;

  private:
    // General Settings
    bool show_thumbnail{false};
    u64 max_thumb_size{8 << 20};

    u64 icon_size_big{48};
    u64 icon_size_small{22};
    u64 icon_size_tool{22};

    bool single_click{false};
    bool single_hover{false};

    bool click_executes{false};

    bool confirm{true};
    bool confirm_delete{true};
    bool confirm_trash{true};

    bool load_saved_tabs{true};

    const std::string date_format_default{"%Y-%m-%d %H:%M:%S"};
    std::string date_format_custom{};

    // Sort by name, size, time
    u64 sort_order{0};
    // ascending, descending
    u64 sort_type{0};

    // Window State
    u64 width{640};
    u64 height{480};
    bool maximized{false};

    // Interface
    bool always_show_tabs{true};
    bool show_close_tab_buttons{false};

    // Units
    bool use_si_prefix{false};

    // Git
    bool git_backed_settings{true};
};

extern AppSettings app_settings;
