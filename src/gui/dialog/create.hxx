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

#include "gui/file-browser.hxx"
#include "gui/file-menu.hxx"

#include "vfs/file.hxx"

namespace gui::dialog
{
enum class create_mode : std::uint8_t
{
    file,
    dir,
    link,
};

i32 create_files(gui::browser* browser, const std::filesystem::path& cwd,
                 const std::shared_ptr<vfs::file>& file, const gui::dialog::create_mode init_mode,
                 AutoOpenCreate* ao) noexcept;
} // namespace gui::dialog
