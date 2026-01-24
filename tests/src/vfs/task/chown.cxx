/**
 * Copyright (C) 2026 Brandon Zorn <brandonzorn@cock.li>
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

#include <ztd/ztd.hxx>

#include "vfs/error.hxx"
#include "vfs/task/task.hxx"

TEST_SUITE("vfs::task" * doctest::description(""))
{
    TEST_CASE("chown")
    {
        vfs::chown task;

        SUBCASE("basic user:group")
        {
            task.user("user").group("group").path("/does-not-exist").compile();

            CHECK_EQ(*task.dump(), "chown --preserve-root  user:group \"/does-not-exist\"");
        }

        SUBCASE("basic user")
        {
            task.user("user").path("/does-not-exist").compile();

            CHECK_EQ(*task.dump(), "chown --preserve-root  user \"/does-not-exist\"");
        }

        SUBCASE("basic group")
        {
            task.group("group").path("/does-not-exist").compile();

            CHECK_EQ(*task.dump(), "chgrp --preserve-root  group \"/does-not-exist\"");
        }

        SUBCASE("basic recursive user:group")
        {
            task.recursive().user("user").group("group").path("/does-not-exist").compile();

            CHECK(*task.dump() ==
                  "chown --preserve-root --recursive user:group \"/does-not-exist\"");
        }

        SUBCASE("basic recursive user")
        {
            task.recursive().user("user").path("/does-not-exist").compile();

            CHECK_EQ(*task.dump(), "chown --preserve-root --recursive user \"/does-not-exist\"");
        }

        SUBCASE("basic recursive group")
        {
            task.recursive().group("group").path("/does-not-exist").compile();

            CHECK_EQ(*task.dump(), "chgrp --preserve-root --recursive group \"/does-not-exist\"");
        }

        SUBCASE("error missing user group")
        {
            task.path("/does-not-exist").compile();

            CHECK(task.error() == vfs::error_code::task_bad_construction);
        }

        SUBCASE("error empty path")
        {
            task.user("user").group("group").path("").compile();

            CHECK(task.error() == vfs::error_code::task_empty_path);
        }

        SUBCASE("error missing path")
        {
            task.user("user").group("group").compile();

            CHECK(task.error() == vfs::error_code::task_bad_construction);
        }
    }
}
