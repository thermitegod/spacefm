/**
 * Copyright (C) 2025 Brandon Zorn <brandonzorn@cock.li>
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

#include <ztd/ztd.hxx>

#include "natsort/strnatcmp.hxx"

namespace sorter
{
struct natural
{
    bool
    operator()(const std::string_view& lhs, const std::string_view& rhs) const
    {
        return strnatcmp(lhs, rhs, false) < 0;
    }
};

struct natural_fold
{
    bool
    operator()(const std::string_view& lhs, const std::string_view& rhs) const
    {
        return strnatcmp(lhs, rhs, true) < 0;
    }
};
} // namespace sorter

TEST_SUITE("natsort" * doctest::description(""))
{
    TEST_CASE("natsort")
    {
        std::vector<std::string> unsorted;
        std::vector<std::string> sorted;

        SUBCASE("dates")
        {
            // clang-format on
            unsorted = {
                "2000-1-10",
                "2000-1-2",
                "1999-12-25",
                "2000-3-23",
                "1999-3-3",
            };

            sorted = {
                "1999-3-3",
                "1999-12-25",
                "2000-1-2",
                "2000-1-10",
                "2000-3-23",
            };
            // clang-format on

            SUBCASE("default") {}

            SUBCASE("with extension")
            {
                for (auto& str : unsorted)
                {
                    str.append(".txt");
                }

                for (auto& str : sorted)
                {
                    str.append(".txt");
                }
            }
        }

        SUBCASE("small and large numbers")
        {
            // clang-format off
            unsorted = {
                "6",
                "7",
                "1000000",
                "8",
                "10",
                "100",
                "2",
                "100000",
                "10000",
                "4",
                "1",
                "0",
                "1000",
                "5",
                "3",
                "9",
            };

            sorted = {
                "0",
                "1",
                "2",
                "3",
                "4",
                "5",
                "6",
                "7",
                "8",
                "9",
                "10",
                "100",
                "1000",
                "10000",
                "100000",
                "1000000",
            };
            // clang-format on

            SUBCASE("default") {}

            SUBCASE("with extension")
            {
                for (auto& str : unsorted)
                {
                    str.append(".txt");
                }

                for (auto& str : sorted)
                {
                    str.append(".txt");
                }
            }

            SUBCASE("with padded leading zero")
            {
                for (auto& str : unsorted)
                {
                    str = ztd::zfill(str, 10);
                }

                for (auto& str : sorted)
                {
                    str = ztd::zfill(str, 10);
                }
            }

            SUBCASE("with padded leading zero and extension")
            {
                for (auto& str : unsorted)
                {
                    str = ztd::zfill(str, 10).append(".txt");
                }

                for (auto& str : sorted)
                {
                    str = ztd::zfill(str, 10).append(".txt");
                }
            }
        }

        SUBCASE("version numbers")
        {
            // clang-format off
            unsorted = {
                "1.011.02",
                "1.010.12",
                "1.009.02",
                "1.009.20",
                "1.009.10",
                "1.002.08",
                "1.002.03",
                "1.002.01",
            };

            sorted = {
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

            SUBCASE("default") {}

            SUBCASE("with extension")
            {
                for (auto& str : unsorted)
                {
                    str.append(".txt");
                }

                for (auto& str : sorted)
                {
                    str.append(".txt");
                }
            }
        }

        SUBCASE("words")
        {
            // clang-format off
            unsorted = {
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

            sorted = {
                "1-02",
                "1-2",
                "1-20",
                "10-20",
                "fred",
                "jane",
                "pic   7",
                "pic 4 else",
                "pic 5",
                "pic 5 ",
                "pic 5 something",
                "pic 6",
                "pic01",
                "pic02",
                "pic02a",
                "pic2",
                "pic3",
                "pic4",
                "pic05",
                "pic100",
                "pic100a",
                "pic120",
                "pic121",
                "pic02000",
                "tom",
                "x2-g8",
                "x2-y7",
                "x2-y08",
                "x8-y8",
            };
            // clang-format on

            SUBCASE("default") {}

            SUBCASE("with extension")
            {
                for (auto& str : unsorted)
                {
                    str.append(".txt");
                }

                for (auto& str : sorted)
                {
                    str.append(".txt");
                }
            }
        }

        SUBCASE("numbers")
        {
            // clang-format off
            unsorted = {
                "13",
                "2",
                "10",
                "15",
                "0",
                "5",
                "7",
                "19",
                "8",
                "17",
                "20",
                "18",
                "3",
                "4",
                "11",
                "6",
                "1",
                "12",
                "9",
                "16",
                "14",
            };

            sorted = {
                "0",
                "1",
                "2",
                "3",
                "4",
                "5",
                "6",
                "7",
                "8",
                "9",
                "10",
                "11",
                "12",
                "13",
                "14",
                "15",
                "16",
                "17",
                "18",
                "19",
                "20",
            };
            // clang-format on

            SUBCASE("default") {}

            SUBCASE("with extension")
            {
                for (auto& str : unsorted)
                {
                    str.append(".txt");
                }

                for (auto& str : sorted)
                {
                    str.append(".txt");
                }
            }

            SUBCASE("with padded leading zero")
            {
                for (auto& str : unsorted)
                {
                    str = ztd::zfill(str, 10);
                }

                for (auto& str : sorted)
                {
                    str = ztd::zfill(str, 10);
                }
            }

            SUBCASE("with padded leading zero and extension")
            {
                for (auto& str : unsorted)
                {
                    str = ztd::zfill(str, 10).append(".txt");
                }

                for (auto& str : sorted)
                {
                    str = ztd::zfill(str, 10).append(".txt");
                }
            }
        }

        SUBCASE("mixed numbered and numbers with decimals")
        {
            // clang-format off
            unsorted = {
                "2",
                "15.5",
                "20",
                "6",
                "18.5",
                "13",
                "7",
                "5.5",
                "19.5",
                "14.5",
                "20.5",
                "17.5",
                "16.5",
                "15",
                "12",
                "1",
                "5",
                "9.5",
                "6.5",
                "3",
                "11",
                "13.5",
                "16",
                "14",
                "8",
                "8.5",
                "1.5",
                "18",
                "7.5",
                "2.5",
                "9",
                "0.5",
                "10.5",
                "10",
                "17",
                "12.5",
                "11.5",
                "4",
                "19",
                "0",
                "4.5",
                "3.5",
            };

            sorted = {
                "0",
                "0.5",
                "1",
                "1.5",
                "2",
                "2.5",
                "3",
                "3.5",
                "4",
                "4.5",
                "5",
                "5.5",
                "6",
                "6.5",
                "7",
                "7.5",
                "8",
                "8.5",
                "9",
                "9.5",
                "10",
                "10.5",
                "11",
                "11.5",
                "12",
                "12.5",
                "13",
                "13.5",
                "14",
                "14.5",
                "15",
                "15.5",
                "16",
                "16.5",
                "17",
                "17.5",
                "18",
                "18.5",
                "19",
                "19.5",
                "20",
                "20.5",
            };
            // clang-format on

            SUBCASE("default") {}

            SUBCASE("with extension")
            {
                for (auto& str : unsorted)
                {
                    str.append(".txt");
                }

                for (auto& str : sorted)
                {
                    str.append(".txt");
                }
            }
        }

        SUBCASE("decimals")
        {
            // clang-format off
            unsorted = {
                "1.9",
                "1.7",
                "1.5",
                "0.0",
                "1.6",
                "1.0",
                "1.1",
                "2.0",
                "1.3",
                "1.8",
                "1.2",
                "10.0",
                "1.4",
            };

            sorted = {
                "0.0",
                "1.0",
                "1.1",
                "1.2",
                "1.3",
                "1.4",
                "1.5",
                "1.6",
                "1.7",
                "1.8",
                "1.9",
                "2.0",
                "10.0",
            };
            // clang-format on

            SUBCASE("default") {}

            SUBCASE("with extension")
            {
                for (auto& str : unsorted)
                {
                    str.append(".txt");
                }

                for (auto& str : sorted)
                {
                    str.append(".txt");
                }
            }
        }

        SUBCASE("non numeric usage of decimals")
        {
            // clang-format off
            unsorted = {
                "a.b.c.h",
                "a.b.c.j",
                "a.b.c.f",
                "a.b.c.a",
                "a.b.c.b",
                "a.b.c.i",
                "a.b.c.c",
                "a.b.c.d",
                "a.b.c.e",
                "a.b.c.g",
            };

            sorted = {
                "a.b.c.a",
                "a.b.c.b",
                "a.b.c.c",
                "a.b.c.d",
                "a.b.c.e",
                "a.b.c.f",
                "a.b.c.g",
                "a.b.c.h",
                "a.b.c.i",
                "a.b.c.j",
            };
            // clang-format on

            SUBCASE("default") {}

            SUBCASE("with extension")
            {
                for (auto& str : unsorted)
                {
                    str.append(".txt");
                }

                for (auto& str : sorted)
                {
                    str.append(".txt");
                }
            }
        }

        SUBCASE("dashes alpha")
        {
            // clang-format off
            unsorted = {
                "A-5",
                "A-7",
                "A-9",
                "B",
                "A-8",
                "A-6",
                "A",
                "A-3",
                "B-1",
                "A-4",
                "A-1",
                "A-2",
            };

            sorted = {
                "A",
                "A-1",
                "A-2",
                "A-3",
                "A-4",
                "A-5",
                "A-6",
                "A-7",
                "A-8",
                "A-9",
                "B",
                "B-1",
            };
            // clang-format on

            SUBCASE("default") {}

            SUBCASE("with extension")
            {
                for (auto& str : unsorted)
                {
                    str.append(".txt");
                }

                for (auto& str : sorted)
                {
                    str.append(".txt");
                }
            }

            SUBCASE("with text after dashed numbers")
            {
                for (auto& str : unsorted)
                {
                    str.append(" Z.txt");
                }

                for (auto& str : sorted)
                {
                    str.append(" Z.txt");
                }
            }
        }

        SUBCASE("dashes numeric")
        {
            // clang-format off
            unsorted = {
                "41",
                "40-6",
                "40-2",
                "40",
                "40-7",
                "40-3",
                "40-1",
                "40-4",
                "40-9",
                "40-5",
                "41-1",
                "40-8",
            };

            sorted = {
                "40",
                "40-1",
                "40-2",
                "40-3",
                "40-4",
                "40-5",
                "40-6",
                "40-7",
                "40-8",
                "40-9",
                "41",
                "41-1",
            };
            // clang-format on

            SUBCASE("default") {}

            SUBCASE("with extension")
            {
                for (auto& str : unsorted)
                {
                    str.append(".txt");
                }

                for (auto& str : sorted)
                {
                    str.append(".txt");
                }
            }

            SUBCASE("with text after dashed numbers")
            {
                for (auto& str : unsorted)
                {
                    str.append(" Z.txt");
                }

                for (auto& str : sorted)
                {
                    str.append(" Z.txt");
                }
            }
        }

        SUBCASE("hidden")
        {
            // clang-format off
            unsorted = {
                ".b",
                ".f",
                ".d",
                ".e",
                ".c",
                ".a",
            };

            sorted = {
                ".a",
                ".b",
                ".c",
                ".d",
                ".e",
                ".f",
            };
            // clang-format on

            SUBCASE("default") {}

            SUBCASE("with extension")
            {
                for (auto& str : unsorted)
                {
                    str.append(".txt");
                }

                for (auto& str : sorted)
                {
                    str.append(".txt");
                }
            }
        }

        SUBCASE("same basename, different extension")
        {
            // clang-format off
            unsorted = {
                "z.txt",
                "z.gif",
                "z.mkv",
                "z.jpg",
                "z.zip",
                "z.png",
                "z.json",
                "z.mp4",
                "z.toml",
            };

            sorted = {
                "z.gif",
                "z.jpg",
                "z.json",
                "z.mkv",
                "z.mp4",
                "z.png",
                "z.toml",
                "z.txt",
                "z.zip",
            };
            // clang-format on
        }

        REQUIRE(!unsorted.empty());
        REQUIRE(!sorted.empty());
        CHECK_EQ(unsorted.size(), sorted.size());

        std::ranges::sort(unsorted, sorter::natural{});

        CHECK_MESSAGE(unsorted == sorted,
                      std::format("\nResult: {}\nWanted: {}",
                                  ztd::join(unsorted, ", "),
                                  ztd::join(sorted, ", ")));
    }
}
