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

#include <functional>

#include <gtkmm.h>
#include <glibmm.h>
#include <sigc++/sigc++.h>

#include <ztd/ztd.hxx>

namespace vfs
{
struct monitor
{
    enum class event
    {
        created,
        deleted,
        changed,
        other,
    };

    // Callback function which will be called when monitored events happen
    using callback_t =
        std::function<void(const vfs::monitor::event event, const std::filesystem::path& path)>;

    monitor() = delete;
    monitor(const std::filesystem::path& path, const callback_t& callback);
    ~monitor();
    monitor(const monitor& other) = delete;
    monitor(monitor&& other) = delete;
    monitor& operator=(const monitor& other) = delete;
    monitor& operator=(monitor&& other) = delete;

    [[nodiscard]] static const std::shared_ptr<monitor> create(const std::filesystem::path& path,
                                                               const callback_t& callback) noexcept;

  private:
    [[nodiscard]] bool on_inotify_event(const Glib::IOCondition condition) const noexcept;
    void dispatch_event(const event event, const std::filesystem::path& path) const noexcept;

    i32 inotify_fd_{-1};
    i32 inotify_wd_{-1};

    std::filesystem::path path_;

#if (GTK_MAJOR_VERSION == 4)
    Glib::RefPtr<Glib::IOChannel> inotify_io_channel_ = nullptr;
#elif (GTK_MAJOR_VERSION == 3)
    Glib::RefPtr<Glib::IOChannel> inotify_io_channel_;
#endif
    sigc::connection signal_io_handler_;

    callback_t callback_;
};
} // namespace vfs
