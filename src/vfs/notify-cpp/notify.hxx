/*
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

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <queue>
#include <stop_token>
#include <vector>

#include "vfs/notify-cpp/event.hxx"
#include "vfs/notify-cpp/file_system_event.hxx"

/**
 * @brief Base class
 */
namespace notify
{
class notify_base
{
  public:
    notify_base() = default;
    virtual ~notify_base() = default;

    virtual void watch_file(const file_system_event&) = 0;
    virtual void watch_directory(const file_system_event&) = 0;
    virtual void unwatch(const file_system_event&) = 0;

    [[nodiscard]] virtual std::shared_ptr<file_system_event>
    get_next_event(const std::stop_token& stoken) = 0;

    [[nodiscard]] virtual std::uint32_t get_event_mask(const notify::event) const = 0;

    void ignore(const std::filesystem::path& p) noexcept;
    void ignore_once(const std::filesystem::path& p) noexcept;

    void watch_path_recursively(const file_system_event& fse);

  protected:
    [[nodiscard]] bool check_watch_file(const file_system_event& fse) const;
    [[nodiscard]] bool check_watch_directory(const file_system_event& fse) const;
    [[nodiscard]] bool is_ignored(const std::filesystem::path& p) const noexcept;
    [[nodiscard]] bool is_ignored_once(const std::filesystem::path& p) noexcept;
    [[nodiscard]] std::filesystem::path path_from_fd(const std::int32_t fd) const noexcept;

    std::queue<std::shared_ptr<file_system_event>> queue_;

    std::mutex mutex_;
    std::condition_variable_any cv_;
    std::chrono::milliseconds thread_sleep_ = std::chrono::milliseconds(250);

  private:
    std::vector<std::filesystem::path> ignored_;
    std::vector<std::filesystem::path> ignored_once_;
};
} // namespace notify
