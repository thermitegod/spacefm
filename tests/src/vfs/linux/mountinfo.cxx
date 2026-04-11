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

#include <filesystem>
#include <string>

#include <doctest/doctest.h>

#include "vfs/linux/mountinfo.hxx"

TEST_SUITE("vfs::proc" * doctest::description(""))
{
    const auto root = std::filesystem::path() / TEST_DATA_PATH / "vfs/linux";

    TEST_CASE("vfs::proc::mountinfo()")
    {
        REQUIRE(std::filesystem::exists(root));

        SUBCASE("empty")
        {
            auto entry = vfs::proc::mountinfo_entry::create("");

            CHECK_FALSE(entry);
        }

        SUBCASE("bad line")
        {
            // clang-format off
            auto entry = vfs::proc::mountinfo_entry::create("48 1 0:24 / / rw,noatime shared:1 - zfs zroot/ROOT/gentoo");
            // clang-format on

            CHECK_FALSE(entry);
        }

        SUBCASE("zfs")
        {
            // clang-format off
            const std::string line  = "48 1 0:24 / / rw,noatime shared:1 - zfs zroot/ROOT/gentoo rw,xattr,posixacl,casesensitive";
            // clang-format on

            auto entry = vfs::proc::mountinfo_entry::create(line);

            REQUIRE(entry);

            // clang-format off
            CHECK_EQ(entry->mount_id(), 48);
            CHECK_EQ(entry->parent_id(), 1);
            CHECK_EQ(entry->major(), 0);
            CHECK_EQ(entry->minor(), 24);
            CHECK_EQ(entry->root(), "/");
            CHECK_EQ(entry->mount_point(), "/");
            CHECK_EQ(entry->mount_options(), "rw,noatime");
            CHECK_EQ(entry->optional_fields(), "shared:1");
            CHECK_EQ(entry->filesystem_type(), "zfs");
            CHECK_EQ(entry->mount_source(), "zroot/ROOT/gentoo");
            CHECK_EQ(entry->super_options(), "rw,xattr,posixacl,casesensitive");
            // clang-format on
        }

        SUBCASE("ext4")
        {
            // clang-format off
            const std::string line  = "32 2 259:2 / / rw,relatime shared:1 - ext4 /dev/nvme0n1p2 rw";
            // clang-format on

            auto entry = vfs::proc::mountinfo_entry::create(line);

            REQUIRE(entry);

            // clang-format off
            CHECK_EQ(entry->mount_id(), 32);
            CHECK_EQ(entry->parent_id(), 2);
            CHECK_EQ(entry->major(), 259);
            CHECK_EQ(entry->minor(), 2);
            CHECK_EQ(entry->root(), "/");
            CHECK_EQ(entry->mount_point(), "/");
            CHECK_EQ(entry->mount_options(), "rw,relatime");
            CHECK_EQ(entry->optional_fields(), "shared:1");
            CHECK_EQ(entry->filesystem_type(), "ext4");
            CHECK_EQ(entry->mount_source(), "/dev/nvme0n1p2");
            CHECK_EQ(entry->super_options(), "rw");
            // clang-format on
        }

        SUBCASE("sshfs")
        {
            // clang-format off
            const std::string line  = "280 32 0:113 / /home/brandon/media/sshfs rw,nosuid,nodev,relatime shared:254 - fuse.sshfs brandon@192.168.0.244:/mnt/anime rw,user_id=1000,group_id=1000,default_permissions,allow_other";
            // clang-format on

            auto entry = vfs::proc::mountinfo_entry::create(line);

            REQUIRE(entry);

            // clang-format off
            CHECK_EQ(entry->mount_id(), 280);
            CHECK_EQ(entry->parent_id(), 32);
            CHECK_EQ(entry->major(), 0);
            CHECK_EQ(entry->minor(), 113);
            CHECK_EQ(entry->root(), "/");
            CHECK_EQ(entry->mount_point(), "/home/brandon/media/sshfs");
            CHECK_EQ(entry->mount_options(), "rw,nosuid,nodev,relatime");
            CHECK_EQ(entry->optional_fields(), "shared:254");
            CHECK_EQ(entry->filesystem_type(), "fuse.sshfs");
            CHECK_EQ(entry->mount_source(), "brandon@192.168.0.244:/mnt/anime");
            CHECK_EQ(entry->super_options(), "rw,user_id=1000,group_id=1000,default_permissions,allow_other");
            // clang-format on
        }

        SUBCASE("empty file")
        {
            auto entries = vfs::proc::mountinfo(root / "mountinfo-empty");

            CHECK(entries.empty());
        }

        SUBCASE("ubuntu")
        {
            auto entries = vfs::proc::mountinfo(root / "mountinfo-ubuntu");

            CHECK(!entries.empty());
        }
    }
}
