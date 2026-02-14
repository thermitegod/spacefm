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
#include <string_view>
#include <vector>

#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "settings/settings.hxx"

#include "gui/file-task.hxx"

#include "gui/action/trash.hxx"

#include "gui/dialog/action-list.hxx"

#include "vfs/file.hxx"

#include "logger.hxx"

void
gui::action::trash_files(GtkWindow* parent_win, const std::filesystem::path& cwd,
                         const std::span<const std::shared_ptr<vfs::file>> selected_files,
                         GtkTreeView* task_view) noexcept
{
    (void)cwd;

    if (selected_files.empty())
    {
        logger::warn<logger::gui>("Trying to trash an empty file list");
        return;
    }

    if (config::global::settings->confirm_trash)
    {
        const bool confirmed = gui::dialog::list_files("Trash selected files?", selected_files);
        if (!confirmed)
        {
            return;
        }
    }

    std::vector<std::filesystem::path> file_list;
    file_list.reserve(selected_files.size());
    for (const auto& file : selected_files)
    {
        file_list.push_back(file->path());
    }

    gui::file_task* ptask = gui_file_task_new(vfs::file_task::type::trash,
                                              file_list,
                                              parent_win ? GTK_WINDOW(parent_win) : nullptr,
                                              GTK_WIDGET(task_view));
    ptask->run();
}
