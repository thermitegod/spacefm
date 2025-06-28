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
#include <gtkmm.h>

#include <glaze/glaze.hpp>

#include <ztd/extra/glaze.hxx>
#include <ztd/ztd.hxx>

#include "gui/clipboard.hxx"
#include "gui/file-task.hxx"

#include "gui/dialog/text.hxx"

#include "vfs/clipboard.hxx"
#include "vfs/execute.hxx"

void
gui::clipboard::copy_text(const std::string_view text) noexcept
{
    vfs::clipboard::set_text(text);
}

void
gui::clipboard::copy_as_text(
    const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept
{
    std::string text;
    for (const auto& file : selected_files)
    {
        text.append(vfs::execute::quote(file->path().string()));
        text.append(" ");
    }
    text = ztd::strip(text);

    vfs::clipboard::set_text(text);
}

void
gui::clipboard::copy_name(const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept
{
    std::string text;
    for (const auto& file : selected_files)
    {
        text.append(vfs::execute::quote(file->name()));
        text.append(" ");
    }
    text = ztd::strip(text);

    vfs::clipboard::set_text(text);
}

void
gui::clipboard::copy_files(
    const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept
{
    vfs::clipboard::set(vfs::clipboard::clipboard_data{
        .mode = vfs::clipboard::mode::copy,
        .files =
            [&selected_files]()
        {
            std::vector<std::filesystem::path> files;
            files.reserve(selected_files.size());
            for (const auto& file : selected_files)
            {
                files.push_back(file->path());
            }
            return files;
        }(),
    });
}

void
gui::clipboard::copy_files(const std::span<const std::string> selected_files) noexcept
{
    vfs::clipboard::set(vfs::clipboard::clipboard_data{
        .mode = vfs::clipboard::mode::copy,
        .files =
            [&selected_files]()
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
        }(),
    });
}

void
gui::clipboard::cut_files(const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept
{
    vfs::clipboard::set(vfs::clipboard::clipboard_data{
        .mode = vfs::clipboard::mode::move,
        .files =
            [&selected_files]()
        {
            std::vector<std::filesystem::path> files;
            files.reserve(selected_files.size());
            for (const auto& file : selected_files)
            {
                files.push_back(file->path());
            }
            return files;
        }(),
    });
}

void
gui::clipboard::cut_files(const std::span<const std::string> selected_files) noexcept
{
    vfs::clipboard::set(vfs::clipboard::clipboard_data{
        .mode = vfs::clipboard::mode::move,
        .files =
            [&selected_files]()
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
        }(),
    });
}

void
gui::clipboard::paste_files(GtkWindow* parent, const std::filesystem::path& dest_dir,
                            GtkTreeView* task_view) noexcept
{
    auto data = vfs::clipboard::get();
    if (!data || (data && data->files.empty()))
    {
        return;
    }

    /*
     * If only one item is selected and the item is a
     * directory, paste the files in that directory;
     * otherwise, paste the file in current directory.
     */

    gui::file_task* ptask =
        gui_file_task_new(data->mode == vfs::clipboard::mode::move ? vfs::file_task::type::move
                                                                   : vfs::file_task::type::copy,
                          data->files,
                          dest_dir,
                          parent ? GTK_WINDOW(parent) : nullptr,
                          GTK_WIDGET(task_view));
    ptask->run();

    vfs::clipboard::clear();
}

void
gui::clipboard::paste_links(GtkWindow* parent, const std::filesystem::path& dest_dir,
                            GtkTreeView* task_view) noexcept
{
    auto data = vfs::clipboard::get();
    if (!data || (data && data->files.empty()))
    {
        return;
    }

    gui::file_task* ptask = gui_file_task_new(vfs::file_task::type::link,
                                              data->files,
                                              dest_dir,
                                              parent ? GTK_WINDOW(parent) : nullptr,
                                              task_view ? GTK_WIDGET(task_view) : nullptr);
    ptask->run();

    vfs::clipboard::clear();
}

void
gui::clipboard::paste_targets(GtkWindow* parent, const std::filesystem::path& dest_dir,
                              GtkTreeView* task_view) noexcept
{
    auto data = vfs::clipboard::get();
    if (!data || (data && data->files.empty()))
    {
        return;
    }

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

    gui::file_task* ptask = gui_file_task_new(vfs::file_task::type::copy,
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

    vfs::clipboard::clear();
}

std::vector<std::filesystem::path>
gui::clipboard::get_file_paths(bool* is_cut, i32* missing_targets) noexcept
{
    auto data = vfs::clipboard::get();
    if (!data || (data && data->files.empty()))
    {
        return {};
    }

    *is_cut = data->mode == vfs::clipboard::mode::move;
    *missing_targets = 0;

    for (auto& file : data->files)
    {
        if (!std::filesystem::exists(file))
        {
            *missing_targets += 1;
        }
    }

    vfs::clipboard::clear();

    return data->files;
}
