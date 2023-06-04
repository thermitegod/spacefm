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

#include <glib.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-dir.hxx"
#include "vfs/vfs-user-dirs.hxx"

#include "vfs/vfs-mime-monitor.hxx"

static u32 mime_change_timer = 0;
static vfs::dir mime_dir = nullptr;

static bool
on_mime_change_timer(void* user_data)
{
    (void)user_data;

    // ztd::logger::debug("MIME-UPDATE on_timer");
    const auto data_dir = vfs::user_dirs->data_dir();
    const std::string command1 = std::format("update-mime-database {}/mime", data_dir.string());
    ztd::logger::info("COMMAND={}", command1);
    Glib::spawn_command_line_async(command1);

    const std::string command2 =
        std::format("update-desktop-database {}/applications", data_dir.string());
    ztd::logger::info("COMMAND={}", command2);
    Glib::spawn_command_line_async(command2);

    g_source_remove(mime_change_timer);
    mime_change_timer = 0;
    return false;
}

static void
mime_change()
{
    if (mime_change_timer != 0)
    {
        // timer is already running, so ignore request
        // ztd::logger::debug("MIME-UPDATE already set");
        return;
    }

    if (mime_dir)
    {
        // update mime database in 2 seconds
        mime_change_timer = g_timeout_add_seconds(2, (GSourceFunc)on_mime_change_timer, nullptr);
        // ztd::logger::debug("MIME-UPDATE timer started");
    }
}

void
vfs_mime_monitor()
{
    // start watching for changes
    if (mime_dir)
    {
        return;
    }

    const auto path = vfs::user_dirs->data_dir() / "mime" / "packages";
    if (!std::filesystem::is_directory(path))
    {
        return;
    }

    mime_dir = vfs_dir_get_by_path(path);
    if (!mime_dir)
    {
        return;
    }

    // ztd::logger::debug("MIME-UPDATE watch started");
    mime_dir->add_event<spacefm::signal::file_listed>(mime_change);
    mime_dir->add_event<spacefm::signal::file_changed>(mime_change);
    mime_dir->add_event<spacefm::signal::file_deleted>(mime_change);
    mime_dir->add_event<spacefm::signal::file_changed>(mime_change);
}
