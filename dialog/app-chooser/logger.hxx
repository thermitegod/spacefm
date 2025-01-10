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
#include <unordered_map>

#if __has_include(<magic_enum/magic_enum.hpp>)
// >=magic_enum-0.9.7
#include <magic_enum/magic_enum.hpp>
#else
// <=magic_enum-0.9.6
#include <magic_enum.hpp>
#endif

#include <spdlog/spdlog.h>

namespace logger
{
enum class domain : std::uint8_t
{
    basic,
    dev,
    autosave,
    signals,
    socket,
    ptk,
    vfs,
};

void initialize() noexcept;

void initialize(const std::unordered_map<std::string, std::string>& options,
                const std::filesystem::path& logfile = "") noexcept;

template<domain d = domain::basic, typename... Args>
void
trace(spdlog::format_string_t<Args...> fmt, Args&&... args) noexcept
{
    spdlog::get(magic_enum::enum_name(d).data())->trace(fmt, std::forward<Args>(args)...);
}

template<domain d = domain::basic, typename... Args>
void
debug(spdlog::format_string_t<Args...> fmt, Args&&... args) noexcept
{
    spdlog::get(magic_enum::enum_name(d).data())->debug(fmt, std::forward<Args>(args)...);
}

template<domain d = domain::basic, typename... Args>
void
info(spdlog::format_string_t<Args...> fmt, Args&&... args) noexcept
{
    spdlog::get(magic_enum::enum_name(d).data())->info(fmt, std::forward<Args>(args)...);
}

template<domain d = domain::basic, typename... Args>
void
warn(spdlog::format_string_t<Args...> fmt, Args&&... args) noexcept
{
    spdlog::get(magic_enum::enum_name(d).data())->warn(fmt, std::forward<Args>(args)...);
}

template<domain d = domain::basic, typename... Args>
void
error(spdlog::format_string_t<Args...> fmt, Args&&... args) noexcept
{
    spdlog::get(magic_enum::enum_name(d).data())->error(fmt, std::forward<Args>(args)...);
}

template<domain d = domain::basic, typename... Args>
void
critical(spdlog::format_string_t<Args...> fmt, Args&&... args) noexcept
{
    spdlog::get(magic_enum::enum_name(d).data())->critical(fmt, std::forward<Args>(args)...);
}

template<domain d = domain::basic, typename T>
void
trace(const T& msg) noexcept
{
    spdlog::get(magic_enum::enum_name(d).data())->trace(msg);
}

template<domain d = domain::basic, typename T>
void
debug(const T& msg) noexcept
{
    spdlog::get(magic_enum::enum_name(d).data())->debug(msg);
}

template<domain d = domain::basic, typename T>
void
info(const T& msg) noexcept
{
    spdlog::get(magic_enum::enum_name(d).data())->info(msg);
}

template<domain d = domain::basic, typename T>
void
warn(const T& msg) noexcept
{
    spdlog::get(magic_enum::enum_name(d).data())->warn(msg);
}

template<domain d = domain::basic, typename T>
void
error(const T& msg) noexcept
{
    spdlog::get(magic_enum::enum_name(d).data())->error(msg);
}

template<domain d = domain::basic, typename T>
void
critical(const T& msg) noexcept
{
    spdlog::get(magic_enum::enum_name(d).data())->critical(msg);
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
