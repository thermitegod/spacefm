/*
  strnatcmp.c -- Perform 'natural order' comparisons of strings in C.
  Copyright (C) 2000, 2004 by Martin Pool <mbp sourcefrog net>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include <concepts>
#include <string_view>
#include <type_traits>

#include <cstdint>

namespace natsort
{
[[nodiscard]] std::int32_t compare(std::string_view lhs, std::string_view rhs,
                                   const bool fold_case = false) noexcept;

namespace detail
{
template<typename T>
[[nodiscard]] static constexpr std::string_view
to_view(const T& val) noexcept
{
    if constexpr (requires { val.native(); })
    {
        return std::string_view{val.native()};
    }
    else
    {
        return std::string_view{val};
    }
}
} // namespace detail

struct sorter
{
    using is_transparent = void;

    template<typename L, typename R>
    [[nodiscard]] bool
    operator()(const L& lhs, const R& rhs) const noexcept
        requires(std::same_as<L, R> &&
                 (!std::same_as<L, std::string_view> && !std::same_as<R, std::string_view>))
    {
        return compare(detail::to_view(lhs), detail::to_view(rhs), false) < 0;
    }

    template<typename L, typename R>
    [[nodiscard]] bool
    operator()(const L lhs, const R rhs) const noexcept
        requires(std::same_as<L, R> &&
                 (std::same_as<L, std::string_view> && std::same_as<R, std::string_view>))
    {
        return compare(lhs, rhs, false) < 0;
    }
};

struct sorter_fold
{
    using is_transparent = void;

    template<typename L, typename R>
    [[nodiscard]] bool
    operator()(const L& lhs, const R& rhs) const noexcept
        requires(std::same_as<L, R> &&
                 (!std::same_as<L, std::string_view> && !std::same_as<R, std::string_view>))
    {
        return compare(detail::to_view(lhs), detail::to_view(rhs), true) < 0;
    }

    template<typename L, typename R>
    [[nodiscard]] bool
    operator()(const L lhs, const R rhs) const noexcept
        requires(std::same_as<L, R> &&
                 (std::same_as<L, std::string_view> && std::same_as<R, std::string_view>))
    {
        return compare(lhs, rhs, true) < 0;
    }
};
} // namespace natsort
