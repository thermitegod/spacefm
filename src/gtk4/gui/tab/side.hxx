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

#include <filesystem>
#include <string_view>

#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "settings/settings.hxx"

namespace gui
{
class side final : public Gtk::Box
{
  public:
    side(const std::shared_ptr<config::settings>& settings);

  private:
    std::shared_ptr<config::settings> settings_;

    void add_location(const std::string_view name, const std::filesystem::path& path,
                      const std::string_view icon_name = "folder-symbolic") noexcept;

    void add_separator() noexcept;

  public:
    [[nodiscard]] auto
    signal_chdir() noexcept
    {
        return signal_chdir_;
    }

  private:
    sigc::signal<void(std::filesystem::path)> signal_chdir_;
};
} // namespace gui
