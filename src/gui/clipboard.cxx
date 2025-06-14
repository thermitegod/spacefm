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
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glibmm.h>

#include <glaze/glaze.hpp>

#include <ztd/extra/glaze.hxx>
#include <ztd/ztd.hxx>

#include "utils/shell-quote.hxx"

#include "gui/clipboard.hxx"
#include "gui/file-task.hxx"

#include "gui/dialog/text.hxx"

#include "logger.hxx"

struct clipboard_data final
{
    gui::clipboard::mode mode;
    std::vector<std::filesystem::path> files;
};

static void
set_clipboard(clipboard_data data) noexcept
{
    const auto binary = Glib::find_program_in_path("wl-copy");
    if (binary.empty())
    {
        logger::error("Failed to find wl-copy");
        return;
    }

    const auto buffer = glz::write_json(data);
    if (!buffer)
    {
        logger::error("Failed to create JSON: {}", glz::format_error(buffer));
        return;
    }

    const auto command = std::format(R"({} -- {})", binary, utils::shell_quote(buffer.value()));

    Glib::spawn_command_line_async(command);
}

static void
set_clipboard(const std::string_view data) noexcept
{
    const auto binary = Glib::find_program_in_path("wl-copy");
    if (binary.empty())
    {
        logger::error("Failed to find wl-copy");
        return;
    }

    const auto command = std::format(R"({} -- {})", binary, utils::shell_quote(data));

    Glib::spawn_command_line_async(command);
}

static std::optional<clipboard_data>
get_clipboard() noexcept
{
    const auto binary = Glib::find_program_in_path("wl-paste");
    if (binary.empty())
    {
        logger::error("Failed to find wl-paste");
        return std::nullopt;
    }

    i32 exit_status = 0;
    std::string standard_output;
    Glib::spawn_command_line_sync(binary, &standard_output, nullptr, exit_status.unwrap());

#if defined(DEV_MODE) && __has_feature(address_sanitizer)
    if (standard_output.empty())
#else
    if (exit_status != 0 || standard_output.empty())
#endif
    {
        return std::nullopt;
    }

    const auto response = glz::read_json<clipboard_data>(standard_output);
    if (!response)
    {
        // logger::error("Failed to decode JSON: {}", glz::format_error(response.error(), standard_output));
        return std::nullopt;
    }

    return response.value();
}

static void
clear_clipboard() noexcept
{
    const auto binary = Glib::find_program_in_path("wl-copy");
    if (binary.empty())
    {
        logger::error("Failed to find wl-copy");
        return;
    }

    const auto command = std::format(R"({} -c)", binary);

    Glib::spawn_command_line_async(command);
}

bool
gui::clipboard::is_content_valid() noexcept
{
    auto data = get_clipboard();

    return data != std::nullopt;
}

std::string
gui::clipboard::get_text() noexcept
{
    const auto binary = Glib::find_program_in_path("wl-paste");
    if (binary.empty())
    {
        logger::error("Failed to find wl-paste");
        return "";
    }

    i32 exit_status = 0;
    std::string standard_output;
    Glib::spawn_command_line_sync(binary, &standard_output, nullptr, exit_status.unwrap());

#if defined(DEV_MODE) && __has_feature(address_sanitizer)
    if (standard_output.empty())
#else
    if (exit_status != 0 || standard_output.empty())
#endif
    {
        return "";
    }

    return standard_output;
}

void
gui::clipboard::copy_as_text(
    const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept
{
    std::string text;
    for (const auto& file : selected_files)
    {
        text.append(::utils::shell_quote(file->path().string()));
        text.append(" ");
    }
    text = ztd::strip(text);

    set_clipboard(text);
}

void
gui::clipboard::copy_name(const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept
{
    std::string text;
    for (const auto& file : selected_files)
    {
        text.append(::utils::shell_quote(file->name()));
        text.append(" ");
    }
    text = ztd::strip(text);

    set_clipboard(text);
}

void
gui::clipboard::copy_text(const std::string_view text) noexcept
{
    set_clipboard(text);
}

void
gui::clipboard::cut_or_copy_files(const std::span<const std::shared_ptr<vfs::file>> selected_files,
                                  const mode mode) noexcept
{
    set_clipboard(clipboard_data{.mode = mode,
                                 .files = [&selected_files]()
                                 {
                                     std::vector<std::filesystem::path> files;
                                     files.reserve(selected_files.size());
                                     for (const auto& file : selected_files)
                                     {
                                         files.push_back(file->path());
                                     }
                                     return files;
                                 }()});
}

void
gui::clipboard::cut_or_copy_files(const std::span<const std::string> selected_files,
                                  const mode mode) noexcept
{
    set_clipboard(clipboard_data{.mode = mode,
                                 .files = [&selected_files]()
                                 {
                                     std::vector<std::filesystem::path> files;
                                     for (std::filesystem::path file : selected_files)
                                     {
                                         if (file.is_absolute())
                                         {
                                             files.emplace_back(file);
                                         }
                                     }
                                     return files;
                                 }()});
}

void
gui::clipboard::paste_files(GtkWindow* parent, const std::filesystem::path& dest_dir,
                            GtkTreeView* task_view) noexcept
{
    auto data = get_clipboard();
    if (!data || (data && data->files.empty()))
    {
        return;
    }

    const auto action =
        data->mode == mode::move ? vfs::file_task::type::move : vfs::file_task::type::copy;

    /*
     * If only one item is selected and the item is a
     * directory, paste the files in that directory;
     * otherwise, paste the file in current directory.
     */

    gui::file_task* ptask = gui_file_task_new(action,
                                              data->files,
                                              dest_dir,
                                              parent ? GTK_WINDOW(parent) : nullptr,
                                              GTK_WIDGET(task_view));
    ptask->run();

    clear_clipboard();
}

void
gui::clipboard::paste_links(GtkWindow* parent, const std::filesystem::path& dest_dir,
                            GtkTreeView* task_view) noexcept
{
    auto data = get_clipboard();
    if (!data || (data && data->files.empty()))
    {
        return;
    }

    const auto action = vfs::file_task::type::link;

    gui::file_task* ptask = gui_file_task_new(action,
                                              data->files,
                                              dest_dir,
                                              parent ? GTK_WINDOW(parent) : nullptr,
                                              task_view ? GTK_WIDGET(task_view) : nullptr);
    ptask->run();

    clear_clipboard();
}

void
gui::clipboard::paste_targets(GtkWindow* parent, const std::filesystem::path& dest_dir,
                              GtkTreeView* task_view) noexcept
{
    auto data = get_clipboard();
    if (!data || (data && data->files.empty()))
    {
        return;
    }

    const auto action = vfs::file_task::type::copy;

    i32 missing_targets = 0;
    for (auto& file : data->files)
    {
        if (std::filesystem::is_symlink(file))
        { // canonicalize target
            file = std::filesystem::read_symlink(file);
        }

        if (!std::filesystem::exists(file))
        {
            missing_targets += 1;
        }
    }

    gui::file_task* ptask = gui_file_task_new(action,
                                              data->files,
                                              dest_dir,
                                              parent ? GTK_WINDOW(parent) : nullptr,
                                              GTK_WIDGET(task_view));
    ptask->run();

    if (missing_targets > 0)
    {
        gui::dialog::error("Error",
                           std::format("{} target{} missing",
                                       missing_targets,
                                       missing_targets > 1 ? "s are" : " is"));
    }

    clear_clipboard();
}

std::vector<std::filesystem::path>
gui::clipboard::get_file_paths(bool* is_cut, i32* missing_targets) noexcept
{
    auto data = get_clipboard();
    if (!data || (data && data->files.empty()))
    {
        return {};
    }

    *is_cut = data->mode == mode::move;
    *missing_targets = 0;

    for (auto& file : data->files)
    {
        if (!std::filesystem::exists(file))
        {
            *missing_targets += 1;
        }
    }

    clear_clipboard();

    return data->files;
}
