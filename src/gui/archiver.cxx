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

#include <format>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include <glibmm.h>

#include "xset/xset.hxx"

#include "gui/archiver.hxx"

#include "gui/dialog/text.hxx"

#include "vfs/execute.hxx"

static bool
is_archiver_installed() noexcept
{
    const auto archiver = Glib::find_program_in_path("file-roller");
    if (archiver.empty())
    {
        gui::dialog::error("Missing Archiver", "Failed to find file-roller in $PATH");
        return false;
    }
    return true;
}

static std::string
archiver_create_shell_file_list(
    const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept
{
    std::string file_list;
    for (const auto& file : selected_files)
    {
        file_list.append(vfs::execute::quote(file->path().string()));
        file_list.append(" ");
    }
    return file_list;
}

void
gui::archiver::create(gui::browser* browser,
                      const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept
{
    (void)browser;

    if (!is_archiver_installed() || selected_files.empty())
    {
        return;
    }

    vfs::execute::command_line_async("file-roller --add {}",
                                     archiver_create_shell_file_list(selected_files));
}

void
gui::archiver::extract(gui::browser* browser,
                       const std::span<const std::shared_ptr<vfs::file>> selected_files,
                       const std::filesystem::path& dest_dir) noexcept
{
    if (!is_archiver_installed() || selected_files.empty())
    {
        return;
    }

    const auto shell_file_list = archiver_create_shell_file_list(selected_files);

    std::string command = "file-roller ";
    if (dest_dir.empty())
    {
        // This will create a dialog where the to extraction path can be selected.
        command.append("--extract ");
    }
    else
    {
        command.append(
            std::format("--extract-to={} ", vfs::execute::quote(browser->cwd().string())));
    }
    command.append(shell_file_list);

    vfs::execute::command_line_async(command);
}

void
gui::archiver::open(gui::browser* browser,
                    const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept
{
    (void)browser;

    if (!is_archiver_installed() || selected_files.empty())
    {
        return;
    }

    vfs::execute::command_line_async("file-roller {}",
                                     archiver_create_shell_file_list(selected_files));
}
