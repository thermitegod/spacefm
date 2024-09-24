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

#include <glaze/glaze.hpp>

#include <magic_enum/magic_enum.hpp>

#include <ztd/ztd.hxx>

#include "utils/shell-quote.hxx"

#include "xset/xset.hxx"

#include "ptk/ptk-dialog.hxx"
#include "ptk/ptk-file-action-rename.hxx"
#include "ptk/ptk-file-browser.hxx"
#include "ptk/ptk-file-task.hxx"

#include "vfs/vfs-file.hxx"

i32
ptk::action::rename_files(ptk::browser* browser, const std::filesystem::path& cwd,
                          const std::shared_ptr<vfs::file>& file, const char* dest_dir,
                          const bool clip_copy) noexcept
{
    const auto buffer = glz::write_json(datatype::rename::request{
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

    if (!buffer)
    {
        logger::error("Failed to create json: {}", glz::format_error(buffer));
        return -1;
    }

#if defined(HAVE_DEV)
    const auto binary = std::format("{}/{}", DIALOG_BUILD_ROOT, DIALOG_FILE_RENAME);
#else
    const auto binary = Glib::find_program_in_path(DIALOG_FILE_RENAME);
#endif
    if (binary.empty())
    {
        logger::error("Failed to find rename dialog binary: {}", DIALOG_FILE_RENAME);
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

    const auto results = glz::read_json<datatype::rename::response>(standard_output);
    if (!results)
    {
        logger::error<logger::domain::ptk>("Failed to decode json: {}",
                                           glz::format_error(results.error(), standard_output));
        return -1;
    }

    const auto source = results->source;
    const auto dest = results->dest;
    const auto mode = results->mode;
    const auto overwrite = results->overwrite;

    if (mode == datatype::rename::mode::cancel)
    { // Cancel pressed
        return -1;
    }

    // update settings
    xset_set_b(xset::name::move_copy, results->settings.copy);
    xset_set_b(xset::name::move_copyt, results->settings.copyt);
    xset_set_b(xset::name::move_filename, results->settings.filename);
    xset_set_b(xset::name::move_link, results->settings.link);
    xset_set_b(xset::name::move_linkt, results->settings.linkt);
    xset_set_b(xset::name::move_parent, results->settings.parent);
    xset_set_b(xset::name::move_path, results->settings.path);
    xset_set_b(xset::name::move_target, results->settings.target);
    xset_set_b(xset::name::move_type, results->settings.type);
    xset_set_b(xset::name::move_dlg_confirm_create, results->settings.confirm);

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
