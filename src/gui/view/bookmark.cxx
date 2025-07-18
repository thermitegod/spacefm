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

#include "gui/file-browser.hxx"

#include "gui/view/bookmark.hxx"

#include "vfs/bookmarks.hxx"

void
gui::view::bookmark::add(const std::filesystem::path& path) noexcept
{
    if (path.empty())
    {
        return;
    }

    vfs::bookmarks::add(path);
}

void
gui::view::bookmark::add_callback(GtkMenuItem* menuitem, gui::browser* browser) noexcept
{
    (void)menuitem;

    gui::view::bookmark::add(browser->cwd());
}
