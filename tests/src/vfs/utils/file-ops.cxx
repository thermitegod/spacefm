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
#include <system_error>

#include <doctest/doctest.h>

#include "vfs/utils/file-ops.hxx"

using namespace std::string_literals;

TEST_SUITE("vfs::utils file-ops" * doctest::description(""))
{
    const auto root = std::filesystem::temp_directory_path() / PACKAGE_NAME / "file-ops";

    TEST_CASE("vfs::utils::read_file")
    {
        const auto test_path = root / "read";
        if (std::filesystem::exists(test_path))
        {
            std::filesystem::remove_all(test_path);
        }
        std::filesystem::create_directories(test_path);
        REQUIRE(std::filesystem::exists(test_path));

        auto result = vfs::utils::write_file(test_path / "test.txt", "data"s);
        REQUIRE(result == std::error_code{});

        SUBCASE("good read")
        {
            auto data = vfs::utils::read_file(test_path / "test.txt");
            REQUIRE(data);
            CHECK_EQ(*data, "data"s);
        }

        SUBCASE("good read /proc")
        {
            auto data = vfs::utils::read_file("/proc/version");
            REQUIRE(data);
            CHECK_GE(data.value().size(), 10uz);
        }

        SUBCASE("read /proc max size")
        {
            auto data = vfs::utils::read_file("/proc/version", 1uz);
            REQUIRE(!data);
            CHECK_EQ(data.error(), vfs::error_code::file_too_large);
        }

        SUBCASE("file max size")
        {
            auto data = vfs::utils::read_file(test_path / "test.txt", 1uz);
            REQUIRE(!data);
            CHECK_EQ(data.error(), vfs::error_code::file_too_large);
        }

        SUBCASE("file open failure")
        {
            auto data = vfs::utils::read_file(test_path / "bad_path" / "test.txt");
            REQUIRE(!data);
            CHECK_EQ(data.error(), vfs::error_code::file_open_failure);
        }

        if (std::filesystem::exists(test_path))
        {
            std::filesystem::remove_all(test_path);
        }
    }

    TEST_CASE("vfs::utils::write_file")
    {
        const auto test_path = root / "write";
        if (std::filesystem::exists(test_path))
        {
            std::filesystem::remove_all(test_path);
        }
        std::filesystem::create_directories(test_path);
        REQUIRE(std::filesystem::exists(test_path));

        SUBCASE("good write")
        {
            auto result = vfs::utils::write_file(test_path / "test.txt", "data"s);
            CHECK_EQ(result, std::error_code{});
        }

        SUBCASE("file open failure")
        {
            auto result = vfs::utils::write_file(test_path / "bad_path" / "test.txt", "data"s);
            CHECK_EQ(result, vfs::error_code::file_open_failure);
        }

        if (std::filesystem::exists(test_path))
        {
            std::filesystem::remove_all(test_path);
        }
    }
}
