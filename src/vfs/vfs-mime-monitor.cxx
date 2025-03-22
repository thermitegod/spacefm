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
#include <functional>
#include <memory>
#include <string>

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include "vfs/vfs-dir.hxx"
#include "vfs/vfs-mime-monitor.hxx"
#include "vfs/vfs-user-dirs.hxx"

#include "logger.hxx"

namespace global
{
u32 mime_change_timer = 0;
}

struct mime_monitor
{
    mime_monitor(const std::shared_ptr<vfs::dir>& dir) : dir(dir) {}

    [[nodiscard]] static const std::shared_ptr<mime_monitor>
    create(const std::shared_ptr<vfs::dir>& dir) noexcept;

    std::shared_ptr<vfs::dir> dir;

    // signals
    static void on_mime_change(const std::shared_ptr<vfs::file>& file) noexcept;
    static bool on_mime_change_timer(void* user_data) noexcept;
};

const std::shared_ptr<mime_monitor>
mime_monitor::create(const std::shared_ptr<vfs::dir>& dir) noexcept
{
    return std::make_shared<mime_monitor>(dir);
}

void
mime_monitor::on_mime_change(const std::shared_ptr<vfs::file>& file) noexcept
{
    (void)file;

    if (global::mime_change_timer != 0)
    {
        // timer is already running, so ignore request
        // logger::debug<logger::domain::vfs>("MIME-UPDATE already set");
        return;
    }

    // update mime database in 2 seconds
    global::mime_change_timer =
        g_timeout_add_seconds(2, (GSourceFunc)mime_monitor::on_mime_change_timer, nullptr);
    // logger::debug<logger::domain::vfs>("MIME-UPDATE timer started");
}

namespace global
{
std::shared_ptr<mime_monitor> user_mime_monitor = nullptr;
}

bool
mime_monitor::on_mime_change_timer(void* user_data) noexcept
{
    (void)user_data;

    // logger::debug<logger::domain::vfs>("MIME-UPDATE on_timer");
    const auto data_dir = vfs::user::data();
    const std::string command1 = std::format("update-mime-database {}/mime", data_dir.string());
    logger::info<logger::domain::vfs>("COMMAND({})", command1);
    Glib::spawn_command_line_async(command1);

    const std::string command2 =
        std::format("update-desktop-database {}/applications", data_dir.string());
    logger::info<logger::domain::vfs>("COMMAND({})", command2);
    Glib::spawn_command_line_async(command2);

    g_source_remove(global::mime_change_timer);
    global::mime_change_timer = 0;
    return false;
}

void
vfs::mime_monitor() noexcept
{
    if (global::user_mime_monitor)
    {
        return;
    }

    const auto path = vfs::user::data() / "mime" / "packages";
    if (!std::filesystem::is_directory(path))
    {
        return;
    }

    global::user_mime_monitor = mime_monitor::create(vfs::dir::create(path, nullptr));

    // logger::debug<logger::domain::vfs>("MIME-UPDATE watch started");
    global::user_mime_monitor->dir->signal_file_created().connect(
        [](auto f) { mime_monitor::on_mime_change(f); });
    global::user_mime_monitor->dir->signal_file_changed().connect(
        [](auto f) { mime_monitor::on_mime_change(f); });
    global::user_mime_monitor->dir->signal_file_deleted().connect(
        [](auto f) { mime_monitor::on_mime_change(f); });
}
