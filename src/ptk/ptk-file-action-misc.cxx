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

#include <string>
#include <string_view>

#include <format>

#include <span>
#include <vector>

#include <memory>

#include <gtkmm.h>

#include <glaze/glaze.hpp>

#if __has_include(<magic_enum/magic_enum.hpp>)
// >=magic_enum-0.9.7
#include <magic_enum/magic_enum.hpp>
#else
// <=magic_enum-0.9.6
#include <magic_enum.hpp>
#endif

#include <ztd/ztd.hxx>

#include "logger.hxx"

#include "datatypes/datatypes.hxx"

#include "settings/settings.hxx"

#include "vfs/vfs-file.hxx"

#include "ptk/ptk-file-task.hxx"

#include "ptk/ptk-file-action-misc.hxx"

static bool
create_file_action_dialog(GtkWindow* parent, const std::string_view header_text,
                          const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept
{
    (void)parent;

    // Create

    std::vector<datatype::file_action::request> file_data;
    for (const auto& file : selected_files)
    {
        file_data.push_back({file->name().data(), file->size(), file->is_directory()});
    }

    const auto buffer = glz::write_json(file_data);
    if (!buffer)
    {
        logger::error("Failed to create json: {}", glz::format_error(buffer));
        return false;
    }

    // Run

#if defined(HAVE_DEV)
    const auto binary = std::format("{}/{}", DIALOG_BUILD_ROOT, DIALOG_FILE_ACTION);
#else
    const auto binary = Glib::find_program_in_path(DIALOG_FILE_ACTION);
#endif
    if (binary.empty())
    {
        logger::error("Failed to find file action dialog binary: {}", DIALOG_FILE_ACTION);
        return false;
    }

    const auto command =
        std::format(R"({} --header '{}' --json '{}')", binary, header_text, buffer.value());

    std::string standard_output;
    Glib::spawn_command_line_sync(command, &standard_output);

    // Result

    if (standard_output.empty())
    {
        logger::error<logger::domain::ptk>("Bad response from file action dialog with command: {}",
                                           command);
        return false;
    }

    const auto data = glz::read_json<datatype::file_action::response>(standard_output);
    if (!data)
    {
        logger::error<logger::domain::ptk>("Failed to decode json: {}",
                                           glz::format_error(data.error(), standard_output));
        return false;
    }
    const auto& response = data.value();

    return response.result == "Confirm";
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

    if (config::settings.confirm_delete)
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

    if (config::settings.confirm_trash)
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
