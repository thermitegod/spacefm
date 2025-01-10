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

#include <filesystem>

#include <doctest/doctest.h>

#include "spacefm/vfs/utils/file-ops.hxx"

const std::filesystem::path test_data_path = TEST_DATA_PATH;

TEST_SUITE("vfs::utils file-ops" * doctest::description(""))
{
    TEST_CASE("vfs::utils::read_file")
    {
        REQUIRE(std::filesystem::exists(test_data_path));

        SUBCASE("")
        {
            // TODO
        }
    }

    TEST_CASE("vfs::utils::read_file_partial")
    {
        REQUIRE(std::filesystem::exists(test_data_path));

        SUBCASE("")
        {
            // TODO
        }
    }

    TEST_CASE("vfs::utils::write_file")
    {
        REQUIRE(std::filesystem::exists(test_data_path));

        SUBCASE("")
        {
            // TODO
        }
    }
}
