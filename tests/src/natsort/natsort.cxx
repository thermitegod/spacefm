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
#include <filesystem>
#include <format>
#include <random>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include <doctest/doctest.h>

#include <ztd/ztd.hxx>

#include "natsort/natsort.hxx"

template<typename T>
void
shuffle_vector(std::vector<T>& vec) noexcept
{
    static thread_local std::mt19937 rng(std::random_device{}());

    std::shuffle(vec.begin(), vec.end(), rng);
}

TEST_SUITE("natsort" * doctest::description(""))
{
    TEST_CASE("natsort::compare")
    {
        SUBCASE("case matching")
        {
            CHECK(natsort::compare("a", "a", false) == 0);
            CHECK(natsort::compare("A", "A", false) == 0);

            CHECK(natsort::compare("A", "a", false) < 0);
            CHECK(natsort::compare("a", "A", false) > 0);
        }

        SUBCASE("case insensitive matching")
        {
            CHECK(natsort::compare("a", "a", true) == 0);
            CHECK(natsort::compare("A", "A", true) == 0);

            CHECK(natsort::compare("a", "A", true) == 0);
            CHECK(natsort::compare("A", "a", true) == 0);
        }
    }

    TEST_CASE_TEMPLATE("natsort::sorter", T, std::string, std::string_view, std::filesystem::path)
    {
        auto test_transformations = [](std::vector<std::string> raw_sorted, bool test_zfill = true)
        {
            REQUIRE(!raw_sorted.empty());

            auto append_ext = [](std::vector<std::string>& vec, std::string_view ext)
            {
                for (auto& str : vec)
                {
                    str.append(ext);
                }
            };

            auto apply_zfill =
                [](std::vector<std::string>& vec, std::size_t width, std::string_view ext = "")
            {
                for (auto& str : vec)
                {
                    str = ztd::zfill(str, width);
                    if (!ext.empty())
                    {
                        str.append(ext);
                    }
                }
            };

            auto prepend_path = [](std::vector<std::string>& vec, std::string_view parent_path)
            {
                for (auto& str : vec)
                {
                    str = std::string(parent_path) + "/" + str;
                }
            };

            SUBCASE("transformations")
            {
                SUBCASE("none") {}

                SUBCASE("with text extension")
                {
                    append_ext(raw_sorted, ".txt");
                }

                SUBCASE("with single tar extension")
                {
                    append_ext(raw_sorted, ".tar");
                }

                SUBCASE("with multi part tar extension")
                {
                    SUBCASE(".tar.gz")
                    {
                        append_ext(raw_sorted, ".tar.gz");
                    }

                    SUBCASE(".tar.bz2")
                    {
                        append_ext(raw_sorted, ".tar.bz2");
                    }
                }

                SUBCASE("with numeric extension")
                {
                    SUBCASE(".7z")
                    {
                        append_ext(raw_sorted, ".7z");
                    }

                    SUBCASE(".bz2")
                    {
                        append_ext(raw_sorted, ".bz2");
                    }
                }

                SUBCASE("with symbol extension")
                {
                    SUBCASE(".@@@")
                    {
                        append_ext(raw_sorted, ".@@@");
                    }

                    SUBCASE(".$$$")
                    {
                        append_ext(raw_sorted, ".$$$");
                    }
                }

                SUBCASE("relative path")
                {
                    prepend_path(raw_sorted, "../home/user");
                }

                SUBCASE("relative path with extension")
                {
                    prepend_path(raw_sorted, "../home/user");
                    append_ext(raw_sorted, ".txt");
                }

                SUBCASE("relative path with numeric extension")
                {
                    prepend_path(raw_sorted, "../home/user");
                    append_ext(raw_sorted, ".7z");
                }

                SUBCASE("absolute path")
                {
                    prepend_path(raw_sorted, "/home/user");
                }

                SUBCASE("absolute path with extension")
                {
                    prepend_path(raw_sorted, "/home/user");
                    append_ext(raw_sorted, ".txt");
                }

                SUBCASE("absolute path with numeric extension")
                {
                    prepend_path(raw_sorted, "/home/user");
                    append_ext(raw_sorted, ".7z");
                }

                if (test_zfill)
                {
                    SUBCASE("padded leading zero")
                    {
                        apply_zfill(raw_sorted, 10);
                    }

                    SUBCASE("padded leading zero with ext")
                    {
                        apply_zfill(raw_sorted, 10, ".txt");
                    }

                    SUBCASE("padded leading zero numeric extension")
                    {
                        apply_zfill(raw_sorted, 10, ".7z");
                    }
                }
            }

            /////////////////////////////////////////////////////

            std::vector<T> sorted(raw_sorted.begin(), raw_sorted.end());
            std::vector<T> unsorted = sorted;

            REQUIRE(!unsorted.empty());
            REQUIRE(!sorted.empty());
            REQUIRE_EQ(unsorted.size(), sorted.size());

            std::size_t attempts = 0;
            do
            {
                shuffle_vector(unsorted);
                ++attempts;
            } while (unsorted == sorted && attempts < 100);
            REQUIRE_MESSAGE(unsorted != sorted, "Failed to get a random sort");

            /////////////////////////////////////////////////////

            std::ranges::sort(unsorted, natsort::sorter{});

            /////////////////////////////////////////////////////

            auto format_diff = [](const auto& result, const auto& wanted)
            {
                std::string output = "\n";
                for (std::size_t i = 0; i < result.size(); ++i)
                {
                    output += std::format("result {:<25} | wanted {}\n", result[i], wanted[i]);
                }
                return output;
            };
            CHECK_MESSAGE(unsorted == sorted, format_diff(unsorted, sorted));
        };

        /////////////////////////////////////////////////////

        std::vector<std::string> raw_sorted;

        SUBCASE("dates")
        {
            // clang-format off
            test_transformations(
                {
                    "1914-6-28",
                    "1967-6-8",
                    "1999-3-3",
                    "1999-12-25",
                    "2000-1-2",
                    "2000-1-10",
                    "2000-3-23",
                    "2016-5-28",
                    "2019-8-10",
                    "2020-1-1",
                    "2021-4-9",
                    "2023-11-30",
                    "2024-2-28",
                    "2024-2-29",
                    "2025-1-1",
                    "2025-7-20",
                    "2025-12-31",
                    "2026-3-15",
                    "2026-7-25",
                },
                false);
            // clang-format on
        }

        SUBCASE("dates zero pad")
        {
            // clang-format off
            test_transformations({
                "1914-06-28",
                "1967-06-08",
                "1999-03-03",
                "1999-12-25",
                "2000-01-02",
                "2000-01-10",
                "2000-03-23",
                "2016-05-28",
                "2019-08-10",
                "2020-01-01",
                "2021-04-09",
                "2023-11-30",
                "2024-02-28",
                "2024-02-29",
                "2025-01-01",
                "2025-07-20",
                "2025-12-31",
                "2026-03-15",
                "2026-07-25",
            });
            // clang-format on
        }

        SUBCASE("small and large numbers")
        {
            // clang-format off
            test_transformations({
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
            });
            // clang-format on
        }

        SUBCASE("version numbers")
        {
            // clang-format off
            test_transformations({
                "1.002.01",
                "1.002.03",
                "1.002.08",
                "1.009.02",
                "1.009.10",
                "1.009.20",
                "1.010.12",
                "1.011.02",
            });
            // clang-format on
        }

        SUBCASE("words")
        {
            // clang-format off
            test_transformations({
                "1-2",
                "1-02",
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
                "pic2",
                "pic02",
                "pic02a",
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
            }, false);
            // clang-format on
        }

        SUBCASE("numbers")
        {
            // clang-format off
            test_transformations({
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
            });
            // clang-format on
        }

        SUBCASE("mixed numbered and numbers with decimals")
        {
            // clang-format off
            test_transformations({
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
            }, false);
            // clang-format on
        }

        SUBCASE("decimals")
        {
            // clang-format off
            test_transformations({
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
            }, false);
            // clang-format on
        }

        SUBCASE("non numeric usage of decimals")
        {
            // clang-format off
            test_transformations({
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
            });
            // clang-format on
        }

        SUBCASE("dashes alpha")
        {
            // clang-format off
            test_transformations({
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
            }, false);
            // clang-format on
        }

        SUBCASE("dashes numeric")
        {
            // clang-format off
            test_transformations({
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
            }, false);
            // clang-format on
        }

        SUBCASE("hidden")
        {
            // clang-format off
            test_transformations({
                ".a",
                ".b",
                ".c",
                ".d",
                ".e",
                ".f",
            });
            // clang-format on
        }

        SUBCASE("same basename, different extension")
        {
            // clang-format off
            test_transformations({
                "z.gif",
                "z.jpg",
                "z.json",
                "z.mkv",
                "z.mp4",
                "z.png",
                "z.toml",
                "z.txt",
                "z.zip",
            }, false);
            // clang-format on
        }
    }
}
