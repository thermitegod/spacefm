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

#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>

#include <doctest/doctest.h>

#include <ztd/ztd.hxx>

#include "vfs/task-manager.hxx"

struct test_sync
{
    std::mutex mutex;
    std::condition_variable cv;
    std::size_t completed = 0;
    std::size_t error = 0;
    std::vector<std::uint64_t> finished;

    void
    reset() noexcept
    {
        std::scoped_lock lock(mutex);
        completed = 0;
        error = 0;
        finished.clear();
    }

    void
    wait() noexcept
    {
        wait_for(1);
    }

    void
    wait_for(std::size_t expected) noexcept
    {
        std::unique_lock lock(mutex);
        cv.wait(lock, [this, expected] { return completed >= expected || error != 0; });
    }

    void
    notify_success(std::uint64_t id) noexcept
    {
        std::scoped_lock lock(mutex);
        completed++;
        finished.push_back(id);
        cv.notify_all();
    }

    void
    notify_error(const vfs::task_error& e) noexcept
    {
        (void)e;
        std::scoped_lock lock(mutex);
        error++;
        cv.notify_all();
    }
};

static std::size_t
count_files(const std::filesystem::path& path, bool recursive = false) noexcept
{
    size_t count = 0;
    if (recursive)
    {
        for (const auto& _ : std::filesystem::recursive_directory_iterator(path))
        {
            count += 1;
        }
    }
    else
    {
        for (const auto& _ : std::filesystem::directory_iterator(path))
        {
            count += 1;
        }
    }
    return count;
}

TEST_SUITE("vfs::task_manager" * doctest::description(""))
{
    const auto root = std::filesystem::temp_directory_path() / PACKAGE_NAME / "task-manager";
    // const auto root = std::filesystem::path() / "/tmp" / PACKAGE_NAME / "task-manager";

    TEST_CASE("vfs::create_file_task")
    {
        const auto test_path = root / "create_file_task";
        if (std::filesystem::exists(test_path))
        {
            std::filesystem::remove_all(test_path);
        }
        std::filesystem::create_directories(test_path);

        test_sync sync;

        auto manager = vfs::task_manager::create();
        manager->signal_task_finished().connect([&](std::uint64_t task_id)
                                                { sync.notify_success(task_id); });
        manager->signal_task_error().connect([&](const vfs::task_error& error)
                                             { sync.notify_error(error); });
        manager->signal_task_collision().connect(
            [](const vfs::task_collision& c)
            { c.resolved(c.task_id, vfs::collision_resolve::skip, {}); });

        /////////////////////////////////////////////////////

        SUBCASE("create_file_task")
        {
            const auto path = test_path / "test.txt";
            manager->add(vfs::create_file_task{.path = path});
            sync.wait();

            CHECK(manager->empty());

            CHECK(std::filesystem::exists(path));
            CHECK(std::filesystem::is_regular_file(path));
        }

        SUBCASE("create_file_task nested")
        {
            const auto nested_path = test_path / "nested/test.txt";
            manager->add(vfs::create_file_task{.path = nested_path});
            sync.wait();

            CHECK(manager->empty());

            CHECK(std::filesystem::exists(nested_path));
            CHECK(std::filesystem::is_regular_file(nested_path));
        }

        SUBCASE("create_file_task error")
        {
            const auto path = test_path / "test.txt";

            std::ofstream(path).close();

            manager->add(vfs::create_file_task{.path = path});
            sync.wait();

            CHECK_EQ(sync.error, 1);
            CHECK_EQ(sync.completed, 0);
        }

        SUBCASE("create_file_task loop")
        {
            std::size_t loop = 1000;

            for (const auto i : std::views::iota(0uz, loop))
            {
                const auto path = test_path / std::format("{}.txt", i);
                manager->add(vfs::create_file_task{.path = path});
            }
            sync.wait_for(loop);

            CHECK(manager->empty());

            CHECK_EQ(count_files(test_path), loop);
            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, loop);
        }

        SUBCASE("create_file_task nested loop")
        {
            std::size_t loop = 1000;

            const auto nested_path = test_path / "nested";
            for (const auto i : std::views::iota(0uz, loop))
            {
                const auto path = nested_path / std::format("{}.txt", i);
                manager->add(vfs::create_file_task{.path = path});
            }
            sync.wait_for(loop);

            CHECK(manager->empty());

            CHECK_EQ(count_files(nested_path), loop);
            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, loop);
        }

        if (std::filesystem::exists(test_path))
        {
            std::filesystem::remove_all(test_path);
        }
    }

    TEST_CASE("vfs::create_directory_task")
    {
        const auto test_path = root / "create_directory_task";
        if (std::filesystem::exists(test_path))
        {
            std::filesystem::remove_all(test_path);
        }
        std::filesystem::create_directories(test_path);

        test_sync sync;

        auto manager = vfs::task_manager::create();
        manager->signal_task_finished().connect([&](std::uint64_t task_id)
                                                { sync.notify_success(task_id); });
        manager->signal_task_error().connect([&](const vfs::task_error& error)
                                             { sync.notify_error(error); });
        manager->signal_task_collision().connect(
            [](const vfs::task_collision& c)
            { c.resolved(c.task_id, vfs::collision_resolve::skip, {}); });

        /////////////////////////////////////////////////////

        SUBCASE("create_directory_task")
        {
            const auto path = test_path / "nested/directory";
            manager->add(vfs::create_directory_task{.path = path});
            sync.wait();

            CHECK(manager->empty());

            CHECK(std::filesystem::exists(path));
            CHECK(std::filesystem::is_directory(path));
        }

        SUBCASE("create_directory_task loop")
        {
            std::size_t loop = 1000;
            const auto nested_path = test_path / "nested/directory/loop";
            for (const auto i : std::views::iota(0uz, loop))
            {
                const auto path = nested_path / std::format("{}", i);
                manager->add(vfs::create_directory_task{.path = path});
            }
            sync.wait_for(loop);

            CHECK(manager->empty());

            CHECK_EQ(count_files(nested_path), loop);
            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, loop);
        }

        if (std::filesystem::exists(test_path))
        {
            std::filesystem::remove_all(test_path);
        }
    }
}
