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

#include <string_view>

#include <doctest/doctest.h>

#include "vfs/mime-type/chrome/mime-utils.hxx"

TEST_SUITE("vfs::detail::mime_type::chrome" * doctest::description(""))
{
    using namespace vfs::detail::mime_type::chrome;

    TEST_CASE("application/octet-stream")
    {
        constexpr std::string_view mime_type = "application/octet-stream";

        SUBCASE("empty")
        {
            const auto type = GetFileMimeType("");

            CHECK_EQ(type, mime_type);
        }

        SUBCASE("no ext")
        {
            const auto type = GetFileMimeType("file");

            CHECK_EQ(type, mime_type);
        }
    }

    TEST_CASE("image/jpeg")
    {
        constexpr std::string_view mime_type = "image/jpeg";

        SUBCASE("ext jpg")
        {
            const auto type = GetFileMimeType("file.jpg");

            CHECK_EQ(type, mime_type);
        }

        SUBCASE("ext jpeg")
        {
            const auto type = GetFileMimeType("file.jpeg");

            CHECK_EQ(type, mime_type);
        }
    }

    TEST_CASE("image/jxl")
    {
        constexpr std::string_view mime_type = "image/jxl";

        SUBCASE("ext jxl")
        {
            const auto type = GetFileMimeType("file.jxl");

            CHECK_EQ(type, mime_type);
        }
    }

    TEST_CASE("image/png")
    {
        constexpr std::string_view mime_type = "image/png";

        SUBCASE("ext jxl")
        {
            const auto type = GetFileMimeType("file.png");

            CHECK_EQ(type, mime_type);
        }
    }

    TEST_CASE("image/png")
    {
        constexpr std::string_view mime_type = "image/png";

        SUBCASE("ext png")
        {
            const auto type = GetFileMimeType("file.png");

            CHECK_EQ(type, mime_type);
        }
    }

    TEST_CASE("text/plain")
    {
        constexpr std::string_view mime_type = "text/plain";

        SUBCASE("ext txt")
        {
            const auto type = GetFileMimeType("file.txt");

            CHECK_EQ(type, mime_type);
        }
    }
}
