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

// This is altered source code.
// Original code can be found at https://github.com/sourcefrog/natsort

#include <algorithm>
#include <string_view>

#include <cctype>
#include <cstdint>

#include "vfs/utils/vfs-utils.hxx"

#include "strnatcmp.hxx"

[[nodiscard]] static std::int32_t
strnatcmp0(const std::string_view lhs, const std::string_view rhs, const bool fold_case) noexcept
{
    const auto* it_lhs = lhs.cbegin();
    const auto* it_rhs = rhs.cbegin();

    while (it_lhs != lhs.cend() || it_rhs != rhs.cend())
    {
        if ((it_lhs != lhs.cend() && (std::isdigit(*it_lhs) || *it_lhs == '.')) &&
            (it_rhs != rhs.cend() && (std::isdigit(*it_rhs) || *it_rhs == '.')))
        {
            const auto is_digit = [](const auto c) { return std::isdigit(c) || c == '.'; };
            const auto* end_lhs = std::ranges::find_if_not(std::string_view{&(*it_lhs)}, is_digit);
            const auto* end_rhs = std::ranges::find_if_not(std::string_view{&(*it_rhs)}, is_digit);

            std::string_view num_lhs(it_lhs, std::string_view::size_type(end_lhs - it_lhs));
            std::string_view num_rhs(it_rhs, std::string_view::size_type(end_rhs - it_rhs));

            float val_lhs{};
            float val_rhs{};
            // TODO use placeholder variable when switching to c++26
            const auto [ptr_lhs, ec_lhs] =
                std::from_chars(num_lhs.data(), num_lhs.data() + num_lhs.size(), val_lhs);
            const auto [ptr_rhs, ec_rhs] =
                std::from_chars(num_rhs.data(), num_rhs.data() + num_rhs.size(), val_rhs);
            if (ec_lhs == std::errc() && ec_rhs == std::errc())
            {
                const auto result = (val_lhs > val_rhs) - (val_lhs < val_rhs);
                if (result != 0)
                {
                    return result;
                }
            }
        }

        if (it_lhs == lhs.cend() && it_rhs == rhs.cend())
        {
            return 0;
        }

        auto ca = (it_lhs != lhs.cend()) ? *it_lhs : '\0';
        auto cb = (it_rhs != rhs.cend()) ? *it_rhs : '\0';

        if (fold_case && (ca && cb))
        {
            ca = static_cast<char>(std::toupper(ca));
            cb = static_cast<char>(std::toupper(cb));
        }

        if (ca < cb)
        {
            return -1;
        }
        if (ca > cb)
        {
            return 1;
        }

        if (it_lhs != lhs.cend())
        {
            ++it_lhs;
        }
        if (it_rhs != rhs.cend())
        {
            ++it_rhs;
        }
    }

    return 0;
}

[[nodiscard]] std::int32_t
strnatcmp(const std::string_view lhs, const std::string_view rhs, const bool fold_case) noexcept
{
    // TODO use placeholder variable when switching to c++26
    const auto [basename_lhs, ext_lhs, multi_lhs] = vfs::utils::split_basename_extension(lhs);
    const auto [basename_rhs, ext_rhs, multi_rhs] = vfs::utils::split_basename_extension(rhs);

    std::int32_t result{0};
    if (!basename_lhs.empty() && !basename_rhs.empty())
    {
        result = strnatcmp0(basename_lhs, basename_rhs, fold_case);
        if (result == 0)
        {
            result = strnatcmp0(ext_lhs, ext_rhs, fold_case);
        }
    }
    return result;
}
