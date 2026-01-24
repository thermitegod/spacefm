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
    TEST_CASE("copy")
    {
        vfs::copy task;

        SUBCASE("basic")
        {
            task.source("/source/path").destination("/destination/path").compile();

            CHECK_EQ(*task.dump(), "cp  \"/source/path\" \"/destination/path\"");
        }

        SUBCASE("basic archive")
        {
            task.archive().source("/source/path").destination("/destination/path").compile();

            CHECK_EQ(*task.dump(), "cp --archive \"/source/path\" \"/destination/path\"");
        }

        SUBCASE("basic recursive")
        {
            task.recursive().source("/source/path").destination("/destination/path").compile();

            CHECK_EQ(*task.dump(), "cp --recursive \"/source/path\" \"/destination/path\"");
        }

        SUBCASE("basic force")
        {
            task.force().source("/source/path").destination("/destination/path").compile();

            CHECK_EQ(*task.dump(), "cp --force \"/source/path\" \"/destination/path\"");
        }

        SUBCASE("basic force,recursive")
        {
            task.force()
                .recursive()
                .source("/source/path")
                .destination("/destination/path")
                .compile();

            CHECK_EQ(*task.dump(), "cp --force --recursive \"/source/path\" \"/destination/path\"");
        }

        SUBCASE("error preserve-root source")
        {
            task.source("/").destination("/destination/path").compile();

            CHECK(task.error() == vfs::error_code::task_root_preserve_source);
        }

        SUBCASE("error preserve-root destination")
        {
            task.source("/source/path").destination("/").compile();

            CHECK(task.error() == vfs::error_code::task_root_preserve_destination);
        }

        SUBCASE("error empty source")
        {
            task.source("").destination("/destination/path").compile();

            CHECK(task.error() == vfs::error_code::task_empty_source);
        }

        SUBCASE("error empty destination")
        {
            task.source("/source/path").destination("").compile();

            CHECK(task.error() == vfs::error_code::task_empty_destination);
        }

        SUBCASE("error missing source")
        {
            task.destination("/destination/path").compile();

            CHECK(task.error() == vfs::error_code::task_bad_construction);
        }

        SUBCASE("error missing destination")
        {
            task.source("/source/path").compile();

            CHECK(task.error() == vfs::error_code::task_bad_construction);
        }
    }
}
