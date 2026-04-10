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

#include "vfs/utils/utils.hxx"

#include "utils.hxx"

TEST_SUITE("vfs::utils" * doctest::description(""))
{
    const auto root = std::filesystem::temp_directory_path() / PACKAGE_NAME / "vfs-utils";

    TEST_CASE("vfs::utils::unique_path")
    {
        const auto test_path = root / "unique_path";
        if (std::filesystem::exists(test_path))
        {
            std::filesystem::remove_all(test_path);
        }
        std::filesystem::create_directories(test_path);
        REQUIRE(std::filesystem::exists(test_path));

        SUBCASE("file no collision")
        {
            const auto result = vfs::utils::unique_path(test_path, "test");
            CHECK_EQ(result, test_path / "test");
        }

        SUBCASE("file no collision with tag")
        {
            const auto result = vfs::utils::unique_path(test_path, "test", "-copy");
            CHECK_EQ(result, test_path / "test");
        }

        SUBCASE("file single extension no collision")
        {
            const auto result = vfs::utils::unique_path(test_path, "test.txt");
            CHECK_EQ(result, test_path / "test.txt");
        }

        SUBCASE("file single extension no collision with tag")
        {
            const auto result = vfs::utils::unique_path(test_path, "test.txt", "-copy");
            CHECK_EQ(result, test_path / "test.txt");
        }

        SUBCASE("file multiple extension no collision")
        {
            const auto result = vfs::utils::unique_path(test_path, "test.tar.gz");
            CHECK_EQ(result, test_path / "test.tar.gz");
        }

        SUBCASE("file multiple extension no collision with tag")
        {
            const auto result = vfs::utils::unique_path(test_path, "test.tar.gz", "-copy");
            CHECK_EQ(result, test_path / "test.tar.gz");
        }

        SUBCASE("file no extension")
        {
            create_file(test_path / "test");
            ////
            create_file(test_path / "test1");
            create_file(test_path / "test2");
            create_file(test_path / "test3");
            create_file(test_path / "test4");
            create_file(test_path / "test5");
            create_file(test_path / "test6");
            create_file(test_path / "test7");
            create_file(test_path / "test8");
            create_file(test_path / "test9");
            create_file(test_path / "test10");

            const auto result = vfs::utils::unique_path(test_path, "test");
            CHECK_EQ(result, test_path / "test11");
        }

        SUBCASE("file no extension with tag")
        {
            create_file(test_path / "test");
            ////
            create_file(test_path / "test-copy1");
            create_file(test_path / "test-copy2");
            create_file(test_path / "test-copy3");
            create_file(test_path / "test-copy4");
            create_file(test_path / "test-copy5");
            create_file(test_path / "test-copy6");
            create_file(test_path / "test-copy7");
            create_file(test_path / "test-copy8");
            create_file(test_path / "test-copy9");
            create_file(test_path / "test-copy10");

            const auto result = vfs::utils::unique_path(test_path, "test", "-copy");
            CHECK_EQ(result, test_path / "test-copy11");
        }

        SUBCASE("file single extension")
        {
            create_file(test_path / "test.txt");
            ////
            create_file(test_path / "test1.txt");
            create_file(test_path / "test2.txt");
            create_file(test_path / "test3.txt");
            create_file(test_path / "test4.txt");
            create_file(test_path / "test5.txt");
            create_file(test_path / "test6.txt");
            create_file(test_path / "test7.txt");
            create_file(test_path / "test8.txt");
            create_file(test_path / "test9.txt");
            create_file(test_path / "test10.txt");

            const auto result = vfs::utils::unique_path(test_path, "test.txt");
            CHECK_EQ(result, test_path / "test11.txt");
        }

        SUBCASE("file single extension with tag")
        {
            create_file(test_path / "test.txt");
            ////
            create_file(test_path / "test-copy1.txt");
            create_file(test_path / "test-copy2.txt");
            create_file(test_path / "test-copy3.txt");
            create_file(test_path / "test-copy4.txt");
            create_file(test_path / "test-copy5.txt");
            create_file(test_path / "test-copy6.txt");
            create_file(test_path / "test-copy7.txt");
            create_file(test_path / "test-copy8.txt");
            create_file(test_path / "test-copy9.txt");
            create_file(test_path / "test-copy10.txt");

            const auto result = vfs::utils::unique_path(test_path, "test.txt", "-copy");
            CHECK_EQ(result, test_path / "test-copy11.txt");
        }

        SUBCASE("file multiple extension")
        {
            create_file(test_path / "test.tar.gz");
            ////
            create_file(test_path / "test1.tar.gz");
            create_file(test_path / "test2.tar.gz");
            create_file(test_path / "test3.tar.gz");
            create_file(test_path / "test4.tar.gz");
            create_file(test_path / "test5.tar.gz");
            create_file(test_path / "test6.tar.gz");
            create_file(test_path / "test7.tar.gz");
            create_file(test_path / "test8.tar.gz");
            create_file(test_path / "test9.tar.gz");
            create_file(test_path / "test10.tar.gz");

            const auto result = vfs::utils::unique_path(test_path, "test.tar.gz");
            CHECK_EQ(result, test_path / "test11.tar.gz");
        }

        SUBCASE("file multiple extension with tag")
        {
            create_file(test_path / "test.tar.gz");
            ////
            create_file(test_path / "test-copy1.tar.gz");
            create_file(test_path / "test-copy2.tar.gz");
            create_file(test_path / "test-copy3.tar.gz");
            create_file(test_path / "test-copy4.tar.gz");
            create_file(test_path / "test-copy5.tar.gz");
            create_file(test_path / "test-copy6.tar.gz");
            create_file(test_path / "test-copy7.tar.gz");
            create_file(test_path / "test-copy8.tar.gz");
            create_file(test_path / "test-copy9.tar.gz");
            create_file(test_path / "test-copy10.tar.gz");

            const auto result = vfs::utils::unique_path(test_path, "test.tar.gz", "-copy");
            CHECK_EQ(result, test_path / "test-copy11.tar.gz");
        }

        SUBCASE("directory")
        {
            std::filesystem::create_directories(test_path / "test");
            ////
            std::filesystem::create_directories(test_path / "test1");
            std::filesystem::create_directories(test_path / "test2");
            std::filesystem::create_directories(test_path / "test3");
            std::filesystem::create_directories(test_path / "test4");
            std::filesystem::create_directories(test_path / "test5");
            std::filesystem::create_directories(test_path / "test6");
            std::filesystem::create_directories(test_path / "test7");
            std::filesystem::create_directories(test_path / "test8");
            std::filesystem::create_directories(test_path / "test9");
            std::filesystem::create_directories(test_path / "test10");

            const auto result = vfs::utils::unique_path(test_path, "test");
            CHECK_EQ(result, test_path / "test11");
        }

        SUBCASE("directory with tag")
        {
            std::filesystem::create_directories(test_path / "test");
            ////
            std::filesystem::create_directories(test_path / "test-copy1");
            std::filesystem::create_directories(test_path / "test-copy2");
            std::filesystem::create_directories(test_path / "test-copy3");
            std::filesystem::create_directories(test_path / "test-copy4");
            std::filesystem::create_directories(test_path / "test-copy5");
            std::filesystem::create_directories(test_path / "test-copy6");
            std::filesystem::create_directories(test_path / "test-copy7");
            std::filesystem::create_directories(test_path / "test-copy8");
            std::filesystem::create_directories(test_path / "test-copy9");
            std::filesystem::create_directories(test_path / "test-copy10");

            const auto result = vfs::utils::unique_path(test_path, "test", "-copy");
            CHECK_EQ(result, test_path / "test-copy11");
        }

        SUBCASE("mixed")
        {
            create_file(test_path / "test");
            ////
            create_file(test_path / "test1");
            std::filesystem::create_directories(test_path / "test2");
            create_file(test_path / "test3");
            std::filesystem::create_directories(test_path / "test4");
            create_file(test_path / "test5");
            std::filesystem::create_directories(test_path / "test6");
            create_file(test_path / "test7");
            std::filesystem::create_directories(test_path / "test8");
            create_file(test_path / "test9");
            std::filesystem::create_directories(test_path / "test10");

            const auto result = vfs::utils::unique_path(test_path, "test");
            CHECK_EQ(result, test_path / "test11");
        }

        SUBCASE("mixed with tag")
        {
            create_file(test_path / "test");
            ////
            create_file(test_path / "test-copy1");
            std::filesystem::create_directories(test_path / "test-copy2");
            create_file(test_path / "test-copy3");
            std::filesystem::create_directories(test_path / "test-copy4");
            create_file(test_path / "test-copy5");
            std::filesystem::create_directories(test_path / "test-copy6");
            create_file(test_path / "test-copy7");
            std::filesystem::create_directories(test_path / "test-copy8");
            create_file(test_path / "test-copy9");
            std::filesystem::create_directories(test_path / "test-copy10");

            const auto result = vfs::utils::unique_path(test_path, "test", "-copy");
            CHECK_EQ(result, test_path / "test-copy11");
        }

        if (std::filesystem::exists(test_path))
        {
            std::filesystem::remove_all(test_path);
        }
    }

    TEST_CASE("vfs::utils::filename_stem_and_extension")
    {
        SUBCASE("empty")
        {
            const auto [stem, extension] = vfs::utils::filename_stem_and_extension("");

            CHECK_EQ(stem, "");
            CHECK_EQ(extension, "");
        }

        SUBCASE("no extension")
        {
            const auto [stem, extension] = vfs::utils::filename_stem_and_extension("test");

            CHECK_EQ(stem, "test");
            CHECK_EQ(extension, "");
        }

        SUBCASE("multiple extension")
        {
            const auto [stem, extension] = vfs::utils::filename_stem_and_extension("test.tar.gz");

            CHECK_EQ(stem, "test");
            CHECK_EQ(extension, ".tar.gz");
        }

        SUBCASE("single extension")
        {
            const auto [stem, extension] = vfs::utils::filename_stem_and_extension("test.txt");

            CHECK_EQ(stem, "test");
            CHECK_EQ(extension, ".txt");
        }

        SUBCASE("hidden no extension")
        {
            const auto [stem, extension] = vfs::utils::filename_stem_and_extension(".hidden");

            CHECK_EQ(stem, ".hidden");
            CHECK_EQ(extension, "");
        }

        SUBCASE("hidden single extension")
        {
            const auto [stem, extension] = vfs::utils::filename_stem_and_extension(".hidden.txt");

            CHECK_EQ(stem, ".hidden");
            CHECK_EQ(extension, ".txt");
        }

        SUBCASE("hidden multiple extension")
        {
            const auto [stem, extension] =
                vfs::utils::filename_stem_and_extension(".hidden.tar.zst");

            CHECK_EQ(stem, ".hidden");
            CHECK_EQ(extension, ".tar.zst");
        }
    }
}
