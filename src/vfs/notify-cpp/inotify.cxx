/*
 * Copyright (c) 2017 Erik Zenker <erikzenker@hotmail.com>
 * Copyright (c) 2018 Rafael Sadowski <rafael@sizeofvoid.org>
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

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

#include <cstring>

#include <sys/inotify.h>

#include "vfs/notify-cpp/inotify.hxx"

// #define PRINT_DBG
#if defined(PRINT_DBG)
#include <print>
#endif

notify::inotify::inotify() : inotify_fd_(0)
{
    this->inotify_fd_ = inotify_init1(IN_NONBLOCK);
    if (this->inotify_fd_ == -1)
    {
        throw std::runtime_error(std::format("inotify init failed: {}", std::strerror(errno)));
    }
}

notify::inotify::~inotify()
{
    close(this->inotify_fd_);
}

void
notify::inotify::watch_file(const file_system_event& fse)
{
    if (!this->check_watch_file(fse))
    {
        return;
    }

    this->watch(fse);
}

void
notify::inotify::watch_directory(const file_system_event& fse)
{
    if (!this->check_watch_directory(fse))
    {
        return;
    }

    this->watch(fse);
}

void
notify::inotify::watch(const file_system_event& fse)
{
    const auto wd =
        inotify_add_watch(this->inotify_fd_, fse.path().c_str(), this->get_event_mask(fse.event()));

    if (wd == -1)
    {
        if (errno == 28)
        {
            throw std::runtime_error(
                std::format("adding inotify watch failed with '{}' (Help: increase "
                            "/proc/sys/fs/inotify/max_user_watches) for path '{}'",
                            std::strerror(errno),
                            fse.path().string()));
        }
        throw std::runtime_error(std::format("adding inotify watch failed with '{}' for path '{}'",
                                             std::strerror(errno),
                                             fse.path().string()));
    }

    this->directory_map_.emplace(wd, fse.path());
}

void
notify::inotify::unwatch(const file_system_event& fse)
{
    const auto it = std::ranges::find_if(this->directory_map_,
                                         [&](const std::pair<std::int32_t, std::string>& key)
                                         { return key.second == fse.path(); });

    if (it != this->directory_map_.cend())
    {
        this->remove_watch(it->first);
    }
}

void
notify::inotify::remove_watch(const std::int32_t wd) const
{
    const auto result = inotify_rm_watch(this->inotify_fd_, wd);
    if (result == -1)
    {
        throw std::runtime_error(
            std::format("removing inotify watch failed with '{}'", std::strerror(errno)));
    }
}

std::filesystem::path
notify::inotify::wd_to_path(const std::int32_t wd) const noexcept
{
    return this->directory_map_.at(wd);
}

std::shared_ptr<notify::file_system_event>
notify::inotify::get_next_event()
{
    static constexpr std::size_t MAX_EVENTS = 4096;
    static constexpr std::size_t EVENT_SIZE = (sizeof(inotify_event));
    static constexpr std::size_t EVENT_BUF_LEN = (MAX_EVENTS * (EVENT_SIZE + 16));

    std::unique_lock<std::mutex> lock(this->mutex_);

    std::array<char, EVENT_BUF_LEN> buffer{};

    while (this->queue_.empty() && this->is_running())
    {
        std::ptrdiff_t length = 0;
        buffer.fill('\0');
        while (length <= 0 && this->is_running())
        {
            this->cv_.wait_for(lock, this->thread_sleep_, [this] { return this->is_stopped(); });

            length = read(this->inotify_fd_, buffer.data(), buffer.size());
            if (length == -1)
            {
                if (errno != EINTR)
                {
                    continue;
                }
            }
        }

        if (this->is_stopped())
        {
            return nullptr;
        }

        std::size_t i = 0;
        while (std::cmp_less(i, length) && this->is_running())
        {
            const auto* event = reinterpret_cast<inotify_event*>(&buffer[i]);
            if (!event)
            {
                return nullptr;
            }

            const auto path = this->wd_to_path(event->wd);

            if (!this->is_ignored_once(path))
            {
                // remove IN_ISDIR bit from event mask, if the
                // mask is (IN_CREATE | IN_ISDIR) we only want IN_CREATE
                // TODO, report event occurred to a directory
                const auto mask = event->mask & static_cast<std::uint32_t>(IN_ISDIR)
                                      ? event->mask & ~static_cast<std::uint32_t>(IN_ISDIR)
                                      : event->mask;

                this->queue_.push(
                    std::make_shared<file_system_event>(event->len ? path / event->name : path,
                                                        event_handler::get_inotify(mask)));

#if defined(PRINT_DBG)
                std::println("raw = {}\t| lookup = {},{}\t| cookie = {}\t| {}",
                             event->mask,
                             event_handler::get_inotify(mask),
                             static_cast<std::uint32_t>(event_handler::get_inotify(mask)),
                             event->cookie,
                             event->len ? (path / event->name).string() : path.string());
#endif
            }
            i += EVENT_SIZE + event->len;
        }
    }

    if (this->is_stopped() || this->queue_.empty())
    {
        return nullptr;
    }

    // Return next event
    auto event = this->queue_.front();
    this->queue_.pop();
    return event;
}

std::uint32_t
notify::inotify::get_event_mask(const notify::event event) const
{
    return event_handler::convert_to_inotify_events(event);
}
