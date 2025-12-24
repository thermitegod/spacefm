/**
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <filesystem>
#include <flat_map>
#include <format>
#include <string>
#include <string_view>

namespace logger
{
enum domain : std::uint8_t
{
    basic,
    dev,
    autosave,
    execute,
    socket,
    gui,
    vfs,
};

namespace detail
{
enum loglevel : std::uint8_t
{
    trace,
    debug,
    info,
    warn,
    err,
    critical,
    off,
};

void logger(const loglevel level, const domain d, const std::string_view msg) noexcept;
} // namespace detail

void initialize() noexcept;

void initialize(const std::flat_map<std::string, std::string>& options,
                const std::filesystem::path& logfile = "") noexcept;

template<domain d = basic, typename... Args>
void
trace(std::format_string<Args...> fmt, Args&&... args) noexcept
{
    detail::logger(detail::trace, d, std::format(fmt, std::forward<Args>(args)...));
}

template<domain d = basic, typename... Args>
void
trace_if(bool cond, std::format_string<Args...> fmt, Args&&... args) noexcept
{
    if (cond)
    {
        trace<d>(fmt, std::forward<Args>(args)...);
        // detail::trace(d, std::format(fmt, std::forward<Args>(args)...));
    }
}

template<domain d = basic, typename... Args>
void
debug(std::format_string<Args...> fmt, Args&&... args) noexcept
{
    detail::logger(detail::debug, d, std::format(fmt, std::forward<Args>(args)...));
}

template<domain d = basic, typename... Args>
void
debug_if(bool cond, std::format_string<Args...> fmt, Args&&... args) noexcept
{
    if (cond)
    {
        debug<d>(fmt, std::forward<Args>(args)...);
    }
}

template<domain d = basic, typename... Args>
void
info(std::format_string<Args...> fmt, Args&&... args) noexcept
{
    detail::logger(detail::info, d, std::format(fmt, std::forward<Args>(args)...));
}

template<domain d = basic, typename... Args>
void
info_if(bool cond, std::format_string<Args...> fmt, Args&&... args) noexcept
{
    if (cond)
    {
        info<d>(fmt, std::forward<Args>(args)...);
    }
}

template<domain d = basic, typename... Args>
void
warn(std::format_string<Args...> fmt, Args&&... args) noexcept
{
    detail::logger(detail::warn, d, std::format(fmt, std::forward<Args>(args)...));
}

template<domain d = basic, typename... Args>
void
warn_if(bool cond, std::format_string<Args...> fmt, Args&&... args) noexcept
{
    if (cond)
    {
        warn<d>(fmt, std::forward<Args>(args)...);
    }
}

template<domain d = basic, typename... Args>
void
error(std::format_string<Args...> fmt, Args&&... args) noexcept
{
    detail::logger(detail::err, d, std::format(fmt, std::forward<Args>(args)...));
}

template<domain d = basic, typename... Args>
void
error_if(bool cond, std::format_string<Args...> fmt, Args&&... args) noexcept
{
    if (cond)
    {
        error<d>(fmt, std::forward<Args>(args)...);
    }
}

template<domain d = basic, typename... Args>
void
critical(std::format_string<Args...> fmt, Args&&... args) noexcept
{
    detail::logger(detail::critical, d, std::format(fmt, std::forward<Args>(args)...));
}

template<domain d = basic, typename... Args>
void
critical_if(bool cond, std::format_string<Args...> fmt, Args&&... args) noexcept
{
    if (cond)
    {
        critical<d>(fmt, std::forward<Args>(args)...);
    }
}

namespace utils
{
template<typename T>
const void*
ptr(T p) noexcept
{
    static_assert(std::is_pointer_v<T>);
    return (void*)p;
}
template<typename T>
const void*
ptr(const std::unique_ptr<T>& p) noexcept
{
    return (void*)p.get();
}
template<typename T>
const void*
ptr(const std::shared_ptr<T>& p) noexcept
{
    return (void*)p.get();
}
} // namespace utils
} // namespace logger
