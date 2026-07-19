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

#include "settings/settings.hxx"

#include "gui/tab/files/base.hxx"

#include "vfs/task-manager.hxx"

namespace gui
{
class grid final : public Gtk::GridView, public files_base
{
  public:
    grid(const config::grid_state& state, const std::shared_ptr<vfs::task_manager>& task_manager,
         const std::shared_ptr<config::settings>& settings);
    ~grid() = default;

    grid(const grid&) = delete;
    grid& operator=(const grid&) = delete;
    grid(grid&&) = delete;
    grid& operator=(grid&&) = delete;

  private:
    Pango::AttrList attrs_;

    void on_setup_item(const Glib::RefPtr<Gtk::ListItem>& item) noexcept;
    void on_bind_item(const Glib::RefPtr<Gtk::ListItem>& item) noexcept;
    void on_unbind_item(const Glib::RefPtr<Gtk::ListItem>& item) noexcept;

    void on_background_click(std::int32_t n_press, double x, double y) noexcept;

    Glib::RefPtr<Gdk::ContentProvider> on_drag_prepare(double x, double y) const noexcept;
    bool on_drag_data_received(const Glib::ValueBase& value, double x, double y) noexcept;
    Gdk::DragAction on_drag_motion(double, double) noexcept;
};
} // namespace gui
