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

#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "vfs/task-manager.hxx"

namespace gui
{
class task : public Gtk::Box
{
  public:
    task(Gtk::ApplicationWindow& parent, const std::shared_ptr<vfs::task_manager>& task_manager);

  private:
    Gtk::ApplicationWindow& parent_;
    std::shared_ptr<vfs::task_manager> task_manager_;
};
} // namespace gui
