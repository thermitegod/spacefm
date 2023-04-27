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

#include <string>
#include <string_view>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "settings/app.hxx"

AppSettings app_settings = AppSettings();

bool
AppSettings::get_show_thumbnail() const noexcept
{
    return this->show_thumbnail;
}

void
AppSettings::set_show_thumbnail(bool val) noexcept
{
    this->show_thumbnail = val;
}

u64
AppSettings::get_max_thumb_size() const noexcept
{
    return this->max_thumb_size;
}

void
AppSettings::set_max_thumb_size(u64 val) noexcept
{
    this->max_thumb_size = val;
}

u64
AppSettings::get_icon_size_big() const noexcept
{
    return this->icon_size_big;
}

void
AppSettings::set_icon_size_big(u64 val) noexcept
{
    this->icon_size_big = val;
}

u64
AppSettings::get_icon_size_small() const noexcept
{
    return this->icon_size_small;
}

void
AppSettings::set_icon_size_small(u64 val) noexcept
{
    this->icon_size_small = val;
}

u64
AppSettings::get_icon_size_tool() const noexcept
{
    return this->icon_size_tool;
}

void
AppSettings::set_icon_size_tool(u64 val) noexcept
{
    this->icon_size_tool = val;
}

bool
AppSettings::get_single_click() const noexcept
{
    return this->single_click;
}

void
AppSettings::set_single_click(bool val) noexcept
{
    this->single_click = val;
}

bool
AppSettings::get_single_hover() const noexcept
{
    return this->single_hover;
}

void
AppSettings::set_single_hover(bool val) noexcept
{
    this->single_hover = val;
}

bool
AppSettings::get_click_executes() const noexcept
{
    return this->click_executes;
}

void
AppSettings::set_click_executes(bool val) noexcept
{
    this->click_executes = val;
}

bool
AppSettings::get_confirm() const noexcept
{
    return this->confirm;
}

void
AppSettings::set_confirm(bool val) noexcept
{
    this->confirm = val;
}

bool
AppSettings::get_confirm_delete() const noexcept
{
    return this->confirm_delete;
}

void
AppSettings::set_confirm_delete(bool val) noexcept
{
    this->confirm_delete = val;
}

bool
AppSettings::get_confirm_trash() const noexcept
{
    return this->confirm_trash;
}

void
AppSettings::set_confirm_trash(bool val) noexcept
{
    this->confirm_trash = val;
}

bool
AppSettings::get_load_saved_tabs() const noexcept
{
    return this->load_saved_tabs;
}

void
AppSettings::set_load_saved_tabs(bool val) noexcept
{
    this->load_saved_tabs = val;
}

const std::string&
AppSettings::get_date_format() const noexcept
{
    if (this->date_format_custom.empty())
    {
        return this->date_format_default;
    }
    return this->date_format_custom;
}

void
AppSettings::set_date_format(std::string_view val) noexcept
{
    if (val.empty())
    {
        return;
    }
    this->date_format_custom = val.data();
}

u64
AppSettings::get_sort_order() const noexcept
{
    return this->sort_order;
}

void
AppSettings::set_sort_order(u64 val) noexcept
{
    this->sort_order = val;
}

u64
AppSettings::get_sort_type() const noexcept
{
    return this->sort_type;
}

void
AppSettings::set_sort_type(u64 val) noexcept
{
    this->sort_type = val;
}

u64
AppSettings::get_width() const noexcept
{
    return this->width;
}

void
AppSettings::set_width(u64 val) noexcept
{
    this->width = val;
}

u64
AppSettings::get_height() const noexcept
{
    return this->height;
}

void
AppSettings::set_height(u64 val) noexcept
{
    this->height = val;
}

bool
AppSettings::get_maximized() const noexcept
{
    return this->maximized;
}

void
AppSettings::set_maximized(bool val) noexcept
{
    this->maximized = val;
}

bool
AppSettings::get_always_show_tabs() const noexcept
{
    return this->always_show_tabs;
}

void
AppSettings::set_always_show_tabs(bool val) noexcept
{
    this->always_show_tabs = val;
}

bool
AppSettings::get_show_close_tab_buttons() const noexcept
{
    return this->show_close_tab_buttons;
}

void
AppSettings::set_show_close_tab_buttons(bool val) noexcept
{
    this->show_close_tab_buttons = val;
}

bool
AppSettings::get_use_si_prefix() const noexcept
{
    return this->use_si_prefix;
}

void
AppSettings::set_use_si_prefix(bool val) noexcept
{
    this->use_si_prefix = val;
}

bool
AppSettings::get_git_backed_settings() const noexcept
{
    return this->git_backed_settings;
}

void
AppSettings::set_git_backed_settings(bool val) noexcept
{
    this->git_backed_settings = val;
}
