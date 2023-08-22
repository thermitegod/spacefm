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

#include <vector>

#include <memory>

#include <ztd/ztd.hxx>

struct VFSFileMonitor;
struct VFSFileMonitorCallbackEntry;

namespace vfs
{
    enum class file_monitor_event
    {
        created,
        deleted,
        changed,
        other,
    };

    using file_monitor = std::shared_ptr<VFSFileMonitor>;

    // Callback function which will be called when monitored events happen
    using file_monitor_callback = void (*)(const vfs::file_monitor& monitor,
                                           vfs::file_monitor_event event,
                                           const std::filesystem::path& file_name, void* user_data);

    using file_monitor_callback_entry = std::shared_ptr<VFSFileMonitorCallbackEntry>;

} // namespace vfs

struct VFSFileMonitor
{
    VFSFileMonitor() = delete;
    VFSFileMonitor(const std::filesystem::path& real_path, i32 wd);
    ~VFSFileMonitor();

    void add_user() noexcept;
    void remove_user() noexcept;
    bool has_users() const noexcept;

    const std::filesystem::path& path() const noexcept;
    i32 wd() const noexcept;

    void add_callback(vfs::file_monitor_callback callback, void* user_data) noexcept;
    void remove_callback(vfs::file_monitor_callback callback, void* user_data) noexcept;

    const std::vector<vfs::file_monitor_callback_entry> callbacks() const noexcept;

  private:
    std::filesystem::path path_{};
    i32 wd_{0};
    std::vector<vfs::file_monitor_callback_entry> callbacks_{};

    // user ref count
    i32 user_count{1};
};

/*
 * Init monitor:
 * Establish connection with inotify.
 */
bool vfs_file_monitor_init();

/*
 * Monitor changes of a file or directory.
 *
 * Parameters:
 * path: the file/dir to be monitored
 * cb: callback function to be called when file event happens.
 * user_data: user data to be passed to callback function.
 */
vfs::file_monitor vfs_file_monitor_add(const std::filesystem::path& path,
                                       vfs::file_monitor_callback cb, void* user_data);

/*
 * Remove previously installed monitor.
 */
void vfs_file_monitor_remove(const vfs::file_monitor& monitor, vfs::file_monitor_callback cb,
                             void* user_data);

/*
 * Clean up and shutdown file alteration monitor.
 */
void vfs_file_monitor_clean();
