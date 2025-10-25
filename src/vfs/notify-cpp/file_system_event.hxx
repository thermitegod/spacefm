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

#pragma once

#include <filesystem>
#include <set>

#include "vfs/notify-cpp/event.hxx"

namespace notify
{
class file_system_event
{
  public:
    /**
     * @brief file_system_event
     * @param path - path to install watch at
     */
    file_system_event(const std::filesystem::path& path);

    /**
     * @brief file_system_event
     * @param path - path to install watch at
     * @param event - bitmask of events to be watched for
     */
    file_system_event(const std::filesystem::path& path, const notify::event event);

    /**
     * @brief file_system_event
     * @param path - path to install watch at
     * @param events - set of events to be watched for
     */
    file_system_event(const std::filesystem::path& path, const std::set<notify::event>& events);

    [[nodiscard]] std::filesystem::path path() const noexcept;
    [[nodiscard]] event event() const noexcept;

  private:
    notify::event event_ = notify::event::none;
    // absoulte path + filename
    std::filesystem::path path_;
};
} // namespace notify
