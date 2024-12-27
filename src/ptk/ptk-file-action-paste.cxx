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

#include <filesystem>

#include <vector>

#include <glibmm.h>

#if __has_include(<magic_enum/magic_enum.hpp>)
// >=magic_enum-0.9.7
#include <magic_enum/magic_enum.hpp>
#else
// <=magic_enum-0.9.6
#include <magic_enum.hpp>
#endif

#include <ztd/ztd.hxx>

#include "logger.hxx"

#include "xset/xset.hxx"

#include "vfs/vfs-file.hxx"

#include "ptk/ptk-dialog.hxx"
#include "ptk/ptk-file-browser.hxx"
#include "ptk/ptk-clipboard.hxx"
#include "ptk/ptk-file-action-rename.hxx"
#include "ptk/ptk-file-action-paste.hxx"

void
ptk::action::paste_files(ptk::browser* browser, const std::filesystem::path& cwd) noexcept
{
    bool is_cut = false;
    i32 missing_targets = 0;

    const std::vector<std::filesystem::path> files =
        ptk::clipboard::get_file_paths(cwd, &is_cut, &missing_targets);

    for (const auto& file_path : files)
    {
        const auto file = vfs::file::create(file_path);

        const auto result = ptk::action::rename_files(browser,
                                                      file_path.parent_path(),
                                                      file,
                                                      cwd.c_str(),
                                                      !is_cut,
                                                      ptk::action::rename_mode::rename);
        if (!result)
        {
            missing_targets = 0;
            break;
        }
    }

    if (missing_targets > 0)
    {
        GtkWidget* parent = nullptr;
        if (browser)
        {
#if (GTK_MAJOR_VERSION == 4)
            parent = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(browser)));
#elif (GTK_MAJOR_VERSION == 3)
            parent = gtk_widget_get_toplevel(GTK_WIDGET(browser));
#endif
        }

        ptk::dialog::error(GTK_WINDOW(parent),
                           "Error",
                           std::format("{} target{} missing",
                                       missing_targets,
                                       missing_targets > 1 ? "s are" : " is"));
    }
}
