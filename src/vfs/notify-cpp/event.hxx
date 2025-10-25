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

#include <array>
#include <format>
#include <string>
#include <type_traits>

#include <cstdint>

#include <sys/inotify.h>

namespace notify
{
template<typename Enum> struct EnableBitMaskOperators
{
    static const bool enable = false;
};

template<typename Enum>
Enum
operator|(Enum lhs, Enum rhs)
    requires EnableBitMaskOperators<Enum>::enable
{
    using underlying = std::underlying_type_t<Enum>;
    return static_cast<Enum>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}
template<typename Enum>
Enum
operator&(Enum lhs, Enum rhs)
    requires EnableBitMaskOperators<Enum>::enable
{
    using underlying = std::underlying_type_t<Enum>;
    return static_cast<Enum>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

enum class event : std::uint32_t
{
    none = 0,

    // Supported events
    access = IN_ACCESS,               // File was accessed.
    modify = IN_MODIFY,               // File was modified.
    attrib = IN_ATTRIB,               // Metadata changed.
    close_write = IN_CLOSE_WRITE,     // Writtable file was closed.
    close_nowrite = IN_CLOSE_NOWRITE, // Unwrittable file closed.
    open = IN_OPEN,                   // File was opened.
    moved_from = IN_MOVED_FROM,       // File was moved from X.
    moved_to = IN_MOVED_TO,           // File was moved to Y.
    create = IN_CREATE,               // Subfile was created.
    delete_sub = IN_DELETE,           // Subfile was deleted.
    delete_self = IN_DELETE_SELF,     // Self was deleted.
    move_self = IN_MOVE_SELF,         // Self was moved.

    // Events sent by the kernel.
    umount = IN_UNMOUNT,            // Backing fs was unmounted.
    queue_overflow = IN_Q_OVERFLOW, // Event queued overflowed.
    ignored = IN_IGNORED,           // File was ignored.

    // Helper events
    close = IN_CLOSE, // Close.
    move = IN_MOVE,   // Moves.

    // All events which a program can wait on.
    all = IN_ALL_EVENTS,
};

inline constexpr std::array<notify::event, 18> all_inotify_events = {
    event::access,
    event::modify,
    event::attrib,
    event::close_write,
    event::close_nowrite,
    event::open,
    event::moved_from,
    event::moved_to,
    event::create,
    event::delete_sub,
    event::delete_self,
    event::move_self,
    event::umount,
    event::queue_overflow,
    event::ignored,
    event::close,
    event::move,
    event::all,
};

template<> struct EnableBitMaskOperators<event>
{
    static const bool enable = true;
};

namespace event_handler
{
[[nodiscard]] std::uint32_t convert_to_inotify_events(const notify::event event) noexcept;
[[nodiscard]] event get_inotify(const std::uint32_t event) noexcept;
}; // namespace event_handler
}; // namespace notify

template<> struct std::formatter<notify::event> : std::formatter<std::string>
{
    auto
    format(const notify::event& event, format_context& ctx) const
    {
        constexpr auto resolve_name = [](notify::event event) -> std::string
        {
            switch (event)
            {
                case notify::event::access:
                    return "access";
                case notify::event::modify:
                    return "modify";
                case notify::event::attrib:
                    return "attrib";
                case notify::event::close_write:
                    return "close_write";
                case notify::event::close_nowrite:
                    return "close_nowrite";
                case notify::event::open:
                    return "open";
                case notify::event::moved_from:
                    return "moved_from";
                case notify::event::moved_to:
                    return "moved_to";
                case notify::event::create:
                    return "create";
                case notify::event::delete_sub:
                    return "delete";
                case notify::event::delete_self:
                    return "delete_self";
                case notify::event::move_self:
                    return "move_self";
                case notify::event::umount:
                    return "umount";
                case notify::event::queue_overflow:
                    return "queue_overflow";
                case notify::event::ignored:
                    return "ignored";
                case notify::event::close:
                    return "close";
                case notify::event::move:
                    return "move";
                case notify::event::all:
                    return "all";
                case notify::event::none:
                    return "none";
            }
            return "ERROR";
        };

        std::string events;
        for (const auto& e : notify::all_inotify_events)
        {
            if ((event & e) == e)
            {
                if (events.empty())
                {
                    events = resolve_name(e);
                }
                else
                {
                    events.append(std::format(",{}", resolve_name(e)));
                }
            }
        }

        return std::formatter<std::string>::format(events, ctx);
    }
};
