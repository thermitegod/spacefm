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
#include <string>
#include <string_view>
#include <vector>

#include <glibmm.h>
#include <gtkmm.h>

#include <ztd/ztd.hxx>

#include "xset/xset.hxx"

#include "gui/dialog/text.hxx"

#include "vfs/app-desktop.hxx"

#include "vfs/utils/editor.hxx"

#include "logger.hxx"

void
vfs::utils::open_editor(const std::filesystem::path& path) noexcept
{
    const auto editor = xset_get_s(xset::name::editor);
    if (!editor)
    {
        ptk::dialog::error(nullptr,
                           "Editor Not Set",
                           "Please set your editor in View|Preferences|Advanced");
        return;
    }

    if (!editor->ends_with(".desktop"))
    {
        logger::error<logger::domain::vfs>("Editor is not set to a .desktop file");
        return;
    }

    const auto desktop = vfs::desktop::create(editor.value());
    if (!desktop)
    {
        return;
    }

    const std::vector<std::filesystem::path> open_files{path};

    const auto opened = desktop->open_files(path.parent_path(), open_files);
    if (!opened)
    {
        ptk::dialog::error(
            nullptr,
            "Error",
            std::format("Unable to use '{}' to open file:\n{}", editor.value(), path.string()));
    }
}
