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

#include <memory>
#include <span>

#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "settings/settings.hxx"

#include "vfs/dir.hxx"
#include "vfs/file.hxx"

namespace gui
{
class statusbar final : public Gtk::Box
{
  public:
    statusbar(const std::shared_ptr<config::settings>& settings);

    void update(const std::shared_ptr<vfs::dir>& dir,
                const std::span<const std::shared_ptr<vfs::file>> selected_files,
                const bool show_hidden_files) noexcept;

  private:
    std::shared_ptr<config::settings> settings_;

    Gtk::Label statusbar_;
};
} // namespace gui
