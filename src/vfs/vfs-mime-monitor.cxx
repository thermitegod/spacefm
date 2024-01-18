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

#include <memory>

#include <functional>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "vfs/vfs-dir.hxx"
#include "vfs/vfs-user-dirs.hxx"

#include "vfs/vfs-mime-monitor.hxx"

static u32 mime_change_timer = 0;
static bool on_mime_change_timer(void* user_data);

struct mime_monitor
{
    mime_monitor(const std::shared_ptr<vfs::dir>& dir) : dir(dir){};

    static const std::shared_ptr<mime_monitor>
    create(const std::shared_ptr<vfs::dir>& dir) noexcept;

    std::shared_ptr<vfs::dir> dir;

    // signals
    void on_mime_change(const std::shared_ptr<vfs::file>& file);
};

const std::shared_ptr<mime_monitor>
mime_monitor::create(const std::shared_ptr<vfs::dir>& dir) noexcept
{
    return std::make_shared<mime_monitor>(dir);
}

void
mime_monitor::on_mime_change(const std::shared_ptr<vfs::file>& file)
{
    (void)file;

    if (mime_change_timer != 0)
    {
        // timer is already running, so ignore request
        // ztd::logger::debug("MIME-UPDATE already set");
        return;
    }

    // update mime database in 2 seconds
    mime_change_timer = g_timeout_add_seconds(2, (GSourceFunc)on_mime_change_timer, nullptr);
    // ztd::logger::debug("MIME-UPDATE timer started");
}

std::shared_ptr<mime_monitor> user_mime_monitor = nullptr;

static bool
on_mime_change_timer(void* user_data)
{
    (void)user_data;

    // ztd::logger::debug("MIME-UPDATE on_timer");
    const auto data_dir = vfs::user_dirs->data_dir();
    const std::string command1 = std::format("update-mime-database {}/mime", data_dir.string());
    ztd::logger::info("COMMAND({})", command1);
    Glib::spawn_command_line_async(command1);

    const std::string command2 =
        std::format("update-desktop-database {}/applications", data_dir.string());
    ztd::logger::info("COMMAND({})", command2);
    Glib::spawn_command_line_async(command2);

    g_source_remove(mime_change_timer);
    mime_change_timer = 0;
    return false;
}

void
vfs_mime_monitor()
{
    if (user_mime_monitor)
    {
        return;
    }

    const auto path = vfs::user_dirs->data_dir() / "mime" / "packages";
    if (!std::filesystem::is_directory(path))
    {
        return;
    }

    user_mime_monitor = mime_monitor::create(vfs::dir::create(path));

    // ztd::logger::debug("MIME-UPDATE watch started");
    user_mime_monitor->dir->add_event<spacefm::signal::file_created>(
        std::bind(&mime_monitor::on_mime_change, user_mime_monitor, std::placeholders::_1));
    user_mime_monitor->dir->add_event<spacefm::signal::file_changed>(
        std::bind(&mime_monitor::on_mime_change, user_mime_monitor, std::placeholders::_1));
    user_mime_monitor->dir->add_event<spacefm::signal::file_deleted>(
        std::bind(&mime_monitor::on_mime_change, user_mime_monitor, std::placeholders::_1));
}
