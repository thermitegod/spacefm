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
#include <stop_token>
#include <utility>

#include "vfs/notify-cpp/controller.hxx"

notify::controller::controller(const std::filesystem::path& path, std::set<event> events)
    : notify_(path, events)
{
}

void
notify::controller::run(const std::stop_token& stoken) noexcept
{
    // std::stop_callback cb(stoken, [this]() { notify_.stop(); });

    while (!stoken.stop_requested())
    {
        run_once(stoken);
    }
}

void
notify::controller::run_once(const std::stop_token& stoken) noexcept
{
    std::stop_callback cb(stoken, [this]() { notify_.stop(); });

    const auto fse = notify_.get_next_event(stoken);
    if (!fse)
    {
        return;
    }

    switch (fse->event)
    {
        case event::access:
            signal_access().emit(fse->path);
            return;
        case event::modify:
            signal_modify().emit(fse->path);
            return;
        case event::attrib:
            signal_attrib().emit(fse->path);
            return;
        case event::close_write:
            signal_close_write().emit(fse->path);
            signal_close().emit(fse->path);
            return;
        case event::close_nowrite:
            signal_close_nowrite().emit(fse->path);
            signal_close().emit(fse->path);
            return;
        case event::open:
            signal_open().emit(fse->path);
            return;
        case event::moved_from:
            signal_moved_from().emit(fse->path);
            signal_move().emit(fse->path);
            return;
        case event::moved_to:
            signal_moved_to().emit(fse->path);
            signal_move().emit(fse->path);
            return;
        case event::create:
            signal_create().emit(fse->path);
            return;
        case event::delete_sub:
            signal_delete().emit(fse->path);
            return;
        case event::delete_self:
            signal_delete_self().emit(fse->path);
            return;
        case event::move_self:
            signal_move_self().emit(fse->path);
            return;
        case event::umount:
            signal_umount().emit(fse->path);
            return;
        case event::queue_overflow:
            signal_queue_overflow().emit(fse->path);
            return;
        case event::ignored:
            signal_ignored().emit(fse->path);
            return;
        case event::close:
            signal_close().emit(fse->path);
            return;
        case event::move:
            signal_move().emit(fse->path);
            return;
        case event::none:
        case event::all:
        default:
            std::unreachable();
    }
}
