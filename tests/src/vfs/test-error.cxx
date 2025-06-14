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

#include "spacefm/vfs/error.hxx"

TEST_SUITE("vfs::error_code" * doctest::description(""))
{
    TEST_CASE("vfs::error_code")
    {
        SUBCASE("vfs::error_code::none")
        {
            const auto ec = vfs::make_error_code(vfs::error_code::none);

            CHECK_EQ(ec.category().name(), "vfs::error_category()");

            CHECK_EQ(bool(ec), false);
            CHECK_EQ(ec.message(), "none");
            CHECK_EQ(ec == vfs::error_code::none, true);
        }

        SUBCASE("vfs::error_code::parse_error")
        {
            const auto ec = vfs::make_error_code(vfs::error_code::parse_error);

            CHECK_EQ(ec.category().name(), "vfs::error_category()");

            CHECK_EQ(bool(ec), true);
            CHECK_EQ(ec.message(), "parse error");
            CHECK_EQ(ec == vfs::error_code::parse_error, true);
        }

        SUBCASE("vfs::error_code::key_not_found")
        {
            const auto ec = vfs::make_error_code(vfs::error_code::key_not_found);

            CHECK_EQ(ec.category().name(), "vfs::error_category()");

            CHECK_EQ(bool(ec), true);
            CHECK_EQ(ec.message(), "key not found");
            CHECK_EQ(ec == vfs::error_code::key_not_found, true);
        }

        SUBCASE("vfs::error_code::unknown_key")
        {
            const auto ec = vfs::make_error_code(vfs::error_code::unknown_key);

            CHECK_EQ(ec.category().name(), "vfs::error_category()");

            CHECK_EQ(bool(ec), true);
            CHECK_EQ(ec.message(), "unknown key");
            CHECK_EQ(ec == vfs::error_code::unknown_key, true);
        }

        SUBCASE("vfs::error_code::missing_key")
        {
            const auto ec = vfs::make_error_code(vfs::error_code::missing_key);

            CHECK_EQ(ec.category().name(), "vfs::error_category()");

            CHECK_EQ(bool(ec), true);
            CHECK_EQ(ec.message(), "missing key");
            CHECK_EQ(ec == vfs::error_code::missing_key, true);
        }

        SUBCASE("vfs::error_code::file_not_found")
        {
            const auto ec = vfs::make_error_code(vfs::error_code::file_not_found);

            CHECK_EQ(ec.category().name(), "vfs::error_category()");

            CHECK_EQ(bool(ec), true);
            CHECK_EQ(ec.message(), "file not found");
            CHECK_EQ(ec == vfs::error_code::file_not_found, true);
        }

        SUBCASE("vfs::error_code::file_too_large")
        {
            const auto ec = vfs::make_error_code(vfs::error_code::file_too_large);

            CHECK_EQ(ec.category().name(), "vfs::error_category()");

            CHECK_EQ(bool(ec), true);
            CHECK_EQ(ec.message(), "file too large");
            CHECK_EQ(ec == vfs::error_code::file_too_large, true);
        }

        SUBCASE("vfs::error_code::file_open_failure")
        {
            const auto ec = vfs::make_error_code(vfs::error_code::file_open_failure);

            CHECK_EQ(ec.category().name(), "vfs::error_category()");

            CHECK_EQ(bool(ec), true);
            CHECK_EQ(ec.message(), "file open failure");
            CHECK_EQ(ec == vfs::error_code::file_open_failure, true);
        }

        SUBCASE("vfs::error_code::file_read_failure")
        {
            const auto ec = vfs::make_error_code(vfs::error_code::file_read_failure);

            CHECK_EQ(ec.category().name(), "vfs::error_category()");

            CHECK_EQ(bool(ec), true);
            CHECK_EQ(ec.message(), "file read failure");
            CHECK_EQ(ec == vfs::error_code::file_read_failure, true);
        }

        SUBCASE("vfs::error_code::file_write_failure")
        {
            const auto ec = vfs::make_error_code(vfs::error_code::file_write_failure);

            CHECK_EQ(ec.category().name(), "vfs::error_category()");

            CHECK_EQ(bool(ec), true);
            CHECK_EQ(ec.message(), "file write failure");
            CHECK_EQ(ec == vfs::error_code::file_write_failure, true);
        }

        SUBCASE("vfs::error_code::file_close_failure")
        {
            const auto ec = vfs::make_error_code(vfs::error_code::file_close_failure);

            CHECK_EQ(ec.category().name(), "vfs::error_category()");

            CHECK_EQ(bool(ec), true);
            CHECK_EQ(ec.message(), "file close failure");
            CHECK_EQ(ec == vfs::error_code::file_close_failure, true);
        }
    }
}
