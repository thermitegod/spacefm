/**
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 *
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

#include <filesystem>

#include <memory>

#include <gtkmm.h>
#include <glibmm.h>
#include <sigc++/sigc++.h>

#include <ztd/ztd.hxx>

namespace vfs
{
    enum class file_monitor_event
    {
        created,
        deleted,
        changed,
        other,
    };

    // Callback function which will be called when monitored events happen
    using file_monitor_callback_t = void (*)(const vfs::file_monitor_event event,
                                             const std::filesystem::path& path, void* user_data);

    struct file_monitor
    {
        file_monitor() = delete;
        file_monitor(const std::filesystem::path& path, vfs::file_monitor_callback_t callback,
                     void* user_data);
        ~file_monitor();

      private:
        bool on_inotify_event(const Glib::IOCondition condition);
        void dispatch_event(vfs::file_monitor_event event,
                            const std::filesystem::path& file_name) noexcept;

        i32 inotify_fd_{-1};
        i32 inotify_wd_{-1};

        std::filesystem::path path_{};

#if (GTK_MAJOR_VERSION == 4)
        Glib::RefPtr<Glib::IOChannel> inotify_io_channel = nullptr;
#elif (GTK_MAJOR_VERSION == 3)
        Glib::RefPtr<Glib::IOChannel> inotify_io_channel;
#endif
        sigc::connection signal_io_handler;

        struct callback_entry
        {
            vfs::file_monitor_callback_t callback{nullptr};
            void* user_data{nullptr};
        };
        // std::vector<callback_entry> callbacks_{};
        callback_entry callback_{};
    };
} // namespace vfs
