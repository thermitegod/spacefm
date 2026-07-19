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

#include <filesystem>
#include <memory>
#include <span>

#include <gtkmm.h>

#include "vfs/file.hxx"

namespace gui::archiver
{
void create(Gtk::ApplicationWindow& parent,
            const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept;

void extract(Gtk::ApplicationWindow& parent,
             const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept;

void extract_to(Gtk::ApplicationWindow& parent,
                const std::span<const std::shared_ptr<vfs::file>> selected_files,
                const std::filesystem::path& dest_dir) noexcept;

void open(Gtk::ApplicationWindow& parent,
          const std::span<const std::shared_ptr<vfs::file>> selected_files) noexcept;
} // namespace gui::archiver
