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
#include <array>
#include <charconv>
#include <string>
#include <string_view>

#include <cctype>
#include <cstdint>

#include "natsort.hxx"

[[nodiscard]] constexpr bool
is_digit(char c) noexcept
{
    return std::isdigit(static_cast<unsigned char>(c));
}

[[nodiscard]] constexpr char
fold_char(char c, bool fold) noexcept
{
    return fold ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c;
}

[[nodiscard]] constexpr std::size_t
count_zeros(std::string_view::const_iterator it, std::string_view::const_iterator end) noexcept
{
    std::size_t count = 0;
    while (it != end && *it == '0')
    {
        ++count;
        ++it;
    }
    return count;
}

[[nodiscard]] std::int32_t
strnatcmp0(std::string_view lhs, std::string_view rhs, bool fold_case) noexcept
{
    auto it_lhs = lhs.cbegin();
    auto it_rhs = rhs.cbegin();

    while (it_lhs != lhs.cend() && it_rhs != rhs.cend())
    {
        if (is_digit(*it_lhs) && is_digit(*it_rhs))
        {
            std::uint64_t val_lhs{};
            std::uint64_t val_rhs{};

            const char* lhs_ptr = lhs.data() + std::distance(lhs.cbegin(), it_lhs);
            const char* lhs_end = lhs.data() + lhs.size();

            const char* rhs_ptr = rhs.data() + std::distance(rhs.cbegin(), it_rhs);
            const char* rhs_end = rhs.data() + rhs.size();

            const auto [ptr_lhs, ec_lhs] = std::from_chars(lhs_ptr, lhs_end, val_lhs);
            const auto [ptr_rhs, ec_rhs] = std::from_chars(rhs_ptr, rhs_end, val_rhs);

            if (ec_lhs == std::errc{} && ec_rhs == std::errc{})
            {
                if (val_lhs < val_rhs)
                {
                    return -1;
                }
                if (val_lhs > val_rhs)
                {
                    return 1;
                }

                // tie breaker for leading zeros
                const auto zeros_lhs = count_zeros(it_lhs, lhs.cend());
                const auto zeros_rhs = count_zeros(it_rhs, rhs.cend());
                if (zeros_lhs != zeros_rhs)
                {
                    return zeros_lhs < zeros_rhs ? -1 : 1;
                }

                it_lhs += (ptr_lhs - lhs_ptr);
                it_rhs += (ptr_rhs - rhs_ptr);
                continue;
            }
        }

        const char ca = fold_char(*it_lhs, fold_case);
        const char cb = fold_char(*it_rhs, fold_case);

        if (ca != cb)
        {
            return ca < cb ? -1 : 1;
        }

        ++it_lhs;
        ++it_rhs;
    }

    if (it_lhs == lhs.cend() && it_rhs == rhs.cend())
    {
        return 0;
    }
    return (it_lhs == lhs.cend()) ? -1 : 1;
}

[[nodiscard]] constexpr bool
is_non_numeric_ext(std::string_view sv) noexcept
{
    for (char c : sv)
    {
        if (!is_digit(c))
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] constexpr std::pair<std::string_view, std::string_view>
smart_decompose_filename(std::string_view filepath) noexcept
{
    if (filepath.empty())
    {
        return {{}, {}};
    }

    const auto dir_pos = filepath.find_last_of("/\\");
    const auto filename_start = (dir_pos == std::string_view::npos) ? 0 : dir_pos + 1;
    const auto filename = filepath.substr(filename_start);

    const auto pos_in_filename = filename.find_last_of('.');

    if (pos_in_filename != std::string_view::npos && pos_in_filename != 0 &&
        pos_in_filename != filename.length() - 1)
    {
        const auto ext_after_dot = filename.substr(pos_in_filename + 1);

        if (is_non_numeric_ext(ext_after_dot))
        {
            const auto pos = filename_start + pos_in_filename;
            const auto stem = filepath.substr(0, pos);
            const auto ext = filepath.substr(pos);

            if (stem.ends_with(".tar"))
            {
                const auto tar_pos_in_filename =
                    filename.substr(0, pos_in_filename).find_last_of('.');
                if (tar_pos_in_filename != std::string_view::npos && tar_pos_in_filename != 0)
                {
                    const auto abs_tar_pos = filename_start + tar_pos_in_filename;
                    return {filepath.substr(0, abs_tar_pos), filepath.substr(abs_tar_pos)};
                }
            }
            else
            {
                return {stem, ext};
            }
        }
    }

    return {filepath, {}};
}

std::int32_t
natsort::compare(std::string_view lhs, std::string_view rhs, bool fold_case) noexcept
{
    const auto [stem_lhs, ext_lhs] = smart_decompose_filename(lhs);
    const auto [stem_rhs, ext_rhs] = smart_decompose_filename(rhs);

    auto result = strnatcmp0(stem_lhs, stem_rhs, fold_case);
    if (result == 0)
    {
        result = strnatcmp0(ext_lhs, ext_rhs, fold_case);
    }
    return result;
}
