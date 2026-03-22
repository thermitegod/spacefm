/*
 * Copyright (c) 2017 Erik Zenker <erikzenker@hotmail.com>
 * Copyright (c) 2018 Rafael Sadowski <rafael.sadowski@computacenter.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <filesystem>
#include <queue>
#include <set>
#include <stop_token>

#include <cstdint>

#include <sys/inotify.h>

#include <sigc++/sigc++.h>

namespace notify
{
enum class event : std::uint32_t
{
    none = 0,

    // Supported events
    access = IN_ACCESS,               // File was accessed.
    modify = IN_MODIFY,               // File was modified.
    attrib = IN_ATTRIB,               // Metadata changed.
    close_write = IN_CLOSE_WRITE,     // Writtable file was closed.
    close_nowrite = IN_CLOSE_NOWRITE, // Unwrittable file closed.
    open = IN_OPEN,                   // File was opened.
    moved_from = IN_MOVED_FROM,       // File was moved from X.
    moved_to = IN_MOVED_TO,           // File was moved to Y.
    create = IN_CREATE,               // Subfile was created.
    delete_sub = IN_DELETE,           // Subfile was deleted.
    delete_self = IN_DELETE_SELF,     // Self was deleted.
    move_self = IN_MOVE_SELF,         // Self was moved.

    // Events sent by the kernel.
    umount = IN_UNMOUNT,            // Backing fs was unmounted.
    queue_overflow = IN_Q_OVERFLOW, // Event queued overflowed.
    ignored = IN_IGNORED,           // File was ignored.

    // Helper events
    close = IN_CLOSE, // Close.
    move = IN_MOVE,   // Moves.

    // All events which a program can wait on.
    all = IN_ALL_EVENTS,
};

class inotify
{
  public:
    inotify(const std::filesystem::path& path, std::set<event> events);
    ~inotify() noexcept;

    struct file_system_event final
    {
        // absoulte path + filename
        std::filesystem::path path;
        event event;
    };

    /**
     * @brief Blocking wait on new events of watched files/directories
     *        specified on the eventmask. file_system_event's
     *        will be returned one by one. Thus this
     *        function can be called in some while(true)
     *        loop.
     * @return A new file_system_event
     */
    [[nodiscard]] std::optional<file_system_event> get_next_event(const std::stop_token& stoken);

    void stop() const noexcept;

  private:
    [[nodiscard]] file_system_event get_next_event_from_queue() noexcept;

    [[nodiscard]] event get_inotify(const std::uint32_t event) noexcept;

    std::int32_t inotify_fd_ = 0;
    std::int32_t event_fd_ = 0;
    std::int32_t epoll_fd_ = 0;

    std::int32_t wd_ = 0;
    std::filesystem::path path_;

    std::queue<file_system_event> queue_;
};
} // namespace notify
