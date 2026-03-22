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
#include <thread>

#include "vfs/execute.hxx"
#include "vfs/mime-monitor.hxx"
#include "vfs/notify-cpp/controller.hxx"
#include "vfs/user-dirs.hxx"

namespace
{
std::jthread thread;
} // namespace

void
vfs::mime_monitor_init() noexcept
{
    const auto path = vfs::user::data() / "mime" / "packages";
    if (!std::filesystem::is_directory(path))
    {
        return;
    }

    static notify::controller notifier(path);

    auto slot = [](const std::filesystem::path&)
    {
        if (std::filesystem::exists(vfs::user::data() / "mime"))
        {
            vfs::execute::command_line_async("update-mime-database {}/mime",
                                             vfs::user::data().string());
        }

        if (std::filesystem::exists(vfs::user::data() / "applications"))
        {
            vfs::execute::command_line_async("update-desktop-database {}/applications",
                                             vfs::user::data().string());
        }
    };

    notifier.signal_attrib().connect(slot);
    notifier.signal_close_write().connect(slot);
    notifier.signal_moved_from().connect(slot);
    notifier.signal_moved_to().connect(slot);
    notifier.signal_create().connect(slot);
    notifier.signal_delete().connect(slot);

    thread = std::jthread([&](const std::stop_token& stoken) { notifier.run(stoken); });
    pthread_setname_np(thread.native_handle(), "mime notifier");
}

void
vfs::mime_monitor_shutdown() noexcept
{
    thread.request_stop();
    thread.join();
}
