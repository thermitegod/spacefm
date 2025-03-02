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

#include <glaze/glaze.hpp>

#include <magic_enum/magic_enum.hpp>

#include <ztd/ztd.hxx>

#include "utils/shell-quote.hxx"

#include "xset/xset.hxx"

#include "ptk/ptk-file-action-create.hxx"
#include "ptk/ptk-file-browser.hxx"
#include "ptk/ptk-file-task.hxx"

#include "vfs/vfs-file.hxx"

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

    const auto buffer = glz::write_json(datatype::create::request{
        .cwd = cwd,
        .file = file ? file->path() : "",
        .mode = datatype::create::mode(magic_enum::enum_integer(init_mode)),
        .settings = {.filename = xset_get_b(xset::name::move_filename),
                     .parent = xset_get_b(xset::name::move_parent),
                     .path = xset_get_b(xset::name::move_path),
                     .target = xset_get_b(xset::name::move_target),
                     .confirm = xset_get_b(xset::name::move_dlg_confirm_create)}});

    if (!buffer)
    {
        logger::error("Failed to create json: {}", glz::format_error(buffer));
        return -1;
    }

#if defined(HAVE_DEV)
    const auto binary = std::format("{}/{}", DIALOG_BUILD_ROOT, DIALOG_FILE_CREATE);
#else
    const auto binary = Glib::find_program_in_path(DIALOG_FILE_CREATE);
#endif
    if (binary.empty())
    {
        logger::error("Failed to find create dialog binary: {}", DIALOG_FILE_CREATE);
        return -1;
    }

    // std::println(R"({} --json '{}')", binary, buffer.value());

    const auto command =
        std::format(R"({} --json {})", binary, ::utils::shell_quote(buffer.value()));

    std::int32_t exit_status = 0;
    std::string standard_output;
    Glib::spawn_command_line_sync(command, &standard_output, nullptr, &exit_status);
#if defined(HAVE_DEV) && __has_feature(address_sanitizer)
    // AddressSanitizer does not like GTK
    if (standard_output.empty())
#else
    if (exit_status != 0 || standard_output.empty())
#endif
    {
        return -1;
    }

    const auto results = glz::read_json<datatype::create::response>(standard_output);
    if (!results)
    {
        logger::error<logger::domain::ptk>("Failed to decode json: {}",
                                           glz::format_error(results.error(), standard_output));
        return -1;
    }

    if (results->target.empty() && results->dest.empty())
    { // Cancel pressed
        return 0;
    }

    const auto target = results->target;
    const auto dest = results->dest;
    const auto mode = results->mode;
    const auto overwrite = results->overwrite;
    const auto auto_open = results->auto_open;

    // update settings
    xset_set_b(xset::name::move_filename, results->settings.filename);
    xset_set_b(xset::name::move_parent, results->settings.parent);
    xset_set_b(xset::name::move_path, results->settings.path);
    xset_set_b(xset::name::move_target, results->settings.target);
    xset_set_b(xset::name::move_dlg_confirm_create, results->settings.confirm);

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
