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
#include <array>
#include <filesystem>
#include <format>
#include <memory>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <vector>

#include <cerrno>
#include <cstddef>
#include <cstring>

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>

#include "vfs/notify-cpp/notify.hxx"

namespace notify
{
// #define PRINT_DBG
#if defined(PRINT_DBG)
#include <print>
#endif

inotify::inotify() : inotify_fd_(0)
{
    this->inotify_fd_ = inotify_init1(IN_NONBLOCK);
    if (this->inotify_fd_ == -1)
    {
        throw std::runtime_error(std::format("inotify init failed: {}", std::strerror(errno)));
    }

    this->event_fd_ = eventfd(0, EFD_NONBLOCK);
    if (this->event_fd_ == -1)
    {
        throw std::runtime_error(std::format("eventfd init failed: {}", std::strerror(errno)));
    }

    this->epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (this->epoll_fd_ == -1)
    {
        throw std::runtime_error(std::format("epoll init failed: {}", std::strerror(errno)));
    }

    std::int32_t result = 0;
    epoll_event event{};

    event = {.events = EPOLLIN, .data = {.fd = this->event_fd_}};
    result = epoll_ctl(this->epoll_fd_, EPOLL_CTL_ADD, this->event_fd_, &event);
    if (result == -1)
    {
        throw std::runtime_error(
            std::format("failed to add eventfd to epoll: {}", std::strerror(errno)));
    }

    event = {.events = EPOLLIN, .data = {.fd = this->inotify_fd_}};
    result = epoll_ctl(this->epoll_fd_, EPOLL_CTL_ADD, this->inotify_fd_, &event);
    if (result == -1)
    {
        throw std::runtime_error(
            std::format("failed to add inotify to epoll: {}", std::strerror(errno)));
    }
}

inotify::~inotify() noexcept
{
    // std::println("notify::~notify({})", static_cast<void*>(this));
    close(this->inotify_fd_);
    close(this->event_fd_);
    close(this->epoll_fd_);
}

void
inotify::ignore(const std::filesystem::path& p) noexcept
{
    this->ignored_.push_back(p);
}

void
inotify::ignore_once(const std::filesystem::path& p) noexcept
{
    this->ignored_once_.push_back(p);
}

bool
inotify::is_ignored_once(const std::filesystem::path& p) noexcept
{
    const auto found = std::ranges::find(this->ignored_once_, p);
    if (found != this->ignored_once_.cend())
    {
        this->ignored_once_.erase(found);
        return true;
    }
    return false;
}

bool
inotify::is_ignored(const std::filesystem::path& p) const noexcept
{
    return std::ranges::contains(this->ignored_, p);
}

std::filesystem::path
inotify::path_from_fd(const std::int32_t fd) const noexcept
{
    const std::filesystem::path link_path = std::format("/proc/self/fd/{}", fd);

    std::error_code ec;
    auto target = std::filesystem::read_symlink(link_path, ec);
    if (ec)
    {
        return {};
    }
    return target;
}

bool
inotify::check_watch_file(const file_system_event& fse) const
{
    if (!std::filesystem::exists(fse.path()))
    {
        throw std::invalid_argument(
            std::format("Failed to watch file, does not exist: {}", fse.path().string()));
    }

    if (!std::filesystem::is_regular_file(fse.path()))
    {
        throw std::invalid_argument(
            std::format("Failed to watch file, not a regular file: {}", fse.path().string()));
    }
    return !this->is_ignored(fse.path());
}

bool
inotify::check_watch_directory(const file_system_event& fse) const
{
    if (!std::filesystem::exists(fse.path()))
    {
        throw std::invalid_argument(
            std::format("Failed to watch path, does not exist: {}", fse.path().string()));
    }

    if (!std::filesystem::is_directory(fse.path()))
    {
        throw std::invalid_argument(
            std::format("Failed to watch path, not a directory: {}", fse.path().string()));
    }
    return !this->is_ignored(fse.path());
}

void
inotify::watch_path_recursively(const file_system_event& fse)
{
    this->watch_directory(fse);
    for (const auto& p : std::filesystem::recursive_directory_iterator(fse.path()))
    {
        if (p.is_directory())
        {
            this->watch_directory({p, fse.event()});
        }
    }
}

void
inotify::stop() const noexcept
{
    std::uint64_t value = 1;
    auto _ = write(this->event_fd_, &value, sizeof(value));
}

void
inotify::watch_file(const file_system_event& fse)
{
    if (!this->check_watch_file(fse))
    {
        return;
    }

    this->watch(fse);
}

void
inotify::watch_directory(const file_system_event& fse)
{
    if (!this->check_watch_directory(fse))
    {
        return;
    }

    this->watch(fse);
}

void
inotify::watch(const file_system_event& fse)
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
inotify::unwatch(const file_system_event& fse)
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
inotify::remove_watch(const std::int32_t wd) const
{
    const auto result = inotify_rm_watch(this->inotify_fd_, wd);
    if (result == -1)
    {
        throw std::runtime_error(
            std::format("removing inotify watch failed with '{}'", std::strerror(errno)));
    }
}

std::filesystem::path
inotify::wd_to_path(const std::int32_t wd) const noexcept
{
    return this->directory_map_.at(wd);
}

std::shared_ptr<notify::file_system_event>
inotify::get_next_event_from_queue() noexcept
{
    auto event = this->queue_.front();
    this->queue_.pop();
    return event;
}

std::shared_ptr<notify::file_system_event>
inotify::get_next_event(const std::stop_token& stoken)
{
    static constexpr std::size_t MAX_EVENTS = 4096;
    static constexpr std::size_t EVENT_SIZE = (sizeof(inotify_event));
    static constexpr std::size_t EVENT_BUF_LEN = (MAX_EVENTS * (EVENT_SIZE + 16));

    if (!this->queue_.empty())
    {
        return this->get_next_event_from_queue();
    }

    std::ptrdiff_t length = 0;
    std::array<char, EVENT_BUF_LEN> buffer{};

    while (!stoken.stop_requested())
    {
        std::array<epoll_event, 1> events{};

        const auto nfds = epoll_pwait(this->epoll_fd_, events.data(), events.size(), -1, nullptr);
        if (nfds == -1)
        {
            return nullptr;
        }

        if (nfds == 0)
        {
            continue;
        }

        const auto& event = events.front();

        if (event.data.fd == this->inotify_fd_)
        {
            buffer.fill('\0');

            length = read(this->inotify_fd_, buffer.data(), buffer.size());
            if (length == -1)
            {
                if (errno != EINTR)
                {
                    continue;
                }
            }
            break;
        }
        else if (event.data.fd == this->event_fd_)
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

    if (stoken.stop_requested() || this->queue_.empty())
    {
        return nullptr;
    }

    return this->get_next_event_from_queue();
}

std::uint32_t
inotify::get_event_mask(const event event) const
{
    return event_handler::convert_to_inotify_events(event);
}
} // namespace notify
