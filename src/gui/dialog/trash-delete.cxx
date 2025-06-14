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

#include "datatypes/datatypes.hxx"
#include "datatypes/external-dialog.hxx"

#include "settings/settings.hxx"

#include "gui/file-task.hxx"

#include "gui/dialog/trash-delete.hxx"

#include "vfs/vfs-file.hxx"

#include "logger.hxx"
#include "package.hxx"

static bool
create_file_action_dialog(GtkWindow* parent, const std::string_view header,
                          const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept
{
    (void)parent;

    // Create

    std::vector<datatype::file_action::data> file_data;
    for (const auto& file : selected_files)
    {
        file_data.push_back({file->name().data(), file->size().data(), file->is_directory()});
    }

    const auto response = datatype::run_dialog_sync<datatype::file_action::response>(
        spacefm::package.dialog.file_action,
        datatype::file_action::request{.header = header.data(), .data = file_data});
    if (!response)
    {
        return false;
    }

    return response->result;
}

void
ptk::action::delete_files(GtkWindow* parent_win, const std::filesystem::path& cwd,
                          const std::span<const std::shared_ptr<vfs::file>> selected_files,
                          GtkTreeView* task_view) noexcept
{
    (void)cwd;

    if (selected_files.empty())
    {
        logger::warn<logger::domain::ptk>("Trying to delete an empty file list");
        return;
    }

    if (config::global::settings->confirm_delete)
    {
        const bool confirmed =
            create_file_action_dialog(parent_win, "Delete selected files?", selected_files);
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

    ptk::file_task* ptask = ptk_file_task_new(vfs::file_task::type::del,
                                              file_list,
                                              parent_win ? GTK_WINDOW(parent_win) : nullptr,
                                              GTK_WIDGET(task_view));
    ptask->run();
}

void
ptk::action::trash_files(GtkWindow* parent_win, const std::filesystem::path& cwd,
                         const std::span<const std::shared_ptr<vfs::file>> selected_files,
                         GtkTreeView* task_view) noexcept
{
    (void)cwd;

    if (selected_files.empty())
    {
        logger::warn<logger::domain::ptk>("Trying to trash an empty file list");
        return;
    }

    if (config::global::settings->confirm_trash)
    {
        const bool confirmed =
            create_file_action_dialog(parent_win, "Trash selected files?", selected_files);
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

    ptk::file_task* ptask = ptk_file_task_new(vfs::file_task::type::trash,
                                              file_list,
                                              parent_win ? GTK_WINDOW(parent_win) : nullptr,
                                              GTK_WIDGET(task_view));
    ptask->run();
}
