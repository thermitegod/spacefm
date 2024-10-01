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

#include <format>

#include <filesystem>

#include <span>
#include <vector>

#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "logger.hxx"

#include "datatypes/datatypes.hxx"

#include "vfs/vfs-file.hxx"

#include "ptk/ptk-file-properties.hxx"

static void
show_file_properties_dialog(GtkWindow* parent, const std::filesystem::path& cwd,
                            const std::span<const std::shared_ptr<vfs::file>> selected_files,
                            i32 page) noexcept
{
    (void)parent;

#if defined(HAVE_DEV)
    const auto binary = std::format("{}/{}", DIALOG_BUILD_ROOT, DIALOG_PROPERTIES);
#else
    const auto binary = Glib::find_program_in_path(DIALOG_PROPERTIES);
#endif
    if (binary.empty())
    {
        logger::error("Failed to find pattern dialog binary: {}", DIALOG_PROPERTIES);
        return;
    }

    // clang-format off
    const auto request = datatype::properties_dialog::request{
        .cwd = cwd,
        .page = (std::uint32_t)page,
        .files = [selected_files]()
        {
            std::vector<std::string> result;
            for (const auto& file : selected_files)
            {
                result.push_back(file->path());
            }
            return result;
        }()
    };
    // clang-format on

    std::string buffer;
    auto ec = glz::write_json(request, buffer);
    if (ec)
    {
        logger::error("Failed to create JSON: {}", glz::format_error(ec, buffer));
        return;
    }
    // logger::trace("{}", buffer);

    const auto command = std::format(R"({} --json '{}')", binary, buffer);
    Glib::spawn_command_line_async(command);
}

void
ptk_show_file_properties(GtkWindow* parent, const std::filesystem::path& cwd,
                         const std::span<const std::shared_ptr<vfs::file>> selected_files,
                         const i32 page) noexcept
{
    if (selected_files.empty())
    {
        const auto file = vfs::file::create(cwd);
        const std::vector<std::shared_ptr<vfs::file>> cwd_selected{file};

        show_file_properties_dialog(parent, cwd.parent_path(), cwd_selected, page);
    }
    else
    {
        show_file_properties_dialog(parent, cwd, selected_files, page);
    }
}
