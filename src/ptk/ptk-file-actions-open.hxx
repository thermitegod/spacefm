/**
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

#include <string_view>

#include <filesystem>

#include <span>

#include <gtkmm.h>

#include "ptk/ptk-file-browser.hxx"

#include "vfs/vfs-file.hxx"

// if app_desktop is empty each file will be opened with its default application
// if xforce, force execute of executable ignoring app_settings.click_executes
// if xnever, never execute an executable
void ptk_open_files_with_app(const std::filesystem::path& cwd,
                             const std::span<const std::shared_ptr<vfs::file>> selected_files,
                             const std::string_view app_desktop, PtkFileBrowser* file_browser,
                             bool xforce, bool xnever);
