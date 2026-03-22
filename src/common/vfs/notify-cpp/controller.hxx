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
#include <stop_token>

#include <sigc++/sigc++.h>

#include "vfs/notify-cpp/notify.hxx"

namespace notify
{
class controller
{
  public:
    controller(const std::filesystem::path& path, std::set<event> events = {event::all});

    void run(const std::stop_token& stoken) noexcept;
    void run_once(const std::stop_token& stoken) noexcept;

  private:
    inotify notify_;

  public:
    // Supported events

    [[nodiscard]] auto
    signal_access() noexcept
    {
        return signal_access_;
    }

    [[nodiscard]] auto
    signal_modify() noexcept
    {
        return signal_modify_;
    }

    [[nodiscard]] auto
    signal_attrib() noexcept
    {
        return signal_attrib_;
    }

    [[nodiscard]] auto
    signal_close_write() noexcept
    {
        return signal_close_write_;
    }

    [[nodiscard]] auto
    signal_close_nowrite() noexcept
    {
        return signal_close_nowrite_;
    }

    [[nodiscard]] auto
    signal_open() noexcept
    {
        return signal_open_;
    }

    [[nodiscard]] auto
    signal_moved_from() noexcept
    {
        return signal_moved_from_;
    }

    [[nodiscard]] auto
    signal_moved_to() noexcept
    {
        return signal_moved_to_;
    }

    [[nodiscard]] auto
    signal_create() noexcept
    {
        return signal_create_;
    }

    [[nodiscard]] auto
    signal_delete() noexcept
    {
        return signal_delete_;
    }

    [[nodiscard]] auto
    signal_delete_self() noexcept
    {
        return signal_delete_self_;
    }

    [[nodiscard]] auto
    signal_move_self() noexcept
    {
        return signal_move_self_;
    }

    // Events sent by the kernel.

    [[nodiscard]] auto
    signal_umount() noexcept
    {
        return signal_umount_;
    }

    [[nodiscard]] auto
    signal_queue_overflow() noexcept
    {
        return signal_queue_overflow_;
    }

    [[nodiscard]] auto
    signal_ignored() noexcept
    {
        return signal_ignored_;
    }

    // Helper events

    [[nodiscard]] auto
    signal_close() noexcept
    {
        return signal_close_;
    }

    [[nodiscard]] auto
    signal_move() noexcept
    {
        return signal_move_;
    }

  private:
    sigc::signal<void(std::filesystem::path)> signal_access_;
    sigc::signal<void(std::filesystem::path)> signal_modify_;
    sigc::signal<void(std::filesystem::path)> signal_attrib_;
    sigc::signal<void(std::filesystem::path)> signal_close_write_;
    sigc::signal<void(std::filesystem::path)> signal_close_nowrite_;
    sigc::signal<void(std::filesystem::path)> signal_open_;
    sigc::signal<void(std::filesystem::path)> signal_moved_from_;
    sigc::signal<void(std::filesystem::path)> signal_moved_to_;
    sigc::signal<void(std::filesystem::path)> signal_create_;
    sigc::signal<void(std::filesystem::path)> signal_delete_;
    sigc::signal<void(std::filesystem::path)> signal_delete_self_;
    sigc::signal<void(std::filesystem::path)> signal_move_self_;

    sigc::signal<void(std::filesystem::path)> signal_umount_;
    sigc::signal<void(std::filesystem::path)> signal_queue_overflow_;
    sigc::signal<void(std::filesystem::path)> signal_ignored_;

    sigc::signal<void(std::filesystem::path)> signal_close_;
    sigc::signal<void(std::filesystem::path)> signal_move_;
};
} // namespace notify
