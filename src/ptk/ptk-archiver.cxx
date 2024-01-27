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

#include <memory>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "utils/shell-quote.hxx"

#include "ptk/ptk-dialog.hxx"

#include "xset/xset.hxx"

#include "ptk/ptk-archiver.hxx"

static bool
is_archiver_installed()
{
    const auto archiver = Glib::find_program_in_path("file-roller");
    if (archiver.empty())
    {
        ptk::dialog::error(nullptr, "Missing Archiver", "Failed to find file-roller in $PATH");
        return false;
    }
    return true;
}

static const std::string
archiver_create_shell_file_list(const std::span<const std::shared_ptr<vfs::file>> sel_files)
{
    std::string file_list;
    for (const auto& file : sel_files)
    {
        file_list.append(::utils::shell_quote(file->path().string()));
        file_list.append(" ");
    }
    return file_list;
}

void
ptk_archiver_create(ptk::browser* file_browser,
                    const std::span<const std::shared_ptr<vfs::file>> sel_files)
{
    (void)file_browser;

    if (!is_archiver_installed() || sel_files.empty())
    {
        return;
    }

    const auto shell_file_list = archiver_create_shell_file_list(sel_files);

    const auto command = std::format("file-roller --add {}", shell_file_list);
    ztd::logger::info("COMMAND({})", command);
    Glib::spawn_command_line_async(command);
}

void
ptk_archiver_extract(ptk::browser* file_browser,
                     const std::span<const std::shared_ptr<vfs::file>> sel_files,
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
            std::format("--extract-to={} ", ::utils::shell_quote(file_browser->cwd().string())));
    }
    command.append(shell_file_list);

    ztd::logger::info("COMMAND({})", command);
    Glib::spawn_command_line_async(command);
}

void
ptk_archiver_open(ptk::browser* file_browser,
                  const std::span<const std::shared_ptr<vfs::file>> sel_files)
{
    (void)file_browser;

    if (!is_archiver_installed() || sel_files.empty())
    {
        return;
    }

    const auto shell_file_list = archiver_create_shell_file_list(sel_files);

    const auto command = std::format("file-roller {}", shell_file_list);
    ztd::logger::info("COMMAND({})", command);
    Glib::spawn_command_line_async(command);
}
