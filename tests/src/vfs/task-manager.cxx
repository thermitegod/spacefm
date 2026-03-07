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
#include <print>

#include <doctest/doctest.h>

#include <ztd/ztd.hxx>

#include "vfs/task-manager.hxx"

#include "vfs/utils/file-ops.hxx"

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
        // std::println("{}", e.message);
        cv.notify_all();
    }
};

static void
create_file(const std::filesystem::path& path, std::string_view content = "data") noexcept
{
    std::filesystem::create_directories(path.parent_path());

    auto result = vfs::utils::write_file(path, content);
    ztd::panic_if(bool(result), "Test Suite bad write");
}

static std::string
read_file(const std::filesystem::path& path) noexcept
{
    auto data = vfs::utils::read_file(path);
    ztd::panic_if(!data, "Test Suite bad read");
    return *data;
}

static std::size_t
count_files(const std::filesystem::path& path, bool recursive = false) noexcept
{
    ztd::panic_if(!std::filesystem::exists(path),
                  "Test Suite missing required path: {}",
                  path.string());

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

        SUBCASE("create")
        {
            const auto path = test_path / "test.txt";
            manager->add(vfs::create_file_task{.path = path});
            sync.wait();

            CHECK(manager->empty());

            CHECK(std::filesystem::exists(path));
            CHECK(std::filesystem::is_regular_file(path));
        }

        SUBCASE("create nested")
        {
            const auto nested_path = test_path / "nested/test.txt";
            manager->add(vfs::create_file_task{.path = nested_path});
            sync.wait();

            CHECK(manager->empty());

            CHECK(std::filesystem::exists(nested_path));
            CHECK(std::filesystem::is_regular_file(nested_path));
        }

        SUBCASE("create fail on existing error")
        {
            const auto path = test_path / "test.txt";

            std::ofstream(path).close();

            manager->add(vfs::create_file_task{.path = path});
            sync.wait();

            CHECK_EQ(sync.error, 1);
            CHECK_EQ(sync.completed, 0);
        }

        SUBCASE("create loop")
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

        SUBCASE("create nested loop")
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

        SUBCASE("create")
        {
            const auto path = test_path / "test";
            manager->add(vfs::create_directory_task{.path = path});
            sync.wait();

            CHECK(manager->empty());

            CHECK(std::filesystem::exists(path));
            CHECK(std::filesystem::is_directory(path));
        }

        SUBCASE("create nested")
        {
            const auto path = test_path / "nested/test";
            manager->add(vfs::create_directory_task{.path = path});
            sync.wait();

            CHECK(manager->empty());

            CHECK(std::filesystem::exists(path));
            CHECK(std::filesystem::is_directory(path));
        }

        SUBCASE("create loop")
        {
            std::size_t loop = 1000;

            for (const auto i : std::views::iota(0uz, loop))
            {
                const auto path = test_path / std::format("{}", i);
                manager->add(vfs::create_directory_task{.path = path});
            }
            sync.wait_for(loop);

            CHECK(manager->empty());

            CHECK_EQ(count_files(test_path), loop);
            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, loop);
        }

        SUBCASE("create nested loop")
        {
            std::size_t loop = 1000;

            const auto nested_path = test_path / "nested/loop";
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

    TEST_CASE("vfs::create_symlink_task")
    {
        const auto test_path = root / "create_symlink_task";
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

        auto target = test_path / "target.txt";
        auto link = test_path / "link.txt";

        std::ofstream(target).close();
        CHECK(std::filesystem::exists(target));
        CHECK(std::filesystem::is_regular_file(target));

        SUBCASE("create")
        {
            manager->add(vfs::create_symlink_task{.target = target, .name = link, .force = false});
            sync.wait();

            CHECK(manager->empty());

            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, 1);

            CHECK(std::filesystem::is_symlink(link));
            CHECK(std::filesystem::read_symlink(link) == target);
        }

        SUBCASE("create fail on existing")
        {
            std::ofstream(link).close();
            CHECK(std::filesystem::exists(link));
            CHECK(std::filesystem::is_regular_file(link));

            manager->add(vfs::create_symlink_task{.target = target, .name = link, .force = false});
            sync.wait();

            CHECK(manager->empty());

            CHECK_EQ(sync.error, 1);
            CHECK_EQ(sync.completed, 0);
        }

        SUBCASE("create loop")
        {
            std::size_t loop = 1000;

            for (const auto i : std::views::iota(0uz, loop))
            {
                const auto path = test_path / std::format("{}", i);
                manager->add(
                    vfs::create_symlink_task{.target = target, .name = path, .force = false});
            }
            sync.wait_for(loop);

            CHECK(manager->empty());

            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, loop);
        }

        SUBCASE("create force")
        {
            manager->add(vfs::create_symlink_task{.target = target, .name = link, .force = true});
            sync.wait();

            CHECK(manager->empty());

            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, 1);

            CHECK(std::filesystem::is_symlink(link));
            CHECK(std::filesystem::read_symlink(link) == target);
        }

        SUBCASE("create force on existing")
        {
            std::ofstream(link).close();
            CHECK(std::filesystem::exists(link));
            CHECK(std::filesystem::is_regular_file(link));

            manager->add(vfs::create_symlink_task{.target = target, .name = link, .force = true});
            sync.wait();

            CHECK(manager->empty());

            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, 1);

            CHECK(std::filesystem::is_symlink(link));
            CHECK(std::filesystem::read_symlink(link) == target);
        }

        if (std::filesystem::exists(test_path))
        {
            std::filesystem::remove_all(test_path);
        }
    }

    TEST_CASE("vfs::remove_task")
    {
        const auto test_path = root / "remove_task";
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

        SUBCASE("remove file")
        {
            const auto path = test_path / "file.txt";
            std::ofstream(path).close();
            CHECK(std::filesystem::exists(path));
            CHECK(std::filesystem::is_regular_file(path));

            manager->add(vfs::remove_task{.path = path});
            sync.wait();

            CHECK(manager->empty());

            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, 1);

            CHECK_FALSE(std::filesystem::exists(path));
        }

        SUBCASE("remove empty directory")
        {
            const auto path = test_path / "directory";
            std::filesystem::create_directories(path);
            CHECK(std::filesystem::exists(path));
            CHECK(std::filesystem::is_directory(path));

            manager->add(vfs::remove_task{.path = path});
            sync.wait();

            CHECK(manager->empty());

            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, 1);

            CHECK_FALSE(std::filesystem::exists(path));
        }

        SUBCASE("remove recursive")
        {
            const auto path = test_path / "directory";
            std::filesystem::create_directories(path);
            CHECK(std::filesystem::exists(path));
            CHECK(std::filesystem::is_directory(path));
            for (const auto i : std::views::iota(0uz, 100uz))
            {
                const auto file = path / std::format("{}.txt", i);
                std::ofstream(file).close();
                CHECK(std::filesystem::exists(file));
                CHECK(std::filesystem::is_regular_file(file));
            }

            ///////////////////////////

            manager->add(vfs::remove_task{.path = path});
            sync.wait();

            CHECK(manager->empty());

            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, 1);

            CHECK_FALSE(std::filesystem::exists(path));
        }

        SUBCASE("remove error does not exist")
        {
            manager->add(vfs::remove_task{.path = "bad.txt"});
            sync.wait();

            CHECK(manager->empty());

            CHECK_EQ(sync.error, 1);
            CHECK_EQ(sync.completed, 0);
        }

        if (std::filesystem::exists(test_path))
        {
            std::filesystem::remove_all(test_path);
        }
    }

    TEST_CASE("vfs::copy_task")
    {
        const auto test_path = root / "copy_task";
        const auto source = test_path / "src";
        const auto destination = test_path / "dest";

        if (std::filesystem::exists(test_path))
        {
            std::filesystem::remove_all(test_path);
        }

        std::filesystem::create_directories(source);
        std::filesystem::create_directories(destination);

        test_sync sync;

        auto manager = vfs::task_manager::create();
        manager->signal_task_finished().connect([&](std::uint64_t task_id)
                                                { sync.notify_success(task_id); });
        manager->signal_task_error().connect([&](const vfs::task_error& error)
                                             { sync.notify_error(error); });
        manager->signal_task_collision().connect(
            [](const vfs::task_collision& c)
            { c.resolved(c.task_id, vfs::collision_resolve::skip, {}); });

        SUBCASE("copy file")
        {
            const auto file = source / "test.txt";
            create_file(file, "data");

            manager->add(vfs::copy_task{.source = file, .destination = destination});
            sync.wait();

            CHECK(manager->empty());

            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, 1);

            const auto expected = destination / "test.txt";
            CHECK(std::filesystem::exists(expected));
            CHECK(std::filesystem::is_regular_file(expected));
            CHECK_EQ(read_file(expected), "data");
        }

        SUBCASE("copy directory empty")
        {
            const auto directory = source / "directory";
            std::filesystem::create_directories(directory);

            manager->add(vfs::copy_task{.source = directory, .destination = destination});
            sync.wait();

            CHECK(manager->empty());

            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, 1);

            CHECK(std::filesystem::exists(destination / "directory"));
            CHECK(std::filesystem::is_directory(destination / "directory"));
            CHECK_EQ(count_files(destination / "directory", true), 0);
        }

        SUBCASE("copy directory with files")
        {
            const auto directory = source / "directory";

            std::filesystem::create_directories(directory);
            std::size_t loop = 100;
            for (const auto i : std::views::iota(0uz, loop))
            {
                const auto path = directory / std::format("{}", i);
                create_file(path, std::format("{}", i));
            }

            manager->add(vfs::copy_task{.source = directory, .destination = destination});
            sync.wait();

            CHECK(manager->empty());

            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, 1);

            CHECK_EQ(count_files(destination / "directory", true), loop);

            for (const auto& entry : std::filesystem::directory_iterator(destination / "directory"))
            {
                CHECK(std::filesystem::exists(entry.path()));
                CHECK(std::filesystem::is_regular_file(entry.path()));
                CHECK_EQ(read_file(entry.path()), entry.path().filename());
            }
        }

        SUBCASE("copy nested directory")
        {
            create_file(source / "directory/a.txt");
            create_file(source / "directory/b.txt");
            create_file(source / "directory/nested/c.txt");
            create_file(source / "directory/nested/d.txt");

            manager->add(
                vfs::copy_task{.source = source / "directory", .destination = destination});
            sync.wait();

            CHECK(manager->empty());

            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, 1);

            CHECK(std::filesystem::exists(destination / "directory/a.txt"));
            CHECK(std::filesystem::exists(destination / "directory/b.txt"));
            CHECK(std::filesystem::exists(destination / "directory/nested/c.txt"));
            CHECK(std::filesystem::exists(destination / "directory/nested/d.txt"));
            CHECK_EQ(count_files(destination / "directory", true), 5);
        }

        SUBCASE("copy directory merge")
        {
            create_file(source / "directory/a.txt");
            create_file(destination / "directory/b.txt");

            manager->add(
                vfs::copy_task{.source = source / "directory", .destination = destination});
            sync.wait();

            CHECK(manager->empty());

            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, 1);

            CHECK(std::filesystem::exists(destination / "directory/a.txt"));
            CHECK(std::filesystem::exists(destination / "directory/b.txt"));
        }

        SUBCASE("copy directory merge nested")
        {
            create_file(source / "directory/a.txt");
            create_file(source / "directory/nested/a.txt");
            create_file(destination / "directory/b.txt");
            create_file(destination / "directory/nested/b.txt");

            manager->add(
                vfs::copy_task{.source = source / "directory", .destination = destination});
            sync.wait();

            CHECK(manager->empty());

            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, 1);

            CHECK(std::filesystem::exists(destination / "directory/a.txt"));
            CHECK(std::filesystem::exists(destination / "directory/nested/a.txt"));
            CHECK(std::filesystem::exists(destination / "directory/b.txt"));
            CHECK(std::filesystem::exists(destination / "directory/nested/b.txt"));
        }

        if (std::filesystem::exists(test_path))
        {
            std::filesystem::remove_all(test_path);
        }
    }

    TEST_CASE("vfs::move_task")
    {
        const auto test_path = root / "move_task";
        const auto source = test_path / "src";
        const auto destination = test_path / "dest";

        if (std::filesystem::exists(test_path))
        {
            std::filesystem::remove_all(test_path);
        }

        std::filesystem::create_directories(source);
        std::filesystem::create_directories(destination);

        test_sync sync;

        auto manager = vfs::task_manager::create();
        manager->signal_task_finished().connect([&](std::uint64_t task_id)
                                                { sync.notify_success(task_id); });
        manager->signal_task_error().connect([&](const vfs::task_error& error)
                                             { sync.notify_error(error); });
        manager->signal_task_collision().connect(
            [](const vfs::task_collision& c)
            { c.resolved(c.task_id, vfs::collision_resolve::skip, {}); });

        SUBCASE("move file")
        {
            const auto file = source / "test.txt";
            create_file(file, "data");

            manager->add(vfs::move_task{.source = file, .destination = destination});
            sync.wait();

            CHECK(manager->empty());

            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, 1);

            const auto expected = destination / "test.txt";
            CHECK(std::filesystem::exists(expected));
            CHECK(std::filesystem::is_regular_file(expected));
            CHECK_EQ(read_file(expected), "data");
        }

        SUBCASE("move directory empty")
        {
            const auto directory = source / "directory";
            std::filesystem::create_directories(directory);

            manager->add(vfs::move_task{.source = directory, .destination = destination});
            sync.wait();

            CHECK(manager->empty());

            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, 1);

            CHECK(std::filesystem::exists(destination / "directory"));
            CHECK(std::filesystem::is_directory(destination / "directory"));
            CHECK_EQ(count_files(destination / "directory", true), 0);
        }

        SUBCASE("move directory with files")
        {
            const auto directory = source / "directory";

            std::filesystem::create_directories(directory);
            std::size_t loop = 100;
            for (const auto i : std::views::iota(0uz, loop))
            {
                const auto path = directory / std::format("{}", i);
                create_file(path, std::format("{}", i));
            }

            manager->add(vfs::move_task{.source = directory, .destination = destination});
            sync.wait();

            CHECK(manager->empty());

            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, 1);

            CHECK_EQ(count_files(destination / "directory", true), loop);

            for (const auto& entry : std::filesystem::directory_iterator(destination / "directory"))
            {
                CHECK(std::filesystem::exists(entry.path()));
                CHECK(std::filesystem::is_regular_file(entry.path()));
                CHECK_EQ(read_file(entry.path()), entry.path().filename());
            }
        }

        SUBCASE("move nested directory")
        {
            create_file(source / "directory/a.txt");
            create_file(source / "directory/b.txt");
            create_file(source / "directory/nested/c.txt");
            create_file(source / "directory/nested/d.txt");

            manager->add(
                vfs::move_task{.source = source / "directory", .destination = destination});
            sync.wait();

            CHECK(manager->empty());

            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, 1);

            CHECK(std::filesystem::exists(destination / "directory/a.txt"));
            CHECK(std::filesystem::exists(destination / "directory/b.txt"));
            CHECK(std::filesystem::exists(destination / "directory/nested/c.txt"));
            CHECK(std::filesystem::exists(destination / "directory/nested/d.txt"));
            CHECK_EQ(count_files(destination / "directory", true), 5);
        }

        SUBCASE("move directory merge")
        {
            create_file(source / "directory/a.txt");
            create_file(source / "directory/b.txt");
            create_file(destination / "directory/c.txt");
            create_file(destination / "directory/d.txt");

            manager->add(
                vfs::move_task{.source = source / "directory", .destination = destination});
            sync.wait();

            CHECK(manager->empty());

            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, 1);

            CHECK(std::filesystem::exists(destination / "directory/a.txt"));
            CHECK(std::filesystem::exists(destination / "directory/b.txt"));
            CHECK(std::filesystem::exists(destination / "directory/c.txt"));
            CHECK(std::filesystem::exists(destination / "directory/d.txt"));
        }

        SUBCASE("move directory merge nested")
        {
            create_file(source / "directory/a.txt");
            create_file(source / "directory/nested/a.txt");
            create_file(destination / "directory/b.txt");
            create_file(destination / "directory/nested/b.txt");

            manager->add(
                vfs::move_task{.source = source / "directory", .destination = destination});
            sync.wait();

            CHECK(manager->empty());

            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, 1);

            CHECK(std::filesystem::exists(destination / "directory/a.txt"));
            CHECK(std::filesystem::exists(destination / "directory/nested/a.txt"));
            CHECK(std::filesystem::exists(destination / "directory/b.txt"));
            CHECK(std::filesystem::exists(destination / "directory/nested/b.txt"));
        }

        if (std::filesystem::exists(test_path))
        {
            std::filesystem::remove_all(test_path);
        }
    }

    TEST_CASE("vfs::rename_task")
    {
        const auto test_path = root / "rename_task";
        const auto source = test_path / "src";
        const auto destination = test_path / "dest";

        if (std::filesystem::exists(test_path))
        {
            std::filesystem::remove_all(test_path);
        }

        std::filesystem::create_directories(source);
        std::filesystem::create_directories(destination);

        test_sync sync;

        auto manager = vfs::task_manager::create();
        manager->signal_task_finished().connect([&](std::uint64_t task_id)
                                                { sync.notify_success(task_id); });
        manager->signal_task_error().connect([&](const vfs::task_error& error)
                                             { sync.notify_error(error); });
        manager->signal_task_collision().connect(
            [](const vfs::task_collision& c)
            { c.resolved(c.task_id, vfs::collision_resolve::skip, {}); });

        SUBCASE("rename file")
        {
            const auto file = source / "test.txt";
            create_file(file, "data");

            manager->add(
                vfs::rename_task{.source = file, .destination = destination / "renamed.txt"});
            sync.wait();

            CHECK(manager->empty());

            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, 1);

            const auto expected = destination / "renamed.txt";
            CHECK_FALSE(std::filesystem::exists(file));
            CHECK(std::filesystem::exists(expected));
            CHECK(std::filesystem::is_regular_file(expected));
            CHECK_EQ(read_file(expected), "data");
        }

        SUBCASE("rename directory empty")
        {
            const auto directory = source / "directory";
            std::filesystem::create_directories(directory);

            manager->add(
                vfs::rename_task{.source = directory, .destination = destination / "renamed"});
            sync.wait();

            CHECK(manager->empty());

            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, 1);

            CHECK_FALSE(std::filesystem::exists(directory));
            CHECK(std::filesystem::exists(destination / "renamed"));
            CHECK(std::filesystem::is_directory(destination / "renamed"));
            CHECK_EQ(count_files(destination / "renamed", true), 0);
        }

        SUBCASE("rename directory with files")
        {
            const auto directory = source / "directory";

            std::filesystem::create_directories(directory);
            std::size_t loop = 100;
            for (const auto i : std::views::iota(0uz, loop))
            {
                const auto path = directory / std::format("{}", i);
                create_file(path, std::format("{}", i));
            }

            manager->add(
                vfs::rename_task{.source = directory, .destination = destination / "renamed"});
            sync.wait();

            CHECK(manager->empty());

            CHECK_EQ(sync.error, 0);
            CHECK_EQ(sync.completed, 1);

            CHECK_EQ(count_files(destination / "renamed", true), loop);

            for (const auto& entry : std::filesystem::directory_iterator(destination / "renamed"))
            {
                CHECK(std::filesystem::exists(entry.path()));
                CHECK(std::filesystem::is_regular_file(entry.path()));
                CHECK_EQ(read_file(entry.path()), entry.path().filename());
            }
        }

        if (std::filesystem::exists(test_path))
        {
            std::filesystem::remove_all(test_path);
        }
    }
}
