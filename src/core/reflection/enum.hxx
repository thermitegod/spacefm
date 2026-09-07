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

#include <array>
#include <concepts>
#include <meta>
#include <string_view>
#include <type_traits>

#include <cstdint>

// https://www.murathepeyiler.com/what-the-heck-is-reflection/

namespace reflection
{
template<typename T>
    requires std::is_enum_v<T>
consteval auto
enumerators() noexcept
{
    return std::define_static_array(std::meta::enumerators_of(^^T));
}

template<typename T>
    requires std::is_enum_v<T>
consteval std::size_t
enum_count() noexcept
{
    return enumerators<T>().size();
}

template<typename T>
    requires std::is_enum_v<T>
constexpr std::string_view
enum_name(T val)
{
    template for (constexpr auto e : enumerators<T>())
    {
        if (val == [:e:])
        {
            return std::meta::identifier_of(e);
        }
    }
    return "<unknown>";
}

template<typename T>
    requires std::is_enum_v<T>
consteval auto
enum_names()
{
    constexpr auto count = enum_count<T>();
    std::array<std::string_view, count> result{};

    std::size_t i = 0;
    template for (constexpr auto e : enumerators<T>())
    {
        result[i++] = std::meta::identifier_of(e);
    }

    return result;
}

template<typename T>
    requires std::is_enum_v<T>
consteval auto
enum_values()
{
    constexpr auto count = enum_count<T>();
    std::array<T, count> result{};

    std::size_t i = 0;
    template for (constexpr auto e : enumerators<T>())
    {
        result[i++] = [:e:];
    }

    return result;
}

template<typename T>
    requires std::is_enum_v<T>
constexpr std::optional<T>
enum_cast(std::string_view name) noexcept
{
    template for (constexpr auto e : enumerators<T>())
    {
        if (name == std::meta::identifier_of(e))
        {
            return [:e:];
        }
    }
    return std::nullopt;
}
} // namespace reflection
