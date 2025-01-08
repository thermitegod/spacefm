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

#include <filesystem>

#include "spacefm/vfs/utils/vfs-utils.hxx"

const std::filesystem::path test_data_path = TEST_DATA_PATH;

TEST_SUITE("vfs::utils" * doctest::description(""))
{
    TEST_CASE("vfs::utils::unique_path")
    {
        REQUIRE(std::filesystem::exists(test_data_path));

        SUBCASE("file missing extension")
        {
            const auto path = test_data_path / "vfs/utils/unique_name/file-extension-missing";
            const std::filesystem::path filename = "test";

            const auto result_wanted = path / "test-copy11";

            const auto result = vfs::utils::unique_path(path, filename, "-copy");

            REQUIRE_EQ(result, result_wanted);
        }

        SUBCASE("file multiple extension")
        {
            const auto path = test_data_path / "vfs/utils/unique_name/file-extension-multiple";
            const std::filesystem::path filename = "test.tar.gz";

            const auto result_wanted = path / "test-copy11.tar.gz";

            const auto result = vfs::utils::unique_path(path, filename, "-copy");

            REQUIRE_EQ(result, result_wanted);
        }

        SUBCASE("file single extension")
        {
            const auto path = test_data_path / "vfs/utils/unique_name/file-extension-single";
            const std::filesystem::path filename = "test.txt";

            const auto result_wanted = path / "test-copy11.txt";

            const auto result = vfs::utils::unique_path(path, filename, "-copy");

            REQUIRE_EQ(result, result_wanted);
        }

        SUBCASE("directory")
        {
            const auto path = test_data_path / "vfs/utils/unique_name/directory";
            const std::filesystem::path filename = "test";

            const auto result_wanted = path / "test-copy11";

            const auto result = vfs::utils::unique_path(path, filename, "-copy");

            REQUIRE_EQ(result, result_wanted);
        }
    }

    TEST_CASE("vfs::utils::split_basename_extension")
    {
        REQUIRE(std::filesystem::exists(test_data_path));

        SUBCASE("empty")
        {
            const auto result = vfs::utils::split_basename_extension("");

            REQUIRE_EQ(result.basename, "");
            REQUIRE_EQ(result.extension, "");
            REQUIRE_EQ(result.is_multipart_extension, false);
        }

        SUBCASE("missing extension")
        {
            const auto result = vfs::utils::split_basename_extension("test");

            REQUIRE_EQ(result.basename, "test");
            REQUIRE_EQ(result.extension, "");
            REQUIRE_EQ(result.is_multipart_extension, false);
        }

        SUBCASE("multiple extension")
        {
            const auto result = vfs::utils::split_basename_extension("test.tar.gz");

            REQUIRE_EQ(result.basename, "test");
            REQUIRE_EQ(result.extension, ".tar.gz");
            REQUIRE_EQ(result.is_multipart_extension, true);
        }

        SUBCASE("single extension")
        {
            const auto result = vfs::utils::split_basename_extension("test.txt");

            REQUIRE_EQ(result.basename, "test");
            REQUIRE_EQ(result.extension, ".txt");
            REQUIRE_EQ(result.is_multipart_extension, false);
        }
    }
}
