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

#include "bookmarks.hxx"

#include "ptk/ptk-file-browser.hxx"
#include "ptk/ptk-bookmark-view.hxx"

void
ptk::view::bookmark::add(const std::filesystem::path& book_path) noexcept
{
    if (book_path.empty())
    {
        return;
    }

    add_bookmarks(book_path);
}

void
ptk::view::bookmark::add_callback(GtkMenuItem* menuitem, ptk::browser* browser) noexcept
{
    (void)menuitem;

    ptk::view::bookmark::add(browser->cwd());
}
