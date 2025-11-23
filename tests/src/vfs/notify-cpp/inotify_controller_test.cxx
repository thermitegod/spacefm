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
#include <future>
#include <stdexcept>
#include <stop_token>
#include <thread>

#define DOCTEST_CONFIG_DOUBLE_STRINGIFY
#include <doctest/doctest.h>

#include "vfs/notify-cpp/event.hxx"
#include "vfs/notify-cpp/notify_controller.hxx"

#include "doctest_utils.hxx"
#include "filesystem_event_helper.hxx"

/*
 * The test cases based on the original work from Erik Zenker for inotify-cpp.
 * In to guarantee a compatibility with inotify-cpp the tests were mostly
 * unchanged.
 */
using namespace notify;

TEST_CASE_FIXTURE(FilesystemEventHelper, "shouldNotAcceptNotExistingPaths")
{
    CHECK_THROWS_AS(
        inotify_controller().watch_path_recursively({"/not/existing/path/", event::all}),
        std::invalid_argument);
    CHECK_THROWS_AS(inotify_controller().watch_file({"/not/existing/file", event::all}),
                    std::invalid_argument);
}

TEST_CASE_FIXTURE(FilesystemEventHelper, "shouldNotifyOnOpenEvent")
{
    notify_controller notifier = inotify_controller();
    notifier.watch_file({test_file_one_, event::close})
        .on_event(event::close,
                  [&](const notification& notification) { promised_open_.set_value(notification); })
        .on_unexpected_event(
            [](const notification& notification)
            { CHECK_MESSAGE(false, std::format("unexpected event: {}", notification.event())); });

    std::jthread thread([&notifier](const std::stop_token& stoken) { notifier.run_once(stoken); });

    open_file(test_file_one_);

    auto future_open_event = promised_open_.get_future();
    CHECK(future_open_event.wait_for(timeout_) == std::future_status::ready);
    const auto notify = future_open_event.get();
    CHECK(notify.event() == event::close);
    CHECK(notify.path() == test_file_one_);

    thread.request_stop();
    thread.join();
}

TEST_CASE_FIXTURE(FilesystemEventHelper, "shouldNotifyOnMultipleEvents")
{
    inotify_controller notifier = inotify_controller();

    event watch_on = event::open | event::close_write;
    CHECK((watch_on & event::close_write) == event::close_write);
    CHECK((watch_on & event::open) == event::open);
    CHECK((watch_on & event::moved_from) != event::moved_from);

    notifier.watch_file({test_file_one_, watch_on})
        .on_event(event::open,
                  [&](const notification& notification) { promised_open_.set_value(notification); })
        .on_event(event::close_write,
                  [&](const notification& notification)
                  { promised_close_no_write_.set_value(notification); })
        .on_unexpected_event(
            [](const notification& notification)
            { CHECK_MESSAGE(false, std::format("unexpected event: {}", notification.event())); });

    std::jthread thread(
        [&notifier](const std::stop_token& stoken)
        {
            notifier.run_once(stoken);
            notifier.run_once(stoken);
        });

    open_file(test_file_one_);

    auto future_open = promised_open_.get_future();
    auto future_close_no_write = promised_close_no_write_.get_future();
    CHECK(future_open.wait_for(timeout_) == std::future_status::ready);
    CHECK(future_open.get().event() == event::open);
    CHECK(future_close_no_write.wait_for(timeout_) == std::future_status::ready);
    CHECK(future_close_no_write.get().event() == event::close_write);

    thread.request_stop();
    thread.join();
}

TEST_CASE_FIXTURE(FilesystemEventHelper, "shouldStopRunOnce")
{
    notify_controller notifier = inotify_controller();
    notifier.watch_file(test_file_one_);

    std::jthread thread([&notifier](const std::stop_token& stoken) { notifier.run_once(stoken); });

    thread.request_stop();
    thread.join();
}

TEST_CASE_FIXTURE(FilesystemEventHelper, "shouldStopRun")
{
    inotify_controller notifier = inotify_controller();
    notifier.watch_file(test_file_one_);

    std::jthread thread([&notifier](const std::stop_token& stoken) { notifier.run(stoken); });

    thread.request_stop();
    thread.join();
}

TEST_CASE_FIXTURE(FilesystemEventHelper, "shouldIgnoreFileOnce")
{
    size_t counter = 0;
    inotify_controller notifier = inotify_controller();
    notifier.watch_file({test_file_one_, event::open})
        .ignore_once(test_file_one_)
        .on_event(event::open,
                  [&](const notification& notification)
                  {
                      (void)notification;

                      ++counter;
                      if (counter == 1)
                      {
                          promised_counter_.set_value(counter);
                      }
                  })
        .on_unexpected_event(
            [](const notification& notification)
            { CHECK_MESSAGE(false, std::format("unexpected event: {}", notification.event())); });

    std::jthread thread([&notifier](const std::stop_token& stoken) { notifier.run(stoken); });

    // Known bug if the events to fast on the same file inotify(7) create only one event so we have to wait
    open_file(test_file_one_);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    open_file(test_file_one_);

    auto future_open = promised_counter_.get_future();
    CHECK(future_open.wait_for(std::chrono::seconds(1)) == std::future_status::ready);

    thread.request_stop();
    thread.join();
}

TEST_CASE_FIXTURE(FilesystemEventHelper, "shouldIgnoreFile")
{
    notify_controller notifier = inotify_controller();
    notifier.ignore(test_file_one_)
        .watch_file({test_file_one_, event::close})
        .on_event(event::close,
                  [&](const notification& notification) { promised_open_.set_value(notification); })
        .on_unexpected_event(
            [](const notification& notification)
            { CHECK_MESSAGE(false, std::format("unexpected event: {}", notification.event())); });

    std::jthread thread([&notifier](const std::stop_token& stoken) { notifier.run_once(stoken); });

    open_file(test_file_one_);

    auto future_open_event = promised_open_.get_future();
    CHECK(future_open_event.wait_for(timeout_) == std::future_status::timeout);

    thread.request_stop();
    thread.join();
}

TEST_CASE_FIXTURE(FilesystemEventHelper, "shouldWatchPathRecursively")
{
    inotify_controller notifier = inotify_controller();
    notifier.watch_path_recursively({test_directory_, event::open})
        .on_event(event::open,
                  [&](const notification& notification) { promised_open_.set_value(notification); })
        .on_unexpected_event(
            [](const notification& notification)
            { CHECK_MESSAGE(false, std::format("unexpected event: {}", notification.event())); });

    std::jthread thread([&notifier](const std::stop_token& stoken) { notifier.run_once(stoken); });

    open_file(test_file_one_);

    auto future_open = promised_open_.get_future();
    CHECK(future_open.wait_for(timeout_) == std::future_status::ready);

    thread.request_stop();
    thread.join();
}

TEST_CASE_FIXTURE(FilesystemEventHelper, "shouldUnwatchPath")
{
    inotify_controller notifier = inotify_controller();
    notifier.watch_file(test_file_one_).unwatch(test_file_one_);

    std::jthread thread([&notifier](const std::stop_token& stoken) { notifier.run_once(stoken); });

    open_file(test_file_one_);
    CHECK(promised_open_.get_future().wait_for(timeout_) != std::future_status::ready);

    thread.request_stop();
    thread.join();
}

TEST_CASE_FIXTURE(FilesystemEventHelper, "shouldCallUserDefinedUnexpectedExceptionObserver")
{
    std::promise<void> observer_called;

    notify_controller notifier = inotify_controller();
    notifier.watch_file(test_file_one_)
        .on_unexpected_event([&](const notification&) { observer_called.set_value(); });

    std::jthread thread([&notifier](const std::stop_token& stoken) { notifier.run_once(stoken); });

    open_file(test_file_one_);

    CHECK(observer_called.get_future().wait_for(timeout_) == std::future_status::ready);

    thread.request_stop();
    thread.join();
}

TEST_CASE_FIXTURE(FilesystemEventHelper, "countEvents")
{
    size_t counter = 0;
    inotify_controller notifier = inotify_controller();
    notifier.watch_file({test_file_one_, event::open})
        .on_event(event::open,
                  [&](const notification& notification)
                  {
                      (void)notification;

                      ++counter;
                      if (counter == 2)
                      {
                          promised_counter_.set_value(counter);
                      }
                  })
        .on_unexpected_event(
            [](const notification& notification)
            { CHECK_MESSAGE(false, std::format("unexpected event: {}", notification.event())); });

    std::jthread thread([&notifier](const std::stop_token& stoken) { notifier.run(stoken); });

    // Known bug if the events to fast on the same file inotify(7) create only one event so we have to wait
    open_file(test_file_one_);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    open_file(test_file_one_);

    auto future_open = promised_counter_.get_future();
    CHECK(future_open.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    CHECK(future_open.get() == 2);

    thread.request_stop();
    thread.join();
}
