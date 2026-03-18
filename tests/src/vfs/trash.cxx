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

#include <chrono>
#include <filesystem>
#include <fstream>

#include <doctest/doctest.h>

#include <glibmm.h>

#include "vfs/trash-can.hxx"

TEST_SUITE("vfs::trash" * doctest::description(""))
{
    const auto root = std::filesystem::temp_directory_path() / PACKAGE_NAME / "trash";
    // const auto root = std::filesystem::path() / "/tmp" / PACKAGE_NAME / "trash";

    TEST_CASE("write")
    {
        const auto test_path = root / "write";
        std::filesystem::create_directories(test_path);

        SUBCASE("good write")
        {
            const auto path = test_path / "write.trashinfo";

            vfs::trashinfo info{.path = "/home/user/file.txt",
                                .time = std::chrono::system_clock::now()};

            auto ec = vfs::trashinfo_write(path, info);

            CHECK_FALSE(bool(ec));
            CHECK(std::filesystem::exists(path));
        }

        if (std::filesystem::exists(test_path))
        {
            std::filesystem::remove_all(test_path);
        }
    }

    TEST_CASE("read")
    {
        const auto test_path = root / "read";
        std::filesystem::create_directories(test_path);

        SUBCASE("good read")
        {
            const auto path = test_path / "read.trashinfo";

            vfs::trashinfo info{.path = "/home/user/file.txt",
                                .time = std::chrono::system_clock::now()};

            vfs::trashinfo_write(path, info);

            auto result = vfs::trashinfo_read(path);

            REQUIRE(result.has_value());
            CHECK(result->path == info.path);
            CHECK(result->time == info.time);
        }

        SUBCASE("file extension wrong")
        {
            const auto path = test_path / "wrong.txt";

            const auto kf = Glib::KeyFile::create();
            kf->set_string("Trash Info", "Path", "/home/user/file.txt");
            kf->set_string("Trash Info", "DeletionDate", "2000-01-01T00:00:00");
            kf->save_to_file(path);

            auto result = vfs::trashinfo_read(path);

            CHECK_FALSE(result.has_value());
        }

        SUBCASE("file extension missing")
        {
            const auto path = test_path / "missing";

            const auto kf = Glib::KeyFile::create();
            kf->set_string("Trash Info", "Path", "/home/user/file.txt");
            kf->set_string("Trash Info", "DeletionDate", "2000-01-01T00:00:00");
            kf->save_to_file(path);

            auto result = vfs::trashinfo_read(path);

            CHECK_FALSE(result.has_value());
        }

        SUBCASE("trashinfo missing path")
        {
            const auto path = test_path / "missing-path.trashinfo";

            const auto kf = Glib::KeyFile::create();
            // kf->set_string("Trash Info", "Path", "/home/user/file.txt");
            kf->set_string("Trash Info", "DeletionDate", "2000-01-01T00:00:00");
            kf->save_to_file(path);

            auto result = vfs::trashinfo_read(path);

            CHECK_FALSE(result.has_value());
        }

        SUBCASE("trashinfo missing date")
        {
            const auto path = test_path / "missing-date.trashinfo";

            const auto kf = Glib::KeyFile::create();
            kf->set_string("Trash Info", "Path", "/home/user/file.txt");
            // kf->set_string("Trash Info", "DeletionDate", "2000-01-01T00:00:00");
            kf->save_to_file(path);

            auto result = vfs::trashinfo_read(path);

            CHECK_FALSE(result.has_value());
        }

        SUBCASE("trashinfo missing all")
        {
            const auto path = test_path / "missing-all.trashinfo";

            const auto kf = Glib::KeyFile::create();
            // kf->set_string("Trash Info", "Path", "/home/user/file.txt");
            // kf->set_string("Trash Info", "DeletionDate", "2000-01-01T00:00:00");
            kf->save_to_file(path);

            auto result = vfs::trashinfo_read(path);

            CHECK_FALSE(result.has_value());
        }

        SUBCASE("trashinfo empty")
        {
            const auto path = test_path / "empty";

            std::ofstream(path).close();

            auto result = vfs::trashinfo_read(path);

            CHECK_FALSE(result.has_value());
        }

        if (std::filesystem::exists(test_path))
        {
            std::filesystem::remove_all(test_path);
        }
    }
}
