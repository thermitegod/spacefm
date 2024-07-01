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

#include <gtest/gtest.h>

#include "spacefm/vfs/utils/vfs-utils.hxx"

const std::filesystem::path test_data_path = TEST_DATA_PATH;

/**
 * vfs::utils::unique_name
 */

TEST(vfs_utils, unique_name__file_missing_extension)
{
    const auto path = test_data_path / "vfs/utils/unique_name/file-extension-missing";
    const std::filesystem::path filename = "test";

    const auto result_wanted = path / "test-copy11";

    const auto result = vfs::utils::unique_name(path, filename, "-copy");

    EXPECT_EQ(result, result_wanted);
}

TEST(vfs_utils, unique_name__file_multiple_extension)
{
    const auto path = test_data_path / "vfs/utils/unique_name/file-extension-multiple";
    const std::filesystem::path filename = "test.tar.gz";

    const auto result_wanted = path / "test-copy11.tar.gz";

    const auto result = vfs::utils::unique_name(path, filename, "-copy");

    EXPECT_EQ(result, result_wanted);
}

TEST(vfs_utils, unique_name__file_single_extension)
{
    const auto path = test_data_path / "vfs/utils/unique_name/file-extension-single";
    const std::filesystem::path filename = "test.txt";

    const auto result_wanted = path / "test-copy11.txt";

    const auto result = vfs::utils::unique_name(path, filename, "-copy");

    EXPECT_EQ(result, result_wanted);
}

TEST(vfs_utils, unique_name__directory)
{
    const auto path = test_data_path / "vfs/utils/unique_name/directory";
    const std::filesystem::path filename = "test";

    const auto result_wanted = path / "test-copy11";

    const auto result = vfs::utils::unique_name(path, filename, "-copy");

    EXPECT_EQ(result, result_wanted);
}

/**
 * vfs::utils::split_basename_extension
 */

TEST(vfs_utils, split_basename_extension__empty)
{
    const auto result = vfs::utils::split_basename_extension("");

    EXPECT_EQ(result.basename, "");
    EXPECT_EQ(result.extension, "");
    EXPECT_EQ(result.is_multipart_extension, false);
}

TEST(vfs_utils, split_basename_extension__missing_extension)
{
    const auto result = vfs::utils::split_basename_extension("test");

    EXPECT_EQ(result.basename, "test");
    EXPECT_EQ(result.extension, "");
    EXPECT_EQ(result.is_multipart_extension, false);
}

TEST(vfs_utils, split_basename_extension__multiple_extension)
{
    const auto result = vfs::utils::split_basename_extension("test.tar.gz");

    EXPECT_EQ(result.basename, "test");
    EXPECT_EQ(result.extension, ".tar.gz");
    EXPECT_EQ(result.is_multipart_extension, true);
}

TEST(vfs_utils, split_basename_extension__single_extension)
{
    const auto result = vfs::utils::split_basename_extension("test.txt");

    EXPECT_EQ(result.basename, "test");
    EXPECT_EQ(result.extension, ".txt");
    EXPECT_EQ(result.is_multipart_extension, false);
}
