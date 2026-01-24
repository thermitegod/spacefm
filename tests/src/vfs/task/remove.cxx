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
    TEST_CASE("remove")
    {
        vfs::remove task;

        SUBCASE("basic")
        {
            task.path("/path/to/remove.txt").compile();

            CHECK_EQ(*task.dump(), "rm --one-file-system --preserve-root  \"/path/to/remove.txt\"");
        }

        SUBCASE("basic force")
        {
            task.force().path("/path/to/remove.txt").compile();

            CHECK_EQ(*task.dump(),
                     "rm --one-file-system --preserve-root --force \"/path/to/remove.txt\"");
        }

        SUBCASE("basic recursive")
        {
            task.recursive().path("/path/to/remove.txt").compile();

            CHECK_EQ(*task.dump(),
                     "rm --one-file-system --preserve-root --recursive \"/path/to/remove.txt\"");
        }

        SUBCASE("basic recursive force")
        {
            task.recursive().force().path("/path/to/remove.txt").compile();

            CHECK_EQ(
                *task.dump(),
                "rm --one-file-system --preserve-root --recursive --force \"/path/to/remove.txt\"");
        }

        SUBCASE("error empty path")
        {
            task.path("").compile();

            CHECK(task.error() == vfs::error_code::task_empty_path);
        }

        SUBCASE("error preserve-root")
        {
            task.path("/").compile();

            CHECK(task.error() == vfs::error_code::task_root_preserve);
        }

        SUBCASE("error missing path")
        {
            task.compile();

            CHECK(task.error() == vfs::error_code::task_bad_construction);
        }
    }
}
