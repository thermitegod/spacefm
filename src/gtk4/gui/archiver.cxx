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

#include <glibmm.h>

#include "gui/archiver.hxx"

#include "vfs/execute.hxx"

static bool
is_archiver_installed(Gtk::ApplicationWindow& parent) noexcept
{
    const auto archiver = Glib::find_program_in_path("file-roller");
    if (archiver.empty())
    {
        auto alert = Gtk::AlertDialog::create("Missing Archiver");
        alert->set_detail("Failed to find 'file-roller' in $PATH");
        alert->set_modal(true);
        alert->show(parent);

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
gui::archiver::create(Gtk::ApplicationWindow& parent,
                      const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept
{
    if (!is_archiver_installed(parent) || selected_files.empty())
    {
        return;
    }

    vfs::execute::command_line_async("file-roller --add {}",
                                     archiver_create_shell_file_list(selected_files));
}

void
gui::archiver::extract(Gtk::ApplicationWindow& parent,
                       const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept
{
    if (!is_archiver_installed(parent) || selected_files.empty())
    {
        return;
    }

    // This will create a dialog where the to extraction path can be selected.
    const auto command =
        std::format("file-roller --extract {}", archiver_create_shell_file_list(selected_files));

    vfs::execute::command_line_async(command);
}

void
gui::archiver::extract_to(Gtk::ApplicationWindow& parent,
                          const std::span<const std::shared_ptr<vfs::file>> selected_files,
                          const std::filesystem::path& dest_dir) noexcept
{
    if (!is_archiver_installed(parent) || selected_files.empty())
    {
        return;
    }

    const auto command = std::format("file-roller --extract-to={} {}",
                                     vfs::execute::quote(dest_dir.string()),
                                     archiver_create_shell_file_list(selected_files));

    vfs::execute::command_line_async(command);
}

void
gui::archiver::open(Gtk::ApplicationWindow& parent,
                    const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept
{
    if (!is_archiver_installed(parent) || selected_files.empty())
    {
        return;
    }

    vfs::execute::command_line_async("file-roller {}",
                                     archiver_create_shell_file_list(selected_files));
}
