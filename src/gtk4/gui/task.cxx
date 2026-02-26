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
        [this](const vfs::task_collision& c)
        {
            auto alert = Gtk::AlertDialog::create("Collision Dialog Not Implemented");
            alert->set_detail(
                std::format("File will be skipped\nTask ID: {}\nSource: {}\nDestination: {}",
                            c.task_id,
                            c.source.string(),
                            c.destination.string()));
            alert->set_modal(true);
            alert->set_buttons({"Close"});
            alert->set_cancel_button(0);
            alert->choose(
                parent_,
                [c, alert](Glib::RefPtr<Gio::AsyncResult>& result) mutable
                {
                    try
                    {
                        [[maybe_unused]] const auto response = alert->choose_finish(result);

                        c.resolved(c.task_id, vfs::collision_resolve::skip, {});
                    }
                    catch (const Gtk::DialogError& err)
                    {
                        logger::warn<logger::gui>("Gtk::AlertDialog error: {}", err.what());
                    }
                    catch (const Glib::Error& err)
                    {
                        logger::warn<logger::gui>("Unexpected exception: {}", err.what());
                    }
                });
        });
}
