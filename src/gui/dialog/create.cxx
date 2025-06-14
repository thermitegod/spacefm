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

#include <cstring>

#include <glibmm.h>

#include <magic_enum/magic_enum.hpp>

#include <ztd/ztd.hxx>

#include "datatypes/external-dialog.hxx"

#include "utils/shell-quote.hxx"

#include "xset/xset.hxx"

#include "gui/file-browser.hxx"
#include "gui/file-task.hxx"

#include "gui/dialog/create.hxx"

#include "vfs/vfs-file.hxx"

#include "package.hxx"

i32
ptk::action::create_files(ptk::browser* browser, const std::filesystem::path& cwd,
                          const std::shared_ptr<vfs::file>& file,
                          const ptk::action::create_mode init_mode, AutoOpenCreate* ao) noexcept
{
    static_assert(magic_enum::enum_integer(ptk::action::create_mode::file) ==
                  magic_enum::enum_integer(datatype::create::mode::file));
    static_assert(magic_enum::enum_integer(ptk::action::create_mode::dir) ==
                  magic_enum::enum_integer(datatype::create::mode::dir));
    static_assert(magic_enum::enum_integer(ptk::action::create_mode::link) ==
                  magic_enum::enum_integer(datatype::create::mode::link));

    const auto response = datatype::run_dialog_sync<datatype::create::response>(
        spacefm::package.dialog.file_create,
        datatype::create::request{
            .cwd = cwd,
            .file = file ? file->path() : "",
            .mode = datatype::create::mode(magic_enum::enum_integer(init_mode)),
            .settings = {.filename = xset_get_b(xset::name::move_filename),
                         .parent = xset_get_b(xset::name::move_parent),
                         .path = xset_get_b(xset::name::move_path),
                         .target = xset_get_b(xset::name::move_target),
                         .confirm = xset_get_b(xset::name::move_dlg_confirm_create)}});
    if (!response)
    {
        return -1;
    }

    if (response->target.empty() && response->dest.empty())
    { // Cancel pressed
        return 0;
    }

    const auto target = response->target;
    const auto dest = response->dest;
    const auto mode = response->mode;
    const auto overwrite = response->overwrite;
    const auto auto_open = response->auto_open;

    // update settings
    xset_set_b(xset::name::move_filename, response->settings.filename);
    xset_set_b(xset::name::move_parent, response->settings.parent);
    xset_set_b(xset::name::move_path, response->settings.path);
    xset_set_b(xset::name::move_target, response->settings.target);
    xset_set_b(xset::name::move_dlg_confirm_create, response->settings.confirm);

    GtkWidget* parent = nullptr;
    GtkWidget* task_view = nullptr;
    if (browser)
    {
        parent = gtk_widget_get_toplevel(GTK_WIDGET(browser));
        task_view = browser->task_view();
    }

    if (mode == datatype::create::mode::link)
    { // new link task
        ptk::file_task* ptask = ptk_file_exec_new("Create Link", parent, task_view);

        if (overwrite)
        {
            ptask->task->exec_command = std::format("ln -sf {} {}",
                                                    ::utils::shell_quote(target),
                                                    ::utils::shell_quote(dest));
        }
        else
        {
            ptask->task->exec_command = std::format("ln -s {} {}",
                                                    ::utils::shell_quote(target),
                                                    ::utils::shell_quote(dest));
        }
        ptask->task->exec_sync = true;
        ptask->task->exec_popup = false;
        ptask->task->exec_show_output = false;
        ptask->task->exec_show_error = true;
        if (auto_open)
        {
            ao->path = dest;
            ao->open_file = true;
            ptask->complete_notify_ = ao->callback;
            ptask->user_data_ = ao;
        }
        ptask->run();
    }
    else if (mode == datatype::create::mode::file)
    { // new file task
        std::string over_cmd;
        if (overwrite)
        {
            over_cmd = std::format("rm -f {} && ", ::utils::shell_quote(dest));
        }

        ptk::file_task* ptask = ptk_file_exec_new("Create New File", parent, task_view);
        ptask->task->exec_command = std::format("{}touch {}", over_cmd, ::utils::shell_quote(dest));
        ptask->task->exec_sync = true;
        ptask->task->exec_popup = false;
        ptask->task->exec_show_output = false;
        ptask->task->exec_show_error = true;
        if (auto_open)
        {
            ao->path = dest;
            ao->open_file = true;
            ptask->complete_notify_ = ao->callback;
            ptask->user_data_ = ao;
        }
        ptask->run();
    }
    else if (mode == datatype::create::mode::dir)
    { // new directory task
        ptk::file_task* ptask = ptk_file_exec_new("Create New Directory", parent, task_view);
        ptask->task->exec_command = std::format("mkdir {}", ::utils::shell_quote(dest));
        ptask->task->exec_sync = true;
        ptask->task->exec_popup = false;
        ptask->task->exec_show_output = false;
        ptask->task->exec_show_error = true;
        if (auto_open)
        {
            ao->path = dest;
            ao->open_file = true;
            ptask->complete_notify_ = ao->callback;
            ptask->user_data_ = ao;
        }
        ptask->run();
    }

    return 0;
}
