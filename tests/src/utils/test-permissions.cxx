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

#include <doctest/doctest.h>

#include "spacefm/utils/permissions.hxx"
#include "spacefm/vfs/vfs-user-dirs.hxx"

TEST_SUITE("utils::permissions" * doctest::description(""))
{
    TEST_CASE("has_read_permission")
    {
        CHECK(utils::has_read_permission(vfs::user::home()));
        CHECK(utils::has_read_permission("/tmp"));

        CHECK(!utils::has_read_permission("/root"));
    }

    TEST_CASE("has_write_permission")
    {
        CHECK(utils::has_write_permission(vfs::user::home()));
        CHECK(utils::has_write_permission("/tmp"));

        CHECK(!utils::has_write_permission("/root"));
    }

    TEST_CASE("has_execute_permission")
    {
        CHECK(utils::has_execute_permission(vfs::user::home()));
        CHECK(utils::has_execute_permission("/tmp"));

        CHECK(!utils::has_execute_permission("/root"));
    }

    TEST_CASE("check_directory_permissions")
    {
        CHECK(utils::check_directory_permissions(vfs::user::home()));
        CHECK(utils::check_directory_permissions("/tmp"));

        CHECK(!utils::check_directory_permissions("/root"));
    }
}
