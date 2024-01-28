/**
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#include <memory>

#include <gtkmm.h>

#include "ptk/ptk-file-browser.hxx"
#include "ptk/ptk-file-menu.hxx"

#include "vfs/vfs-file.hxx"

namespace ptk::action
{
enum class rename_mode
{
    rename,
    new_file,
    new_dir,
    new_link,
};

i32 rename_files(ptk::browser* file_browser, const std::filesystem::path& file_dir,
                 const std::shared_ptr<vfs::file>& file, const char* dest_dir, bool clip_copy,
                 ptk::action::rename_mode create_new, AutoOpenCreate* auto_open = nullptr) noexcept;
} // namespace ptk::action
