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

#include "datatypes/datatypes.hxx"
#include "datatypes/external-dialog.hxx"

#include "gui/dialog/bookmarks.hxx"

#include "vfs/bookmarks.hxx"

#include "package.hxx"

void
gui::dialog::bookmarks() noexcept
{
    // datatype::run_dialog_sync(spacefm::package.dialog.bookmarks);

    const auto response = datatype::run_dialog_sync<datatype::bookmarks::bookmarks>(
        spacefm::package.dialog.bookmarks,
        datatype::bookmarks::bookmarks{.bookmarks = {vfs::bookmarks::bookmarks().cbegin(),
                                                     vfs::bookmarks::bookmarks().cend()}});

    if (response)
    {
        vfs::bookmarks::bookmarks(response.value());
    }
}
