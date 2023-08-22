/**
 * Copyright (C) 2013-2014 OmegaPhil <OmegaPhil@startmail.com>
 * Copyright (C) 2014 IgnorantGuru <ignorantguru@gmx.com>
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

#include <span>

#include <gtk/gtk.h>

#include "ptk/ptk-file-browser.hxx"

namespace ptk::file_archiver
{
    enum archive // TODO class
    {            // Archive operations enum
        compress,
        extract,
        list,
    };
}

// Pass file_browser or desktop depending on where you are calling from
void ptk_file_archiver_create(PtkFileBrowser* file_browser,
                              const std::span<const vfs::file_info> sel_files,
                              const std::filesystem::path& cwd);
void ptk_file_archiver_extract(PtkFileBrowser* file_browser,
                               const std::span<const vfs::file_info> sel_files,
                               const std::filesystem::path& cwd,
                               const std::filesystem::path& dest_dir, i32 job,
                               bool archive_presence_checked);
