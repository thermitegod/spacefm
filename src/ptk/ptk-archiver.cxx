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

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "ptk/ptk-dialog.hxx"

#include "xset/xset.hxx"

#include "ptk/ptk-archiver.hxx"

static bool
is_archiver_installed()
{
    const auto archiver = Glib::find_program_in_path("file-roller");
    if (archiver.empty())
    {
        ptk_show_error(nullptr, "Missing Archiver", "Failed to find file-roller in $PATH");
        return false;
    }
    return true;
}

static const std::string
archiver_create_shell_file_list(const std::span<const vfs::file_info> sel_files)
{
    std::string file_list;
    for (const auto file : sel_files)
    {
        file_list.append(ztd::shell::quote(file->path().string()));
        file_list.append(" ");
    }
    return file_list;
}

void
ptk_file_archiver_create(PtkFileBrowser* file_browser,
                         const std::span<const vfs::file_info> sel_files)
{
    (void)file_browser;

    if (!is_archiver_installed() || sel_files.empty())
    {
        return;
    }

    const auto shell_file_list = archiver_create_shell_file_list(sel_files);

    const auto command = std::format("file-roller --add {}", shell_file_list);
    ztd::logger::info("COMMAND={}", command);
    Glib::spawn_command_line_async(command);
}

void
ptk_file_archiver_extract(PtkFileBrowser* file_browser,
                          const std::span<const vfs::file_info> sel_files,
                          const std::filesystem::path& dest_dir)
{
    if (!is_archiver_installed() || sel_files.empty())
    {
        return;
    }

    const auto shell_file_list = archiver_create_shell_file_list(sel_files);

    std::string command = "file-roller ";
    if (dest_dir.empty())
    {
        // This will create a dialog where the to extraction path can be selected.
        command.append("--extract ");
    }
    else
    {
        command.append(
            std::format("--extract-to={} ", ztd::shell::quote(file_browser->cwd().string())));
    }
    command.append(shell_file_list);

    ztd::logger::info("COMMAND={}", command);
    Glib::spawn_command_line_async(command);
}

void
ptk_file_archiver_open(PtkFileBrowser* file_browser,
                       const std::span<const vfs::file_info> sel_files)
{
    (void)file_browser;

    if (!is_archiver_installed() || sel_files.empty())
    {
        return;
    }

    const auto shell_file_list = archiver_create_shell_file_list(sel_files);

    const auto command = std::format("file-roller {}", shell_file_list);
    ztd::logger::info("COMMAND={}", command);
    Glib::spawn_command_line_async(command);
}
