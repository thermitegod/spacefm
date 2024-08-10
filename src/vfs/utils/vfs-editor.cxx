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

#include <memory>

#include <gtkmm.h>
#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "logger.hxx"

#include "vfs/vfs-app-desktop.hxx"

#include "ptk/ptk-dialog.hxx"

#include "xset/xset.hxx"

#include "vfs/utils/vfs-editor.hxx"

void
vfs::utils::open_editor(const std::filesystem::path& path) noexcept
{
    const auto check_editor = xset_get_s(xset::name::editor);
    if (!check_editor)
    {
        ptk::dialog::error(nullptr,
                           "Editor Not Set",
                           "Please set your editor in View|Preferences|Advanced");
        return;
    }
    const auto& editor = check_editor.value();

    std::shared_ptr<vfs::desktop> desktop;
    if (editor.ends_with(".desktop"))
    {
        desktop = vfs::desktop::create(editor);
    }
    else
    { // this might work
        logger::warn<logger::domain::vfs>("Editor is not set to a .desktop file");
        desktop = vfs::desktop::create(std::format("{}.desktop", editor));
    }

    const std::vector<std::filesystem::path> open_files{path};

    const bool opened = desktop->open_files(path.parent_path(), open_files);
    if (!opened)
    {
        ptk::dialog::error(
            nullptr,
            "Error",
            std::format("Unable to use '{}' to open file:\n{}", editor, path.string()));
    }
}
