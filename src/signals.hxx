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

#pragma once

namespace spacefm
{
    enum class signal
    {
        // vfs::dir
        file_created,
        file_changed,
        file_deleted,
        file_listed,
        file_thumbnail_loaded,
        // file_load_complete,

        // VFSFileTask
        task_finish,

        // PTKFileBrowser
        chdir_before,
        chdir_begin,
        chdir_after,
        open_item,
        change_content,
        change_sel,
        change_pane,
    };
}
