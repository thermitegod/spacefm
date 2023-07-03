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
AppSettings::show_thumbnail() const noexcept
{
    return this->show_thumbnail_;
}

void
AppSettings::show_thumbnail(bool val) noexcept
{
    this->show_thumbnail_ = val;
}

u64
AppSettings::max_thumb_size() const noexcept
{
    return this->max_thumb_size_;
}

void
AppSettings::max_thumb_size(u64 val) noexcept
{
    this->max_thumb_size_ = val;
}

u64
AppSettings::icon_size_big() const noexcept
{
    return this->icon_size_big_;
}

void
AppSettings::icon_size_big(u64 val) noexcept
{
    this->icon_size_big_ = val;
}

u64
AppSettings::icon_size_small() const noexcept
{
    return this->icon_size_small_;
}

void
AppSettings::icon_size_small(u64 val) noexcept
{
    this->icon_size_small_ = val;
}

u64
AppSettings::icon_size_tool() const noexcept
{
    return this->icon_size_tool_;
}

void
AppSettings::icon_size_tool(u64 val) noexcept
{
    this->icon_size_tool_ = val;
}

bool
AppSettings::single_click() const noexcept
{
    return this->single_click_;
}

void
AppSettings::single_click(bool val) noexcept
{
    this->single_click_ = val;
}

bool
AppSettings::single_hover() const noexcept
{
    return this->single_hover_;
}

void
AppSettings::single_hover(bool val) noexcept
{
    this->single_hover_ = val;
}

bool
AppSettings::click_executes() const noexcept
{
    return this->click_executes_;
}

void
AppSettings::click_executes(bool val) noexcept
{
    this->click_executes_ = val;
}

bool
AppSettings::confirm() const noexcept
{
    return this->confirm_;
}

void
AppSettings::confirm(bool val) noexcept
{
    this->confirm_ = val;
}

bool
AppSettings::confirm_delete() const noexcept
{
    return this->confirm_delete_;
}

void
AppSettings::confirm_delete(bool val) noexcept
{
    this->confirm_delete_ = val;
}

bool
AppSettings::confirm_trash() const noexcept
{
    return this->confirm_trash_;
}

void
AppSettings::confirm_trash(bool val) noexcept
{
    this->confirm_trash_ = val;
}

bool
AppSettings::load_saved_tabs() const noexcept
{
    return this->load_saved_tabs_;
}

void
AppSettings::load_saved_tabs(bool val) noexcept
{
    this->load_saved_tabs_ = val;
}

const std::string_view
AppSettings::date_format() const noexcept
{
    if (this->date_format_custom_.empty())
    {
        return this->date_format_default_;
    }
    return this->date_format_custom_;
}

void
AppSettings::date_format(const std::string_view val) noexcept
{
    if (val.empty())
    {
        return;
    }
    this->date_format_custom_ = val.data();
}

u64
AppSettings::sort_order() const noexcept
{
    return this->sort_order_;
}

void
AppSettings::sort_order(u64 val) noexcept
{
    this->sort_order_ = val;
}

u64
AppSettings::sort_type() const noexcept
{
    return this->sort_type_;
}

void
AppSettings::sort_type(u64 val) noexcept
{
    this->sort_type_ = val;
}

u64
AppSettings::width() const noexcept
{
    return this->width_;
}

void
AppSettings::width(u64 val) noexcept
{
    this->width_ = val;
}

u64
AppSettings::height() const noexcept
{
    return this->height_;
}

void
AppSettings::height(u64 val) noexcept
{
    this->height_ = val;
}

bool
AppSettings::maximized() const noexcept
{
    return this->maximized_;
}

void
AppSettings::maximized(bool val) noexcept
{
    this->maximized_ = val;
}

bool
AppSettings::always_show_tabs() const noexcept
{
    return this->always_show_tabs_;
}

void
AppSettings::always_show_tabs(bool val) noexcept
{
    this->always_show_tabs_ = val;
}

bool
AppSettings::show_close_tab_buttons() const noexcept
{
    return this->show_close_tab_buttons_;
}

void
AppSettings::show_close_tab_buttons(bool val) noexcept
{
    this->show_close_tab_buttons_ = val;
}

bool
AppSettings::use_si_prefix() const noexcept
{
    return this->use_si_prefix_;
}

void
AppSettings::use_si_prefix(bool val) noexcept
{
    this->use_si_prefix_ = val;
}

bool
AppSettings::thumbnailer_use_api() const noexcept
{
    return this->thumbnailer_use_api_;
}

void
AppSettings::thumbnailer_use_api(bool val) noexcept
{
    this->thumbnailer_use_api_ = val;
}

bool
AppSettings::git_backed_settings() const noexcept
{
    return this->git_backed_settings_;
}

void
AppSettings::git_backed_settings(bool val) noexcept
{
    this->git_backed_settings_ = val;
}
