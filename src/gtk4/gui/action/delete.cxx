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
#include <span>

#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "settings/settings.hxx"

#include "gui/action/delete.hxx"

#include "gui/dialog/action.hxx"

#include "vfs/file.hxx"

#include "logger.hxx"

static void
do_task(Gtk::ApplicationWindow& parent,
        const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept
{
    (void)selected_files;
    auto alert = Gtk::AlertDialog::create("Not Implemented");
    alert->set_detail("File Tasks are not implemented");
    alert->set_modal(true);
    alert->show(parent);
}

void
gui::action::delete_files(Gtk::ApplicationWindow& parent,
                          const std::span<const std::shared_ptr<vfs::file>> selected_files,
                          const std::shared_ptr<config::settings>& settings) noexcept
{
    if (selected_files.empty())
    {
        logger::warn<logger::gui>("Trying to delete an empty file list");
        return;
    }

    if (settings->general.confirm_delete)
    {
        auto* dialog = Gtk::make_managed<gui::dialog::action>(parent,
                                                              "Delete selected files?",
                                                              selected_files);
        dialog->signal_confirm().connect([&parent, selected_files]()
                                         { do_task(parent, selected_files); });
    }
    else
    {
        do_task(parent, selected_files);
    }
}
