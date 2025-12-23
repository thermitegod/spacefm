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
#include <memory>
#include <queue>
#include <stop_token>
#include <unordered_map>
#include <vector>

#include <cstdint>

#include "vfs/notify-cpp/file_system_event.hxx"

namespace notify
{
class inotify
{
  public:
    inotify();
    ~inotify() noexcept;

    /**
     * @brief Adds a single file to the list of watches.
     * @param path that will be watched
     */
    void watch_file(const file_system_event& fse);

    /**
     * @brief Adds a single directory to the list of watches.
     * @param path that will be watched
     */
    void watch_directory(const file_system_event& fse);

    /**
     * @brief Add watch to a directory, recursively.
     * @param fse that will be watched
     */
    void watch_path_recursively(const file_system_event& fse);

    /**
     * @brief remove watch for a file or directory.
     * @param path that will be unwatched
     */

    /**
     * @brief Remove watch for a file or directory. This is not done recursively.
     * @param fse - filesystem event to remove
     */
    void unwatch(const file_system_event& fse);

    void ignore(const std::filesystem::path& p) noexcept;
    void ignore_once(const std::filesystem::path& p) noexcept;

    /**
     * @brief Blocking wait on new events of watched files/directories
     *        specified on the eventmask. file_system_event's
     *        will be returned one by one. Thus this
     *        function can be called in some while(true)
     *        loop.
     * @return A new file_system_event
     */
    [[nodiscard]] std::shared_ptr<file_system_event> get_next_event(const std::stop_token& stoken);
    [[nodiscard]] std::uint32_t get_event_mask(const event event) const;

    void stop() const noexcept;

  private:
    [[nodiscard]] bool check_watch_file(const file_system_event& fse) const;
    [[nodiscard]] bool check_watch_directory(const file_system_event& fse) const;
    [[nodiscard]] bool is_ignored(const std::filesystem::path& p) const noexcept;
    [[nodiscard]] bool is_ignored_once(const std::filesystem::path& p) noexcept;

    [[nodiscard]] std::shared_ptr<file_system_event> get_next_event_from_queue() noexcept;

    [[nodiscard]] std::filesystem::path wd_to_path(const std::int32_t wd) const noexcept;
    [[nodiscard]] std::filesystem::path path_from_fd(const std::int32_t fd) const noexcept;

    void watch(const file_system_event& fse);
    void remove_watch(const std::int32_t wd) const;

    std::unordered_map<std::int32_t, std::filesystem::path> directory_map_;

    std::int32_t inotify_fd_;
    std::int32_t event_fd_;
    std::int32_t epoll_fd_;

    std::queue<std::shared_ptr<file_system_event>> queue_;

    std::vector<std::filesystem::path> ignored_;
    std::vector<std::filesystem::path> ignored_once_;
};
} // namespace notify
