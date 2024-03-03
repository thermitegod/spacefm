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

#include <memory>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "settings/settings.hxx"

const std::unique_ptr<config::detail::settings> config::settings =
    std::make_unique<config::detail::settings>();

bool
config::detail::settings::show_thumbnail() const noexcept
{
    return this->show_thumbnails_;
}

void
config::detail::settings::show_thumbnail(bool val) noexcept
{
    this->show_thumbnails_ = val;
}

bool
config::detail::settings::thumbnail_size_limit() const noexcept
{
    return this->thumbnail_size_limit_;
}

void
config::detail::settings::thumbnail_size_limit(bool val) noexcept
{
    this->thumbnail_size_limit_ = val;
}

u32
config::detail::settings::max_thumb_size() const noexcept
{
    return this->thumbnail_max_size_;
}

void
config::detail::settings::max_thumb_size(u32 val) noexcept
{
    this->thumbnail_max_size_ = val;
}

i32
config::detail::settings::icon_size_big() const noexcept
{
    return this->icon_size_big_;
}

void
config::detail::settings::icon_size_big(i32 val) noexcept
{
    this->icon_size_big_ = val;
}

i32
config::detail::settings::icon_size_small() const noexcept
{
    return this->icon_size_small_;
}

void
config::detail::settings::icon_size_small(i32 val) noexcept
{
    this->icon_size_small_ = val;
}

i32
config::detail::settings::icon_size_tool() const noexcept
{
    return this->icon_size_tool_;
}

void
config::detail::settings::icon_size_tool(i32 val) noexcept
{
    this->icon_size_tool_ = val;
}

bool
config::detail::settings::single_click() const noexcept
{
    return this->single_click_;
}

void
config::detail::settings::single_click(bool val) noexcept
{
    this->single_click_ = val;
}

bool
config::detail::settings::single_hover() const noexcept
{
    return this->single_hover_;
}

void
config::detail::settings::single_hover(bool val) noexcept
{
    this->single_hover_ = val;
}

bool
config::detail::settings::click_executes() const noexcept
{
    return this->click_executes_;
}

void
config::detail::settings::click_executes(bool val) noexcept
{
    this->click_executes_ = val;
}

bool
config::detail::settings::confirm() const noexcept
{
    return this->confirm_;
}

void
config::detail::settings::confirm(bool val) noexcept
{
    this->confirm_ = val;
}

bool
config::detail::settings::confirm_delete() const noexcept
{
    return this->confirm_delete_;
}

void
config::detail::settings::confirm_delete(bool val) noexcept
{
    this->confirm_delete_ = val;
}

bool
config::detail::settings::confirm_trash() const noexcept
{
    return this->confirm_trash_;
}

void
config::detail::settings::confirm_trash(bool val) noexcept
{
    this->confirm_trash_ = val;
}

bool
config::detail::settings::load_saved_tabs() const noexcept
{
    return this->load_saved_tabs_;
}

void
config::detail::settings::load_saved_tabs(bool val) noexcept
{
    this->load_saved_tabs_ = val;
}

u64
config::detail::settings::width() const noexcept
{
    return this->width_;
}

void
config::detail::settings::width(u64 val) noexcept
{
    this->width_ = val;
}

u64
config::detail::settings::height() const noexcept
{
    return this->height_;
}

void
config::detail::settings::height(u64 val) noexcept
{
    this->height_ = val;
}

bool
config::detail::settings::maximized() const noexcept
{
    return this->maximized_;
}

void
config::detail::settings::maximized(bool val) noexcept
{
    this->maximized_ = val;
}

bool
config::detail::settings::always_show_tabs() const noexcept
{
    return this->always_show_tabs_;
}

void
config::detail::settings::always_show_tabs(bool val) noexcept
{
    this->always_show_tabs_ = val;
}

bool
config::detail::settings::show_close_tab_buttons() const noexcept
{
    return this->show_close_tab_buttons_;
}

void
config::detail::settings::show_close_tab_buttons(bool val) noexcept
{
    this->show_close_tab_buttons_ = val;
}

bool
config::detail::settings::new_tab_here() const noexcept
{
    return this->new_tab_here_;
}

void
config::detail::settings::new_tab_here(bool val) noexcept
{
    this->new_tab_here_ = val;
}

bool
config::detail::settings::use_si_prefix() const noexcept
{
    return this->use_si_prefix_;
}

void
config::detail::settings::use_si_prefix(bool val) noexcept
{
    this->use_si_prefix_ = val;
}

bool
config::detail::settings::thumbnailer_use_api() const noexcept
{
    return this->thumbnailer_use_api_;
}

void
config::detail::settings::thumbnailer_use_api(bool val) noexcept
{
    this->thumbnailer_use_api_ = val;
}

bool
config::detail::settings::git_backed_settings() const noexcept
{
    return this->git_backed_settings_;
}

void
config::detail::settings::git_backed_settings(bool val) noexcept
{
    this->git_backed_settings_ = val;
}
