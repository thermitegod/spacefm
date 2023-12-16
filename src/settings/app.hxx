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
    [[nodiscard]] bool show_thumbnail() const noexcept;
    void show_thumbnail(bool val) noexcept;

    [[nodiscard]] bool thumbnail_size_limit() const noexcept;
    void thumbnail_size_limit(bool val) noexcept;

    [[nodiscard]] u32 max_thumb_size() const noexcept;
    void max_thumb_size(u32 val) noexcept;

    [[nodiscard]] i32 icon_size_big() const noexcept;
    void icon_size_big(i32 val) noexcept;

    [[nodiscard]] i32 icon_size_small() const noexcept;
    void icon_size_small(i32 val) noexcept;

    [[nodiscard]] i32 icon_size_tool() const noexcept;
    void icon_size_tool(i32 val) noexcept;

    [[nodiscard]] bool single_click() const noexcept;
    void single_click(bool val) noexcept;

    [[nodiscard]] bool single_hover() const noexcept;
    void single_hover(bool val) noexcept;

    [[nodiscard]] bool click_executes() const noexcept;
    void click_executes(bool val) noexcept;

    [[nodiscard]] bool confirm() const noexcept;
    void confirm(bool val) noexcept;

    [[nodiscard]] bool confirm_delete() const noexcept;
    void confirm_delete(bool val) noexcept;

    [[nodiscard]] bool confirm_trash() const noexcept;
    void confirm_trash(bool val) noexcept;

    [[nodiscard]] bool load_saved_tabs() const noexcept;
    void load_saved_tabs(bool val) noexcept;

    [[nodiscard]] u64 width() const noexcept;
    void width(u64 val) noexcept;

    [[nodiscard]] u64 height() const noexcept;
    void height(u64 val) noexcept;

    [[nodiscard]] bool maximized() const noexcept;
    void maximized(bool val) noexcept;

    [[nodiscard]] bool always_show_tabs() const noexcept;
    void always_show_tabs(bool val) noexcept;

    [[nodiscard]] bool show_close_tab_buttons() const noexcept;
    void show_close_tab_buttons(bool val) noexcept;

    [[nodiscard]] bool use_si_prefix() const noexcept;
    void use_si_prefix(bool val) noexcept;

    [[nodiscard]] bool thumbnailer_use_api() const noexcept;
    void thumbnailer_use_api(bool val) noexcept;

    [[nodiscard]] bool git_backed_settings() const noexcept;
    void git_backed_settings(bool val) noexcept;

  private:
    // General Settings
    bool show_thumbnails_{false};
    bool thumbnail_size_limit_{true};
    u32 thumbnail_max_size_{8 << 20}; // 8 MiB

    i32 icon_size_big_{48};
    i32 icon_size_small_{22};
    i32 icon_size_tool_{22};

    bool single_click_{false};
    bool single_hover_{false};

    bool click_executes_{false};

    bool confirm_{true};
    bool confirm_delete_{true};
    bool confirm_trash_{true};

    bool load_saved_tabs_{true};

    // Window State
    u64 width_{640};
    u64 height_{480};
    bool maximized_{false};

    // Interface
    bool always_show_tabs_{true};
    bool show_close_tab_buttons_{false};

    // Units
    bool use_si_prefix_{false};

    // thumbnailer backend cli/api
    bool thumbnailer_use_api_{true};

    // Git
    bool git_backed_settings_{true};
};

extern AppSettings app_settings;
