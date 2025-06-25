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

#include <filesystem>
#include <ranges>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "spacefm/vfs/clipboard.hxx"

TEST_SUITE("vfs::clipboard" * doctest::description(""))
{
    TEST_CASE("clear")
    {
        vfs::clipboard::clear();
        REQUIRE_FALSE(vfs::clipboard::is_valid());

        SUBCASE("clear")
        {
            vfs::clipboard::clear();
            CHECK_EQ(vfs::clipboard::get_text(), std::nullopt);
        }

        SUBCASE("set and clear")
        {
            vfs::clipboard::set_text("TEST");
            REQUIRE_FALSE(vfs::clipboard::is_valid());
            CHECK_EQ(vfs::clipboard::get_text().value(), "TEST");

            vfs::clipboard::clear();
            CHECK_EQ(vfs::clipboard::get_text(), std::nullopt);
        }
    }

    TEST_CASE("set_text, get_text")
    {
        vfs::clipboard::clear();
        REQUIRE_FALSE(vfs::clipboard::is_valid());

        SUBCASE("simple")
        {
            vfs::clipboard::set_text("TEST");
            REQUIRE_FALSE(vfs::clipboard::is_valid());
            CHECK_EQ(vfs::clipboard::get_text().value(), "TEST");
        }

        SUBCASE("quotes")
        {
            const char quote = '\"';
            for (auto i : std::views::iota(1u, 10u))
            {
                std::string s;
                s.append(i, quote);

                vfs::clipboard::set_text(s);
                REQUIRE_FALSE(vfs::clipboard::is_valid());
                CHECK_EQ(vfs::clipboard::get_text().value(), s);
            }
        }
    }

    TEST_CASE("set, get")
    {
        vfs::clipboard::clear();
        REQUIRE_FALSE(vfs::clipboard::is_valid());

        SUBCASE("simple")
        {
            const std::vector<std::filesystem::path> files{
                "/home/user/1.txt",
                "/home/user/2.txt",
                "/home/user/3.txt",
                "/home/user/4.txt",
                "/home/user/5.txt",
                "/home/user/6.txt",
            };

            vfs::clipboard::set(
                vfs::clipboard::clipboard_data{.mode = vfs::clipboard::mode::copy, .files = files});

            REQUIRE(vfs::clipboard::is_valid());

            auto data = vfs::clipboard::get().value();
            CHECK_EQ(data.files, files);
        }

        SUBCASE("escaped paths")
        {
            const std::vector<std::filesystem::path> files{
                "/home/user/1 0.txt",
                "/home/user/2 0.txt",
                "/home/user/3 0.txt",
                "/home/user/4 0.txt",
                "/home/user/5 0.txt",
                "/home/user/6 0.txt",
            };

            vfs::clipboard::set(
                vfs::clipboard::clipboard_data{.mode = vfs::clipboard::mode::copy, .files = files});

            REQUIRE(vfs::clipboard::is_valid());

            auto data = vfs::clipboard::get().value();
            CHECK_EQ(data.files, files);
        }
    }
}
