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

#include <array>
#include <filesystem>
#include <format>
#include <set>
#include <stop_token>
#include <utility>

#include <cerrno>
#include <cstddef>
#include <cstring>

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>

#include "vfs/notify-cpp/notify.hxx"

notify::inotify::inotify(const std::filesystem::path& path, std::set<notify::event> events)
    : path_(path)
{
    inotify_fd_ = inotify_init1(IN_NONBLOCK);
    if (inotify_fd_ == -1)
    {
        throw std::runtime_error(std::format("inotify init failed: {}", std::strerror(errno)));
    }

    event_fd_ = eventfd(0, EFD_NONBLOCK);
    if (event_fd_ == -1)
    {
        throw std::runtime_error(std::format("eventfd init failed: {}", std::strerror(errno)));
    }

    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ == -1)
    {
        throw std::runtime_error(std::format("epoll init failed: {}", std::strerror(errno)));
    }

    std::int32_t result = 0;
    epoll_event event{};

    event = {.events = EPOLLIN, .data = {.fd = event_fd_}};
    result = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, event_fd_, &event);
    if (result == -1)
    {
        throw std::runtime_error(
            std::format("failed to add eventfd to epoll: {}", std::strerror(errno)));
    }

    event = {.events = EPOLLIN, .data = {.fd = inotify_fd_}};
    result = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, inotify_fd_, &event);
    if (result == -1)
    {
        throw std::runtime_error(
            std::format("failed to add inotify to epoll: {}", std::strerror(errno)));
    }

    const auto mask = std::invoke(
        [](std::set<notify::event> events) noexcept
        {
            std::uint32_t mask = 0;
            for (const auto& e : events)
            {
                mask = mask | std::to_underlying(e);
            }
            return mask;
        },
        events);

    wd_ = inotify_add_watch(inotify_fd_, path.c_str(), mask);

    if (wd_ == -1)
    {
        if (errno == 28)
        {
            throw std::runtime_error(
                std::format("adding inotify watch failed with '{}' (Help: increase "
                            "/proc/sys/fs/inotify/max_user_watches) for path '{}'",
                            std::strerror(errno),
                            path.string()));
        }
        throw std::runtime_error(std::format("adding inotify watch failed with '{}' for path '{}'",
                                             std::strerror(errno),
                                             path.string()));
    }
}

notify::inotify::~inotify() noexcept
{
    // std::println("notify::~notify({})", static_cast<void*>(this));
    close(inotify_fd_);
    close(event_fd_);
    close(epoll_fd_);
}

void
notify::inotify::stop() const noexcept
{
    std::uint64_t value = 1;
    auto _ = write(event_fd_, &value, sizeof(value));
}

notify::inotify::file_system_event
notify::inotify::get_next_event_from_queue() noexcept
{
    auto event = queue_.front();
    queue_.pop();
    return event;
}

std::optional<notify::inotify::file_system_event>
notify::inotify::get_next_event(const std::stop_token& stoken)
{
    static constexpr std::size_t MAX_EVENTS = 4096;
    static constexpr std::size_t EVENT_SIZE = (sizeof(inotify_event));
    static constexpr std::size_t EVENT_BUF_LEN = (MAX_EVENTS * (EVENT_SIZE + 16));

    if (!queue_.empty())
    {
        return get_next_event_from_queue();
    }

    std::ptrdiff_t length = 0;
    std::array<char, EVENT_BUF_LEN> buffer{};

    while (!stoken.stop_requested())
    {
        std::array<epoll_event, 1> events{};

        const auto nfds = epoll_pwait(epoll_fd_, events.data(), events.size(), -1, nullptr);
        if (nfds == -1)
        {
            return std::nullopt;
        }

        if (nfds == 0)
        {
            continue;
        }

        const auto& event = events.front();

        if (event.data.fd == inotify_fd_)
        {
            buffer.fill('\0');

            length = read(inotify_fd_, buffer.data(), buffer.size());
            if (length == -1)
            {
                if (errno != EINTR)
                {
                    continue;
                }
            }
            break;
        }
        else if (event.data.fd == event_fd_)
        {
            continue;
        }
    }

    std::size_t i = 0;
    while (std::cmp_less(i, length) && !stoken.stop_requested())
    {
        const auto* event = reinterpret_cast<inotify_event*>(&buffer[i]);
        if (!event)
        {
            return std::nullopt;
        }

        // remove IN_ISDIR bit from event mask, if the
        // mask is i.e. (IN_CREATE | IN_ISDIR) we only want IN_CREATE
        const auto mask = event->mask & static_cast<std::uint32_t>(IN_ISDIR)
                              ? event->mask & ~static_cast<std::uint32_t>(IN_ISDIR)
                              : event->mask;

        queue_.push(file_system_event(event->len ? path_ / event->name : path_, get_inotify(mask)));

        i += EVENT_SIZE + event->len;
    }

    if (stoken.stop_requested() || queue_.empty())
    {
        return std::nullopt;
    }

    return get_next_event_from_queue();
}

notify::event
notify::inotify::get_inotify(const std::uint32_t event) noexcept
{
    switch (event)
    {
        case IN_ACCESS:
            return event::access;
        case IN_MODIFY:
            return event::modify;
        case IN_ATTRIB:
            return event::attrib;
        case IN_CLOSE_WRITE:
            return event::close_write;
        case IN_CLOSE_NOWRITE:
            return event::close_nowrite;
        case IN_OPEN:
            return event::open;
        case IN_MOVED_FROM:
            return event::moved_from;
        case IN_MOVED_TO:
            return event::moved_to;
        case IN_CREATE:
            return event::create;
        case IN_DELETE:
            return event::delete_sub;
        case IN_DELETE_SELF:
            return event::delete_self;
        case IN_MOVE_SELF:
            return event::move_self;
        case IN_UNMOUNT:
            return event::umount;
        case IN_Q_OVERFLOW:
            return event::queue_overflow;
        case IN_IGNORED:
            return event::ignored;
        case IN_CLOSE:
            return event::close;
        case IN_MOVE:
            return event::move;
        case IN_ALL_EVENTS:
            return event::all;
        default:
            return event::none;
    }
}
