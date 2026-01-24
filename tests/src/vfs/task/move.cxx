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
    TEST_CASE("move")
    {
        vfs::move task;
        SUBCASE("basic")
        {
            task.source("/path/to/source.txt").destination("/path/to/destination.txt").compile();

            CHECK_EQ(*task.dump(), "mv  \"/path/to/source.txt\" \"/path/to/destination.txt\"");
        }

        SUBCASE("basic force")
        {
            task.force()
                .source("/path/to/source.txt")
                .destination("/path/to/destination.txt")
                .compile();

            CHECK(*task.dump() ==
                  "mv --force \"/path/to/source.txt\" \"/path/to/destination.txt\"");
        }

        SUBCASE("error empty source")
        {
            task.source("").destination("/path/to/destination.txt").compile();

            CHECK(task.error() == vfs::error_code::task_empty_source);
        }

        SUBCASE("error empty destination")
        {
            task.source("/path/to/source.txt").destination("").compile();

            CHECK(task.error() == vfs::error_code::task_empty_destination);
        }

        SUBCASE("error preserve-root source")
        {
            task.source("/").destination("/path/to/destination.txt").compile();

            CHECK(task.error() == vfs::error_code::task_root_preserve_source);
        }

        SUBCASE("error preserve-root destination")
        {
            task.source("/path/to/source.txt").destination("/").compile();

            CHECK(task.error() == vfs::error_code::task_root_preserve_destination);
        }

        SUBCASE("error missing source")
        {
            task.destination("/path/to/destination.txt").compile();

            CHECK(task.error() == vfs::error_code::task_bad_construction);
        }

        SUBCASE("error missing destination")
        {
            task.source("/path/to/source.txt").compile();

            CHECK(task.error() == vfs::error_code::task_bad_construction);
        }
    }
}
