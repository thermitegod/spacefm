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

#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include "gui/task.hxx"

#include "gui/dialog/overwrite.hxx"

#include "vfs/task-manager.hxx"

#include "logger.hxx"

gui::task::task(Gtk::ApplicationWindow& parent,
                const std::shared_ptr<vfs::task_manager>& task_manager)
    : parent_(parent), task_manager_(task_manager)
{
    set_orientation(Gtk::Orientation::VERTICAL);

    task_manager_->signal_task_added().connect([](std::uint64_t task_id) { (void)task_id; });
    task_manager_->signal_task_finished().connect([](std::uint64_t task_id) { (void)task_id; });

    task_manager_->signal_task_error().connect(
        [this](const vfs::task_error& error)
        {
            auto alert = Gtk::AlertDialog::create("Task Error");
            alert->set_detail(std::format("Task ID: {}\n{}", error.task_id, error.message));
            alert->set_modal(true);
            alert->show(parent_);
        });

    task_manager_->signal_task_collision().connect(
         [this](const std::shared_ptr<vfs::task_collision>& c)
         { //
            Glib::signal_idle().connect([this, c]()
            {
                Gtk::make_managed<gui::dialog::overwrite>(parent_, c);

                return false;
            },
            Glib::PRIORITY_DEFAULT);
         });
}
