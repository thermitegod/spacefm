/**
 * Copyright (C) 2024 Brandon Zorn <brandonzorn@cock.li>
 *
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

#include <gtest/gtest.h>

#include <algorithm>

#include "spacefm/ptk/natsort/strnatcmp.h"

#include "test_data.hxx"

TEST(natsort, dates)
{
    auto unsorted = data::dates::unsorted;
    auto sorted = data::dates::sorted;

    std::ranges::sort(unsorted, [](auto a, auto b) { return strnatcmp(a.data(), b.data()) < 0; });

    EXPECT_EQ(unsorted, sorted);
}

TEST(natsort, fractions)
{
    auto unsorted = data::fractions::unsorted;
    auto sorted = data::fractions::sorted;

    std::ranges::sort(unsorted, [](auto a, auto b) { return strnatcmp(a.data(), b.data()) < 0; });

    EXPECT_EQ(unsorted, sorted);
}

TEST(natsort, words)
{
    auto unsorted = data::words::unsorted;
    auto sorted = data::words::sorted;

    std::ranges::sort(unsorted, [](auto a, auto b) { return strnatcmp(a.data(), b.data()) < 0; });

    EXPECT_EQ(unsorted, sorted);
}

TEST(natsort, simple_names)
{
    auto unsorted = data::simple_names::unsorted;
    auto sorted = data::simple_names::sorted;

    std::ranges::sort(unsorted, [](auto a, auto b) { return strnatcmp(a.data(), b.data()) < 0; });

    EXPECT_EQ(unsorted, sorted);
}
