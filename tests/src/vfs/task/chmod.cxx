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
    TEST_CASE("chmod")
    {
        vfs::chmod task;

        SUBCASE("basic owner_all")
        {
            task.mode(std::filesystem::perms::owner_all).path("/does-not-exist").compile();

            CHECK_EQ(*task.dump(), "chmod --preserve-root  700 \"/does-not-exist\"");
        }

        SUBCASE("basic group_all")
        {
            task.mode(std::filesystem::perms::group_all).path("/does-not-exist").compile();

            CHECK_EQ(*task.dump(), "chmod --preserve-root  070 \"/does-not-exist\"");
        }

        SUBCASE("basic others_all")
        {
            task.mode(std::filesystem::perms::others_all).path("/does-not-exist").compile();

            CHECK_EQ(*task.dump(), "chmod --preserve-root  007 \"/does-not-exist\"");
        }

        SUBCASE("perms all")
        {
            task.mode(std::filesystem::perms::owner_all | std::filesystem::perms::group_all |
                      std::filesystem::perms::others_all)
                .path("/does-not-exist")
                .compile();

            CHECK_EQ(*task.dump(), "chmod --preserve-root  777 \"/does-not-exist\"");
        }

        SUBCASE("basic recursive owner_all")
        {
            task.recursive()
                .mode(std::filesystem::perms::owner_all)
                .path("/does-not-exist")
                .compile();

            CHECK_EQ(*task.dump(), "chmod --preserve-root --recursive 700 \"/does-not-exist\"");
        }

        SUBCASE("error preserve-root")
        {
            task.mode(std::filesystem::perms::owner_all).path("/").compile();

            CHECK(task.error() == vfs::error_code::task_root_preserve);
        }

        SUBCASE("error empty path")
        {
            task.mode(std::filesystem::perms::owner_all).path("").compile();

            CHECK(task.error() == vfs::error_code::task_empty_path);
        }

        SUBCASE("error missing path")
        {
            task.mode(std::filesystem::perms::owner_all).compile();

            CHECK(task.error() == vfs::error_code::task_bad_construction);
        }
    }
}
