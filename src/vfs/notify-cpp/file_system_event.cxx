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

#include <filesystem>
#include <set>

#include "vfs/notify-cpp/file_system_event.hxx"

notify::file_system_event::file_system_event(const std::filesystem::path& path)
    : event_(notify::event::all), path_(path)
{
}

notify::file_system_event::file_system_event(const std::filesystem::path& path,
                                             const notify::event event)
    : event_(event), path_(path)
{
}

notify::file_system_event::file_system_event(const std::filesystem::path& path,
                                             const std::set<notify::event>& events)
    : path_(path)
{
    for (const auto& event : events)
    {
        this->event_ = this->event_ | event;
    }
}

std::filesystem::path
notify::file_system_event::path() const noexcept
{
    return this->path_;
}

notify::event
notify::file_system_event::event() const noexcept
{
    return this->event_;
}
