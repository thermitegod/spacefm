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

#pragma once

#include <filesystem>
#include <string_view>

#include <doctest/doctest.h>

#include <ztd/ztd.hxx>

#include "vfs/utils/file-ops.hxx"

inline void
create_file(const std::filesystem::path& path, const std::string_view content = "data") noexcept
{
    std::filesystem::create_directories(path.parent_path());

    auto result = vfs::utils::write_file(path, content);
    REQUIRE_EQ(result, std::error_code{});
    REQUIRE(std::filesystem::exists(path));
}

inline std::string
read_file(const std::filesystem::path& path) noexcept
{
    REQUIRE(std::filesystem::exists(path));
    auto data = vfs::utils::read_file(path);
    REQUIRE(data);
    return *data;
}
