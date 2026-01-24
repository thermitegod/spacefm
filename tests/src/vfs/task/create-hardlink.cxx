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
    TEST_CASE("create hardlink")
    {
        vfs::create_hardlink task;

        SUBCASE("basic")
        {
            task.target("/existing/file.txt").name("/new/hardlink.txt").compile();

            CHECK_EQ(*task.dump(), "ln  \"/existing/file.txt\" \"/new/hardlink.txt\"");
        }

        SUBCASE("basic force")
        {
            task.force().target("/existing/file.txt").name("/new/hardlink.txt").compile();

            CHECK_EQ(*task.dump(), "ln --force \"/existing/file.txt\" \"/new/hardlink.txt\"");
        }

        SUBCASE("error empty target")
        {
            task.target("").name("/new/hardlink.txt").compile();

            CHECK(task.error() == vfs::error_code::task_empty_path);
        }

        SUBCASE("error empty name")
        {
            task.target("/existing/file.txt").name("").compile();

            CHECK(task.error() == vfs::error_code::task_empty_path);
        }

        SUBCASE("error preserve-root target")

        {
            task.target("/").name("/new/hardlink.txt").compile();

            CHECK(task.error() == vfs::error_code::task_root_preserve);
        }

        SUBCASE("error preserve-root name")
        {
            task.target("/existing/file.txt").name("/").compile();

            CHECK(task.error() == vfs::error_code::task_root_preserve);
        }

        SUBCASE("error missing target")
        {
            task.name("/new/hardlink.txt").compile();

            CHECK(task.error() == vfs::error_code::task_bad_construction);
        }

        SUBCASE("error missing name")
        {
            task.target("/existing/file.txt").compile();

            CHECK(task.error() == vfs::error_code::task_bad_construction);
        }
    }
}
