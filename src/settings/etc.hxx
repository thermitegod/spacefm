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

struct ConfigSettings
{
  public:
    // ConfigSettings();
    // ~ConfigSettings();

    const std::string& get_terminal_su() const noexcept;
    void set_terminal_su(const std::string& val) noexcept;

    const std::string& get_tmp_dir() const noexcept;
    void set_tmp_dir(const std::string& val) noexcept;

    const std::string& get_font_view_icon() const noexcept;
    void set_font_view_icon(const std::string& val) noexcept;

    const std::string& get_font_view_compact() const noexcept;
    void set_font_view_compact(const std::string& val) noexcept;

    const std::string& get_font_general() const noexcept;
    void set_font_general(const std::string& val) noexcept;

    bool get_git_backed_settings() const noexcept;
    void set_git_backed_settings(bool val) noexcept;

  private:
    std::string terminal_su{""};
    std::string tmp_dir{"/tmp"};

    std::string font_view_icon{"Monospace 9"};
    std::string font_view_compact{"Monospace 9"};
    std::string font_general{"Monospace 9"}; // NOOP

    bool git_backed_settings{true};
};

extern ConfigSettings etc_settings;
