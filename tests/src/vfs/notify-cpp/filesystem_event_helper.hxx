/*
 * Copyright (c) 2019 Rafael Sadowski <rafael@sizeofvoid.org>
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

#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>

#include <doctest/doctest.h>

#include "vfs/notify-cpp/notification.hxx"

#include "vfs/utils/file-ops.hxx"

using namespace notify;

inline void
open_file(const std::filesystem::path& file)
{
    using namespace std::string_literals;

    auto _ = vfs::utils::write_file(file, "Writing this to a file.\n"s);
}

struct FilesystemEventHelper
{
    FilesystemEventHelper()
    {
        if (!std::filesystem::exists(this->test_directory_))
        {
            std::filesystem::create_directories(this->test_directory_);
            // std::filesystem::create_directories(this->recursive_test_directory_);
        }

        // empty files
        std::ofstream(test_file_one_).close();
        std::ofstream(test_file_two_).close();
    }

    ~FilesystemEventHelper()
    {
        if (std::filesystem::exists(this->test_directory_))
        {
            std::filesystem::remove_all(this->test_directory_);
        }
    }

    // fanotify test suite has to be run as root
    std::filesystem::path test_directory_ =
        std::filesystem::temp_directory_path() /
        (getuid() == 0 ? std::format("{}_test_suite_root", PACKAGE_NAME)
                       : std::format("{}_test_suite", PACKAGE_NAME));

    std::filesystem::path recursive_test_directory_ = test_directory_ / "recursive";
    std::filesystem::path test_file_one_ = test_directory_ / "test1.txt";
    std::filesystem::path test_file_two_ = test_directory_ / "test2.txt";

    std::chrono::seconds timeout_ = std::chrono::seconds(1);

    // Events
    std::promise<size_t> promised_counter_;
    std::promise<notification> promised_open_;
    std::promise<notification> promised_close_no_write_;
};
