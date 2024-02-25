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

#include <string_view>

#include "strnatcmp.hxx"

[[nodiscard]] static i32
compare_right(const char* a, const char* b) noexcept
{
    i32 bias = 0;

    // The longest run of digits wins. That aside, the greatest
    // value wins, but we can't know that it will until we've scanned
    // both numbers to know that they have the same magnitude, so we
    // remember it in BIAS.
    for (;; a++, b++)
    {
        if (!std::isdigit(*a) && !std::isdigit(*b))
        {
            return bias;
        }
        if (!std::isdigit(*a))
        {
            return -1;
        }
        if (!std::isdigit(*b))
        {
            return +1;
        }
        if (*a < *b)
        {
            if (!bias)
            {
                bias = -1;
            }
        }
        else if (*a > *b)
        {
            if (!bias)
            {
                bias = +1;
            }
        }
        else if (!*a && !*b)
        {
            return bias;
        }
    }

    return 0;
}

[[nodiscard]] static i32
compare_left(const char* a, const char* b) noexcept
{
    // Compare two left-aligned numbers: the first to have a
    // different value wins.
    for (;; a++, b++)
    {
        if (!std::isdigit(*a) && !std::isdigit(*b))
        {
            return 0;
        }
        if (!std::isdigit(*a))
        {
            return -1;
        }
        if (!std::isdigit(*b))
        {
            return +1;
        }
        if (*a < *b)
        {
            return -1;
        }
        if (*a > *b)
        {
            return +1;
        }
    }

    return 0;
}

[[nodiscard]] static i32
strnatcmp0(const char* a, const char* b, const bool fold_case) noexcept
{
    i32 ai = 0;
    i32 bi = 0;

    while (true)
    {
        char ca = a[ai];
        char cb = b[bi];

        // skip over leading spaces or zeros
        while (std::isspace(ca))
        {
            ca = a[++ai];
        }

        while (std::isspace(cb))
        {
            cb = b[++bi];
        }

        // process run of digits
        if (std::isdigit(ca) && std::isdigit(cb))
        {
            const bool fractional = (ca == '0' || cb == '0');
            if (fractional)
            {
                const auto result = compare_left(a + ai, b + bi);
                if (result != 0)
                {
                    return result;
                }
            }
            else
            {
                const auto result = compare_right(a + ai, b + bi);
                if (result != 0)
                {
                    return result;
                }
            }
        }

        if (!ca && !cb)
        {
            // The strings compare the same. Perhaps the caller
            // will want to call strcmp to break the tie.
            return 0;
        }

        if (fold_case)
        {
            ca = std::toupper(ca);
            cb = std::toupper(cb);
        }

        if (ca < cb)
        {
            return -1;
        }

        if (ca > cb)
        {
            return +1;
        }

        ++ai;
        ++bi;
    }
}

[[nodiscard]] i32
strnatcmp(const std::string_view a, const std::string_view b) noexcept
{
    return strnatcmp0(a.data(), b.data(), false);
}

[[nodiscard]] i32
strnatcasecmp(const std::string_view a, const std::string_view b) noexcept
{
    return strnatcmp0(a.data(), b.data(), true);
}
