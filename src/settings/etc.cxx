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

#include "settings/etc.hxx"

ConfigSettings etc_settings = ConfigSettings();

const std::string&
ConfigSettings::get_terminal_su() const noexcept
{
    return this->terminal_su;
}

void
ConfigSettings::set_terminal_su(std::string_view val) noexcept
{
    if (std::filesystem::exists(val))
    {
        ztd::logger::error("File not found {}", val);
        return;
    }
    this->terminal_su = val.data();
}

const std::string&
ConfigSettings::get_font_view_icon() const noexcept
{
    return this->font_view_icon;
}

void
ConfigSettings::set_font_view_icon(std::string_view val) noexcept
{
    this->font_view_icon = val.data();
}

const std::string&
ConfigSettings::get_font_view_compact() const noexcept
{
    return this->font_view_compact;
}

void
ConfigSettings::set_font_view_compact(std::string_view val) noexcept
{
    this->font_view_compact = val.data();
}

const std::string&
ConfigSettings::get_font_general() const noexcept
{
    return this->font_general;
}

void
ConfigSettings::set_font_general(std::string_view val) noexcept
{
    this->font_general = val.data();
}

bool
ConfigSettings::get_git_backed_settings() const noexcept
{
    return this->git_backed_settings;
}

void
ConfigSettings::set_git_backed_settings(bool val) noexcept
{
    this->git_backed_settings = val;
}
