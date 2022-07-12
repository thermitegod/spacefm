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
ConfigSettings::set_terminal_su(const std::string& val) noexcept
{
    this->terminal_su = val;
}

const std::string&
ConfigSettings::get_tmp_dir() const noexcept
{
    return this->tmp_dir;
}

void
ConfigSettings::set_tmp_dir(const std::string& val) noexcept
{
    this->tmp_dir = val;
}

const std::string&
ConfigSettings::get_font_view_icon() const noexcept
{
    return this->font_view_icon;
}

void
ConfigSettings::set_font_view_icon(const std::string& val) noexcept
{
    this->font_view_icon = val;
}

const std::string&
ConfigSettings::get_font_view_compact() const noexcept
{
    return this->font_view_compact;
}

void
ConfigSettings::set_font_view_compact(const std::string& val) noexcept
{
    this->font_view_compact = val;
}

const std::string&
ConfigSettings::get_font_general() const noexcept
{
    return this->font_general;
}

void
ConfigSettings::set_font_general(const std::string& val) noexcept
{
    this->font_general = val;
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
