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
#include <filesystem>
#include <format>
#include <mutex>
#include <stdexcept>
#include <vector>

#include "vfs/notify-cpp/notify.hxx"

void
notify::notify_base::ignore(const std::filesystem::path& p) noexcept
{
    this->ignored_.push_back(p);
}

void
notify::notify_base::ignore_once(const std::filesystem::path& p) noexcept
{
    this->ignored_once_.push_back(p);
}

void
notify::notify_base::stop() noexcept
{
    {
        std::lock_guard<std::mutex> lock(this->mutex_);
        this->stopped_ = true;
    }
    this->cv_.notify_all();
}

bool
notify::notify_base::is_stopped() const noexcept
{
    return this->stopped_;
}

bool
notify::notify_base::is_running() const noexcept
{
    return !this->stopped_;
}

bool
notify::notify_base::is_ignored_once(const std::filesystem::path& p) noexcept
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
notify::notify_base::is_ignored(const std::filesystem::path& p) const noexcept
{
    return std::ranges::contains(this->ignored_, p);
}

std::filesystem::path
notify::notify_base::path_from_fd(const std::int32_t fd) const noexcept
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
notify::notify_base::check_watch_file(const file_system_event& fse) const
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
notify::notify_base::check_watch_directory(const file_system_event& fse) const
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
notify::notify_base::watch_path_recursively(const file_system_event& fse)
{
    if (!this->check_watch_directory(fse))
    {
        return;
    }

    for (const auto& p : std::filesystem::recursive_directory_iterator(fse.path()))
    {
        if (this->check_watch_file({p, fse.event()}))
        {
            this->watch_file({p, fse.event()});
        }
    }
}
