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

#include <doctest/doctest.h>

#include "vfs/error.hxx"

TEST_SUITE("vfs::error_code")
{
    TEST_CASE("vfs::error_code")
    {
        SUBCASE("vfs::error_code::none")
        {
            const auto ec = vfs::make_error_code(vfs::error_code::none);

            CHECK_FALSE(bool(ec));
            CHECK(ec.message() == "none");
            CHECK(ec == vfs::error_code::none);
        }

        SUBCASE("vfs::error_code::parse_error")
        {
            const auto ec = vfs::make_error_code(vfs::error_code::parse_error);

            CHECK(bool(ec));
            CHECK(ec.message() == "parse error");
            CHECK(ec == vfs::error_code::parse_error);
        }

        SUBCASE("vfs::error_code::key_not_found")
        {
            const auto ec = vfs::make_error_code(vfs::error_code::key_not_found);

            CHECK(bool(ec));
            CHECK(ec.message() == "key not found");
            CHECK(ec == vfs::error_code::key_not_found);
        }

        SUBCASE("vfs::error_code::unknown_key")
        {
            const auto ec = vfs::make_error_code(vfs::error_code::unknown_key);

            CHECK(bool(ec));
            CHECK(ec.message() == "unknown key");
            CHECK(ec == vfs::error_code::unknown_key);
        }

        SUBCASE("vfs::error_code::missing_key")
        {
            const auto ec = vfs::make_error_code(vfs::error_code::missing_key);

            CHECK(bool(ec));
            CHECK(ec.message() == "missing key");
            CHECK(ec == vfs::error_code::missing_key);
        }

        SUBCASE("vfs::error_code::file_not_found")
        {
            const auto ec = vfs::make_error_code(vfs::error_code::file_not_found);

            CHECK(bool(ec));
            CHECK(ec.message() == "file not found");
            CHECK(ec == vfs::error_code::file_not_found);
        }

        SUBCASE("vfs::error_code::file_too_large")
        {
            const auto ec = vfs::make_error_code(vfs::error_code::file_too_large);

            CHECK(bool(ec));
            CHECK(ec.message() == "file too large");
            CHECK(ec == vfs::error_code::file_too_large);
        }

        SUBCASE("vfs::error_code::file_open_failure")
        {
            const auto ec = vfs::make_error_code(vfs::error_code::file_open_failure);

            CHECK(bool(ec));
            CHECK(ec.message() == "file open failure");
            CHECK(ec == vfs::error_code::file_open_failure);
        }

        SUBCASE("vfs::error_code::file_read_failure")
        {
            const auto ec = vfs::make_error_code(vfs::error_code::file_read_failure);

            CHECK(bool(ec));
            CHECK(ec.message() == "file read failure");
            CHECK(ec == vfs::error_code::file_read_failure);
        }

        SUBCASE("vfs::error_code::file_write_failure")
        {
            const auto ec = vfs::make_error_code(vfs::error_code::file_write_failure);

            CHECK(bool(ec));
            CHECK(ec.message() == "file write failure");
            CHECK(ec == vfs::error_code::file_write_failure);
        }

        SUBCASE("vfs::error_code::file_close_failure")
        {
            const auto ec = vfs::make_error_code(vfs::error_code::file_close_failure);

            CHECK(bool(ec));
            CHECK(ec.message() == "file close failure");
            CHECK(ec == vfs::error_code::file_close_failure);
        }
    }
}
