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
#include "vfs/notify-cpp/event.hxx"
#include "vfs/notify-cpp/notify_controller.hxx"
#include "vfs/user-dirs.hxx"

namespace
{
notify::notify_controller notifier = notify::inotify_controller();
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

    notifier
        .watch_directory({path,
                          {
                              notify::event::attrib,
                              notify::event::close_write,
                              notify::event::moved_from,
                              notify::event::moved_to,
                              notify::event::create,
                              notify::event::delete_sub,
                          }})
        .on_unexpected_event( // this can technically be used as the default handler
            [](const notify::notification&)
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
            });

    thread = std::jthread([&]() { notifier.run(); });
    pthread_setname_np(thread.native_handle(), "mime notifier");
}

void
vfs::mime_monitor_shutdown() noexcept
{
    notifier.stop();
    thread.join();
}
