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

#include <functional>

#include <cstdint>

#include <sys/inotify.h>

#include "vfs/notify-cpp/event.hxx"

static std::uint32_t
get_inotify_event(const notify::event event) noexcept
{
    switch (event)
    {
        case notify::event::access:
            return IN_ACCESS;
        case notify::event::modify:
            return IN_MODIFY;
        case notify::event::attrib:
            return IN_ATTRIB;
        case notify::event::close_write:
            return IN_CLOSE_WRITE;
        case notify::event::close_nowrite:
            return IN_CLOSE_NOWRITE;
        case notify::event::open:
            return IN_OPEN;
        case notify::event::moved_from:
            return IN_MOVED_FROM;
        case notify::event::moved_to:
            return IN_MOVED_TO;
        case notify::event::create:
            return IN_CREATE;
        case notify::event::delete_sub:
            return IN_DELETE;
        case notify::event::delete_self:
            return IN_DELETE_SELF;
        case notify::event::umount:
            return IN_UNMOUNT;
        case notify::event::queue_overflow:
            return IN_Q_OVERFLOW;
        case notify::event::ignored:
            return IN_IGNORED;
        case notify::event::move_self:
            return IN_MOVE_SELF;
        case notify::event::close:
            return IN_CLOSE;
        case notify::event::move:
            return IN_MOVE;
        case notify::event::all:
            return IN_ALL_EVENTS;
        case notify::event::none:
            return 0;
    }
    return 0;
}

static std::uint32_t
convert(const notify::event event,
        const std::function<std::uint32_t(notify::event)>& translator) noexcept
{
    std::uint32_t events = 0;
    for (const auto& e : notify::all_inotify_events)
    {
        if ((event & e) == e)
        {
            events = events | translator(e);
        }
    }
    return events;
}

std::uint32_t
notify::event_handler::convert_to_inotify_events(const notify::event event) noexcept
{
    return convert(event, [](const notify::event event) { return get_inotify_event(event); });
}

notify::event
notify::event_handler::get_inotify(const std::uint32_t event) noexcept
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
