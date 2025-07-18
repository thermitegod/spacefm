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
#include <string_view>
#include <vector>

#include <glibmm.h>

#include <magic_enum/magic_enum.hpp>

#include <ztd/ztd.hxx>

#include "xset/xset.hxx"

#include "gui/clipboard.hxx"
#include "gui/file-browser.hxx"

#include "gui/action/paste.hxx"

#include "gui/dialog/rename.hxx"
#include "gui/dialog/text.hxx"

#include "vfs/file.hxx"

void
gui::action::paste_files(gui::browser* browser, const std::filesystem::path& cwd) noexcept
{
    (void)cwd;

    bool is_cut = false;
    i32 missing_targets = 0;

    const std::vector<std::filesystem::path> files =
        gui::clipboard::get_file_paths(&is_cut, &missing_targets);

    for (const auto& file_path : files)
    {
        const auto file = vfs::file::create(file_path);

        const auto result =
            gui::dialog::rename_files(browser, file_path.parent_path(), file, cwd.c_str(), !is_cut);
        if (result == 0)
        {
            missing_targets = 0;
            break;
        }
    }

    if (missing_targets > 0)
    {
        gui::dialog::error("Error",
                           std::format("{} target{} missing",
                                       missing_targets,
                                       missing_targets > 1 ? "s are" : " is"));
    }
}
