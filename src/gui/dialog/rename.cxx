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

#include <filesystem>
#include <format>
#include <memory>
#include <print>
#include <string>
#include <string_view>
#include <system_error>

#include <cstring>

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "datatypes/external-dialog.hxx"

#include "utils/shell-quote.hxx"

#include "xset/xset.hxx"

#include "gui/file-browser.hxx"
#include "gui/file-task.hxx"

#include "gui/dialog/rename.hxx"
#include "gui/dialog/text.hxx"

#include "vfs/vfs-file.hxx"

#include "package.hxx"

i32
ptk::action::rename_files(ptk::browser* browser, const std::filesystem::path& cwd,
                          const std::shared_ptr<vfs::file>& file, const char* dest_dir,
                          const bool clip_copy) noexcept
{
    const auto response = datatype::run_dialog_sync<datatype::rename::response>(
        spacefm::package.dialog.file_rename,
        datatype::rename::request{
            .cwd = cwd,
            .file = file->path(),
            .dest_dir = dest_dir ? dest_dir : "",
            .clip_copy = clip_copy,
            .settings = {.copy = xset_get_b(xset::name::move_copy),
                         .copyt = xset_get_b(xset::name::move_copyt),
                         .filename = xset_get_b(xset::name::move_filename),
                         .link = xset_get_b(xset::name::move_link),
                         .linkt = xset_get_b(xset::name::move_linkt),
                         .parent = xset_get_b(xset::name::move_parent),
                         .path = xset_get_b(xset::name::move_path),
                         .target = xset_get_b(xset::name::move_target),
                         .type = xset_get_b(xset::name::move_type),
                         .confirm = xset_get_b(xset::name::move_dlg_confirm_create)}});
    if (!response)
    {
        return -1;
    }

    const auto source = response->source;
    const auto dest = response->dest;
    const auto mode = response->mode;
    const auto overwrite = response->overwrite;

    if (mode == datatype::rename::mode::cancel)
    { // Cancel pressed
        return -1;
    }

    // update settings
    xset_set_b(xset::name::move_copy, response->settings.copy);
    xset_set_b(xset::name::move_copyt, response->settings.copyt);
    xset_set_b(xset::name::move_filename, response->settings.filename);
    xset_set_b(xset::name::move_link, response->settings.link);
    xset_set_b(xset::name::move_linkt, response->settings.linkt);
    xset_set_b(xset::name::move_parent, response->settings.parent);
    xset_set_b(xset::name::move_path, response->settings.path);
    xset_set_b(xset::name::move_target, response->settings.target);
    xset_set_b(xset::name::move_type, response->settings.type);
    xset_set_b(xset::name::move_dlg_confirm_create, response->settings.confirm);

    GtkWidget* parent = nullptr;
    GtkWidget* task_view = nullptr;
    if (browser)
    {
        parent = gtk_widget_get_toplevel(GTK_WIDGET(browser));
        task_view = browser->task_view();
    }

    if (mode == datatype::rename::mode::copy)
    { // copy task
        ptk::file_task* ptask = ptk_file_exec_new("Copy", parent, task_view);

        std::string over_opt;
        if (overwrite)
        {
            over_opt = " --remove-destination";
        }

        if (std::filesystem::is_directory(source))
        {
            ptask->task->exec_command = std::format("cp -Pfr {} {}",
                                                    ::utils::shell_quote(source),
                                                    ::utils::shell_quote(dest));
        }
        else
        {
            ptask->task->exec_command = std::format("cp -Pf{} {} {}",
                                                    over_opt,
                                                    ::utils::shell_quote(source),
                                                    ::utils::shell_quote(dest));
        }
        ptask->task->exec_sync = true;
        ptask->task->exec_popup = false;
        ptask->task->exec_show_output = false;
        ptask->task->exec_show_error = true;
        ptask->run();
    }
    else if (mode == datatype::rename::mode::link)
    { // link task
        ptk::file_task* ptask = ptk_file_exec_new("Create Link", parent, task_view);

        if (overwrite)
        {
            ptask->task->exec_command = std::format("ln -sf {} {}",
                                                    ::utils::shell_quote(source),
                                                    ::utils::shell_quote(dest));
        }
        else
        {
            ptask->task->exec_command = std::format("ln -s {} {}",
                                                    ::utils::shell_quote(source),
                                                    ::utils::shell_quote(dest));
        }
        ptask->task->exec_sync = true;
        ptask->task->exec_popup = false;
        ptask->task->exec_show_output = false;
        ptask->task->exec_show_error = true;
        ptask->run();
    }
    else if (mode == datatype::rename::mode::move)
    { // need move?  (do move as task in case it takes a long time)
        ptk::file_task* ptask = ptk_file_exec_new("Move", parent, task_view);

        if (overwrite)
        {
            ptask->task->exec_command = std::format("mv -f {} {}",
                                                    ::utils::shell_quote(source),
                                                    ::utils::shell_quote(dest));
        }
        else
        {
            ptask->task->exec_command =
                std::format("mv {} {}", ::utils::shell_quote(source), ::utils::shell_quote(dest));
        }
        ptask->task->exec_sync = true;
        ptask->task->exec_popup = false;
        ptask->task->exec_show_output = false;
        ptask->task->exec_show_error = true;
        ptask->run();
    }
    else if (mode == datatype::rename::mode::rename)
    { // rename (does overwrite)
        std::error_code ec;
        std::filesystem::rename(source, dest, ec);
        if (ec)
        {
            // Unknown error occurred
            ptk::dialog::error(GTK_WINDOW(parent),
                               "Rename Error",
                               std::format("Error renaming file\n\n{}", std::strerror(errno)));
        }
    }

    return 0;
}
