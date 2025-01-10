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

#include <algorithm>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "spacefm/ptk/natsort/strnatcmp.hxx"

TEST_SUITE("natsort" * doctest::description(""))
{
    TEST_CASE("dates")
    {
        // clang-format off
        std::vector<std::string> unsorted{
            "2000-1-10",
            "2000-1-2",
            "1999-12-25",
            "2000-3-23",
            "1999-3-3",
        };

        std::vector<std::string> sorted{
            "1999-3-3",
            "1999-12-25",
            "2000-1-2",
            "2000-1-10",
            "2000-3-23",
        };
        // clang-format on

        std::ranges::sort(unsorted, [](auto a, auto b) { return strnatcmp(a, b) < 0; });

        REQUIRE_EQ(unsorted, sorted);
    }

    TEST_CASE("fractions")
    {
        // clang-format off
        std::vector<std::string> unsorted{
            "1.011.02",
            "1.010.12",
            "1.009.02",
            "1.009.20",
            "1.009.10",
            "1.002.08",
            "1.002.03",
            "1.002.01",
        };

        std::vector<std::string> sorted{
            "1.002.01",
            "1.002.03",
            "1.002.08",
            "1.009.02",
            "1.009.10",
            "1.009.20",
            "1.010.12",
            "1.011.02",
        };
        // clang-format on

        std::ranges::sort(unsorted, [](auto a, auto b) { return strnatcmp(a, b) < 0; });

        REQUIRE_EQ(unsorted, sorted);
    }

    TEST_CASE("words")
    {
        // clang-format off
        std::vector<std::string> unsorted{
            "fred",
            "pic2",
            "pic100a",
            "pic120",
            "pic121",
            "jane",
            "tom",
            "pic02a",
            "pic3",
            "pic4",
            "1-20",
            "pic100",
            "pic02000",
            "10-20",
            "1-02",
            "1-2",
            "x2-y7",
            "x8-y8",
            "x2-y08",
            "x2-g8",
            "pic01",
            "pic02",
            "pic 6",
            "pic   7",
            "pic 5",
            "pic05",
            "pic 5 ",
            "pic 5 something",
            "pic 4 else",
        };

        std::vector<std::string> sorted{
            "1-02",
            "1-2",
            "1-20",
            "10-20",
            "fred",
            "jane",
            "pic01",
            "pic02",
            "pic02a",
            "pic02000",
            "pic05",
            "pic2",
            "pic3",
            "pic4",
            "pic 4 else",
            "pic 5",
            "pic 5 ",
            "pic 5 something",
            "pic 6",
            "pic   7",
            "pic100",
            "pic100a",
            "pic120",
            "pic121",
            "tom",
            "x2-g8",
            "x2-y08",
            "x2-y7",
            "x8-y8",
        };
        // clang-format on

        std::ranges::sort(unsorted, [](auto a, auto b) { return strnatcmp(a, b) < 0; });

        REQUIRE_EQ(unsorted, sorted);
    }

    TEST_CASE("simple_names")
    {
        // clang-format off
        std::vector<std::string> unsorted{
            "new3",
            "new25.5",
            "new5",
            "new5.5",
            "new1",
            "new10",
            "new100",
            "new2",
            "new25",
        };

        std::vector<std::string> sorted{
            "new1",
            "new2",
            "new3",
            "new5",
            "new5.5",
            "new10",
            "new25",
            "new25.5",
            "new100",
        };
        // clang-format on

        std::ranges::sort(unsorted, [](auto a, auto b) { return strnatcmp(a, b) < 0; });

        REQUIRE_EQ(unsorted, sorted);
    }
}
