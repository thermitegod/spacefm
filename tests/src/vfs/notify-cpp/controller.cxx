/*
 * Copyright (c) 2017 Erik Zenker <erikzenker@hotmail.com>
 * Copyright (c) 2018 Rafael Sadowski <rafael@sizeofvoid.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stop_token>
#include <thread>

#include <doctest/doctest.h>

#include <ztd/ztd.hxx>

#include "vfs/notify-cpp/controller.hxx"

#include "vfs/utils/file-ops.hxx"

struct event_counter
{
    std::int32_t access = 0;
    std::int32_t modify = 0;
    std::int32_t attrib = 0;
    std::int32_t close_write = 0;
    std::int32_t close_nowrite = 0;
    std::int32_t open = 0;
    std::int32_t moved_from = 0;
    std::int32_t moved_to = 0;
    std::int32_t create = 0;
    std::int32_t delete_sub = 0;
    std::int32_t delete_self = 0;
    std::int32_t move_self = 0;

    std::int32_t umount = 0;
    std::int32_t queue_overflow = 0;
    std::int32_t ignored = 0;

    std::int32_t close = 0;
    std::int32_t move = 0;

    void
    reset() noexcept
    {
        access = 0;
        modify = 0;
        attrib = 0;
        close_write = 0;
        close_nowrite = 0;
        open = 0;
        moved_from = 0;
        moved_to = 0;
        create = 0;
        delete_sub = 0;
        delete_self = 0;

        move_self = 0;
        umount = 0;
        queue_overflow = 0;

        ignored = 0;
        close = 0;
        move = 0;
    }
};

static void
create_file(const std::filesystem::path& path) noexcept
{
    using namespace std::string_literals;

    auto result = vfs::utils::write_file(path, "data"s);
    ztd::panic_if(bool(result), "Test Suite bad write");
}

static std::string
read_file(const std::filesystem::path& path) noexcept
{
    auto data = vfs::utils::read_file(path);
    ztd::panic_if(!data, "Test Suite bad read");
    return *data;
}

TEST_SUITE("notify-cpp" * doctest::description(""))
{
    const auto root = std::filesystem::temp_directory_path() / PACKAGE_NAME / "notify-cpp";

    TEST_CASE("notify-cpp")
    {
        const auto test_path = root / "notify";
        if (std::filesystem::exists(test_path))
        {
            std::filesystem::remove_all(test_path);
        }
        std::filesystem::create_directories(test_path);

        /////////////

        using namespace std::chrono_literals;

        event_counter counter{};

        auto notifier = notify::controller(test_path);
        // clang-format off
        notifier.signal_access().connect([&](const auto&) { counter.access++; });
        notifier.signal_modify().connect([&](const auto&) { counter.modify++; });
        notifier.signal_attrib().connect([&](const auto&) { counter.attrib++; });
        notifier.signal_close_write().connect([&](const auto&) { counter.close_write++; });
        notifier.signal_close_nowrite().connect([&](const auto&) { counter.close_nowrite++; });
        notifier.signal_open().connect([&](const auto&) { counter.open++; });
        notifier.signal_moved_from().connect([&](const auto&) { counter.moved_from++; });
        notifier.signal_moved_to().connect([&](const auto&) { counter.moved_to++; });
        notifier.signal_create().connect([&](const auto&) { counter.create++; });
        notifier.signal_delete().connect([&](const auto&) { counter.delete_sub++; });
        notifier.signal_delete_self().connect([&](const auto&) { counter.delete_self++; });
        notifier.signal_move_self().connect([&](const auto&) { counter.move_self++; });
        notifier.signal_umount().connect([&](const auto&) { counter.umount++; });
        notifier.signal_queue_overflow().connect([&](const auto&) { counter.queue_overflow++; });
        notifier.signal_ignored().connect([&](const auto&) { counter.ignored++; });
        notifier.signal_close().connect([&](const auto&) { counter.close++; });
        notifier.signal_move().connect([&](const auto&) { counter.move++; });
        // clang-format on

        std::jthread thread([&notifier](const std::stop_token& stoken) { notifier.run(stoken); });

        REQUIRE_EQ(counter.access, 0);
        REQUIRE_EQ(counter.modify, 0);
        REQUIRE_EQ(counter.attrib, 0);
        REQUIRE_EQ(counter.close_write, 0);
        REQUIRE_EQ(counter.close_nowrite, 0);
        REQUIRE_EQ(counter.open, 0);
        REQUIRE_EQ(counter.moved_from, 0);
        REQUIRE_EQ(counter.moved_to, 0);
        REQUIRE_EQ(counter.create, 0);
        REQUIRE_EQ(counter.delete_sub, 0);
        REQUIRE_EQ(counter.delete_self, 0);
        REQUIRE_EQ(counter.move_self, 0);
        REQUIRE_EQ(counter.umount, 0);
        REQUIRE_EQ(counter.queue_overflow, 0);
        REQUIRE_EQ(counter.ignored, 0);
        REQUIRE_EQ(counter.close, 0);
        REQUIRE_EQ(counter.move, 0);

        /////////////////////////////////////////////////////

        SUBCASE("no events")
        {
            CHECK_EQ(counter.access, 0);
            CHECK_EQ(counter.modify, 0);
            CHECK_EQ(counter.attrib, 0);
            CHECK_EQ(counter.close_write, 0);
            CHECK_EQ(counter.close_nowrite, 0);
            CHECK_EQ(counter.open, 0);
            CHECK_EQ(counter.moved_from, 0);
            CHECK_EQ(counter.moved_to, 0);
            CHECK_EQ(counter.create, 0);
            CHECK_EQ(counter.delete_sub, 0);
            CHECK_EQ(counter.delete_self, 0);
            CHECK_EQ(counter.move_self, 0);
            CHECK_EQ(counter.umount, 0);
            CHECK_EQ(counter.queue_overflow, 0);
            CHECK_EQ(counter.ignored, 0);
            CHECK_EQ(counter.close, 0);
            CHECK_EQ(counter.move, 0);
        }

        SUBCASE("create")
        {
            create_file(test_path / "create.test");
            std::this_thread::sleep_for(50ms);

            CHECK_EQ(counter.access, 0);
            CHECK_EQ(counter.modify, 1);
            CHECK_EQ(counter.attrib, 0);
            CHECK_EQ(counter.close_write, 1);
            CHECK_EQ(counter.close_nowrite, 0);
            CHECK_EQ(counter.open, 1);
            CHECK_EQ(counter.moved_from, 0);
            CHECK_EQ(counter.moved_to, 0);
            CHECK_EQ(counter.create, 1);
            CHECK_EQ(counter.delete_sub, 0);
            CHECK_EQ(counter.delete_self, 0);
            CHECK_EQ(counter.move_self, 0);
            CHECK_EQ(counter.umount, 0);
            CHECK_EQ(counter.queue_overflow, 0);
            CHECK_EQ(counter.ignored, 0);
            CHECK_EQ(counter.close, 1);
            CHECK_EQ(counter.move, 0);

            SUBCASE("read")
            {
                counter.reset();

                auto _ = read_file(test_path / "create.test");
                std::this_thread::sleep_for(50ms);

                CHECK_EQ(counter.access, 1);
                CHECK_EQ(counter.modify, 0);
                CHECK_EQ(counter.attrib, 0);
                CHECK_EQ(counter.close_write, 0);
                CHECK_EQ(counter.close_nowrite, 1);
                CHECK_EQ(counter.open, 1);
                CHECK_EQ(counter.moved_from, 0);
                CHECK_EQ(counter.moved_to, 0);
                CHECK_EQ(counter.create, 0);
                CHECK_EQ(counter.delete_sub, 0);
                CHECK_EQ(counter.delete_self, 0);
                CHECK_EQ(counter.move_self, 0);
                CHECK_EQ(counter.umount, 0);
                CHECK_EQ(counter.queue_overflow, 0);
                CHECK_EQ(counter.ignored, 0);
                CHECK_EQ(counter.close, 1);
                CHECK_EQ(counter.move, 0);
            }

            SUBCASE("delete")
            {
                counter.reset();

                std::filesystem::remove(test_path / "create.test");
                std::this_thread::sleep_for(50ms);

                CHECK_EQ(counter.access, 0);
                CHECK_EQ(counter.modify, 0);
                CHECK_EQ(counter.attrib, 0);
                CHECK_EQ(counter.close_write, 0);
                CHECK_EQ(counter.close_nowrite, 0);
                CHECK_EQ(counter.open, 0);
                CHECK_EQ(counter.moved_from, 0);
                CHECK_EQ(counter.moved_to, 0);
                CHECK_EQ(counter.create, 0);
                CHECK_EQ(counter.delete_sub, 1);
                CHECK_EQ(counter.delete_self, 0);
                CHECK_EQ(counter.move_self, 0);
                CHECK_EQ(counter.umount, 0);
                CHECK_EQ(counter.queue_overflow, 0);
                CHECK_EQ(counter.ignored, 0);
                CHECK_EQ(counter.close, 0);
                CHECK_EQ(counter.move, 0);
            }
        }

        thread.request_stop();
        thread.join();

        if (std::filesystem::exists(test_path))
        {
            std::filesystem::remove_all(test_path);
        }
    }
}
