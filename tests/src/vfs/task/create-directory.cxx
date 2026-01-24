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
    TEST_CASE("create directory")
    {
        vfs::create_directory task;

        SUBCASE("basic")
        {
            task.path("/new/path").compile();

            CHECK_EQ(*task.dump(), "mkdir  \"/new/path\"");
        }

        SUBCASE("basic parents")
        {
            task.create_parents().path("/new/path").compile();

            CHECK_EQ(*task.dump(), "mkdir --parents \"/new/path\"");
        }

        SUBCASE("error empty path")
        {
            task.create_parents().path("").compile();

            CHECK(task.error() == vfs::error_code::task_empty_path);
        }

        SUBCASE("error preserve-root")
        {
            task.create_parents().path("/").compile();

            CHECK(task.error() == vfs::error_code::task_root_preserve);
        }

        SUBCASE("error bad construction")
        {
            task.create_parents().compile();

            CHECK(task.error() == vfs::error_code::task_bad_construction);
        }
    }
}
